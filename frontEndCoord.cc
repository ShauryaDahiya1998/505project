#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <mutex>
#include <ctime>
#include <algorithm>
#include <pthread.h>
#include "gen-cpp/FrontEndCoordOps.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <nlohmann/json.hpp>

using namespace ::FrontEndCoordOps;
using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

struct ServerInfo {
    std::string ip;
    int port;
    int connections = 0;
    bool isActive = true;
};

std::mutex mtx;
std::vector<ServerInfo> servers;

class FrontEndCoordOpsHandler : virtual public FrontEndCoordOpsIf {
public:
    void notifyConnectionClosed(const std::string& serverIP, const std::string& port) override {
        int portNum = std::stoi(port);  // Convert port string to integer
        std::lock_guard<std::mutex> lock(mtx);  // Lock for thread safety

        // Find and update the server info
        for (auto& server : servers) {
            if (server.ip == serverIP && server.port == portNum) {
                if (server.connections > 0) {  // Ensure the count does not go negative
                    server.connections--;
                }
                std::cout << "Connection closed notified for server: " << serverIP << ":" << port
                          << ". Updated connections: " << server.connections << std::endl;
                break;  // Break the loop once the matching server is found and updated
            }
        }
    }

     void markServerInactive(const std::string& serverIP, const std::string& port) {
        int portNum = std::stoi(port);  // Convert port string to integer
        std::lock_guard<std::mutex> lock(mtx);  // Lock for thread safety

        // Find and update the server info
        for (auto& server : servers) {
            if (server.ip == serverIP && server.port == portNum) {
                server.isActive = false;
            }
        }
    }
};

void loadServerConfig(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string ip;
        int port;
        if (std::getline(iss, ip, ':') && iss >> port) {
            servers.push_back({ip, port, 0,true});
        }
    }
}

ServerInfo& selectServer(std::vector<ServerInfo>& servers) {
    std::lock_guard<std::mutex> lock(mtx);  // Lock for thread safety
    auto it = std::min_element(servers.begin(), servers.end(), [](const ServerInfo& a, const ServerInfo& b) {
        // Only consider active servers; inactive servers are treated as if they have the highest possible connections
        int aConnections = a.isActive ? a.connections : std::numeric_limits<int>::max();
        int bConnections = b.isActive ? b.connections : std::numeric_limits<int>::max();
        return aConnections < bConnections;
    });
    if (it != servers.end() && it->isActive) {
        return *it;
    } else {
        throw std::runtime_error("No active servers available");
    }
}

void markInactive(const std::string& serverIP, int portNum, bool isActive) {
        std::lock_guard<std::mutex> lock(mtx);  // Lock for thread safety
        for (auto& server : servers) {
            if (server.ip == serverIP && server.port == portNum) {
                server.isActive = isActive;
                break;
            }
        }
    }

void *runThriftServer(void *arg) {
    int port = *(int*)arg;
    ::std::shared_ptr<FrontEndCoordOpsHandler> handler(new FrontEndCoordOpsHandler());
    ::std::shared_ptr<TProcessor> processor(new FrontEndCoordOpsProcessor(handler));
    ::std::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
    ::std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    ::std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

    TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory);
    std::cout << "Starting the Thrift server on port " << port << "..." << std::endl;
    server.serve();
    return NULL;
}

std::string getServerNodesData() {
    nlohmann::json jArray = nlohmann::json::array();

    for (const auto& server : servers) {
        nlohmann::json jObj = {
            {"ip", server.ip},
            {"port", server.port},
            {"status", server.isActive},
            {"connections", server.connections}
        };
        jArray.push_back(jObj);
    }
    return jArray.dump();  
}

int main() {
    loadServerConfig("config.txt");
    if (servers.empty()) {
        std::cerr << "Error: No server configurations loaded." << std::endl;
        return 1;
    }

    pthread_t thread_id;
    int thriftPort = 9090;
    pthread_create(&thread_id, NULL, runThriftServer, &thriftPort);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[30000] = {0};

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "Socket server is listening on port 8080..." << std::endl;

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        char buffer[30000] = {0}; // Buffer to hold the HTTP request
        int bytes_read = read(new_socket, buffer, sizeof(buffer) - 1); // Read the incoming HTTP request
        if (bytes_read < 0) {
            perror("read");
            close(new_socket);
            continue;
        }

        std::string request(buffer);
        std::string httpResponse;

        // Check if the request is for the root "/" or for "/admin"
        if (request.find("GET / ") != std::string::npos) { // Root path
            try {ServerInfo& selectedServer = selectServer(servers);
            {
                std::lock_guard<std::mutex> lock(mtx);
                selectedServer.connections++;  // Safely increment the connection count
            }

            // Construct redirect response
            httpResponse = "HTTP/1.1 303 See Other\r\n";
            httpResponse += "Location: http://" + selectedServer.ip + ":" + std::to_string(selectedServer.port) + "/\r\n";
            httpResponse += "Content-Length: 0\r\n\r\n"; // End of headers
            }catch (const std::runtime_error& e) {
                httpResponse = "HTTP/1.1 503 Service Unavailable\r\n";
                httpResponse += "Content-Type: application/json\r\n";
                httpResponse += "Content-Length: ..."; // Length of the content
                httpResponse += "\r\n";
                httpResponse += "{\"error\":\"No servers available, please wait.\"}";
            }
        } else if (request.find("GET /admin/nodes") != std::string::npos) { 

            std::string nodesData = getServerNodesData();
            httpResponse = "HTTP/1.1 200 OK\r\n";
            httpResponse += "Content-Type: application/json\r\n";
            httpResponse += "Content-Length: " + std::to_string(nodesData.length()) + "\r\n";
            httpResponse += "\r\n";
            httpResponse += nodesData;
        }else if (request.find("POST /admin/inactive") != std::string::npos) {
            auto header_end = request.find("\r\n\r\n");
            std::string body = request.substr(header_end + 4);
            auto parsed = nlohmann::json::parse(body);
            std::string ip = parsed["ip"];
            int port = parsed["port"];
            bool isActive = parsed["active"];
            markInactive(ip,port,isActive);
            std::string nodesData = getServerNodesData();
            httpResponse = "HTTP/1.1 200 OK\r\n";
            httpResponse += "Content-Type: application/json\r\n";
            httpResponse += "\r\n";
        }
        else if (request.find("GET /admin") != std::string::npos) { // Admin path
            // Load the admin.html page
            std::ifstream file("./pages/admin.html");
            if (file) {
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

                httpResponse = "HTTP/1.1 200 OK\r\n";
                httpResponse += "Content-Type: text/html\r\n";
                httpResponse += "Content-Length: " + std::to_string(content.length()) + "\r\n";
                httpResponse += "\r\n";
                httpResponse += content;
            } else {
                httpResponse = "HTTP/1.1 404 Not Found\r\n";
                httpResponse += "Content-Length: 0\r\n\r\n";
            }
        }

        send(new_socket, httpResponse.c_str(), httpResponse.size(), 0);
        close(new_socket); // Close the socket after sending response
    }   

    pthread_join(thread_id, NULL);
    return 0;
}
