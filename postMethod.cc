#include "customlib.h"
#include <string>
#include <iostream>
#include <random>
#include "gen-cpp/StorageOps.h"
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

bool checkValidUser(string username, string password, StorageOpsClient client) {
    // check if the email and password are correct
    string response;
    client.get(response, username, "password");
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

string createSession(string username, StorageOpsClient client) {
    // create a session and return the session id
    random_device rd;
    mt19937 gen(rd());
    // keep creating random session id until we get a unique one
    uniform_int_distribution<> dis(1000000, 9999999);
    string sessionID = to_string(dis(gen));
    while(1) {
      string resp;
      client.get(resp, sessionID, "1");
      if (resp.find("-ERR") != string::npos) {
        break;
      }
      sessionID = to_string(dis(gen));
    }
    return to_string(dis(gen));
}

string postMethodhandler(string command, string body, StorageOpsClient client) {
    // find the first /
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
          setSession(username, username, username, "6000", sessionID, client);
          
          return createResponseForPostRequest;                                                    
        } else {
            string message = "{\"message\": \"Invalid Login Credentials\"}";
            response.content_type = "application/json";
            response.message = message;
            response.sessionID = "1111";   //what should we pass for default session ID??????
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
            client.put(username, "password", password);
            string message = "{\"message\": \"Account Created\"}";
            response.content_type = "application/json";
            response.message = message;
            response.sessionID = "1111";   //what should we pass for default session ID??????
            string createResponseForPostRequest = response.createPostResponse(response);
            return createResponseForPostRequest;   
            
        } else {
           string message = "{\"message\": \"Account already exists\"}";
            response.content_type = "application/json";
            response.message = message;
            response.sessionID = "1111";   //what should we pass for default session ID??????
            string createResponseForPostRequest = response.createPostResponse(response);
            return createResponseForPostRequest;   
        }

        // return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>About Us</h1>";
    } else if (command == "/login") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>Contact Us</h1>";
    } else if (command == "/deleteFile") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>deleteFile</h1>";
    } else if (command == "/createFolder") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>createFolder</h1>";
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
    }
    else {
        return "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n<h1>404 Not Found</h1>";
    }

}