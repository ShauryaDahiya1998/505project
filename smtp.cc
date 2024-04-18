#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <sstream>

#include <signal.h>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <fstream>
#include <unordered_map>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include "customlib.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace storage;
using namespace std;

bool debugMode = true;
std::vector<pthread_t> threads;
std::map<pthread_t, int> socketMap;
int serverSocket;
std::atomic<int> numThreads(0);
std::mutex mtx1;
std::string dir;
std::unordered_map<std::string, pthread_mutex_t> mailboxMutexMap;
bool extraCredit = false;

void writeToSocket(std::string msg,int socket);

// Get time based on timestamp
std::string getTimeString(time_t timestamp) {
    struct tm* timeinfo = localtime(&timestamp);
    if (timeinfo == nullptr) {
        return "Invalid timestamp";
    }

    char buffer[80];
    strftime(buffer, 80, "%a %b %d %H:%M:%S %Y", timeinfo);

    return std::string(buffer);
}

// Mesage object with mailbox,email,sender details etc

struct Message {
    std::vector<std::string> mailbox;
    std::vector<std::string> mqmailbox;
    std::string sender;
    std::string message;
    int state;
    time_t date;

    Message(const std::string& sender, const std::string& message, int state, time_t date)
        : sender(sender), message(message), state(state), date(date) {}

    void setSender(const std::string& newSender) {
        sender = newSender;
    }
     void setMailbox(const std::string& newMailbox) {
        mailbox.push_back(newMailbox);
    }

    void emptyMailbox() {
        mailbox.clear();
    }

    void setmqMailbox(const std::string& newmqMailbox) {
        mqmailbox.push_back(newmqMailbox);
    }

    void emptymqMailbox() {
        mqmailbox.clear();
    }
    // Setter for message
    void setMessage(const std::string& newMessage) {
        message = newMessage;
    }

    // Setter for state
    void setState(int newState) {
        state = newState;
    }

    // Setter for date
    void setDate(time_t newDate) {
        date = newDate;
    }

    void appendMessage(const std::string& additionalMessage) {
        message += additionalMessage;
    }
};

std::tuple<std::string, std::string> parseEmail(const std::string& email) {
    std::istringstream stream(email);
    std::string line;
    std::string subject;
    std::string body;
    bool headerSection = true;
    bool foundBody = false;

    while (std::getline(stream, line)) {
        if (headerSection) {
            if (line.empty() || line == "\r") {
                headerSection = false;  // Start of the body section
                continue;  // Skip the empty line marking the start of the body
            } else {
                // Check if the line starts with "Subject:"
                if (line.substr(0, 8) == "Subject:") {
                    subject = line.substr(9);
                    // Trim leading spaces
                    size_t start = subject.find_first_not_of(" ");
                    subject = subject.substr(start);
                }
            }
        } else {
            foundBody = true;
            // Append the line to the body, add newline if body is not empty
            if (!body.empty()) body += "\n";
            body += line;
        }
    }

    // Handle cases where no body was found but the headers ended
    if (!foundBody) {
        body = "No body content found.";  // Or set to an empty string as needed
    }

    return std::make_tuple(subject, body);
}

std::tuple<std::shared_ptr<TTransport>, KvsCoordOpsClient> getKVSCoord(){
    shared_ptr<TTransport> socket(new TSocket("127.0.0.1", 8000));
    shared_ptr<TTransport> transport(new TBufferedTransport(socket));
    shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
    KvsCoordOpsClient client(protocol);
    transport->open();
    return std::make_tuple(transport, client);
}

std::string replaceCRLFwithLF(std::string text) {
    std::string toSearch = "\r\n";
    std::string replaceWith = "\n";
    size_t pos = 0;

    while ((pos = text.find(toSearch, pos)) != std::string::npos) {
        text.replace(pos, toSearch.length(), replaceWith);
        pos += replaceWith.length();
    }

    return text;
}

