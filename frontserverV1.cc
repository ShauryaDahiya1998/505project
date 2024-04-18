#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <iostream>
#include <algorithm>
#include <csignal>
#include <set>
#include <vector>
#include <atomic>
#include <map>
#include <mutex>
#include <sys/file.h>
#include <fstream>
#include <ctime>
#include <dirent.h>
#include <iomanip>
#include <sys/stat.h>
#include <stdlib.h>
#include <memory>
#include "customlib.h"
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include "gen-cpp/StorageOps.h"
#include "gen-cpp/KvsCoordOps.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace storage;
using namespace std;



#define PORT 8080
#define BUFFER_SIZE 1024


// Variables to store the debug flag
bool debugFlagGlobal = false;

bool extraCredtFlag = false;  // Flag to enable extra credit features

// Mutex to lock the critical section of updating the currentConnections, threadsId and threadFdMap
mutex mtx;

// Vector to store the thread ids of the worker threads
vector<pthread_t> threadsId;

// Atomic variable to store the current number of connections
atomic<int> currentConnections(0);

const string incorrect_sequence = "503 Bad Sequence of commands\r\n";
const string okay_response = "250 OK\r\n";

// Function to split the input into command and text, considering specific full commands
void splitCommandAndText(const string& input, string& command, string& text) {
    // Normalize the input to uppercase for comparison
    string normalizedInput = input;
    transform(normalizedInput.begin(), normalizedInput.end(), normalizedInput.begin(),[](unsigned char c) { return toupper(c); });

    // Check for specific full commands and handle them accordingly
    if (normalizedInput.rfind("MAIL FROM:", 0) == 0) {
        command = "MAIL FROM:";
    } else if (normalizedInput.rfind("RCPT TO:", 0) == 0) {
        command = "RCPT TO:";
    } else {
        // For other commands, extract the command part up to the first space
        size_t commandEnd = normalizedInput.find(' ');
        if (commandEnd != string::npos) {
            command = normalizedInput.substr(0, commandEnd);
        } else {
            command = normalizedInput; // Entire input is the command if no space found
        }
    }

    // Find the start of the text after the command by skipping spaces
    size_t textStart = input.find_first_not_of(' ', command.length());

    if (textStart != string::npos) {
        // Extract the text following the command
        text = input.substr(textStart);
        // Remove the trailing spaces.
        text.erase(text.find_last_not_of(' ') + 1);
    } else {
        text = ""; // No text follows the command
    }
}

// Function to get the command value
int getCommandValue(const string& command, const string& command_data) {
    // Normalize the command by converting it to uppercase
    string cmd = command;
    if (cmd == "HELO") {  // Starts with "HELO"
        return 1;
    } else if (cmd == "MAIL FROM:") {
        return 2;
    } else if (cmd == "RCPT TO:") {
        return 3;
    } else if (cmd == "DATA") {
        return 4;
    } else if (cmd == "QUIT") {
        return 5;
    } else if (cmd == "RSET") {
        return 6;
    } else if (cmd == "NOOP") {
        return 7;
    } else {
        return -1;  // Unknown command
    }
}


// Signal handler function for main thread
void signalHandlerMainThread(int signal) {
    if (signal == SIGINT) {
        // Send SIGUSR1 to worker threads
        for (pthread_t thread : threadsId) {
          pthread_kill(thread, SIGUSR1);
        }
        // Wait for all worker threads to exit
        for (vector<pthread_t>::iterator it = threadsId.begin(); it != threadsId.end(); ++it) {
           pthread_join(*it, NULL);
        }
      
      //Close the server main thread
      exit(0);
    }

    //If any other signal is received
    else{
      cerr<<"Unknown signal received\r\n";
    
    }
    
}

// Structure to pass the arguments to the worker threads
struct workerArgs{
	int fd;
	int debugFlag;
};

// Map to store the file descriptors of the worker threads
map<pthread_t, int> threadFdMap;

// Signal handler function for worker threads
void signalHandlerWorkerThread(int signal) {
    if (signal == SIGUSR1) {

        string message = "421 Server shutting down\r\n";

        //Print debug message if debug flag is set
        if(debugFlagGlobal == true){
          cerr<<"["<<threadFdMap[pthread_self()]<<"] S: 421 Server shutting down\r\n";
        }

        //Send the message to the client and close the connection
        send(threadFdMap[pthread_self()], (message).c_str(), message.length(), 0);
        close(threadFdMap[pthread_self()]);

        //Delete the thread 
        pthread_exit(NULL);
    }

    //If any other signal is received
    else{
      cerr<<"Unknown signal received\r\n";
    
    }
}

string getWorkerIP(string row, KvsCoordOpsClient client){
    string ip;
    client.get(ip,row);
    return ip;
}

