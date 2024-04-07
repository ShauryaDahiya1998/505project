#ifndef CUSTOMLIB_H
#define CUSTOMLIB_H
#include <string>
#include <map>
#include "gen-cpp/StorageOps.h"
using namespace storage;
using namespace std;
// Function declaration

// Handler to return the get method commands
std::string getMethodHandler(std::string command, StorageOpsClient client);
std::string postMethodhandler(std::string command, std::string body, StorageOpsClient client);
void setSession(string username, string data, string mdata, string expirationTime, string sessionID, StorageOpsClient client);

class HttpResponseCreator {
  public:
    string content_type;
    string methodType;
    string message;
    string status;
    string sessionID;
    string createPostResponse(HttpResponseCreator response);
    string createGetResponse(HttpResponseCreator response);
};

#endif // CUSTOMLIB_H
