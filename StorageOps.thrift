namespace cpp storage

struct Cell {
    1: string row,
    2: string col,
    3: string value
}

service StorageOps {
    // PUT: Insert or update a value in a cell
    bool put(1: string row, 2: string col, 3: string value),

    bool replicateData(1: string row, 2: string col, 3: string value),

    // GET: Retrieve the value of a cell
    string get(1: string row, 2: string col),

    // Delete: Remove the value of a cell
    bool deleteCell(1: string row, 2: string col),

    bool deleteReplicate(1: string row, 2: string col),

    // CPUT: Compare and update a value in a cell
    bool cput(1: string row, 2: string col, 3: string old_value, 4: string new_value),

    //SYNC: kvs client sends across its logfile and tablet file when it receives this from another kvs client
    string sync(1: string which)
}