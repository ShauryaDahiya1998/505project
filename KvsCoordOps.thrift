service KvsCoordOps {
    //GET: retrieve the IP:PORT associated with a rowkey
    string get(1: string row)
}