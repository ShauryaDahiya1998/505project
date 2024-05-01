#include "customlib.h"
#include <string>
#include <iostream>
#include <random>
#include <openssl/sha.h>

#include <sstream>
#include <iomanip>
#include <chrono>
#include "gen-cpp/StorageOps.h"
#include <nlohmann/json.hpp>
#include "gen-cpp/KvsCoordOps.h"
using namespace storage;
using namespace std;

map<string, string> parseJsonLikeString(const string& jsonString) {
    map<string, string> result;
    size_t pos = 0;
    size_t end;

    while ((pos = jsonString.find('"', pos)) != string::npos) {
        end = jsonString.find('"', pos + 1);
        string key = jsonString.substr(pos + 1, end - pos - 1);

        // Start searching for the next quote after the colon following the key
        pos = jsonString.find(':', end) + 1;
        // Skip any whitespace or other characters (like quotes) before the value
        while (jsonString[pos] == ' ' || jsonString[pos] == '\"') {
            pos++;
        }
        end = jsonString.find('"', pos + 1);
        string value = jsonString.substr(pos, end - pos);

        result[key] = value;

        // Ensure the next search starts after the closing quote of the current value
        pos = end + 1;
    }

    return result;
}

std::vector<std::string> splitStrings(std::string input, std::string delimiter) {
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

std::string vectorToString( std::vector<string> vec, std::string separator) {
    std::stringstream ss;
    for(size_t i = 0; i < vec.size(); ++i) {
        if(i != 0)
            ss << separator;
        ss << vec[i]; // Directly works for string, use std::to_string for other types
    }
    return ss.str();
}

bool checkValidUser(string username, string password, KvsCoordOpsClient client) {
    // check if the email and password are correct
    string response;
    auto [transport, kvsClient] =  getKVSClient(getWorkerIP(username,client));
    kvsClient.get(response, username, "password");
    transport->close();
    //If response starts with -ERR then return false
    cout<<"Response: "<<response<<endl;
    if(response.find("-ERR") != string::npos) {
        return false;
    }
    else if (response != password) {
        return false;
    }

    return true;
}

string createSession(string username, KvsCoordOpsClient client) {
    // create a session and return the session id
    random_device rd;
    mt19937 gen(rd());
    // keep creating random session id until we get a unique one
    uniform_int_distribution<> dis(1000000, 9999999);
    string sessionID = to_string(dis(gen));
    while(1) {
      string resp;
      auto [transport, kvsClient] =  getKVSClient(getWorkerIP(sessionID,client));
      kvsClient.get(resp, sessionID, "1");
      transport->close();
      if (resp.find("-ERR") != string::npos) {
        break;
      }
      sessionID = to_string(dis(gen));
    }
    return to_string(dis(gen));
}

std::string unescape(const std::string& input) {
    std::string output;
    output.reserve(input.length()); // Optimize memory allocation

    for (std::size_t i = 0; i < input.length(); ++i) {
        if (input[i] == '\\' && i + 1 < input.length()) {
            // Handle common escape sequences
            switch (input[i + 1]) {
                case 'n': output += '\n'; ++i; break; // New line
                case 't': output += '\t'; ++i; break; // Tab
                case '\\': output += '\\'; ++i; break; // Backslash
                case '"': output += '"'; ++i; break; // Double quote
                case 'r': output += '\r'; ++i; break; // Carriage return
                // Add more cases as needed
                default: output += input[i]; break; // Not an escape sequence
            }
        } else {
            output += input[i];
        }
    }

    return output;
}

std::string getUsernameFromEmail(const std::string& email) {
    size_t atPos = email.find('@');
    
    if (atPos != std::string::npos) {
        return email.substr(0, atPos);
    }
  
    return "";
}

std::string getDomainFromEmail(std::string email) {
    size_t atPosition = email.find('@');
    if (atPosition != std::string::npos) {
        return email.substr(atPosition + 1); // Return the substring after '@'
    }
    return ""; // Return an empty string if '@' is not found
}

std::string getCurrentTimestamp() {
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    
    // Convert to local time
    std::tm localTime;
    localtime_r(&now_c, &localTime); // POSIX thread-safe
    
    // Format as string
    std::stringstream ss;
    ss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string sha256(string input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, input.c_str(), input.size());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

string postMethodhandler(string command, string body, KvsCoordOpsClient client) {
    // find the first /
    string req = command;

    size_t firstSlash = command.find("/");
    // find the first space after firstSlash index and keep the result in a variable
    command = command.substr(firstSlash);
    command = command.substr(0, command.find(" "));
    HttpResponseCreator response;
    // handle the commandType and return the appropriate response
    if (command == "/login") {
        // here we get a email and password from the body in the format {email:"email", password:"password"}
        // parse this and get the email and passowrd

        // size_t emailStart = body.find("email\":") + 8;
        // string email = body.substr(emailStart, body.find("\",") - emailStart);
        // size_t passwordStart = body.find("password\":") + 11;
        // string password = body.substr(passwordStart, body.find("\"}") - passwordStart);
        auto parsed = parseJsonLikeString(body);
        string username = parsed["username"];
        string password = parsed["password"];
        cout<<"Details: "<<username<<" "<<password<<endl;  

        // check if the email and password are correct
        if (checkValidUser(username, password, client)) {
          // if correct, create a session and return the session id
        //   cout<<client.get(email, password)<<endl; 
          string sessionID = createSession(username, client);
          string message = "{\"message\": \"Login Successful\", \"sessionID\": \"" + sessionID + "\"}";
          response.content_type = "application/json";
          response.message = message;
          response.sessionID = sessionID;
          string createResponseForPostRequest = response.createPostResponse(response);

          // set cookie in the cookie map
          // TODO: Move inside createSession
          setSession(username, username, username, "6000", sessionID, client);
          
          return createResponseForPostRequest;                                                    
        } else {
            string message = "{\"message\": \"Invalid Login Credentials\"}";
            response.content_type = "application/json";
            response.message = message;
            string createResponseForPostRequest = response.createPostResponse(response);
            return createResponseForPostRequest;
        }

    } else if (command == "/signup") {
        // here we get a username, email and password from the body in the format {"username":"hi","email":"hi","password":"hi"}
        // parse this and get the username, email and passowrd
        // size_t pos = 0;
        // size_t end;
        // pos = body.find('"', pos);
        // end = body.find("}");
        // size_t usernameStart = body.find("username\":") + 11;
        // string username = body.substr(usernameStart, body.find("\",") - usernameStart);
        // size_t emailStart = body.find("email\":") + 8;
        // body = body.substr(emailStart);
        // pos = body.find(':"', pos);
        // end = body.find("\",", pos+2);
        // string email = body.substr(pos+2, end - pos-2);
        // size_t passwordStart = body.find("password\":") + 11;
        // body = body.substr(passwordStart);
        // pos = body.find(':"', pos);
        // end = body.find("\"}", pos+2);
        auto parsed = parseJsonLikeString(body);
        string username = parsed["username"];
        //string email = parsed["email"];
        string password = parsed["password"];
        cout<<"Details: "<<username<<" "<<password<<endl;
        // check if the email is already registered
        if (!checkValidUser(username, password, client)) {
            // if not registered, create a new user and return the success message
            auto [transport, kvsClient] =  getKVSClient(getWorkerIP(username,client));
            kvsClient.put(username, "password", password);
            transport->close();
            string message = "{\"message\": \"Account Created\"}";
            response.content_type = "application/json";
            response.message = message;
            string createResponseForPostRequest = response.createPostResponse(response);
            return createResponseForPostRequest;   
            
        } else {
           string message = "{\"message\": \"Account already exists\"}";
            response.content_type = "application/json";
            response.message = message;
            string createResponseForPostRequest = response.createPostResponse(response);
            return createResponseForPostRequest;   
        }

        // return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>About Us</h1>";
    } else if (command == "/login") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>Contact Us</h1>";
    } else if (command == "/deleteFile") {
<<<<<<< HEAD
        cout<<"DELETE File/folder"<<endl;
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
        string user = splitStrings(sessionResp,",")[0];
        string username = getUsernameFromEmail(user);

        auto parsed = nlohmann::json::parse(body);
        string fileName = parsed["fileName"];
        string outerFolderName = parsed["folderName"];
        string row = username+"-storage";
        string col = sha256(fileName);
        string fileType = parsed["fileType"];
        if (fileType == "folder") {
            return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>deleteFolder</h1>";
        } else {
            if (outerFolderName == "root") {
                string fileHashes;
                auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
                kvsClient.get(fileHashes,row,"AllFiles");
                transport->close();
                vector<string> fileHashSet = splitStrings(fileHashes,",");
                // iterate through the hash of the file and delete the file
                string fileHashLeft = "";
                for (const auto& hash : fileHashSet) {
                    if (hash != col+ "-0") {
                        if (fileHashLeft == "") {
                            fileHashLeft = hash;
                        } else {
                            fileHashLeft = fileHashLeft + "," + hash;
                        }
                    }
                }
                cout<<"File Hash Left: "<<fileHashLeft<<endl;
                auto [transport1, kvsClient1] =  getKVSClient(getWorkerIP(row,client));
                kvsClient1.put(row,"AllFiles",fileHashLeft);
                transport1->close();
                // delete all the chunks of the file and also delete its name
                int index = 0;
                while(1) {
                    string chunk = sha256(fileName) + "-" + to_string(index);
                    string file;
                    auto [transport2, kvsClient2] =  getKVSClient(getWorkerIP(row,client));
                    kvsClient2.get(file,row,chunk);
                    transport2->close();
                    cout<<"File: "<<file<<endl;
                    if (file[0] == '-') {
                        break;
                    }
                    auto [transport3, kvsClient3] =  getKVSClient(getWorkerIP(row,client));
                    bool a,b;
                    a = kvsClient3.deleteCell(row,chunk);
                    b = kvsClient3.deleteCell(row,chunk+"-NameOnly");
                    cout<<"Delete: "<<a<<" "<<b<<endl;
                    transport3->close();
                    index++;
                
                }
            } else {
                string folderHashes;
                auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
                kvsClient.get(folderHashes,row,sha256(outerFolderName));
                transport->close();
                vector<string> folderHashesSet = splitStrings(folderHashes,"\r\n");
                vector<string> filesNames = splitStrings(folderHashesSet[3],",");
                string fileHashLeft = "";
                for (const auto& hash : filesNames) {
                    if (hash != col+ "-0") {
                        if (fileHashLeft == "") {
                            fileHashLeft = hash;
                        } else {
                            fileHashLeft = fileHashLeft + "," + hash;
                        }
                    }
                }
                if (fileHashLeft == "") {
                    fileHashLeft = "NULL";
                }
                string newFolderHashes = folderHashesSet[0] + "\r\n" + folderHashesSet[1] + "\r\n" + folderHashesSet[2] + "\r\n" + fileHashLeft;
                auto [transport1, kvsClient1] =  getKVSClient(getWorkerIP(row,client));
                kvsClient1.put(row,sha256(outerFolderName),newFolderHashes);
                transport1->close();
                // delete all the chunks of the file and also delete its name
                int index = 0;
                while(1) {
                    string chunk = sha256(fileName) + "-" + to_string(index);
                    string file;
                    auto [transport2, kvsClient2] =  getKVSClient(getWorkerIP(row,client));
                    kvsClient2.get(file,row,chunk);
                    transport2->close();
                    if (file[0] == '-') {
                        break;
                    }
                    auto [transport3, kvsClient3] =  getKVSClient(getWorkerIP(row,client));
                    kvsClient3.deleteCell(row,chunk);
                    kvsClient3.deleteCell(row,chunk+"-NameOnly");
                    transport3->close();
                    index++;
                }
            
            }
        }
        response.content_type = "application/json";
        response.message = "{}";
        response.sessionID = sessionID;
        string createResponseForPostRequest = response.createPostResponse(response);
        return createResponseForPostRequest;
        
=======
>>>>>>> adminConsole
    } else if (command == "/createFile") {
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
        string user = splitStrings(sessionResp,",")[0];
        string username = getUsernameFromEmail(user);

        string index;
        size_t pos = req.find("Content-Range: ");
        if (pos != string::npos) {
            pos += 15; // "Content-Range:" is 14 characters long
            size_t endOfHeader = req.find('\r', pos);
            index = req.substr(pos, endOfHeader - pos);
        }

        auto parsed = nlohmann::json::parse(body);
        string fileName = parsed["fileName"];
        cout << fileName  << endl;
        string fileContent = unescape(parsed["fileContent"]);
        string fileType = parsed["fileType"];
        string folderName = parsed["folderName"];
        string row = username+"-storage";
        string timestamp = getCurrentTimestamp();
        string file = fileName + "\r\n" + timestamp + "\r\n" + fileType+ "\r\n" + fileContent;
        string col = sha256(fileName)+"-"+index;

        auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
        kvsClient.put(row,col,file);
        kvsClient.put(row,col+"-NameOnly",fileName);
        if(index=="0")
        {
            if (folderName == "root") {
                string fileHashes;
                kvsClient.get(fileHashes,row,"AllFiles");
                if(fileHashes.empty() || fileHashes[0]=='-')
                    fileHashes = col;
                else
                    fileHashes = fileHashes + "," + col;
                kvsClient.put(row,"AllFiles",fileHashes);
            } else {
                string folderHashes;
                
                kvsClient.get(folderHashes,row,sha256(folderName));
                vector<string> folderHashesSet = splitStrings(folderHashes,"\r\n");
                if(folderHashesSet[3] == "NULL")
                {
                    folderHashesSet[3] = col;
                }
                else
                {
                    folderHashesSet[3] = folderHashesSet[3] + "," + col;
                }
                string newFolderHashes = folderHashesSet[0] + "\r\n" + folderHashesSet[1] + "\r\n" + folderHashesSet[2] + "\r\n" + folderHashesSet[3];
                cout<<"New Folder Hashes: "<<newFolderHashes<<endl;
                kvsClient.put(row,sha256(folderName),newFolderHashes);
            }
        } 
        transport->close();

        response.content_type = "application/json";
        response.message = "";
        response.sessionID = sessionID;
        string createResponseForPostRequest = response.createPostResponse(response);

        return createResponseForPostRequest;
    } else if (command == "/viewFile") {
        cout << "VIEW FILE" << endl;
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
        string user = splitStrings(sessionResp,",")[0];
        string username = getUsernameFromEmail(user);

        auto parsed = nlohmann::json::parse(body);

        string fileName = parsed["fileName"];
        cout << fileName << endl;
        string chunk = parsed["chunkIndex"];
        cout << chunk << endl;
        string folderName = parsed["folderName"];
        string row = username+"-storage";
        string col = sha256(fileName)+"-"+chunk;

        //check if the file exists in the folder or not
        if (folderName == "root") {
            string fileHashes;
            auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
            kvsClient.get(fileHashes,row,"AllFiles");
            transport->close();
            vector<string> fileHashSet = splitStrings(fileHashes,",");
            if (find(fileHashSet.begin(), fileHashSet.end(), col) == fileHashSet.end() && chunk== "0") {
                response.content_type = "application/json";
                response.message = "{}";
                response.sessionID = sessionID;   //How to get session ID here??????
                string createResponseForPostRequest = response.createGetResponse(response);
                return createResponseForPostRequest;
            }
        } else {
            string folderHashes;
            auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
            kvsClient.get(folderHashes,row,sha256(folderName));
            transport->close();
            vector<string> folderHashesSet = splitStrings(folderHashes,"\r\n");
            if(folderHashesSet[3] == "NULL" && chunk == "0")
            {
                response.content_type = "application/json";
                response.message = "{}";
                response.sessionID = sessionID;   //How to get session ID here??????
                string createResponseForPostRequest = response.createGetResponse(response);
                return createResponseForPostRequest;
            }
            vector<string> filesNames = splitStrings(folderHashesSet[3],",");
            if (find(filesNames.begin(), filesNames.end(), col) == filesNames.end() && chunk == "0") {
                response.content_type = "application/json";
                response.message = "{}";
                response.sessionID = sessionID;   //How to get session ID here??????
                string createResponseForPostRequest = response.createGetResponse(response);
                return createResponseForPostRequest;
            }
        }
        string file;
        auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
        string time,filetype,filecontent;

        kvsClient.get(file,row,col);
        cout << row << " " << col << " " << file << endl ;
        nlohmann::json j;
        if(file[0] != '-'){
            cout << " ";
            vector<string> failComp = splitStrings(file,"\r\n");
            j["fileName"] = fileName; 
            j["time"] = failComp[1];
            j["fileType"] = failComp[2];
            j["fileContent"] = failComp[3];     
        }   
        string jsonString = j.dump();
        cout << jsonString;
        transport->close();
        response.content_type = "application/json";
        response.message = jsonString;
        response.sessionID = sessionID;   //How to get session ID here??????
        string createResponseForPostRequest = response.createGetResponse(response);
        return createResponseForPostRequest;
    }
    else if (command == "/createFolder") {
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
        string user = splitStrings(sessionResp,",")[0];
        string username = getUsernameFromEmail(user);
        auto parsed = nlohmann::json::parse(body);
        string folderName = parsed["folderName"];
        cout << folderName  << endl;
        string row = username+"-storage";
        string timestamp = getCurrentTimestamp();
        string fileType = "folder";
        auto path = parsed["path"];

        string folder = folderName + "\r\n" + timestamp + "\r\n" + fileType + "\r\n" + "NULL";
        string col = sha256(folderName);

        auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
        kvsClient.put(row,col,folder);
        cout<<path.size()<<endl;
        if (path.size() == 0) {
            string folderHashes;
            kvsClient.get(folderHashes,row,"root");
            if (folderHashes.empty() || folderHashes[0] == '-') {
                cout<<"THIS RAN AGAIN"<<endl;
                folderHashes = col;
            } else {
                // remove the NULL from the end of the string
                cout<<"AND THIS RAN"<<endl;
                folderHashes = folderHashes + "," + col;
            }
            kvsClient.put(row,"root",folderHashes);
        } else {
            string folderHashes;
            string folderNameHash = sha256(path[path.size()-1]);
            kvsClient.get(folderHashes,row, folderNameHash);
            if (folderHashes.empty() || folderHashes[0] == '-') {
                folderHashes = col;
            } else {
                // remove the NULL from the end of the string if there is a NULL
                if (folderHashes.substr(folderHashes.size() - 4) == "NULL") {
                    folderHashes = folderHashes.substr(0, folderHashes.size() - 4);
                }
                folderHashes = folderHashes + "," + col;
            }
            kvsClient.put(row,folderNameHash,folderHashes);
        }
        transport->close();
        response.content_type = "application/json";
        response.message = "{}";
        response.sessionID = sessionID;
        string createResponseForPostRequest = response.createPostResponse(response);
        return createResponseForPostRequest;
        
    } else if (command == "/fetchFolders") {
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
        string user = splitStrings(sessionResp,",")[0];
        string username = getUsernameFromEmail(user);
        //parse the body
        auto parsed = nlohmann::json::parse(body);
        //the paresd content is in the form {"path":["folder1","folder2","folder3"]}
        // make a vector of all the folders in the path
        auto path = parsed["path"];
        string row = username+"-storage";
        auto [transport, kvsClient] =  getKVSClient(getWorkerIP(username+"-storage",client));
        if (path.size() == 0) {
            string folderHashes;
            kvsClient.get(folderHashes,row,"root");
            vector<string> folderHashSet = splitStrings(folderHashes,",");
            vector<string> folderNames;
            for(string hash : folderHashSet){
                string folder;
                kvsClient.get(folder,row,hash);
                if(folder[0] != '-'){
                    vector<string> failComp = splitStrings(folder,"\r\n");
                    folderNames.push_back(failComp[0]);
                }
            }
            string jsonString = nlohmann::json(folderNames).dump();
            transport->close();
            response.content_type = "application/json";
            response.message = jsonString;
            response.sessionID = sessionID;
            string createResponseForPostRequest = response.createGetResponse(response);
            return createResponseForPostRequest;
        } else {
            string folderHashes;
            string folderNameHash = sha256(path[path.size()-1]);
            kvsClient.get(folderHashes,row,folderNameHash);
            vector<string> folderHash = splitStrings(folderHashes,"\r\n");
            // last element has all the folder hashes
            vector<string> folderHashSet = splitStrings(folderHash[3],",");
            if (folderHashSet[0] == "NULL") {
                transport->close();
                response.content_type = "application/json";
                response.message = "{}";
                response.sessionID = sessionID;
                string createResponseForPostRequest = response.createGetResponse(response);
                return createResponseForPostRequest;
            }
            vector<string> folderNames;
            for(string hash : folderHashSet){
                string folder;
                kvsClient.get(folder,row,hash);
                if(folder[0] != '-'){
                    vector<string> failComp = splitStrings(folder,"\r\n");
                    if (failComp[2] == "folder") {
                        folderNames.push_back(failComp[0]);
                    }
                }
            }
            string jsonString = nlohmann::json(folderNames).dump();
            transport->close();
            response.content_type = "application/json";
            response.message = jsonString;
            response.sessionID = sessionID;
            string createResponseForPostRequest = response.createGetResponse(response);
            return createResponseForPostRequest;
        }
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>fetchFolders</h1>";
    } else if (command == "/getFiles") {
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
        } 
        cout << sessionResp << endl;
        string user = splitStrings(sessionResp,",")[0];
        string username = getUsernameFromEmail(user);

        string row = username+"-storage"; 
        string fileHashes;
        // parse body, it has foldername
        auto parsed = nlohmann::json::parse(body);
        auto folderName = parsed["folderName"];
        auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
        if (folderName == "root") {
            kvsClient.get(fileHashes,row,"AllFiles");
            transport->close();
            vector<string> allFiles;
            vector<nlohmann::json> sortedFiles;

            if(!fileHashes.empty() && fileHashes[0]!='-')
                allFiles = splitStrings(fileHashes,",");
            for (const auto& str : allFiles) {
                string file;
                auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
                kvsClient.get(file,row,str+"-NameOnly");
                cout<<"HERE IS THE FILE for root->" << file << endl;
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
            string createResponseForPostRequest = response.createPostResponse(response);
            return createResponseForPostRequest;
        } else {
            string folderHashes;
            kvsClient.get(folderHashes,row,sha256(folderName));
            vector<string > folderHashesSet = splitStrings(folderHashes,"\r\n");
            if(folderHashesSet[3] == "NULL")
            {
                transport->close();
                response.content_type = "application/json";
                response.message = "{}";
                response.sessionID = sessionID;   //How to get session ID here??????
                string createResponseForPostRequest = response.createPostResponse(response);
                return createResponseForPostRequest;
            }
            cout<<"Folder Hashes: "<<folderHashesSet[3]<<endl;
            vector<string> allFiles = splitStrings(folderHashesSet[3],",");
            // check if the fileName is a folder or a file
            vector<nlohmann::json> sortedFiles;
            
            for (const auto& str : allFiles) {
                cout<<str<<endl;
                string file;
                auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
                kvsClient.get(file,row,str+"-NameOnly");
                cout<<"HERE IS THE FILE->" << file << endl;
                if (file[0] == '-') {
                    continue;
                }
                transport->close();
                nlohmann::json j;
                j["hash"] = str; 
                j["name"] = file; 
                string jsonString = j.dump();
                sortedFiles.push_back(j);
            }
            nlohmann::json responseJson = nlohmann::json(sortedFiles);
            string jsonString = responseJson.dump();
            transport->close();
            response.content_type = "application/json";
            response.message = jsonString;
            response.sessionID = sessionID;   //How to get session ID here??????
            string createResponseForPostRequest = response.createPostResponse(response);
            return createResponseForPostRequest;
        }
    } else if (command == "/deleteFolder") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>deleteFolder</h1>";
    } else if (command == "/renameFile") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>renameFile</h1>";
    } else if (command == "/renameFolder") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>renameFolder</h1>";
    } else if (command == "/moveFile") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>moveFile</h1>";
    } else if (command == "/moveFolder") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>moveFolder</h1>";
    } else if (command == "/sendEmail") {
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
        string userFrom = splitStrings(sessionResp,",")[0];

        auto j = nlohmann::json::parse(body);

        string toEmail = j["email"];
        string subject = j["subject"];
        string body = unescape(j["body"]);
        map<string,string> attachments;
        string attchHashes="None";
        for (const auto& item : j["attachments"]) {
            string timestamp = getCurrentTimestamp();
            string fileName = item["filename"];
            string fileContent = item["content"];
            string file = fileName + "\r\n" + timestamp + "\r\n" + fileContent;
            string hash = sha256(item["filename"] );
            attachments.insert(std::make_pair(hash,file));
            if(attchHashes=="None")
                attchHashes = hash;
            else
                attchHashes = attchHashes + "," + hash;
        }

        string username = getUsernameFromEmail(toEmail);
        string domain = getDomainFromEmail(toEmail);
        if(domain!="penncloud"){
            string email =  "Subject: " + subject + "\n" +  body; 
            // int success = smtpClient(parsed,domain,email);
            return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>sendEmail</h1>";
        }
        else{
            string row = username + "-mailbox";
            string timestamp = getCurrentTimestamp();
            string email = userFrom + "\r\n" + timestamp + "\r\n" + subject + "\r\n" +  body; 
            string col = sha256(email);
            std::cout << email << endl;
            auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
            kvsClient.put(row,col,email);

            string emailHashes;
            kvsClient.get(emailHashes,row,"AllEmails");;

            if(emailHashes.empty() || emailHashes[0]=='-')
                emailHashes = col;
            else
                emailHashes = emailHashes + "," + col;

            kvsClient.put(row,"AllEmails",emailHashes);  

            // kvsClient.put(row,col+"-Attachments",attchHashes);
             std::for_each(attachments.begin(), attachments.end(), [&kvsClient, &row, &col](const std::pair<std::string, std::string>& attachment) {
                kvsClient.put(row, col + "-" + attachment.first, attachment.second);
            });

            transport->close();
         
             std::cout<<"Details: "<<toEmail<<" "<<subject<<" "<<body<<endl;

           response.content_type = "application/json";
           response.message = "";
           response.sessionID = sessionID;
           string createResponseForPostRequest = response.createPostResponse(response);
           return createResponseForPostRequest;
        }
    }
    else if (command == "/deleteEmail") {

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
        string user = splitStrings(sessionResp,",")[0];


        auto parsed = parseJsonLikeString(body);
        string emailHash = parsed["emailHash"];

        string username = getUsernameFromEmail(user); 
        string row = username + "-mailbox";
        string emailHashes;
        
        auto [transport, kvsClient] =  getKVSClient(getWorkerIP(row,client));
        kvsClient.get(emailHashes,row,"AllEmails");
        vector<string> emailHashSet = splitStrings(emailHashes,",");
        vector<string> newEmailHashSet;
        for(string hash : emailHashSet){
            if(hash!=emailHash)
                newEmailHashSet.push_back(hash);
        }
        string newEmailHashes = vectorToString(newEmailHashSet,",");
        if(newEmailHashes.empty())
        {
            kvsClient.deleteCell(row,"AllEmails");
        }
        else{
            kvsClient.put(row,"AllEmails",newEmailHashes);
        }
        kvsClient.deleteCell(row,emailHash); 

        string attachHash;
        kvsClient.get(attachHash,row,emailHash+"-Attachments");
        if(!attachHash.empty() && attachHash[0]!='-')
        { 
            vector<string> attachments = splitStrings(attachHash,",");
            for (const auto& att : attachments){
                kvsClient.deleteCell(row,emailHash+"-"+att);
            }
            kvsClient.deleteCell(row,emailHash+"-Attachments");
        }


        transport->close();
        response.content_type = "application/json";
        response.message = "";
        response.sessionID = sessionID;
        string createResponseForPostRequest = response.createPostResponse(response);
        return createResponseForPostRequest;
    }
    else {
        return "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n<h1>404 Not Found</h1>";
    }

}