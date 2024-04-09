#include <iostream>
#include <map>
#include <fstream>
#include <vector>
#include <cmath>
#include <unistd.h>
#include <openssl/sha.h>
#include <sys/stat.h>

#include "gen-cpp/KvsCoordOps.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>


using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace std;

// map <char, string> kvs_worker_map;
vector <string> kvs_workers;
string dir;
int num_nodes;

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

void readConfig(std::string filename) {
    // vector <string> nodes;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        kvs_workers.push_back(line);
    }
    num_nodes = kvs_workers.size();

    // int keys_per_node = ceil(26.0/nodes.size());

    // int i = 1;
    // int j = 0;

    // for(char c: "ABCDEFGHIJKLMNOPQRSTUVWXYZ") {
    //     if(i > keys_per_node) {
    //         i = 1;
    //         j++;
    //     }
    //     kvs_worker_map[c] = nodes[j];
    //     i++;
    // }
    file.close();
}

bool directoryExists() {
    struct stat info;
    if (stat(dir.c_str(), &info) != 0)
        return false;
    return (info.st_mode & S_IFDIR) != 0;
}

class KvsCoordOpsHandler : virtual public KvsCoordOpsIf {
    public:
    KvsCoordOpsHandler() {

    }

    void get(std::string& _return, const std::string& row) {
        int node_index = getNodeIndex(row); //hash the row and get the associated node
        _return = kvs_workers[node_index];
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

    std::string configFileName = dir + "/config.txt";
    readConfig(configFileName);
   
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


    shared_ptr<KvsCoordOpsHandler> handler(new KvsCoordOpsHandler());
    shared_ptr<TProcessor> processor(new KvsCoordOpsProcessor(handler));
    shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
    shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());


    TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory);

    server.serve();
    return 0;
}