std::tuple<std::shared_ptr<TTransport>, StorageOpsClient> getKVSClient(string ipPort){
    size_t colonPos = ipPort.find(':');
    string ip = ipPort.substr(0, colonPos);
    int port = stoi(ipPort.substr(colonPos + 1));
    shared_ptr<TTransport> socket(new TSocket(ip,port));
    shared_ptr<TTransport> transport(new TBufferedTransport(socket));
    shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
    StorageOpsClient client(protocol);
    transport->open();
    return std::make_tuple(transport, client);
}


// Worker function to handle the client requests
void *worker(void *arg) {

  //use pthread_mask to block SIGINT
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);

  workerArgs *args = (workerArgs *)arg;
  int fd = args->fd;
  int debugFlag = args->debugFlag;

  //Buffer to store the data received from the client
  char buffer[BUFFER_SIZE];

  //String to store the command received from the client
  string tmpcommand = "";
  string command = "";

  //Flag to check if the client has quit
  int clientQuit = 0;
  shared_ptr<TTransport> socket(new TSocket("127.0.0.1", 8000));
  shared_ptr<TTransport> transport(new TBufferedTransport(socket));
  shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
  KvsCoordOpsClient client(protocol);
  transport->open();

  while (true) {
    //Clear the buffer
    memset(buffer, 0, sizeof(buffer));

    //Receive the data from the client
    int len = recv(fd, &buffer, sizeof(buffer), 0);
    
    //If the client has closed the connection
    if (len == 0) {
      cerr << "[" << fd<<"] Connection closed by client "<<fd<<endl;
      break;
    }

    //If there is an error in receiving the data
    else if (len < 0) {
      cerr<<"Error in receiving data from client "<<fd<<endl;
      break;
    }

    //If the data is received successfully, add to tempcommand and process the command
    tmpcommand = buffer;
    string content = "";
    cout<<tmpcommand<<endl;
    // check the type of the method, it is till the first space. Could be GET, POST, PUT, DELETE, HEAD
    if (tmpcommand.find(" ") == string::npos) {
      content = "404 Bad request";
    } else {
      size_t pos = tmpcommand.find(" ");
      string method = tmpcommand.substr(0, pos);
      if (method == "GET") {
        content = getMethodHandler(tmpcommand, client);
      } else if (method == "POST") {
        // separate the body of post method from rest of the command
        size_t pos = tmpcommand.find("\r\n\r\n");
        string body = tmpcommand.substr(pos + 4);
        content = postMethodhandler(tmpcommand, body, client);
      } else if (method == "PUT") {
        content = "PUT method";
      } else if (method == "DELETE") {
        content = "DELETE method";
      } else if (method == "HEAD") {
        content = "HEAD method";
      } else {
        content = "404 Bad request";
      }
    }
    
    // string content = "Hello from the C++ server using pthread";
    // string httpResponse = "HTTP/1.1 200 OK\r\nDate: Fri, 31 Dec 1999 23:59:59 GMT\r\nContent-Type: text/plain\r\nContent-Length: " + to_string(content.length()) + "\r\n\r\n" + content;//Hello from the C++ server using pthread!";
    // cout<<content<<endl;
    send(fd, content.c_str(), content.length(),0);
    tmpcommand = "";
  

    // int commandValue;
    // //If the \r\n is found, continue to process the command


    // string command_type = "";
    // string command_data = "";
     
    // splitCommandAndText(command, command_type, command_data);
    // commandValue = getCommandValue(command_type,command_data);
    
    //   //Print the debug message if the debug flag is set
    //   if (debugFlag == 1) {
    //     cerr << "[" << fd<<"] C: "<<command<<endl;
    //   }
    //   string emailFrom;
    //   string emailTo;
    //   switch(commandValue){
    //     case 1: //HELO
          
    //         break;
    //     default:
    //       send(fd, ((string)("500 Command not implemented\r\n")).c_str(), 29 , 0);
    //       if (debugFlag == 1) {
    //         cerr << "[" << fd<<"] S:502 Command not implemented\r\n";
    //       }
    //       break;
    //   }
    

    // //If the client has quit, break the loop
    // if (clientQuit == 1) {
    //   break;
    // }
  

  }
  //Close the connection and delete the arguments. This is a critical section, so lock the mutex
  mtx.lock();
  transport->close();
  //Print the debug message if the debug flag is set
  if (debugFlag == 1) {
    cerr << "[" << fd<<"] Connection closed\r\n";
  }

  //clearbuffers

  //close the connection
  close(fd);

  //Delete the arguments and update the currentConnections, threadsId and threadFdMap
  delete args;
  currentConnections--;
  threadsId.erase(remove(threadsId.begin(), threadsId.end(), pthread_self()), threadsId.end());
  threadFdMap.erase(pthread_self()); 

  //Unlock the mutex
  mtx.unlock();

  //Exit the thread
  pthread_exit(NULL);
}



