#include <iostream>
#include <string>
#include <memory>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include "gen-cpp/KvsCoordOps.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace std;

int main(int argc, char **argv) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <server_ip> <server_port>" << endl;
        return 1;
    }

    string server_ip = argv[1];
    // int server_port = atoi(argv[2]);

    shared_ptr<TTransport> socket(new TSocket("127.0.0.1", 8000));
    shared_ptr<TTransport> transport(new TBufferedTransport(socket));
    shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
    KvsCoordOpsClient client(protocol);


    try {
        transport->open();

        while (true) {
            cout << "Enter operation (get, put, delete, cput) followed by row, column, and value (for put or cput): ";
            string operation, row, col, value;
            cin >> operation;

            if (operation == "get" || operation == "delete") {
                cin >> row;
            } else if (operation == "put" || operation == "cput") {
                cin >> row >> col >> value;
            } else {
                cerr << "Invalid operation" << endl;
                continue;
            }

            if (operation == "get") {
                string result;
                client.get(result, row);
                cout << "GET result: " << result << endl;
            }
        }

        transport->close();
    } catch (TException &tx) {
        cerr << "ERROR: " << tx.what() << endl;
    }

    return 0;
}