int writeToKVS(std::string username,Message emailMsg) {
   
    string row = username + "-mailbox";
    string timestamp = getCurrentTimestamp();
    size_t start = emailMsg.sender.find('<');
    size_t end = emailMsg.sender.find('>');
    std::string subject;
    std::string body;
    std::tie(subject, body) = parseEmail(emailMsg.message);
    subject.pop_back();
    body.pop_back();
    std::cout << "PARSED " << subject << " " << body << endl;
    string email = emailMsg.sender.substr(start + 1, end - start - 1) + "\r\n" + timestamp + "\r\n" +  subject + "\r\n" + replaceCRLFwithLF(body); 
    string col = sha256(email);
    std::cout << "Email WRITTEN " << email << endl;
    auto [transportCoord, client] =  getKVSCoord();
    auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
    kvsClient.put(row,col,email);

    string emailHashes;
    kvsClient.get(emailHashes,row,"AllEmails");;

    if(emailHashes.empty() || emailHashes[0]=='-')
        emailHashes = col;
    else
        emailHashes = emailHashes + "," + col;

    kvsClient.put(row,"AllEmails",emailHashes);  
    transport->close();
    transportCoord->close();
    return 0;
}


void addThread(pthread_t thread){
  std::lock_guard<std::mutex> lock(mtx1);
  threads.push_back(thread);
}

void removeThread(pthread_t thread) {
    std::lock_guard<std::mutex> lock(mtx1); 
    for (auto it = threads.begin(); it != threads.end(); ++it) {
        if (pthread_equal(*it, thread)) {
            threads.erase(it);
            break;
        }
    }
}

int getNumThreads(){
  std::lock_guard<std::mutex> lock(mtx1);
  return threads.size();
}

pthread_t getThread(int index) {
  std::lock_guard<std::mutex> lock(mtx1); 
  return threads[index];
}

// Signal handler for SIGINT
void signalHandler(int signal) {
  // if(debugMode)
  //   std::cout<< "SIGINT recieved in main thread " << pthread_self() << std::endl ;
  // Handle SIGINT here
  int n = numThreads.load();
  for(int i=0;i<getNumThreads();i++){
    // if(debugMode)
    //   std::cout<< "SIGUSER sent in main thread " << getThread(i) << std::endl ;
    pthread_kill(getThread(i), SIGUSR1); //Send SIGUSR1 to all active threads
  }
  
  // if(debugMode)
  //   std::cout<< "ALL KILLED" << std::endl;
  
  for(int i=0;i<getNumThreads();i++){
    pthread_join(getThread(i),NULL);
  }
  
  close(serverSocket);
  std::exit(0);

}

//Signal handler for SIGUSR1
void sigusr_handler(int sig) {
  // if(debugMode)
  //   std::cout<< "SIGUSER recoeved in main thread " << pthread_self() << " " << socketMap[pthread_self()]<< std::endl ;

  std::string msg = "421 Server shutting down\r\n";
  writeToSocket(msg,socketMap[pthread_self()]);
  if(debugMode)
      std::cout<<"["<<socketMap[pthread_self()]<<"] Connection closed"<<std::endl;
  close(socketMap[pthread_self()]);
  
  pthread_exit(NULL);
}




// Method to write any message to given socket
void writeToSocket(std::string msg,int socket){
   int bytes = write(socket, msg.data(), msg.length());
    if (bytes == -1) {
        std::cerr << "Error sending message to client\n";
    } else {
      if(debugMode)
        {std::cout<<"["<<socket<<"] S: "<<msg;
        // std::cout << "Sent " << bytes << " bytes to client\n";
        }
    }
}


