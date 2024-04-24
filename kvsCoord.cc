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

struct kvs_worker {
    bool started = false;
    bool is_alive;
    bool is_crashed;
    bool is_syncing;
    string original_primary;
    string current_primary;
    string secondary;
    string tertiary;
};

// map <char, string> kvs_worker_map;
vector <struct kvs_worker *> kvs_workers;
string dir;
int num_nodes;
map <string, struct kvs_worker *> is_alive;

void print_logs(string s) {
    cerr<<s<<endl;
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

void *handleKeepAlive(void *arg) {
    while(true) {
        sleep(5);
        for (const auto& pair : is_alive) {
            if(!is_alive[pair.first]->is_alive and is_alive[pair.first]->started) {
                //if it gets in here that means the node has crashed
                print_logs("This node is not alive: " + pair.first);
                is_alive[pair.first]->is_crashed = true;
                for(struct kvs_worker *kw: kvs_workers) {
                    if(kw->current_primary == pair.first) {
                        print_logs("PRIMARY: " + pair.first);
                        print_logs("SECONDARY: " + kw->secondary);
                        print_logs("TERTIARY: " + kw->tertiary);
                        print_logs("is_alive secondary: " + to_string(is_alive[kw->secondary]->is_alive));
                        print_logs("is_alive tertiary: " + to_string(is_alive[kw->tertiary]->is_alive));
                        if(is_alive[kw->secondary]->is_alive) {
                            kw->current_primary = kw->secondary;
                        } else {
                            kw->current_primary = kw->tertiary;
                        }
                        break;
                    }
                    // kw_i++;
                }

            } else {
                for(kvs_worker *kw: kvs_workers) {
                    if(kw->original_primary == pair.first) {
                        if(!kw->is_syncing) {
                            kw->current_primary = pair.first;
                        }
                        break;
                    }
                }
                print_logs("IS ALIVE " + pair.first + " is: " + to_string(is_alive[pair.first]->is_alive));
                is_alive[pair.first]->is_alive = false;
            }
            // std::cout << "Key: " << pair.first << std::endl;
        }
    }
}

void readConfig(std::string filename) {
    // vector <string> nodes;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        print_logs("line: " + line);
        string primary = line.substr(0, line.find(";"));
        print_logs("PRIMARYYYY: " + primary);
        line = line.substr(line.find(";") + 1);
        print_logs("line: " + line);
        string secondary = line.substr(0, line.find(";"));
        print_logs("SECONDARYYYY: " + secondary);
        line = line.substr(line.find(";") + 1);
        print_logs("line: " + line);
        string tertiary = line.substr(0, line.find(";"));
        print_logs("TERTIARYYYY: " + tertiary);

        struct kvs_worker *kw = new struct kvs_worker;
        kw->current_primary = primary;
        kw->original_primary = primary;
        kw->secondary = secondary;
        kw->tertiary = tertiary;
        kw->is_alive = false;
        kw->is_crashed = false;
        kw->started = false;

        kvs_workers.push_back(kw);
        is_alive[primary] = kw;
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
        print_logs("Current primary of required node index is: " + kvs_workers[node_index]->current_primary);
        _return = kvs_workers[node_index]->current_primary;
       
    }

    void syncComplete(string &_return, const std:: string &ip) {
        is_alive[ip]->is_syncing = false;
        _return = ip;
    }
    
    void keepAlive(string & _return,  const std:: string& ip) {
        // print_logs("Received keep alive from: " + ip);
        //if the return IP is equal to the 
        is_alive[ip]->started = true;
        if(is_alive[ip]->is_crashed) {
            //node just came back to life, send it the sync message with the node to sync with 
            is_alive[ip]->is_crashed = false;
            is_alive[ip]->is_syncing = true;
            if(is_alive[ip]->current_primary == is_alive[ip]->secondary) {
                _return = "1:" + is_alive[ip]->current_primary;
            }
            else if(is_alive[ip]->current_primary == is_alive[ip]->tertiary) {
                _return = "2:" + is_alive[ip]->current_primary;
            }

        } else {
            is_alive[ip]->is_alive = true;
            _return = ip;

        }
        
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

    pthread_t keep_alive_thread;

    if(pthread_create(&keep_alive_thread, NULL, handleKeepAlive, NULL) != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }


    shared_ptr<KvsCoordOpsHandler> handler(new KvsCoordOpsHandler());
    shared_ptr<TProcessor> processor(new KvsCoordOpsProcessor(handler));
    shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
    shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());


    TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory);

    server.serve();
    return 0;
}