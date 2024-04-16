#include "customlib.h"
#include <stdlib.h>
#include <stdio.h>
#include <openssl/md5.h>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <signal.h>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <fstream>
#include <unordered_map>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <regex>
#include <iomanip> 


#include <string.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include <sys/stat.h>

bool debugMode = true;
#define MAX_DOMAIN_LENGTH 256


void sendCommand(int sockfd,  const std::string& command) {
    char buffer[1024] = {0};
    strcpy(buffer, command.c_str());
    strcat(buffer, "\r\n");
    send(sockfd, buffer, strlen(buffer), 0);
    if(debugMode)
        std::cout << "[C] " << command << std::endl;
}

// Connect to the IP resolved from domain and port 25
int connect_to_mail_server(const char *mail_server_ip, int port) {
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        return -1;
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, mail_server_ip, &servaddr.sin_addr); // Use the resolved IP address

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
        perror("Connection to mail server failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}


// Function to send an email
int send_email(const char *mail_server_ip, int port,  map<string,string> json,std::string body) {
    int sockfd = connect_to_mail_server(mail_server_ip, port);
    if (sockfd == -1) {
        return -1;
    }

    char buffer[1024] = {0};
    recv(sockfd, buffer, sizeof(buffer), 0); 
    if(debugMode)
        std::cout << "[S] " << buffer << std::endl;

    // Check for error cases
    if (buffer[0] == '5') {
        std::cerr << "Error response received from server" << std::endl;
        return -1;
    }

    // Send HELO command
    sendCommand(sockfd, "HELO seas.upenn.edu");
    recv(sockfd, buffer, sizeof(buffer), 0);
    if(debugMode)
        std::cout << "[S] " << buffer << std::endl;
        // Check for error cases
    if (buffer[0] == '5') {
        std::cerr << "Error response received from server" <<std::endl;
        return -1;
    }

    // Send MAIL FROM command

    sendCommand(sockfd, "MAIL FROM:<sukritiy@seas.upenn.edu>");
    recv(sockfd, buffer, sizeof(buffer), 0);
    if(debugMode)
        std::cout << "[S] " << buffer << std::endl;
        // Check for error cases
    if (buffer[0] == '5') {
        std::cerr << "Error response received from server" <<std::endl;
        return -1;
    }

    // Send RCPT TO command
    string rcptEmail = json["email"];
    std::string rcpt = "RCPT TO:<" + rcptEmail + ">";
    sendCommand(sockfd, rcpt);
    recv(sockfd, buffer, sizeof(buffer), 0);
    if(debugMode)
        std::cout << "[S] " << buffer << std::endl;
        // Check for error cases
    if (buffer[0] == '5') {
        std::cerr << "Error response received from server" << std::endl;
        return -1;
    }

    // Send DATA command
    sendCommand(sockfd, "DATA");
    recv(sockfd, buffer, sizeof(buffer), 0);
    if(debugMode)
        std::cout << "[S] " << buffer << std::endl;
        // Check for error cases
    if (buffer[0] == '5') {
        std::cerr << "Error response received from server" << std::endl;
        return -1;
    }

    // Sending the message body
    sendCommand(sockfd,body);
    // recv(sockfd, buffer, sizeof(buffer), 0);
    // std::cout << "Received: " << buffer << std::endl;

    // End the email
    sendCommand(sockfd, ".");
    recv(sockfd, buffer, sizeof(buffer), 0);
    if(debugMode)
        std::cout << "[S] " << buffer << std::endl;
    if (buffer[0] == '5') {
        std::cerr << "Error response received from server" << std::endl;
        return -1;
    }


    // Send QUIT command
    sendCommand(sockfd, "QUIT");
    recv(sockfd, buffer, sizeof(buffer), 0);
    
    if(debugMode)
        std::cout << "[S] " << buffer << std::endl;
    // Check for error cases
    if (buffer[0] == '5') {
        std::cerr << "Error response received from server" << std::endl;
        return -1;
    }


    close(sockfd);

    return 0;
}

int lookup_mx_records(const std::string &domain, std::string &mail_server_ip) {
    u_char answer[NS_MAXMSG];
	res_init();
    // res query on the domain name eg seas.upenn.edu
	int answer_length = res_query(domain.c_str(), ns_c_in, ns_t_mx, answer, sizeof(answer));
    
    if (answer_length < 0) {
        std::cerr << "Error MX Record "<< strerror(errno) << " (" << errno << ")"<<std::endl;
        return -1;
    }

    ns_msg msg;
    int ret = ns_initparse(answer, answer_length, &msg);
    if (ret < 0) {
        std::cerr << "Error ns_initparse "<< strerror(errno) << " (" << errno << ")"<<std::endl;
        return -1;
    }

    int answer_count = ns_msg_count(msg, ns_s_an);
    
    for (int i = 0; i < answer_count; i++) {
        ns_rr rr;
        ret = ns_parserr(&msg, ns_s_an, i, &rr);
        if (ret < 0) {
            std::cerr << "Error Parsing Record "<< strerror(errno) << " (" << errno << ")"<<std::endl;
            return -1;
        }

        // std::cout << "Type : " << ns_rr_type(rr) << ns_t_mx << std::endl;
        if (ns_rr_type(rr) == ns_t_mx) {
            char mx_domain[MAX_DOMAIN_LENGTH];
            ns_name_uncompress(ns_msg_base(msg), ns_msg_end(msg), ns_rr_rdata(rr) + 2,
                                   mx_domain, sizeof(mx_domain));
            // std::cout << mx_domain << std::endl;
            // Getting the mx host records
            struct hostent *mx_host = gethostbyname(mx_domain);
             if (mx_host == NULL) {
                std::cerr << "Error Parsing Record "<< strerror(errno) << " (" << errno << ")"<<std::endl;
                return -1;
            }
            // Converting the IP address and storing it
            mail_server_ip = inet_ntoa(*(struct in_addr *)mx_host->h_addr_list[0]);
            if(debugMode)
                std::cout <<"MAIL SERVER " << mail_server_ip << " " << mx_host->h_name << std::endl;
            return 0;
        }
        
    }
    if(debugMode)
        std::cout << "No MX records found" << std::endl;
    return -1;
}
int smtpClient(map<string, string> json,std::string domain, std::string body)
{

    std::string mail_server_ip;
    if (lookup_mx_records(domain, mail_server_ip) == -1) {
        std::cerr << "Error looking up MX records for destination domain" << std::endl;
        return -1;
    }
    else if (send_email(mail_server_ip.c_str(), 25, json,body) == -1) {
        std::cerr << "Error sending email" << std::endl;
        return -1;
    }
    return 0;
}

