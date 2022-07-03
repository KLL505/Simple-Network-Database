#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include "msg.h"
#include "common.h"
#include "common_threads.h"
#define BUF 512

void Usage(char *progname);
void PrintOut(int fd, struct sockaddr *addr, size_t addrlen);
void PrintReverseDNS(struct sockaddr *addr, size_t addrlen);
void PrintServerSide(int client_fd, int sock_family);
int  Listen(char *portnum, int *sock_family);
void* HandleClient(void* args);
void WriteToClient(int fd, char* msg);
int ReadFromClient(int fd, struct msg* readbuf);
int Put(struct record* sr,int32_t fd);
int Get(struct record *rd, int32_t fd);
int Delete(struct record *rd, int32_t fd);

//structure to hold all the thread parameters
struct Parameters {
int fd;
struct sockaddr* addr;
size_t addrlen;
int family; 
};

//template record used to overwrite deleted records
struct record deleted;
int32_t main_fd;

int 
main(int argc, char **argv) {

  // Expect the port number as a command line argument.
  if (argc != 2) {
    Usage(argv[0]);
  }

  int sock_family;
  int listen_fd = Listen(argv[1], &sock_family);
  if (listen_fd <= 0) {
    // We failed to bind/listen to a socket.  Quit with failure.
    printf("Couldn't bind to any addresses.\n");
    return EXIT_FAILURE;
  }

  //opens the databse file or creates one if it doesnt exist
  main_fd = open("db.txt", O_CREAT | O_RDWR, S_IRWXU);
  if (main_fd == -1){
     perror("open failed");   
     exit(EXIT_FAILURE);
  } 

//pads delted with spaces and make the id 0 to properly delete records by overwriting them
  for(int i = 0; i < MAX_NAME_LENGTH; i++){
  	deleted.name[i] = ' ';
  }
  for(int i =0; i < 10; i++){
  	deleted.pad[i] = ' ';
  }
  deleted.id = 0;

  // Loop forever, accepting a connection from a client and doing
  // an echo trick to it.
  while (1) {
    struct sockaddr_storage caddr;
    socklen_t caddr_len = sizeof(caddr);
    int client_fd = accept(listen_fd,
                           (struct sockaddr *)(&caddr),
                           &caddr_len);
    if (client_fd < 0) {
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
        continue;
      printf("Failure on accept:%s \n ", strerror(errno));
      break;
    }

    //sets up thread prameters
    struct Parameters param;
    param.fd = client_fd;
    param.addr = (struct sockaddr*) (&caddr);
    param.addrlen = caddr_len;
    param.family = sock_family;

    //creates thread to handle client
    pthread_t handler_thread;
    Pthread_create(&handler_thread, NULL, HandleClient, &param);
  }

  // Close socket
  close(listen_fd);
  return EXIT_SUCCESS;
}

//prints out the usage of the program
void Usage(char *progname) {
  printf("usage: %s port \n", progname);
  exit(EXIT_FAILURE);
}

//prints out information of connected client
void 
PrintOut(int fd, struct sockaddr *addr, size_t addrlen) {
  printf("Socket [%d] is bound to: \n", fd);
  if (addr->sa_family == AF_INET) {
    // Print out the IPV4 address and port

    char astring[INET_ADDRSTRLEN];
    struct sockaddr_in *in4 = (struct sockaddr_in *)(addr);
    inet_ntop(AF_INET, &(in4->sin_addr), astring, INET_ADDRSTRLEN);
    printf(" IPv4 address %s", astring);
    printf(" and port %d\n", ntohs(in4->sin_port));

  } else if (addr->sa_family == AF_INET6) {
    // Print out the IPV6 address and port

    char astring[INET6_ADDRSTRLEN];
    struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)(addr);
    inet_ntop(AF_INET6, &(in6->sin6_addr), astring, INET6_ADDRSTRLEN);
    printf("IPv6 address %s", astring);
    printf(" and port %d\n", ntohs(in6->sin6_port));

  } else {
    printf(" ???? address and port ???? \n");
  }
}

//prints out the Reverse DNS of server
void 
PrintReverseDNS(struct sockaddr *addr, size_t addrlen) {
  char hostname[1024];  // ought to be big enough.
  if (getnameinfo(addr, addrlen, hostname, 1024, NULL, 0, 0) != 0) {
    sprintf(hostname, "[reverse DNS failed]");
  }
  printf("DNS name: %s \n", hostname);
}

