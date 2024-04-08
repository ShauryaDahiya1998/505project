#include "customlib.h"
#include <string>
#include <iostream>
#include <fstream>
#include "gen-cpp/StorageOps.h"
#include <sstream>
#include <nlohmann/json.hpp>

using namespace storage;
using namespace std;


std::vector<std::string> splitString(std::string input, std::string delimiter) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = input.find(delimiter);

    while (end != std::string::npos) {
        result.push_back(input.substr(start, end - start));
        start = end + delimiter.length();
        end = input.find(delimiter, start);
    }

    result.push_back(input.substr(start));

    return result;
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

    if (command == "/" || command == "/signup" || command == "/login") {
        //check if the cookie that we got in sessionID exists in cookie map, if yes send hey
        cout<<req<<endl;
        int cookieIndex = req.find("Cookie: sessionID=");
        if (cookieIndex != -1) {
            string sessionID = req.substr(cookieIndex + 18, 7);
            if (sessions.find(sessionID) != sessions.end()) {
                cout<<"Session Found"<<endl;
                string msg = "<h1>Hey " + sessions[sessionID].username + "</h1>";
                response.content_type = "text/html";
                response.message = msg;
                response.sessionID = "1111";   //How to get session ID here??????
                string createResponseForPostRequest = response.createGetResponse(response);
                return createResponseForPostRequest;
                
                // string httpResponseHeader = "HTTP/1.1 200 OK\r\n"
                //                    "Content-Type: text/html\r\n"
                //                    "Content-Length: " + to_string(msg.size()) + "\r\n\r\n";
                // return httpResponseHeader + msg;
            }
        } 
        ifstream ifs("./pages/index.html");
        string homepage((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>()));
        response.content_type = "text/html";
        response.message = homepage;
        response.sessionID = "1111";   //How to get session ID here??????
        string createResponseForPostRequest = response.createGetResponse(response);
        return createResponseForPostRequest;
    } 
    // else if (command == "/login") {
    //     ifstream ifs("./pages/index.html");
    //     string homepage((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>()));
    //     string httpResponseHeader = "HTTP/1.1 200 OK\r\n"
    //                                "Content-Type: text/html\r\n"
    //                                "Content-Length: " + std::to_string(homepage.size()) + "\r\n\r\n";
    //     return httpResponseHeader + homepage;
    // } 
    else if (command == "/landing") {
        ifstream ifs("./pages/landing.html");
        string homepage((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>()));
        response.content_type = "text/html";
        response.message = homepage;
        response.sessionID = "1111";   //How to get session ID here??????
        string createResponseForPostRequest = response.createGetResponse(response);
        return createResponseForPostRequest;

    } else if (command == "/mailbox") {
        cout << "HER";
        ifstream ifs("./pages/mailbox.html");
        string homepage((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>()));
        response.content_type = "text/html";
        response.message = homepage;
        response.sessionID = "1111";   //How to get session ID here??????
        string createResponseForPostRequest = response.createGetResponse(response);
        return createResponseForPostRequest;
    } else if (command == "/getMail") {
        string row = "sukriti-mailbox"; //TODO
        string emailHashes;
        client.get(emailHashes,row,"AllEmails");
        cout << "HERE" << emailHashes << endl;
        vector<string> allEmails;
        if(!emailHashes.empty() && emailHashes[0]!='-')
            allEmails = splitString(emailHashes,",");
        vector<nlohmann::json> sortedEmails;
        for (const auto& str : allEmails) {
            string email;
            client.get(email,row,str);
            vector<string> emailComp = splitString(email,"\r\n");
            nlohmann::json j;
            j["hash"] = str;
            j["from"] = emailComp[0]; 
            j["time"] = emailComp[1]; 
            j["subject"] = emailComp[2]; 
            j["body"] = emailComp[3]; 
            string jsonString = j.dump();
            // responseJson[str] = j;
            sortedEmails.push_back(j);
            cout << jsonString << endl;
        }
        sort(sortedEmails.begin(), sortedEmails.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
            return a["time"] > b["time"]; 
        });
        nlohmann::json responseJson = nlohmann::json(sortedEmails);
        string jsonString = responseJson.dump();
        response.content_type = "application/json";
        response.message = jsonString;
        response.sessionID = "1111";   //How to get session ID here??????
        string createResponseForPostRequest = response.createGetResponse(response);
        return createResponseForPostRequest;
    }else if (command == "/downloadFile") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>downloadFile</h1>";
    } else if (command == "/listFile") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>listFile</h1>";
    }
    else {
        return "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n<h1>404 Not Found</h1>";
    }

}