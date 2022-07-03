#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>  
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "msg.h"

#define BUF 512

void Usage(char *progname);
void WriteToServer(int fd, struct msg* msg);
void ReadFromServer(int fd, char readbuf[BUF]);

int LookupName(char *name,
                unsigned short port,
                struct sockaddr_storage *ret_addr,
                size_t *ret_addrlen);

int Connect(const struct sockaddr_storage *addr,
             const size_t addrlen,
             int *ret_fd);

int 
main(int argc, char **argv) {
  if (argc != 3) {
    Usage(argv[0]);
  }

  unsigned short port = 0;
  if (sscanf(argv[2], "%hu", &port) != 1) {
    Usage(argv[0]);
  }

  // Get an appropriate sockaddr structure.
  struct sockaddr_storage addr;
  size_t addrlen;
  if (!LookupName(argv[1], port, &addr, &addrlen)) {
    Usage(argv[0]);
  }

  // Connect to the remote host.
  int socket_fd;
  if (!Connect(&addr, addrlen, &socket_fd)) {
    Usage(argv[0]);
  }
  char choice[10];
  int8_t flag;
  char readbuf[BUF];
  flag = 1;

  //temporary to avoid function not used error
  if(flag == -1)
  ReadFromServer(socket_fd,readbuf);

  //Main loop that takes user input to execute commands
  while(flag){
    struct msg s_msg;
    struct record s_rd;
    printf("Enter your choice (1 to put, 2 to get, 3 to delete, 0 to quit): ");

    //converts the users choice into an integer to be stored in message
    fgets(choice,10,stdin);
    choice[strlen(choice)-1] = '\0';
    s_msg.type = atoi(choice);

    //checks if command entered is valid or not
    if(s_msg.type == 1 || s_msg.type == 2|| s_msg.type == 3){ 
      char status[sizeof(s_rd)];
      char id[11];

      //if command is put
      if (s_msg.type == 1){

        //get the persons name
        printf("Enter the Name: ");
        fgets(s_rd.name,MAX_NAME_LENGTH,stdin);
        s_rd.name[strlen(s_rd.name)-1] = '\0';
      
        //get the person's id
        printf("Enter the id: ");
        fgets(id,11,stdin);
        id[strlen(id)-1] = '\0';

        //checks if id is valid
        if(atoi(id) == 0 || strlen(id) > 10){
          printf("Invalid ID\n");
          continue;
        }

        //writes the record into the message and sends it to server
        s_rd.id = atoi(id);
        s_msg.rd = s_rd;
        WriteToServer(socket_fd,&s_msg);

        //wait for servers response if command was successful or not
        ReadFromServer(socket_fd,status);
    }
    //if the command was delete or get
    else{
      //gets the persons id
      printf("Enter the id: ");
      fgets(id,11,stdin);
      id[strlen(id)-1] = '\0';

      //Checks is id is valid or not
      if(atoi(id) == 0 || strlen(id) > 10){
        printf("Invalid ID\n");
        continue;
      }

      //write id to msg and send it to server
      s_rd.id = atoi(id);
      s_msg.rd = s_rd;
      WriteToServer(socket_fd,&s_msg);

      //wait for servers response if command was successful or not
      ReadFromServer(socket_fd,status);

    }

      //prints the status of the command sent by the server
      printf("%s\n", status);
    }

    //if the command wasnt a valid command
    else if(strcmp(choice,"0") != 0)
      printf("Invalid Command\n");

    //if the command was 0 send msg to server to close connection and exit program
    else{
      s_msg.type = 5;
    WriteToServer(socket_fd,&s_msg);
    flag = 0;
    }

  }

    // Clean up.
    close(socket_fd);
    return EXIT_SUCCESS;
  }

  //function to read a message from the server
  void
  ReadFromServer(int fd, char readbuf[BUF]){
    int res;
    while (1) {
      res = read(fd, readbuf, BUF-1);

      //if read reaches eof
      if (res == 0) {
        printf("socket closed prematurely \n");
        close(fd);
        exit(EXIT_FAILURE);
      }

      //if read returns an error
      if (res == -1) {
        if (errno == EINTR)
          continue;
        printf("socket read failure \n");
        close(fd);
        exit(EXIT_FAILURE);
      }
      readbuf[res] = '\0';
      break;
  }
}

//function to write to server
void 
WriteToServer(int fd, struct msg* msg){

  //gets the size of the message
  size_t msg_size = sizeof(*msg);

  while (1) {
    int wres = write(fd, msg, msg_size);

    //if write reaches eof
    if (wres == 0) {
     printf("socket closed prematurely \n");
      close(fd);
      exit(EXIT_FAILURE);
    }

    //if write returns an error
    if (wres == -1) {
      if (errno == EINTR)
        continue;
      printf("socket write failure \n");
      close(fd);
      exit(EXIT_FAILURE);
    }
    break;
  }
}

//prints usage of program
void 
Usage(char *progname) {
  printf("usage: %s  hostname port \n", progname);
  exit(EXIT_FAILURE);
}

//looks up hostname from dns to get an IP adress
int 
LookupName(char *name,
                unsigned short port,
                struct sockaddr_storage *ret_addr,
                size_t *ret_addrlen) {
  struct addrinfo hints, *results;
  int retval;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  // Do the lookup by invoking getaddrinfo().
  if ((retval = getaddrinfo(name, NULL, &hints, &results)) != 0) {
    printf( "getaddrinfo failed: %s", gai_strerror(retval));
    return 0;
  }

  // Set the port in the first result.
  if (results->ai_family == AF_INET) {
    struct sockaddr_in *v4addr =
            (struct sockaddr_in *) (results->ai_addr);
    v4addr->sin_port = htons(port);
  } else if (results->ai_family == AF_INET6) {
    struct sockaddr_in6 *v6addr =
            (struct sockaddr_in6 *)(results->ai_addr);
    v6addr->sin6_port = htons(port);
  } else {
    printf("getaddrinfo failed to provide an IPv4 or IPv6 address \n");
    freeaddrinfo(results);
    return 0;
  }

  // Return the first result.
  assert(results != NULL);
  memcpy(ret_addr, results->ai_addr, results->ai_addrlen);
  *ret_addrlen = results->ai_addrlen;

  // Clean up.
  freeaddrinfo(results);
  return 1;
}

//establishes connection with server
int 
Connect(const struct sockaddr_storage *addr,
             const size_t addrlen,
             int *ret_fd) {
  // Create the socket.
  int socket_fd = socket(addr->ss_family, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    printf("socket() failed: %s", strerror(errno));
    return 0;
  }

  // Connect the socket to the remote host.
  int res = connect(socket_fd,
                    (const struct sockaddr *)(addr),
                    addrlen);
  if (res == -1) {
    printf("connect() failed: %s", strerror(errno));
    return 0;
  }

  *ret_fd = socket_fd;
  return 1;
}
