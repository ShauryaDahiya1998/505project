#include "customlib.h"
#include <string>
#include <iostream>

using namespace std;

bool checkValidUser(string email, string password) {
    // check if the email and password are correct
    return true;
}

bool checkNewUser(string email) {
    // check if the email is already registered
    return true;
}

string createSession(string email) {
    // create a session and return the session id
    return "5555";
}

string postMethodhandler(string command, string body) {
    // find the first /
    size_t firstSlash = command.find("/");
    // find the first space after firstSlash index and keep the result in a variable
    command = command.substr(firstSlash);
    command = command.substr(0, command.find(" "));

    // handle the commandType and return the appropriate response
    if (command == "/login") {
        // here we get a email and password from the body in the format {email:"email", password:"password"}
        // parse this and get the email and passowrd

        size_t emailStart = body.find("email\":") + 8;
        string email = body.substr(emailStart, body.find("\",") - emailStart);
        size_t passwordStart = body.find("password\":") + 11;
        string password = body.substr(passwordStart, body.find("\"}") - passwordStart);

        // check if the email and password are correct
        if (checkValidUser(email, password)) {
          // if correct, create a session and return the session id
          string sessionID = createSession(email);
          string message = "{\"message\": \"Session Created\", \"sessionID\": \"" + sessionID + "\"}";
          string createResponseForPostRequest = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: " + std::to_string(message.size()) + "\r\n"
                      "Set-Cookie: sessionID=" + sessionID + "\r\n\r\n" +
                      "{\"message\": \"Session Created\", \"sessionID\": \"" + sessionID + "\"}";
            return createResponseForPostRequest;                                                    
        } else {
            return "HTTP/1.1 401 Unauthorized\nContent-Type: text/html\n\n<h1>Unauthorized</h1>";
        }
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>POST Method</h1>";
    } else if (command == "/signup") {
        // here we get a username, email and password from the body in the format {"username":"hi","email":"hi","password":"hi"}
        // parse this and get the username, email and passowrd

        size_t usernameStart = body.find("username\":") + 11;
        string username = body.substr(usernameStart, body.find("\",") - usernameStart);
        size_t emailStart = body.find("email\":") + 8;
        string email = body.substr(emailStart, body.find("\",") - emailStart);
        size_t passwordStart = body.find("password\":") + 11;
        string password = body.substr(passwordStart, body.find("\"}") - passwordStart);

        // check if the email is already registered
        if (checkNewUser(email)) {
            // if not registered, create a new user and return the success message
            string message = "{\"message\": \"Account Created\"}";
            string createResponseForPostRequest = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n" + message;
            return createResponseForPostRequest;    
            
        } else {
           string message = "{\"message\": \"Account already exists\"}";
            string createResponseForPostRequest = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n" + message;
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