#include "customlib.h"

using namespace std;

map<string, UserSession> sessions;

string HttpResponseCreator::createPostResponse(HttpResponseCreator response) {
    string createResponseForPostRequest = "HTTP/1.1 200 OK\r\n"
                "Content-Type: " + response.content_type + "\r\n"
                "Content-Length: " + std::to_string(response.message.size()) + "\r\n"
                "Set-Cookie: sessionID=" + response.sessionID + "\r\n\r\n" +
                response.message;
    return createResponseForPostRequest;
}

void UserSession::setSession(string username, string email, string data, string mdata, string expirationTime) {
    this->username = username;
    this->email = email;
    this->data = data;
    this->mdata = mdata;
    this->expirationTime = expirationTime;
}