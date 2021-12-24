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

// Analogy: You bought a phone(socket) and bound to a # (port#)
int get_server_socket(char *hostname, char *port) {
   struct addrinfo hints, *servinfo, *p;
   int status;
   int server_socket;
   int yes = 1;

   memset(&hints, 0, sizeof hints);
   hints.ai_family = PF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;

   if ((status = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
      printf("getaddrinfo: %s\n", gai_strerror(status));
      exit(1);
   }

   for (p = servinfo; p != NULL; p = p ->ai_next) {
      // step 1: create a socket
      if ((server_socket = socket(p->ai_family, p->ai_socktype,
                          p->ai_protocol)) == -1) {
          printf("socket socket \n");
          continue;
      }
      // if the port is not released yet, reuse it.
      if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
         printf("socket option\n");
         continue;
      }

      // step 2: bind socket to an IP addr and port
      if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
         printf("socket bind \n");
         continue;
      }
      break;
   }
   print_ip(servinfo);
   freeaddrinfo(servinfo);   // servinfo structure is no longer needed. free it.

   return server_socket;
}
// Analogy: get a phone and sign up for service
int start_server(char *hostname, char *port, int backlog) {
   int status = 0;
   int serv_socket = get_server_socket(hostname, port);
   // Analogy: ask phone company to activate the phone. 
   if ((status = listen(serv_socket, backlog)) == -1) {
      printf("socket listen error\n");
   }
   return serv_socket;
}

// Analogy: Answer a call on your phone
int accept_client(int serv_sock) {
   int reply_sock_fd = -1;
   socklen_t sin_size = sizeof(struct sockaddr_storage);
   struct sockaddr_storage client_addr;
   char client_printable_addr[INET6_ADDRSTRLEN];

   // accept a connection request from a client
   // the returned file descriptor from accept will be used
   // to communicate with this client.
   if ((reply_sock_fd = accept(serv_sock, 
           (struct sockaddr *)&client_addr, &sin_size)) == -1) {
      printf("socket accept error\n");
   }
   else {
      // here is for info only, not really needed.
       inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), 
                   client_printable_addr, sizeof client_printable_addr);
       printf("server: connection from %s at port %d\n", client_printable_addr,
                  ((struct sockaddr_in*)&client_addr)->sin_port);
   }
   return reply_sock_fd;
}

// ======= HELP FUNCTIONS =========== //
/* the following is a function designed for testing.
   it prints the ip address and port returned from
   getaddrinfo() function */
void print_ip( struct addrinfo *ai) {
   struct addrinfo *p;
   void *addr;
   char *ipver;
   char ipstr[INET6_ADDRSTRLEN];
   struct sockaddr_in *ipv4;
   struct sockaddr_in6 *ipv6;
   short port = 0;

   for (p = ai; p !=  NULL; p = p->ai_next) {
      if (p->ai_family == AF_INET) {
         ipv4 = (struct sockaddr_in *)p->ai_addr;
         addr = &(ipv4->sin_addr);
         port = ipv4->sin_port;
         ipver = "IPV4";
      }
      else {
         ipv6= (struct sockaddr_in6 *)p->ai_addr;
         addr = &(ipv6->sin6_addr);
         port = ipv4->sin_port;
         ipver = "IPV6";
      }
      inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
      printf("serv ip info: %s - %s @%d\n", ipstr, ipver, ntohs(port));
   }
}

void *get_in_addr(struct sockaddr * sa) {
   if (sa->sa_family == AF_INET) {
      printf("ipv4\n");
      return &(((struct sockaddr_in *)sa)->sin_addr);
   }
   else {
      printf("ipv6\n");
      return &(((struct sockaddr_in6 *)sa)->sin6_addr);
   }
}
