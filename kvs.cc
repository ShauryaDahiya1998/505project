#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <fstream>
#include <dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <atomic>
#include <string.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <map>
#include <filesystem>
#include "gen-cpp/StorageOps.h"
#include "gen-cpp/KvsCoordOps.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <memory>
#include <thrift/transport/TSocket.h>
#include <openssl/sha.h>


using namespace std;
namespace fs = filesystem;

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace  ::storage;


#define LogCompactSize 100

string dir;
vector<string> logFileNames;
vector<string> tabletFileNames;
vector<map<string, map<string, string>>> tablets;
vector<ofstream *> logFileFDs;
vector<string> replicas;
string my_ip;
int my_index = 0;
int num_nodes = 0;

bool openLogFile(string filename) {
    ofstream *logFileFD = new ofstream;
    logFileFD->open(filename, ios::app); // Open in append mode
    cout << "DF" << endl;
    logFileFDs.push_back(logFileFD);
    return logFileFD->is_open();
}

bool doLogCompact(string filename, uintmax_t threshold) {
    uintmax_t file_size = fs::file_size(filename);
    // cout << "Log file size " << file_size << endl;
    return file_size > threshold;
}

int getNodeIndex(const string& value) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, value.c_str(), value.length());
    SHA256_Final(hash, &sha256);

    // Convert the first 4 bytes of the hash to an integer
    int hashInt = *((int*)hash);

    // Ensure hashInt is non-negative
    hashInt = hashInt < 0 ? -hashInt : hashInt;

    // Return the node index by taking modulus with the number of nodes
    return hashInt % num_nodes;
}

int whichNode(const string& value) {
    int intended_node = getNodeIndex(value);
    if(intended_node == my_index) {
        return 0;
    }
    if((intended_node == my_index - 1) or ((intended_node == num_nodes - 1) and (my_index == 0))) {
        //this node is the secondary replica
        return 1;
    }
    if((intended_node == my_index - 2) or ((intended_node == num_nodes - 2 and my_index == 0)) or (intended_node == num_nodes - 1 and my_index == 1)) {
        //this node is the tertiary replica
        return 2;
    }
    return 0;
}

void writeTabletToDisk(string filename, int which_tablet) {
    cerr<<"\nWriting tablet to disk with filename: " + filename; 
    ofstream outfile(filename);

    if (outfile.is_open()) {
        for (const auto& pair : tablets[which_tablet]) {
            outfile << pair.first << ":";
            for (const auto& innerPair : pair.second) {
                outfile << innerPair.first << "=" << innerPair.second << ",";
            }
            outfile << endl;
        }
        outfile.close();
        cout << "KVS Server : Map has been written to " << filename << endl;
    } else {
        cerr << "KVS Server : Failed to open the output file: " << filename << endl;
    }
}

vector<string> splitString(const string& s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    tokens.push_back("");
    return tokens;
}

void readTabletFromDisk(string filename, int which_tablet) {
    ifstream tabletFile(filename);
    string line;

    if (tabletFile.is_open()) {
        while (getline(tabletFile, line)) {
            istringstream iss(line);
            string row;
            if (!(getline(iss, row, ':'))) {
                continue;
            }
            string value;
            while (getline(iss, value, ',')) {
                string col, val;
                istringstream iss2(value);
                if (getline(getline(iss2, col, '=') >> ws, val)) {
                    tablets[which_tablet][row][col] = val;
                }
            }
        }
        tabletFile.close();
        cout << "KVS Server : Map has been read from " << filename << endl;
    }
}

void readConfig(string filename) {
    ifstream file(filename);
    
    if (!file.is_open()) {
        cerr << "Failed to open file: " << filename << endl;
    }
    
    string line;
    int i = 0;
    while (getline(file, line)) {
        if(i == my_index) {
            vector <string> ips = splitString(line, ';');
            my_ip = ips[0];
            replicas.push_back(ips[1]);
            replicas.push_back(ips[2]);
            // cerr<<"\nthis is alsp printing";
            // break;
        }
        num_nodes++;
        i++;
    }
    file.close();
}

void writeLog(string log, int which_file) {
    if (logFileFDs[which_file]->is_open()) {
        *logFileFDs[which_file] << log << endl;
    } else {
        cerr << "Error: Log file is not open." << endl;
    }
    if(doLogCompact(logFileNames[which_file],LogCompactSize)){
        writeTabletToDisk(logFileNames[which_file], which_file);
        logFileFDs[which_file]->close(); 
        logFileFDs[which_file]->open(logFileNames[which_file], ios::trunc); 
    }
}


void replayLogFile(string filename, int which_file){
    readTabletFromDisk(tabletFileNames[which_file], which_file);
    ifstream logFD(filename);
    string line;
    while (getline(logFD, line)) {
        istringstream iss(line);
        string command;
        string row, col, value;
        if (iss >> command)
        { 
            // cout << command << endl;
            if(command == "PUT") 
            {
                if (!(iss >> row >> col >> value)) {
                    cerr << "Error: Invalid PUT log line - " << line << endl;
                    continue;
                }
                tablets[which_file][row][col] = value;
            }
            else if(command == "DELETE") 
            {
                if (!(iss >> row >> col)) {
                    cerr << "Error: Invalid DELETE log line - " << line << endl;
                    continue;
                }
                tablets[which_file][row].erase(col);
            }
        }
    }
    logFD.close();
}

