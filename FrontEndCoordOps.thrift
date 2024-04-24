namespace cpp FrontEndCoordOps

struct ServerDetails {
    1: string serverIP;
    2: string port;
}

service FrontEndCoordOps {
    void notifyConnectionClosed(1: string serverIP, 2: string port);
    void markServerInactive(1: string serverIP, 2: string port);
}