int main(int argc, char *argv[])
{

  //Signal handler for SIGINT
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  struct sigaction sa;
  sa.sa_mask = mask;
  sa.sa_handler = signalHandlerMainThread;
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);

  //Signal handler for SIGUSR1
  sigset_t mask1;
  sigemptyset(&mask1);
  sigaddset(&mask1, SIGINT);
  struct sigaction sa1;
  sa1.sa_mask = mask1;
  sa1.sa_handler = signalHandlerWorkerThread;
  sa1.sa_flags = 0;
  sigaction(SIGUSR1, &sa1, nullptr);
  
  //Default port number and debug flag
  int portNumber = 8080;
  int debugFlag = 0;
  
  //Initial number of connections
  currentConnections = 0;

  //Parsing the command line arguments
  int opt;
  while ((opt = getopt(argc, argv, "p:ave")) != -1) {
      switch (opt) {
      case 'v':
          debugFlag = 1;
          debugFlagGlobal = true;
          break;
		
    case 'a':
      cerr << "*** Author: Abhishek Goyal (abhi2358)" <<endl;
      return 1;
    
    case 'e':
        extraCredtFlag = true;
        break;
      
	  case 'p':
	  	//Try catch to check for error if the port number is not an integer
		  try{
			portNumber=stoi(optarg);
         	break;
		  } catch(exception) {
			cerr<<"Port number should be an Integer\n";
			return 3;
		  }

       
      default: 
          fprintf(stderr, "Usage: %s [-p portnumber] [-v] [-a] [-e]\n",
                  argv[0]);
          exit(EXIT_FAILURE);
      }
  }

  pthread_t smtpThread;
    int result;

    // Create the pthread to run the SMTP server
    result = pthread_create(&smtpThread, NULL, startSMTPServer, NULL);
    if (result != 0) {
        std::cerr << "Failed to create the SMTP thread" << std::endl;
        return -1;
    }
    pthread_detach(smtpThread);


    // Continue with the rest of the main server initialization
    std::cout << "Main server is starting..." << std::endl;

  //Create a socket
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

  //Check if the socket is created successfully
  if (listen_fd < 0) {
    cerr << "Error: Could not create socket" << endl;
    return 3;
  }

  //string inital_message = "220 receiver.localhost Simple Mail Trasnfer Service Ready \r\n";
  string inital_message = "220 localhost *\r\n";
  //Send a simple greeting message to the client
  string greetingMessage = "+OK Server ready (Author: Abhishek Goyal / abhi2358)\r\n";
  
  //Bind the socket to the port  
  struct sockaddr_in servaddr;
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htons(INADDR_ANY);
  servaddr.sin_port = htons(portNumber);
  int enable = 1;

  //Set the socket options
  int ret1 = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &enable, sizeof(enable));

  //Check if the socket options are set successfully
  if (ret1 < 0) 
  {
      cerr<< "Fail to setsockopt: "<<strerror(errno)<<endl;
      return 3;
  }

  //Bind the socket to the port
  int ret = bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));
  if (ret < 0) {
        cerr<< "Fail to bind: "<<strerror(errno)<<endl;
        return 3;
  }

  //Listen for the incoming connections
  int ret2 = listen(listen_fd, 100);
  if (ret2 < 0) {
        cerr<< "Fail to listen: "<<strerror(errno)<<endl;
        return 3;
  }

  //Start accepting the incoming connections
  while (true) {

    //If the current number of connections is less than 100, accept the incoming connection. Else wait for some connection to close
    if (currentConnections <= 100) {

      //Accept the incoming connection
      struct sockaddr_in clientaddr;
      socklen_t clientaddrlen = sizeof(clientaddr);
      int *fd = new int;
      *fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
      if (*fd < 0) {
        cerr << "Error: Could not accept connection" << endl;
        continue;
      }
      if (debugFlag == 1) {
        cerr << "[" << *fd<<"] New connection" <<endl;
      }

      //Send the greeting message to the client
    //   send(*fd, inital_message.c_str(), inital_message.length(), 0);
    //   //send(*fd, greetingMessage.c_str(), greetingMessage.length(), 0);
    //   if (debugFlag == 1) {
    //     cerr << "[" << *fd<<"] S: "<<greetingMessage;
    //   }

      //Update the currentConnections, threadsId and threadFdMap. This is a critical section, so lock the mutex
      mtx.lock();
      currentConnections++;
      pthread_t thread;
      workerArgs *args = new workerArgs;
      args->fd = *fd;
      args->debugFlag = debugFlag;

      //Create a new worker thread
      pthread_create(&thread, NULL, worker, (void *)args);
      threadsId.push_back(thread);
      threadFdMap[thread] = *fd;
      delete fd;
      mtx.unlock();
    }
  }
  return 0;
}
