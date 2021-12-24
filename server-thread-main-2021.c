/*
** A simple Server
   This program runs together with client-thread-main-2021.c as one system.

   To demo the whole system, you must:
   1. compile the server program:
        gcc -lpthread -o server server-thread-2021.c server-thread-main-2021.c
   2. run the program: server 41000 &
   3. compile the cliient program:
        gcc -o client client-thread-2021.c client-thread-main-2021.c
   4. run the client on another machine: client HOST 41000 webpage
         replacing HOST and HTTPPORT with the host the server runs
         on and the port # the server runs at.

   FOR STUDNETS:
     If you want to try this demo, you MUST use the port # assigned
     to you by your instructor. Since each http port can be associated
     to only one process (or one server), if another student tries to
     run the server with the default port, it will be rejected.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <pthread.h>
#include "server-thread-2021.h"

#define HOST "freebsd1.cs.scranton.edu"
#define BACKLOG 10
#define BUFFERSIZE 256

void start_subserver(int reply_sock_fd);
void *subserver(void * reply_sock_fd_as_ptr);
void handle_http_request(int reply_sock_fd);
void handle_greeting(int reply_sock_fd);

int main(int argc, char *argv[]) {
   int http_sock_fd;			// http server socket
   int reply_sock_fd;  	                // client connection 
   int yes;

   if (argc != 2) {
      printf("Run: program port#\n");
      return 1;
   }
   http_sock_fd = start_server(HOST, argv[1], BACKLOG);

   if (http_sock_fd ==-1) {
      printf("start server error\n");
      exit(1);
   }

   while(1) {
      if ((reply_sock_fd = accept_client(http_sock_fd)) == -1) {
         continue;
      }
      start_subserver(reply_sock_fd);
   }
} 

void start_subserver(int reply_sock_fd) {
   int type = 0;

   int read_count = recv(reply_sock_fd, &type, sizeof(int), 0);
   while (type > 0 && read_count != 0) {
      if (type == 1) {
         handle_http_request(reply_sock_fd);
      } else if (type == 2) {
         handle_greeting(reply_sock_fd);
      } else {
         printf("Client sent invalid type\n");
      }
      read_count = recv(reply_sock_fd, &type, sizeof(int), 0);
   }
   close(reply_sock_fd);
   printf("Client closed the connection\n");
}

void handle_greeting(int reply_sock_fd) {
   int nBytes = 0;
   int read_count = 0;
   char buffer[BUFFERSIZE];
   read_count = recv(reply_sock_fd, &nBytes, sizeof(int), 0);
   read_count = recv(reply_sock_fd, buffer, nBytes, 0);
   printf("Client sent: %s\n", buffer);
}

void handle_http_request(int reply_sock_fd) {
   char *html_file;
   int html_file_fd;
   int read_count = -1;
   char buffer[BUFFERSIZE+1];

   int nBytes = 0;
   read_count = recv(reply_sock_fd, &nBytes, sizeof(int), 0);
   read_count = recv(reply_sock_fd, buffer, nBytes, 0);
   printf("%s\n", buffer);

   // get the file name according to HTTP GET method protocol
   html_file = strtok(&buffer[5], " \t\n");
   printf("FILENAME: %s\n", html_file);
   html_file_fd = open(html_file, O_RDONLY);
   while ((read_count = read(html_file_fd, buffer, BUFFERSIZE))>0) {
      send(reply_sock_fd, &read_count, sizeof(int), 0);
      send(reply_sock_fd, buffer, read_count, 0);
   }
   read_count = -1;
   send(reply_sock_fd, &read_count, sizeof(int), 0);
   return;
}
