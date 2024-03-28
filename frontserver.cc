#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
using namespace std;

#define PORT 8080
#define BUFFER_SIZE 1024

// Thread function to handle client
void* handleClient(void* arg) {
    int clientSocket = *((int*)arg);
    free(arg); // Free the heap memory allocated for the client socket

    char buffer[BUFFER_SIZE] = {0};
    read(clientSocket, buffer, BUFFER_SIZE);

    // Print the request to the console
    std::cout << "Received request:\n" << buffer << std::endl;

    // Send a simple HTTP response
    std::string httpResponse = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello from the C++ server using pthread!";
    write(clientSocket, httpResponse.c_str(), httpResponse.length());

    close(clientSocket);
    pthread_exit(NULL);
}


int main() {
    int serverFd, *newSocket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    pthread_t threadId;

    // Creating socket file descriptor
    if ((serverFd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the port 8080
    if (bind(serverFd, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(serverFd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (true) {
        cout<<"HERE MAKING"<<endl;
        newSocket = (int*)malloc(sizeof(int));
        if ((*newSocket = accept(serverFd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Create a new thread for each connection
        if(pthread_create(&threadId, NULL, handleClient, (void*)newSocket) != 0) {
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }

        // Detach the thread so that resources are freed once it finishes
        pthread_detach(threadId);
    }

    return 0;
}
