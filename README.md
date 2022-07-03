# Simple-Network-Database
Made in C


This program was built in Linux using sockets and client-server architecture to emulate a remote network database. This program allows clients to connect to a server across any network just by giving a DNS and port number. Once connected, clients can then store, access, and delete records (which is a 512-byte string that consists of a name and 10-digit ID) within this server's database. The database is stored as a local file on the server's machine that is created when the server is first to run on the machine. Multiple clients can connect to the server at once and perform actions synchronously due to the implementation of multithreading and locks.  

First, the server must be set up on a machine that is connected to a stable network. The user must then input a valid port number to bind the server to where then the server will create a file to store the database and then wait for a client to connect. Once the server has been established, A client must provide the DNS/IP address and the Port number that the server is bound to. Once connected the server will display the information of the connected client and create a new thread to wait for the client to send a command while the main thread will continue to listen for any other clients wanting to establish a connection. The client can then input a number corresponding to action to prepare the server for said action. Then the client will input data for the server to read and perform that action. Once the action is completed, the server will send any relevant data and a success message back to the client and then await the client's next command. The client can disconnect from the server by inputting 0 for the action which will properly close the connection on the server and client-side of the program.

List of Action Commands:

1 - Add A new record to the database
  When the client inputs 1 for the action, the server will prepare itself to read a 512-byte record that the client will be inputted to send. Once received, the server will parse the database file for the first empty index, lock that index to prevent any other threads from interfering, then store the record, if no empty indexes are found, the server will simply append the record to the end of the file. Once completed, the server will send a success message to the client to indicate the operation has finished.
  
2- Get a record from the database
  When the client inputs 2, the server will then prepare itself to read an 11-byte string representing an ID the client will input. This ID will be used as a key for the server to search the database with and if found, will return the 512-byte record associated with that ID to the client along with a success message. If the ID is not found, the server will send an error message to the client indicating that the ID is not in the database.
  
3- Delete a record from the database
    When the client inputs 3, the server will then prepare itself to read an 11-byte string representing an ID the client will input. The server will use this ID as a key to finding a record within the database. Once found, the server will return the 512-byte record to the client along with a success message. Then, the server will lock the index to prevent other threads from interfering and overwrite the record in the database with the placeholder value 0 in order to indicate that index as being empty. If the ID is not found in any record within the file, the server will return an error message indicating that the ID is now in the database.
  
