#ifndef CUSTOMLIB_H
#define CUSTOMLIB_H
#include <string>
#include <map>
using namespace std;
// Function declaration

// Handler to return the get method commands
std::string getMethodHandler(std::string command);
std::string postMethodhandler(std::string command, std::string body);

class UserSession {
  public:
    string username;
    string email;
    string data;
    string mdata;
    string expirationTime;
    string sessionID;
    void setSession(string username, string email, string data, string mdata, string expirationTime);
};

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

extern map<string, UserSession> sessions;

#endif // CUSTOMLIB_H
