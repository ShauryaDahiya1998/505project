#include <iostream>
#include <string>
#include <memory>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include "gen-cpp/StorageOps.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace storage;
using namespace std;

int main(int argc, char **argv) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <server_ip> <server_port>" << endl;
        return 1;
    }

    string server_ip = argv[1];
    int server_port = atoi(argv[2]);

    shared_ptr<TTransport> socket(new TSocket("127.0.0.1", server_port));
    shared_ptr<TTransport> transport(new TBufferedTransport(socket));
    shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
    StorageOpsClient client(protocol);


    try {
        transport->open();

        while (true) {
            cout << "Enter operation (get, put, delete, cput) followed by row, column, and value (for put or cput): ";
            string operation, row, col, value;
            cin >> operation;

            if (operation == "get" || operation == "delete") {
                cin >> row >> col;
            } else if (operation == "put" || operation == "cput") {
                cin >> row >> col >> value;
            } else {
                cerr << "Invalid operation" << endl;
                continue;
            }

            if (operation == "get") {
                string result;
                client.get(result, row, col);
                cout << "GET result: " << result << endl;
            } else if (operation == "put") {
                bool success = client.put(row, col, value);
                cout << "PUT " << (success ? "successful" : "failed") << endl;
            } else if (operation == "delete") {
                bool success = client.deleteCell(row, col);
                cout << "DELETE " << (success ? "successful" : "failed") << endl;
            } else if (operation == "cput") {
                cout << "Enter old value: ";
                string old_value;
                cin >> old_value;
                bool success = client.cput(row, col, old_value, value);
                cout << "CPUT " << (success ? "successful" : "failed") << endl;
            }
        }

        transport->close();
    } catch (TException &tx) {
        cerr << "ERROR: " << tx.what() << endl;
    }

    return 0;
}