//Matches command and excutes steps for that command
int commandMatcher(std::string command,int clientSocket,int &state,Message &email){
  std::string lowercaseCommand;

  // Convert the original string to lowercase
  for (char c : command) {
      lowercaseCommand += static_cast<char>(std::tolower(c));
  }
  size_t pos = lowercaseCommand.find(' ');
  std::string firstWord = lowercaseCommand.substr(0, pos);

  // std::cout << "STATE " << state << std::endl;
  
  //Check is we are after data state first
  if(state==4 ){
     int writeFails = 0;
      std::string msg = "";
      if(command==".\r\n"){
        for (const auto& mailbox : email.mailbox){
        //   std::string filename = dir + "/" + mailbox + ".mbox";
        //   writeFails = writeFails + writeFile(filename,email,clientSocket,"");
            writeToKVS(mailbox,email);
        }

        if(debugMode)
          std::cout<<"["<<socket<<"] S: Writes Failed"<<writeFails << "/"<<email.mailbox.size() << std::endl;
        if(writeFails==0 || writeFails < email.mailbox.size())
        {
          msg= "250 OK\r\n";
        }
        else{
          msg= "451 Lock on files could not be obtained. Writing mail failed\r\n";
        }
        writeToSocket(msg,clientSocket);
        //Resets state
        email.setState(1);
        email.setSender("");
        email.setMessage("");
        email.emptyMailbox();
        email.emptymqMailbox();
        state = 1;
      }
      else
        email.appendMessage(command);
    return 0;
  }
  else if(firstWord=="helo"){
    std::size_t clrfPos = lowercaseCommand.find("helo");
    std::string text = command.substr(clrfPos+4);
    std::string msg = "";
   
    if(state>1){
      msg= "503 Bad sequence of commands\r\n";
    }
    else if(text.empty()){
      msg="501 Incorrect Syntax : Missing args\r\n";
    }
    else{
      state = 1;
      msg="250 localhost\r\n";
      email.setState(1);
    }
    writeToSocket(msg,clientSocket); // Writes the  text to client
    return 0;
  }
  else if(lowercaseCommand.find("mail from:")!=std::string::npos){
    std::size_t clrfPos = lowercaseCommand.find("mail from:");
    std::string text = command.substr(clrfPos+10);
    
    std::string result;
    for (char c : text) {
        if (!std::isspace(c)) {
            result += c;
        }
    }
    text = result;

    if(debugMode)
      std::cout << "["<<clientSocket<<"] S: MAIL FROM ID - " << text << std::endl;
    
    std::string msg = "";


    // Checks state and arg validity
    if(state!=1){
      msg= "503 Bad sequence of commands\r\n";
    }
    else if(text.front() != '<' || text.back()!='>' ){
      msg = "501 Incorrect Syntax : Missing <> around email\r\n";
    }
    else{
      email.setState(2);
      email.setSender(text);
      email.setDate(time(nullptr));
      state = 2;
      msg="250 OK\r\n";
    }
    writeToSocket(msg,clientSocket); // Writes the echo text to client
    return 0;
  }
  else if(lowercaseCommand.find("rcpt to:")!=std::string::npos){
    std::size_t clrfPos = lowercaseCommand.find("rcpt to:");
    std::string text = command.substr(clrfPos+8);

    std::string result;
    for (char c : text) {
        if (!std::isspace(c)) {
            result += c;
        }
    }
    text = result;

    if(debugMode)
      std::cout << "["<<clientSocket<<"] S: RCPT TO ID - " << text << std::endl;
    
    int atPosition = text.find('@');
    std::string msg = "";

    // Checks state and arg validity

    if(state!=2 && state!=3){
      msg= "503 Bad sequence of commands\r\n";
      writeToSocket(msg,clientSocket); // Writes the echo text to client
      return 0;
    }
    else if(atPosition == std::string::npos){
      msg = "501 Incorrect Syntax : Invalid email\r\n";
      writeToSocket(msg,clientSocket); // Writes the echo text to client
      return 0;
    }
    std::string domain = text.substr(atPosition + 1);
    
    if(text.front() != '<' || text.back()!='>' ){
      msg = "501 Incorrect Syntax : Missing <> around email\r\n";
      writeToSocket(msg,clientSocket); // Writes the error text to client when unknown command is sent
      return 0;
    }
    std::string user = text.substr(1,atPosition-1);
    std::string filename = dir + "/" + user + ".mbox";
    domain = domain.substr(0, domain.length() - 1);

    // if(debugMode)
    //   std::cout << "["<<clientSocket<<"] S: MBOX FILE - " << filename << " " << domain<< std::endl;
    if(user.empty()){
      msg = "501 Incorrect Syntax : Invalid email\r\n";
    }
    else if(domain!="penncloud" ){    
        msg = "551 Incorrect Syntax : Invalid email\r\n";
    }
    // else if(!fileExists(filename)){
    //   msg = "550 Incorrect Syntax : Not valid email\r\n";
    // } 
    else{
      email.setState(3);
      email.setMailbox(user);
      state = 3;
      msg="250 OK\r\n";
    }
    writeToSocket(msg,clientSocket); // Writes the echo text to client
    return 0;
  }
  else if(firstWord=="data"){
    std::string msg = "";
    std::size_t clrfPos = lowercaseCommand.find("data");
    std::string text = command.substr(clrfPos+4);

    std::string result;
    for (char c : text) {
        if (!std::isspace(c)) {
            result += c;
        }
    }
    text = result;
   
       // Checks state and arg validity

    if(state!=3){
      msg= "503 Bad sequence of commands\r\n";
    }
    else if(!text.empty()){
      msg="501 Incorrect Syntax : Extra args\r\n";
    }
    else{
      state = 4;
      email.setState(4);
      email.setMessage("");
      msg = "354 Start mail input; end with <CRLF>.<CRLF>\r\n";
    }
    writeToSocket(msg,clientSocket); // Writes the echo text to client
    return 0;
   }
  else if(firstWord=="noop"){
    std::string msg;
    std::size_t clrfPos = lowercaseCommand.find("noop");
    std::string text = command.substr(clrfPos+4);

    std::string result;
    for (char c : text) {
        if (!std::isspace(c)) {
            result += c;
        }
    }
    text = result;

    // Checks state and arg validity

    if(state==0){
      msg= "503 Bad sequence of commands\r\n";
    } else if(!text.empty()){
      msg="501 Incorrect Syntax : Extra args\r\n";
    }
    else{
      msg= "250 OK\r\n";
    }
    writeToSocket(msg,clientSocket); // Writes the error text to client when unknown command is sent
    return 0;
  }
  else if(firstWord=="quit"){

    std::size_t clrfPos = lowercaseCommand.find("quit");
    std::string msg= "";
    std::string text = command.substr(clrfPos+4);

    std::string result;
    for (char c : text) {
        if (!std::isspace(c)) {
            result += c;
        }
    }
    text = result;

    // Checks arg validity

    if(!text.empty()){
      msg="501 Incorrect Syntax : Extra args\r\n";
      writeToSocket(msg,clientSocket);
      return 0;
    }

    msg= "221 Service closing connection\r\n";
    writeToSocket(msg,clientSocket); // Writes the Goodbye text to client
    close(clientSocket); // Closes socket to client
    if(debugMode)
      std::cout<<"["<<clientSocket<<"] Connection closed"<<std::endl;
    removeThread(pthread_self()); //Updates global thread list
    numThreads--;
    return 1;
  }
  else if(firstWord=="rset"){
    std::string msg;
    std::size_t clrfPos = lowercaseCommand.find("rset");
    std::string text = command.substr(clrfPos+4);

    std::string result;
    for (char c : text) {
        if (!std::isspace(c)) {
            result += c;
        }
    }
    text = result;

    // Checks state and arg validity

    if(state==0){
      msg= "503 Bad sequence of commands\r\n";
    } else if(!text.empty()){
      msg="501 Incorrect Syntax : Extra args\r\n";
    }
    else{
      //Reset to helo state
      state = 1;
      email.setState(1);
      email.setSender("");
      email.setMessage("");
      email.emptyMailbox();
      email.emptymqMailbox();
      msg= "250 OK\r\n";
    }
    writeToSocket(msg,clientSocket); // Writes the error text to client when unknown command is sent
    return 0;
  }
  else{
    std::string msg = "500 Unknown Command\r\n";
    writeToSocket(msg,clientSocket); // Writes the error text to client when unknown command is sent
    return 0;
  }
  return 0;
}

