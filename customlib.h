#ifndef CUSTOMLIB_H
#define CUSTOMLIB_H
#include <string>
#include <map>
#include "gen-cpp/StorageOps.h"
#include "gen-cpp/KvsCoordOps.h"
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
using namespace storage;
using namespace std;
// Function declaration


using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

// Handler to return the get method commands
std::string getMethodHandler(std::string command, KvsCoordOpsClient client);
std::string postMethodhandler(std::string command, std::string body, KvsCoordOpsClient client);
void setSession(string username, string data, string mdata, string expirationTime, string sessionID, KvsCoordOpsClient client);
int smtpClient(map<string, string> json,std::string domain,std::string body);
std::string getUsernameFromEmail(const std::string& email);
string getWorkerIP(string row, KvsCoordOpsClient client);
std::tuple<std::shared_ptr<TTransport>, StorageOpsClient> getKVSClient(string ipPort);

class HttpResponseCreator {
  public:
    string content_type;
    string methodType;
    string message;
    string status;
    string sessionID;
    string createPostResponse(HttpResponseCreator response);
    string createGetResponse(HttpResponseCreator response);
    HttpResponseCreator() {
      content_type = "";
      methodType = "";
      message = "";
      status = "";
      sessionID = "";
    }
};

#endif // CUSTOMLIB_H
