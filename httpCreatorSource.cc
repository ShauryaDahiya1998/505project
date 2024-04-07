#include "customlib.h"

using namespace std;

string HttpResponseCreator::createPostResponse(HttpResponseCreator response) {
    string createResponseForPostRequest = "HTTP/1.1 200 OK\r\n"
                "Content-Type: " + response.content_type + "\r\n"
                "Content-Length: " + std::to_string(response.message.size()) + "\r\n"
                "Set-Cookie: sessionID=" + response.sessionID + "\r\n\r\n" +
                response.message;
    return createResponseForPostRequest;
}
string HttpResponseCreator::createGetResponse(HttpResponseCreator response) {
    string createResponseForPostRequest = "HTTP/1.1 200 OK\r\n"
                "Content-Type: " + response.content_type + "\r\n"
                "Content-Length: " + std::to_string(response.message.size()) + "\r\n"
                "Set-Cookie: sessionID=" + response.sessionID + "\r\n\r\n" +
                response.message;
    return createResponseForPostRequest;
}

void setSession(string username, string data, string mdata, string expirationTime, string sessionID, StorageOpsClient client) {
    string putValue = "";
    putValue += username + "," + "," + data + "," + mdata + "," + expirationTime;
    client.put(putValue, sessionID, "1");
}