//Prints out server side interface
void 
PrintServerSide(int client_fd, int sock_family) {
  char hname[1024];
  hname[0] = '\0';

  printf("Server side interface is ");
  if (sock_family == AF_INET) {
    // The server is using an IPv4 address.
    struct sockaddr_in srvr;
    socklen_t srvrlen = sizeof(srvr);
    char addrbuf[INET_ADDRSTRLEN];
    getsockname(client_fd, (struct sockaddr *) &srvr, &srvrlen);
    inet_ntop(AF_INET, &srvr.sin_addr, addrbuf, INET_ADDRSTRLEN);
    printf("%s", addrbuf);
    // Get the server's dns name, or return it's IP address as
    // a substitute if the dns lookup fails.
    getnameinfo((const struct sockaddr *) &srvr,
                srvrlen, hname, 1024, NULL, 0, 0);
    printf(" [%s]\n", hname);
  } else {
    // The server is using an IPv6 address.
    struct sockaddr_in6 srvr;
    socklen_t srvrlen = sizeof(srvr);
    char addrbuf[INET6_ADDRSTRLEN];
    getsockname(client_fd, (struct sockaddr *) &srvr, &srvrlen);
    inet_ntop(AF_INET6, &srvr.sin6_addr, addrbuf, INET6_ADDRSTRLEN);
    printf("%s", addrbuf);
    // Get the server's dns name, or return it's IP address as
    // a substitute if the dns lookup fails.
    getnameinfo((const struct sockaddr *) &srvr,
                srvrlen, hname, 1024, NULL, 0, 0);
    printf(" [%s]\n", hname);
  }
}

//Listens for client connections
int 
Listen(char *portnum, int *sock_family) {

  // Populate the "hints" addrinfo structure for getaddrinfo().
  // ("man addrinfo")
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;       // IPv6 (also handles IPv4 clients)
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "in6addr_any" address
  hints.ai_flags |= AI_V4MAPPED;    // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  // Use argv[1] as the string representation of our portnumber to
  // pass in to getaddrinfo().  getaddrinfo() returns a list of
  // address structures via the output parameter "result".
  struct addrinfo *result;
  int res = getaddrinfo(NULL, portnum, &hints, &result);

  // Did addrinfo() fail?
  if (res != 0) {
	printf( "getaddrinfo failed: %s", gai_strerror(res));
    return -1;
  }

  // Loop through the returned address structures until we are able
  // to create a socket and bind to one.  The address structures are
  // linked in a list through the "ai_next" field of result.
  int listen_fd = -1;
  struct addrinfo *rp;
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    listen_fd = socket(rp->ai_family,
                       rp->ai_socktype,
                       rp->ai_protocol);
    if (listen_fd == -1) {
      // Creating this socket failed.  So, loop to the next returned
      // result and try again.
      printf("socket() failed:%s \n ", strerror(errno));
      listen_fd = -1;
      continue;
    }

    // Configure the socket; we're setting a socket "option."  In
    // particular, we set "SO_REUSEADDR", which tells the TCP stack
    // so make the port we bind to available again as soon as we
    // exit, rather than waiting for a few tens of seconds to recycle it.
    int optval = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof(optval));

    // Try binding the socket to the address and port number returned
    // by getaddrinfo().
    if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      // Bind worked!  Print out the information about what
      // we bound to.
      PrintOut(listen_fd, rp->ai_addr, rp->ai_addrlen);

      // Return to the caller the address family.
      *sock_family = rp->ai_family;
      break;
    }

    // The bind failed.  Close the socket, then loop back around and
    // try the next address/port returned by getaddrinfo().
    close(listen_fd);
    listen_fd = -1;
  }

  // Free the structure returned by getaddrinfo().
  freeaddrinfo(result);

  // If we failed to bind, return failure.
  if (listen_fd == -1)
    return listen_fd;

  // Success. Tell the OS that we want this to be a listening socket.
  if (listen(listen_fd, SOMAXCONN) != 0) {
    printf("Failed to mark socket as listening:%s \n ", strerror(errno));
    close(listen_fd);
    return -1;
  }

  // Return to the client the listening file descriptor.
  return listen_fd;
}

//Handles the client
void* 
HandleClient(void* args) {

  struct Parameters param =*(struct Parameters*) args;
  struct record rd;

  // Print out information about the client.
  printf("\nNew client connection \n" );
  PrintOut(param.fd, param.addr, param.addrlen);
  PrintReverseDNS(param.addr, param.addrlen);
  PrintServerSide(param.fd, param.family);

  //Opens the database file
  int32_t thread_fd = open("db.txt", O_CREAT | O_RDWR, S_IRWXU);
  if (thread_fd == -1){
     perror("open failed");   
     exit(EXIT_FAILURE);
  }  

  //while loop to process the clients commands
  while (1) {
    struct msg c_msg;

    //reads message from client 
    if(ReadFromClient(param.fd,&c_msg) != 0) 
    	break;

    //if client sends fail close connection
    if(c_msg.type == 5)
    	break;

    //if client chose put
    if(c_msg.type == 1){
      rd = c_msg.rd;

      //prints person name and id to server
      printf("Name: %s\n",rd.name);
      printf("ID: %d\n",rd.id);

      //adds a newline to end of name for easier organization in file
      strncat(rd.name, "\n", 1);

      //calls put function and writes its status to client
      if(Put(&rd,thread_fd)==0)
        WriteToClient(param.fd,"Put Success");
      else
        WriteToClient(param.fd,"Put Failed");    
     }

    //if client chose get
    if(c_msg.type == 2){
	    rd = c_msg.rd;

      //prints id to server
      printf("ID: %d\n",rd.id);

      //calls get method and returns its status to client
      if(Get(&rd,thread_fd) == 0){

        //writes formatted output to client of matching record
        char message[sizeof(rd)];
        sprintf(message,"Name: %s\nID: %d\nGet Success",rd.name, rd.id);
            WriteToClient(param.fd,message);
      }
      else
            WriteToClient(param.fd,"Get Failed");
    }

    //if client chose delete
    if(c_msg.type == 3){
      rd = c_msg.rd;

      //prints id to server
      printf("ID: %d\n",rd.id);

      //calls delete method and returns its status to client
      if(Delete(&rd,thread_fd) == 0){

        //writes formatted output to client of matching record
        char message[sizeof(rd)];
        sprintf(message,"Name: %s\nID: %d\nDelete Success",rd.name, rd.id);
        WriteToClient(param.fd,message);
      }
      else
            WriteToClient(param.fd,"Delete Failed");
    }
  }

  //closes file descriptors and sockets and ends connection with client
  close(thread_fd);
  close(param.fd);
  printf("Connection closed with client:\n");
  pthread_exit(NULL);

}