bool valueExists(string row, string col, int which_node){
    auto rowIterator = tablets[which_node].find(row);
    if (rowIterator != tablets[which_node].end()) {
        auto columnIterator = rowIterator->second.find(col);
        if (columnIterator != rowIterator->second.end()) {
            return true;
        } else {
           return false;
        }
    } else {
        return false;
    }
}

bool directoryExists() {
    struct stat info;
    if (stat(dir.c_str(), &info) != 0)
        return false;
    return (info.st_mode & S_IFDIR) != 0;
}

void *sendKeepAlive(void *arg) {
    ::shared_ptr<TTransport> socket(new TSocket("127.0.0.1", 8000));
    ::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
    ::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
    KvsCoordOpsClient client(protocol);
    while(true) {
        sleep(1);
        transport->open();
        string ip;
        cerr<<"\nSENDING KEEP ALIVE WITH IP: "<<my_ip;
        client.keepAlive(ip, my_ip);
        if(ip != my_ip) {
            //this node just crashed, it needs to sync with the current primary
            vector<string> ip_address = splitString(ip, ':');
            int which_replica = stoi(ip_address[0]);
            ::shared_ptr<TTransport> sync_socket(new TSocket(ip_address[1], stoi(ip_address[2])));
            ::shared_ptr<TTransport> sync_transport(new TBufferedTransport(sync_socket));
            ::shared_ptr<TProtocol> sync_protocol(new TBinaryProtocol(sync_transport));
            StorageOpsClient sync_client(sync_protocol);

            sync_transport->open();

            string logs_and_table;

            sync_client.sync(logs_and_table, to_string(which_replica));
            cerr<<"\nlogs_and_table result: "<<logs_and_table;

            vector <string> landt = splitString(logs_and_table, '$');
            cerr<<"\nCHECKPT3";
            string logs = landt[0];
            cerr<<"\nCHECKPT4";
            string tablet = landt[1];
            cerr<<"\nCHECKPT5";

            //write logs.txt to log files[0] and table to tablet files[0]
            // logFileFDs[0]
            ofstream outfile(logFileNames[0], ios::trunc);
            cerr<<"\nCHECKPT6";

            if (outfile.is_open()) {
                outfile << logs;
                
            } 
            cerr<<"\nCHECKPT7";
            outfile.close();

            ofstream tabletoutfile(tabletFileNames[0], ios::trunc);

            if (tabletoutfile.is_open()) {
                tabletoutfile << tablet;
                
            } 
            tabletoutfile.close();

            replayLogFile(logFileNames[0], 0);

            sync_transport->close();
            client.syncComplete(ip, my_ip);

        }
        transport->close();
    }
}

void replicatePut(const string& row, const string& col, const string& value) {
    
    for (const auto& replica : replicas) {
        cout << replica;
        int retry = 0;
        size_t colonPos = replica.find(':');
        string replicaIP = replica.substr(0, colonPos);
        int replicaPort = stoi(replica.substr(colonPos + 1));
       
        ::shared_ptr<TTransport> socket(new TSocket(replicaIP, replicaPort));
        ::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
        ::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
        StorageOpsClient client(protocol);
        try {
            transport->open();
            while(!client.replicateData(row, col, value) && retry <3)
                retry++;
        }
        catch (TException &tx) {
            cerr << "ERROR: " << tx.what() << endl;
        }
    }
}

void deleteData(const string& row, const string& col, int which_node) {
    
    for (const auto& replica : replicas) {
        int retry = 0;
        size_t colonPos = replica.find(':');
        string replicaIP = replica.substr(0, colonPos);
        int replicaPort = stoi(replica.substr(colonPos + 1));
        ::shared_ptr<TTransport> socket(new TSocket(replicaIP, replicaPort));
        ::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
        ::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
        StorageOpsClient client(protocol);
        try {
            transport->open();
            while(!client.deleteReplicate(row, col) && retry <3)
                retry++;
        }
        catch (TException &tx) {
            cerr << "ERROR: " << tx.what() << endl;
        }
    }
}

class StorageOpsHandler : virtual public StorageOpsIf {
 public:
  StorageOpsHandler() {
  }



  bool put(const string& row, const string& col, const string& value) {
    // Your implementation goes here
    tablets[whichNode(row)][row][col] = value;
    replicatePut(row,col,value);
    string log = "PUT " + row + " " + col + " " + value;
    writeLog(log, whichNode(row));
    return true;
  }

