service KvsCoordOps {
    //GET: retrieve the IP:PORT associated with a rowkey
    string get(1: string row),

    //keepAlive: receive keep alive from worker node periodically 
    string keepAlive(1: string ip),

    //syncComplete: once sync is complete from kvs client, tell the coordinator
    string syncComplete(1: string ip)


}