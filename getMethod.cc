#include "customlib.h"
#include <string>
#include <iostream>
#include <fstream>
#include "gen-cpp/StorageOps.h"
using namespace storage;
using namespace std;

string sendToLandingPage(StorageOpsClient client, string sessionId) {
    ifstream ifs("./pages/landing.html");
    string landingPage((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>()));
    HttpResponseCreator response;
    response.content_type = "text/html";
    response.message = landingPage;
    if (sessionId != "") {
        response.sessionID = sessionId;
    }
    string createResponseForPostRequest = response.createGetResponse(response);
    return createResponseForPostRequest;
}

string sendToLoginPage(StorageOpsClient client) {
    ifstream ifs("./pages/index.html");
    string homepage((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>()));
    HttpResponseCreator response;
    response.content_type = "text/html";
    response.message = homepage;
    response.sessionID = "";
    string createResponseForPostRequest = response.createGetResponse(response);
    return createResponseForPostRequest;
}

string getMethodHandler(string command, StorageOpsClient client) {
    string req = command;
    // find the first /
    size_t firstSlash = command.find("/");
    // find the first space after firstSlash index and keep the result in a variable
    command = command.substr(firstSlash);
    command = command.substr(0, command.find(" "));
    HttpResponseCreator response;
    // handle the commandType and return the appropriate response

    if (command == "/" || command == "/login") {
        //check if the cookie that we got in sessionID exists in cookie map, if yes send hey
        cout<<req<<endl;
        int cookieIndex = req.find("Cookie: sessionID=");
        if (cookieIndex != -1) {
            string sessionID = req.substr(cookieIndex + 18, 7);
            string sessionResp;
            client.get(sessionResp, sessionID, "1");
            cout<<"check seeesion id here -> "<<sessionID<<endl;
            if (sessionResp.find("-ERR") == string::npos){
                return sendToLandingPage(client, sessionID);
            }
        } 
        return sendToLoginPage(client);
        // ifstream ifs("./pages/index.html");
        // string homepage((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>()));
        // response.content_type = "text/html";
        // response.message = homepage;
        // string createResponseForPostRequest = response.createGetResponse(response);
        // return createResponseForPostRequest;
    } 
    else if (command == "/signup") {
        return sendToLoginPage(client);
    } 
    else if (command == "/landing") {
        string sessionId ="";
        return sendToLandingPage(client, sessionId);
    } else if (command == "/inbox") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>Inbox Us</h1>";
    } else if (command == "/downloadFile") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>downloadFile</h1>";
    } else if (command == "/listFile") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>listFile</h1>";
    } else if(command == "/logout") {
        int cookieIndex = req.find("Cookie: sessionID=");
        string sessionID = req.substr(cookieIndex + 18, 7);
        // response.content_type = "text/html";
        // response.message = "Logged out successfully";
        client.deleteCell(sessionID, "1");
        string hel = sendToLoginPage(client);
        cout<<hel<<endl;
        return hel;
        // string createResponseForPostRequest = response.createGetResponse(response);
        // return createResponseForPostRequest;
    }

    else {
        return "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n<h1>404 Not Found</h1>";
    }

}