void* threadFunction(void* args){
  // std::cout<<"I AM " << pthread_self() << std::endl;
  addThread(pthread_self());
  int state = 0;
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  pthread_sigmask(SIG_BLOCK, &set, NULL); // Blocks SIGINT in thread

  int clientSocket = *(int*)args;
  socketMap[pthread_self()] = clientSocket;

  std::string recString = "";
  int bufSize = 1024;
  char buf[1024];
  Message email("", "", 0, time(nullptr));
  // std::cout << "Address of 'email' object: " << &email << std::endl;
  while(true){
    int bytes = read(clientSocket,&buf,bufSize-1);
    if(bytes==-1){
      std::cerr << "Error receiving message from client"<< strerror(errno) << " (" << errno << ")"<<std::endl;
      break;
    }
    else if(bytes==0){
      close(clientSocket); 
      if(debugMode)
        std::cout<<"["<<clientSocket<<"] Connection closed"<<std::endl;
      removeThread(pthread_self()); //Updates global thread list
      numThreads--;
      break;
    }
    else{
      buf[bytes] = '\0';
      recString = recString + std::string(buf);

      std::size_t clrfPos = recString.find("\r\n");
      // Searches for CRLF and builds command using it
      while(clrfPos!=std::string::npos){
        std::string command = "";
        if(email.state!=4)
          command = recString.substr(0, clrfPos);
        else
          command = recString.substr(0, clrfPos+2);
        // std::cout << "Received command: " << command << std::endl;
        if(debugMode)
          std::cout<<"["<<clientSocket<<"] C: "<<command<<std::endl;
        int execCommand = commandMatcher(command,clientSocket,state,email);
        if(execCommand>0)
          pthread_exit(NULL);
        recString.erase(0, clrfPos + 2);
        clrfPos = recString.find("\r\n");
      }
    }
  }
  pthread_exit(NULL);
}

