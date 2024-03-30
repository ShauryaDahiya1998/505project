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
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <memory>
#include <thrift/transport/TSocket.h>



namespace fs = std::filesystem;

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace  ::storage;


#define LogCompactSize 100

std::string dir;
std::string logFileName;
std::string tabletFileName;
std::map<std::string, std::map<std::string, std::string>> tablet;
std::ofstream logFileFD;
std::vector<std::string> replicas;


bool openLogFile(std::string filename) {
    logFileFD.open(filename, std::ios::app); // Open in append mode
    std::cout << "DF" << std::endl;
    return logFileFD.is_open();
}

bool doLogCompact(std::string filename, std::uintmax_t threshold) {
    std::uintmax_t file_size = fs::file_size(filename);
    // std::cout << "Log file size " << file_size << std::endl;
    return file_size > threshold;
}

void writeTabletToDisk(std::string filename) {
    std::ofstream outfile(filename);

    if (outfile.is_open()) {
        for (const auto& pair : tablet) {
            outfile << pair.first << ":";
            for (const auto& innerPair : pair.second) {
                outfile << innerPair.first << "=" << innerPair.second << ",";
            }
            outfile << std::endl;
        }
        outfile.close();
        std::cout << "KVS Server : Map has been written to " << filename << std::endl;
    } else {
        std::cerr << "KVS Server : Failed to open the output file: " << filename << std::endl;
    }
}

void readTabletFromDisk(std::string filename) {
    std::ifstream tabletFile(filename);
    std::string line;

    if (tabletFile.is_open()) {
        while (std::getline(tabletFile, line)) {
            std::istringstream iss(line);
            std::string row;
            if (!(std::getline(iss, row, ':'))) {
                continue;
            }
            std::string value;
            while (std::getline(iss, value, ',')) {
                std::string col, val;
                std::istringstream iss2(value);
                if (std::getline(std::getline(iss2, col, '=') >> std::ws, val)) {
                    tablet[row][col] = val;
                }
            }
        }
        tabletFile.close();
        std::cout << "KVS Server : Map has been read from " << filename << std::endl;
    }
}

void readConfig(std::string filename) {
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        replicas.push_back(line);
    }
    file.close();
}

void writeLog(std::string log) {
    if (logFileFD.is_open()) {
        logFileFD << log << std::endl;
    } else {
        std::cerr << "Error: Log file is not open." << std::endl;
    }
    if(doLogCompact(logFileName,LogCompactSize)){
        writeTabletToDisk(tabletFileName);
        logFileFD.close(); 
        logFileFD.open(logFileName, std::ios::trunc); 
    }
}

void replayLogFile(std::string filename){
    readTabletFromDisk(tabletFileName);
    std::ifstream logFD(filename);
    std::string line;
    while (std::getline(logFD, line)) {
        std::istringstream iss(line);
        std::string command;
        std::string row, col, value;
        if (iss >> command)
        { 
            // std::cout << command << std::endl;
            if(command == "PUT") 
            {
                if (!(iss >> row >> col >> value)) {
                    std::cerr << "Error: Invalid PUT log line - " << line << std::endl;
                    continue;
                }
                tablet[row][col] = value;
            }
            else if(command == "DELETE") 
            {
                if (!(iss >> row >> col)) {
                    std::cerr << "Error: Invalid DELETE log line - " << line << std::endl;
                    continue;
                }
                tablet[row].erase(col);
            }
        }
    }
    logFD.close();
}

