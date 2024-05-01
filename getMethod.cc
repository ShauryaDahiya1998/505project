#include "customlib.h"
#include <string>
#include <iostream>
#include <fstream>
#include "gen-cpp/StorageOps.h"
#include <sstream>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <nlohmann/json.hpp>
#include "gen-cpp/KvsCoordOps.h"


using namespace storage;
using namespace std;

string sendToLandingPage(string sessionId) {
    ifstream ifs("./pages/landing2.html");
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

string sendToLoginPage() {
    ifstream ifs("./pages/index.html");
    string homepage((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>()));
    HttpResponseCreator response;
    response.content_type = "text/html";
    response.message = homepage;
    response.sessionID = "";
    string createResponseForPostRequest = response.createGetResponse(response);
    return createResponseForPostRequest;
}
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

string getMethodHandler(string command, KvsCoordOpsClient client) {
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
            auto [transport, kvsClient] =  getKVSClient(getWorkerIP(sessionID,client));
            kvsClient.get(sessionResp, sessionID, "1");
            transport->close();
            cout<<"check seeesion id here -> "<<sessionID<<endl;
            if (sessionResp.find("-ERR") == string::npos){
                return sendToLandingPage(sessionID);
            }
        } 
        return sendToLoginPage();
        // ifstream ifs("./pages/index.html");
        // string homepage((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>()));
        // response.content_type = "text/html";
        // response.message = homepage;
        // string createResponseForPostRequest = response.createGetResponse(response);
        // return createResponseForPostRequest;
    } 
    else if (command == "/signup") {
        return sendToLoginPage();
    } 
    else if (command == "/landing") {
        string sessionId ="";
        return sendToLandingPage(sessionId);
    } else if (command == "/inbox") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>Inbox Us</h1>";
    } else if (command == "/downloadFile") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>downloadFile</h1>";
    } else if (command == "/mailbox") {
        cout << "HER";
        int cookieIndex = req.find("Cookie: sessionID=");
        string sessionResp;
        string sessionID = "1111";
        if (cookieIndex != -1) {
            sessionID = req.substr(cookieIndex + 18, 7);
        } 
        cout << sessionResp << endl;
        ifstream ifs("./pages/mailbox.html");
        string homepage((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>()));
        response.content_type = "text/html";
        response.message = homepage;
        response.sessionID = sessionID;   //How to get session ID here??????
        string createResponseForPostRequest = response.createGetResponse(response);
        return createResponseForPostRequest;
    } else if (command == "/getUser") {
        cout << "HER1245";
        int cookieIndex = req.find("Cookie: sessionID=");
        string sessionResp;
        string sessionID = "1111";
        if (cookieIndex != -1) {
            sessionID = req.substr(cookieIndex + 18, 7);
            auto [transport, kvsClient] =  getKVSClient(getWorkerIP(sessionID,client));
            kvsClient.get(sessionResp, sessionID, "1");
            transport->close();
            cout<<"check seeesion id here -> "<<sessionID<<endl;
        } 
        cout << sessionResp << endl;
        string user = getUsernameFromEmail(splitString(sessionResp,",")[0]);
        nlohmann::json j;
        j["user"] = user;
        response.content_type = "application/json";
        string jsonString = j.dump();
        response.message = jsonString;
        response.sessionID = sessionID;   //How to get session ID here??????
        string createResponseForPostRequest = response.createGetResponse(response);
        return createResponseForPostRequest;
    } else if (command == "/getMail") {
        int cookieIndex = req.find("Cookie: sessionID=");
        string sessionResp;
        string sessionID = "1111";
        if (cookieIndex != -1) {
            sessionID = req.substr(cookieIndex + 18, 7);
            auto [transport, kvsClient] =  getKVSClient(getWorkerIP(sessionID,client));
            kvsClient.get(sessionResp, sessionID, "1");
            transport->close();
            cout<<"check seeesion id here -> "<<sessionID<<endl;
            if (sessionResp.find("-ERR") != string::npos){
                return sendToLandingPage(sessionID);
            }
        } 
        cout << sessionResp << endl;
        string user = splitString(sessionResp,",")[0];
        string row = getUsernameFromEmail(user)+"-mailbox"; 
        string emailHashes;
        auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
        kvsClient.get(emailHashes,row,"AllEmails");
        transport->close();
        cout << "HERE" << emailHashes << endl;
        vector<string> allEmails;
        if(!emailHashes.empty() && emailHashes[0]!='-')
            allEmails = splitString(emailHashes,",");
        vector<nlohmann::json> sortedEmails;
        for (const auto& str : allEmails) {
            string email;
            auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
            kvsClient.get(email,row,str);
            
            std::cout << "EMAIL GOTTEN " << email << endl;
            vector<string> emailComp = splitString(email,"\r\n");
                     std::cout << emailComp[0] << " Completed " << endl;
            std::cout << emailComp[1] << " Completed " << endl;
            std::cout << emailComp[2] << " Completed " << endl;
            std::cout << emailComp[3] << " Completed " << endl;
            nlohmann::json j;
            j["hash"] = str;
            j["from"] = emailComp[0]; 
            j["time"] = emailComp[1]; 
            j["subject"] = emailComp[2]; 
            j["body"] = emailComp[3]; 
   

            // string attachHash;
            // kvsClient.get(attachHash,row,str+"-Attachments");
            // if(!attachHash.empty() && attachHash[0]!='-')
            // {            
            //     vector<string> attachments;
            //     vector<nlohmann::json> attJsons;
            //     attachments = splitString(attachHash,",");
            //     nlohmann::json ja;
            //     for (const auto& att : attachments){
            //         string attachment;
            //         kvsClient.get(attachment,row,str+"-"+att);
            //         vector<string> failComp = splitString(attachment,"\r\n");
            //         ja["fileName"] = failComp[0]; 
            //         ja["fileContent"] = failComp[2]; 
            //         attJsons.push_back(ja);
            //     }
            //     j["attachment"] = attJsons;
            // }
            string jsonString = j.dump();
            // responseJson[str] = j;
            sortedEmails.push_back(j);
            cout << jsonString << endl;
        }
        sort(sortedEmails.begin(), sortedEmails.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
            return a["time"] > b["time"]; 
        });
            transport->close();

        nlohmann::json responseJson = nlohmann::json(sortedEmails);
        string jsonString = responseJson.dump();
        response.content_type = "application/json";
        response.message = jsonString;
        response.sessionID = sessionID;   //How to get session ID here??????
        string createResponseForPostRequest = response.createGetResponse(response);
        return createResponseForPostRequest;
    } else if (command == "/drive") {
        cout << "HER";
        int cookieIndex = req.find("Cookie: sessionID=");
        string sessionResp;
        string sessionID = "1111";
        if (cookieIndex != -1) {
            sessionID = req.substr(cookieIndex + 18, 7);
        } 
        cout << sessionResp << endl;
        ifstream ifs("./pages/drive.html");
        string homepage((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>()));
        response.content_type = "text/html";
        response.message = homepage;
        response.sessionID = sessionID;   //How to get session ID here??????
        string createResponseForPostRequest = response.createGetResponse(response);
        return createResponseForPostRequest;
    }  else if (command == "/getFiles") {
        cout << "GET FILE"<<endl;
        int cookieIndex = req.find("Cookie: sessionID=");
        string sessionResp;
        string sessionID = "1111";
        if (cookieIndex != -1) {
            sessionID = req.substr(cookieIndex + 18, 7);
            auto [transport, kvsClient] =  getKVSClient(getWorkerIP(sessionID,client));
            kvsClient.get(sessionResp,sessionID,"1");
            transport->close();
            cout<<"check seeesion id here -> "<<sessionID<<endl;
            if (sessionResp.find("-ERR") != string::npos){
                return sendToLandingPage(sessionID);
            }
        } 
        cout << sessionResp << endl;
        string user = splitString(sessionResp,",")[0];
        string username = getUsernameFromEmail(user);

        string row = username+"-storage"; 
        string fileHashes;
        auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
        kvsClient.get(fileHashes,row,"AllFiles");
        transport->close();
        cout << "HERE" << fileHashes << endl;
        vector<string> allFiles;
        vector<nlohmann::json> sortedFiles;

        if(!fileHashes.empty() && fileHashes[0]!='-')
            allFiles = splitString(fileHashes,",");
        for (const auto& str : allFiles) {
            string file;
            auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
            kvsClient.get(file,row,str+"-NameOnly");
            transport->close();
            nlohmann::json j;
            j["hash"] = str; 
            j["name"] = file; 
            string jsonString = j.dump();
            sortedFiles.push_back(j);
        }
  
        nlohmann::json responseJson = nlohmann::json(sortedFiles);
        string jsonString = responseJson.dump();
        response.content_type = "application/json";
        response.message = jsonString;
        response.sessionID = sessionID;   //How to get session ID here??????
        string createResponseForPostRequest = response.createGetResponse(response);
        return createResponseForPostRequest;
    } else if (command == "/listFile") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>listFile</h1>";
    } else if(command == "/logout") {
        int cookieIndex = req.find("Cookie: sessionID=");
        string sessionID = req.substr(cookieIndex + 18, 7);
        // response.content_type = "text/html";
        // response.message = "Logged out successfully";
        auto [transport, kvsClient] =  getKVSClient(getWorkerIP(sessionID,client));
        kvsClient.deleteCell(sessionID, "1");
        transport->close();
        string hel = sendToLoginPage();
        cout<<hel<<endl;
        return hel;
        // string createResponseForPostRequest = response.createGetResponse(response);
        // return createResponseForPostRequest;
    }

    else {
        return "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n<h1>404 Not Found</h1>";
    }

}