void* startSMTPServer(void* arg)
{
  // Signal handler for SIGINT is setup
  struct sigaction act;
  act.sa_handler = signalHandler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  sigaction(SIGINT, &act, NULL);

  // Signal handler for SIGUSR1 is setup

  struct sigaction act2;
  act2.sa_handler = sigusr_handler;
  sigemptyset(&act2.sa_mask);
  act2.sa_flags = 0;
  sigaction(SIGUSR1, &act2, NULL);

  int port = 2500;
  

  serverSocket = socket(PF_INET, SOCK_STREAM, 0);
  if (serverSocket == -1) {
    std::cerr << "Error creating socket\n";
    pthread_exit(NULL);
  }
  struct sockaddr_in servaddr;
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htons(INADDR_ANY);
  servaddr.sin_port = htons(port);
  const char* msg = "220 localhost OK Server Ready\r\n";
  const int nThreads = 100;
  int opt = 1;
  int ret = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &opt, sizeof(opt));
  if (bind(serverSocket, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
      std::cerr << "Error binding socket: " << strerror(errno) << " (" << errno << ")\n";
      close(serverSocket);
      pthread_exit(NULL);
    }

  if (listen(serverSocket, 10) == -1) {
    std::cerr << "Error listening on socket: "<< strerror(errno) << " (" << errno << ")\n";;
    close(serverSocket);
    pthread_exit(NULL);
  }

  if(debugMode)
    std::cout << "S: Server listening on port " << port <<std::endl;

  int i =0;
  while (true){
    pthread_t thread;
    struct sockaddr_in clientAddr;
    socklen_t clientAddrSize = sizeof(clientAddr);
   
   //Only accepts connections if less than 100 connections are present
    if(numThreads<100)
    {
      int *clientSocket = (int*)malloc(sizeof(int));
      *clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrSize);
      if(debugMode)
        std::cout<<"["<<*clientSocket<<"] New connections " << numThreads << std::endl;
      numThreads++;
      writeToSocket(msg,*clientSocket); // Sends a welcome message to client
      int result = pthread_create(&thread, NULL, threadFunction, clientSocket);
      if (result != 0) {
          std::cerr << "Error creating thread\n";
          pthread_exit(NULL);
      }
      pthread_detach(thread);
      i++;
    }
  }


  close(serverSocket);


 pthread_exit(NULL);
}