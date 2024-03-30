#include "customlib.h"
#include <string>
#include <iostream>

using namespace std;

bool checkValidUser(string email, string password) {
    // check if the email and password are correct
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
            return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>SessionID: " + sessionID + "</h1>";
        } else {
            return "HTTP/1.1 401 Unauthorized\nContent-Type: text/html\n\n<h1>Unauthorized</h1>";
        }
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>POST Method</h1>";
    } else if (command == "/signup") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>About Us</h1>";
    } else if (command == "/login") {
        return "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>Contact Us</h1>";
    } else {
        return "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n<h1>404 Not Found</h1>";
    }

}