bool valueExists(std::string row,std::string col){
    auto rowIterator = tablet.find(row);
    if (rowIterator != tablet.end()) {
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

void replicatePut(const std::string& row, const std::string& col, const std::string& value) {
    
    for (const auto& replica : replicas) {
        std::cout << replica;
        int retry = 0;
        size_t colonPos = replica.find(':');
        std::string replicaIP = replica.substr(0, colonPos);
        int replicaPort = std::stoi(replica.substr(colonPos + 1));
       
        ::std::shared_ptr<TTransport> socket(new TSocket(replicaIP, replicaPort));
        ::std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
        ::std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
        StorageOpsClient client(protocol);
        try {
            transport->open();
            while(!client.replicateData(row, col, value) && retry <3)
                retry++;
        }
        catch (TException &tx) {
            std::cerr << "ERROR: " << tx.what() << std::endl;
        }
    }
}

void deleteData(const std::string& row, const std::string& col) {
    
    for (const auto& replica : replicas) {
        int retry = 0;
        size_t colonPos = replica.find(':');
        std::string replicaIP = replica.substr(0, colonPos);
        int replicaPort = std::stoi(replica.substr(colonPos + 1));
        ::std::shared_ptr<TTransport> socket(new TSocket(replicaIP, replicaPort));
        ::std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
        ::std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
        StorageOpsClient client(protocol);
        try {
            transport->open();
            while(!client.deleteReplicate(row, col) && retry <3)
                retry++;
        }
        catch (TException &tx) {
            std::cerr << "ERROR: " << tx.what() << std::endl;
        }
    }
}

class StorageOpsHandler : virtual public StorageOpsIf {
 public:
  StorageOpsHandler() {
  }



  bool put(const std::string& row, const std::string& col, const std::string& value) {
    // Your implementation goes here
    
    std::cout << "HERE";
    tablet[row][col] = value;
    replicatePut(row,col,value);
    std::string log = "PUT " + row + " " + col + " " + value;
    writeLog(log);
    return true;
  }

  bool replicateData(const std::string& row, const std::string& col, const std::string& value) {
    tablet[row][col] = value;
    std::string log = "PUT " + row + " " + col + " " + value;
    writeLog(log);
    return true;
  }

void get(std::string& _return, const std::string& row, const std::string& col) {
    if(!valueExists(row,col))
        _return = "-ERR invalid row/column\r\n";
    else
        _return = tablet[row][col];
  }

  bool deleteCell(const std::string& row, const std::string& col) {
    if(!valueExists(row,col))
        return false;
    else
    {
        tablet[row].erase(col);
        deleteData(row,col);
        std::string log = "DELETE " + row + " " + col;
        writeLog(log);
        return true;
    }
    return false;
  }

  bool deleteReplicate(const std::string& row, const std::string& col) {
    if(!valueExists(row,col))
        return false;
    else
    {
        tablet[row].erase(col);
        std::string log = "DELETE " + row + " " + col;
        writeLog(log);
        return true;
    }
    return false;
  }

  bool cput(const std::string& row, const std::string& col, const std::string& old_value, const std::string& new_value) {
    if(!valueExists(row,col) || old_value!=tablet[row][col])
        return false;
    else
    {
        tablet[row][col] = new_value;
        replicateData(row,col,new_value);
        std::string log = "PUT " + row + " " + col + " " + new_value;
        writeLog(log);
        return true;
    }
    return false;
  }
};


int main(int argc, char *argv[])
{
    if(argc<2){
        std::cerr<<"No directory specified"<<std::endl;
    }
    dir = argv[argc-1];
    if(!directoryExists())
    {
        std::cerr<<"No directory exists"<<std::endl;
        return 1;
    }

    logFileName = dir + "/log.txt";
    std::string configFileName = dir + "/config.txt";
    tabletFileName = dir + "/tablet.txt";
    readConfig(configFileName);
    replayLogFile(logFileName);
    openLogFile(logFileName);
   
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

    int port = 8000;
    if(portArg!=nullptr)
        port=std::atoi(portArg);


    ::std::shared_ptr<StorageOpsHandler> handler(new StorageOpsHandler());
    ::std::shared_ptr<TProcessor> processor(new StorageOpsProcessor(handler));
    ::std::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
    ::std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    ::std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());


    TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory);

    server.serve();
    return 0;
}