  bool replicateData(const string& row, const string& col, const string& value) {
    int intended_index = getNodeIndex(row);
    string log = "PUT " + row + " " + col + " " + value;
    if((intended_index == my_index - 1) or ((intended_index == num_nodes - 1) and (my_index == 0))) {
        //this node is the secondary replica
        tablets[1][row][col] = value;
        writeLog(log, 1);
    } else if((intended_index == my_index - 2) or ((intended_index == num_nodes - 2 and my_index == 0)) or (intended_index == num_nodes - 1 and my_index == 1)) {
        //this node is the tertiary replica
        tablets[2][row][col] = value;
        writeLog(log, 2);
    } else {
        tablets[0][row][col] = value;
        writeLog(log, 0);
    }
    return true;
  }

  void get(string& _return, const string& row, const string& col) {
    int which_node = whichNode(row);
    if(!valueExists(row,col,which_node))
        _return = "-ERR invalid row/column\r\n";
    else
        _return = tablets[which_node][row][col];
  }

  bool deleteCell(const string& row, const string& col) {
    int which_node = whichNode(row);
    if(!valueExists(row, col, which_node))
        return false;
    else
    {
        tablets[which_node][row].erase(col);
        deleteData(row, col, which_node);
        string log = "DELETE " + row + " " + col;
        writeLog(log, which_node);
        return true;
    }
    return false;
  }

  bool deleteReplicate(const string& row, const string& col) {
    int which_node = whichNode(row);
    if(!valueExists(row, col, which_node))
        return false;
    else
    {
        tablets[which_node][row].erase(col);
        string log = "DELETE " + row + " " + col;
        writeLog(log, which_node);
        return true;
    }
    return false;
  }

  bool cput(const string& row, const string& col, const string& old_value, const string& new_value) {
    int which_node = whichNode(row);
    if(!valueExists(row, col, which_node) || old_value!=tablets[which_node][row][col])
        return false;
    else
    {
        tablets[whichNode(row)][row][col] = new_value;
        replicateData(row, col, new_value);
        string log = "PUT " + row + " " + col + " " + new_value;
        writeLog(log, whichNode(row));
        return true;
    }
    return false;
  }

  void sync(std::string& _return, const std::string& which) {
    int version = stoi(which);

    ifstream inputFile(logFileNames[version]);
    if (!inputFile.is_open()) {
        std::cerr << "Error opening file for reading!" << std::endl;
        return;
    } 
    // Read the contents of the file into a stringstream
    stringstream buffer;
    buffer << inputFile.rdbuf();
    // Close the file
    inputFile.close();
    // Extract the string from the stringstream
    string logs = buffer.str();

    ifstream inputFileTablet(tabletFileNames[version]);
    if (!inputFileTablet.is_open()) {
        std::cerr << "Error opening file for reading!" << std::endl;
        return;
    } 
    // Read the contents of the file into a stringstream
    stringstream bufferTablet;
    bufferTablet << inputFileTablet.rdbuf();
    // Close the file
    inputFileTablet.close();
    // Extract the string from the stringstream
    string tablet = bufferTablet.str();

    _return = logs + "$" + tablet;

  }

};


// kvs worker should take in 1. index number for current worker node and 2. directory
int main(int argc, char *argv[])
{  
    int c = 0;
    char* portArg = nullptr;
    while((c=getopt(argc,argv,"p:"))!=-1){
        switch(c)
        {
        case 'p':
            portArg = optarg;
            break;
        default:
            break;
        }
    }

    if(optind < argc) {
        my_index = atoi(argv[optind++]);
        if(optind < argc) {
            dir = argv[optind];
        } else {
            cout<<"Please enter index of worker node and storage directory\n";
            return 0;
        }
    } else {
        cout<<"Please enter index of worker node and storage directory\n";
        return 0;
    }

    if(!directoryExists())
    {
        cerr<<"No directory exists"<<endl;
        return 1;
    }
    map<string, map<string, string>> primary_tablet;
    map<string, map<string, string>> secondary_tablet;
    map<string, map<string, string>> tertiary_tablet;
    tablets.push_back(primary_tablet);
    tablets.push_back(secondary_tablet);
    tablets.push_back(tertiary_tablet);

    logFileNames.push_back(dir + "/primary_log.txt");
    logFileNames.push_back(dir + "/secondary_log.txt");
    logFileNames.push_back(dir + "/tertiary_log.txt");
    string configFileName = dir + "/config.txt";

    tabletFileNames.push_back(dir + "/primary_tablet.txt");
    tabletFileNames.push_back(dir + "/secondary_tablet.txt");
    tabletFileNames.push_back(dir + "/tertiary_tablet.txt");

    readConfig(configFileName);
    replayLogFile(logFileNames[0], 0);
    replayLogFile(logFileNames[1], 1);
    replayLogFile(logFileNames[2], 2);

    openLogFile(logFileNames[0]);
    openLogFile(logFileNames[1]);
    openLogFile(logFileNames[2]);

    int port = 8000;
    if(portArg!=nullptr)
        port=atoi(portArg);


    pthread_t keep_alive_thread;
    if(pthread_create(&keep_alive_thread, NULL, sendKeepAlive, NULL) != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }


    ::shared_ptr<StorageOpsHandler> handler(new StorageOpsHandler());
    ::shared_ptr<TProcessor> processor(new StorageOpsProcessor(handler));
    ::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
    ::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    ::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());


    TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory);

    server.serve();
    return 0;
}