//function to read from client
int
ReadFromClient(int fd, struct msg* readbuf){
  int res;
  size_t length = sizeof(*readbuf);

  while (1) {
    res = read(fd, (struct msg*)readbuf, length);

    //exit if read reaches eof
    if (res == 0) {
      printf("[The client disconnected.] \n");
      return -1;
    }

    //if read returns an error
    if (res == -1) {
      if (errno == EINTR)
        continue;
      printf(" Error on client socket:%s \n ", strerror(errno));
      return -1;
    }
    return 0;
  }
}

//function to write to client
void
WriteToClient(int fd, char* msg){
  //gets the size of the message
  size_t msg_size = strlen(msg);

  while (1) {
    int wres = write(fd, msg, msg_size);

    //if write reaches eof
    if (wres == 0) {
     printf("socket closed prematurely \n");
     break;
    }

    //if write returns error
    if (wres == -1) {
      if (errno == EINTR)
        continue;
      printf(" Error on client socket:%s \n ", strerror(errno));
      break;
    }
    break;
  }
}

//function to put a record into the database
int
Put(struct record* sr, int32_t fd){
  char buffer[sizeof(*sr)];
  int checkid;
  int res;
  int i = 0;

  while(1){
    //updates offset to index i then reads
    lseek(fd,i,SEEK_SET);
    res = read(fd,buffer,sizeof(*sr));

    //if any bytes were read store the id read into check id
    if(res > 0){
      buffer[res] = '\0';
      sscanf(buffer,"%d",&checkid);
    }

    //if read reaches the eof or a record with id 0 break from the loop
    if (res == 0 || checkid == 0){
      lseek(fd,i,SEEK_SET);
      break;
    }

    //if read returns an error
    else if(res == -1){
      if (errno == EINTR)
        continue;
      return -1;
    }

    //update i to go to next record
    i += sizeof(*sr);
  }

  //writes the recoord into the file
  dprintf(fd,"%d %s", sr->id, sr->name);
  return 0;
}

//function to retrive a record from the database
int 
Get(struct record *rd, int32_t fd){
  int check = rd->id;
  int i = 0;
  int res;
  char buffer[sizeof(*rd)];

  while(1){

    //updates offset to index i then reads 
    lseek(fd,i,SEEK_SET);
    res = read(fd,buffer,sizeof(*rd));

    //if read reaches eof exit loop
    if(res == 0)
      break;

    //if read returns error
    else if(res == -1){
      if (errno == EINTR)
        continue;
      return -1;
    }
    
    //scan the currnet record from the file and store it in rd
    buffer[res] = '\0';  
    sscanf(buffer,"%d %127[^\n]",&rd->id,rd->name);

    //if the records id matches the given id, return that record 
    if (rd->id  == check){
      return 0;
    }

    //updates i to the next record
    i += sizeof(*rd);
  }

  //returns failed if record wasnt found
  return 1;
}

//function to delete a record from the database
int 
Delete(struct record *rd, int32_t fd){
  int i = 0;
  int check = rd->id;
  int res;
  char buffer[sizeof(*rd)];

  while(1){
    //updates offset to index i then reads 
    lseek(fd, i, SEEK_SET);
    res = read(fd, buffer, sizeof(*rd));

    //if read reaches eof exit loop
    if(res == 0)
        break;

    //if read returns an error
    else if(res == -1){
      if(errno == EINTR)
        continue;
      return -1;
    }
    
    //scan the current recrd from the file and store it in rd
    buffer[res] = '\0';
    sscanf(buffer,"%d %127[^\n]",&rd->id,rd->name);

    //checks if the records id matches check if so overwrite and return the current record
    if(rd->id == check){
      //overwrites the currnet deleted record template 
      lseek(fd,i,SEEK_SET);
      dprintf(fd,"%d%s%s", deleted.id, deleted.pad,deleted.name);
      return 0;
    }

    //update i to the next record
    i += sizeof(*rd);
  }

  //returns failed if record wasnt found
  return 1;
}

