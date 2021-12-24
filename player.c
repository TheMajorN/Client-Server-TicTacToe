/* 
Contributors:   Nick, Myles, Morgan
Compile: gcc -o player player.c client-thread-2021.c
Run:     ./player freebsd1.cs.scranton.edu 17100 client-thread-2021.h

This program connects to tic-tac-toe server using
socket commands and represents a player. It contains
functions to determine player status, send a move to
the server, and determine win, loss, or draw.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "client-thread-2021.h"

void playGame(int playersockfd);
void makeMove(int playersockfd);
void recvBoard(int playersockfd);
int recvUpdate(int playersockfd);
void checkGameStat(int playersockfd, int gameStat);
void player1(int playersockfd);
void player2(int playersockfd);
char get(char *board, int i, int j);
int sendNamePass(int playersockfd);
void recvNames(int playersockfd);
void recvGameContext(int playersockfd);

/* Main function which establishes connection to the
   server, starts the game, and closes the connection.
*/
int main(int argc, char *argv[]) {
   int playersockfd;  

   // Proper parameters are missing in run statement
   if(argc != 4) {
      printf("Missing proper parameters\n");
      exit(1);
   }
   
   playersockfd = get_server_connection(argv[1], argv[2]);
   
   // Could not connect to the server
   if(playersockfd == -1) {
      printf("Connection error\n");
      exit(1);
   }
   printf("Welcome to Tic-Tac-Toe\n\n");

   // Player was logged in or registered
   if(sendNamePass(playersockfd) > 0) {
      playGame(playersockfd);
      recvGameContext(playersockfd);
   }
   // Game ended or player was not logged in or registered
   close(playersockfd);
}

/* Player receives his/her own win, loss, and 
   tie status as well as the opponent's. It is
   then printed to the player. 
*/
void recvGameContext(int playersockfd) {
   int nameSize, wins, losses, ties;
   int opNameSize, opWins, opLosses, opTies;
   char name[21];
   char opName[21];

   // Receiving names of player and opponent
   recv(playersockfd, &nameSize, sizeof(int), 0);
   recv(playersockfd, name, nameSize, 0);
   recv(playersockfd, &opNameSize, sizeof(int), 0);
   recv(playersockfd, opName, opNameSize, 0);

   // Receiving win, loss, and tie stats of player and opponent
   recv(playersockfd, &wins, sizeof(int), 0);
   recv(playersockfd, &losses, sizeof(int), 0);
   recv(playersockfd, &ties, sizeof(int), 0);
   recv(playersockfd, &opWins, sizeof(int), 0);
   recv(playersockfd, &opLosses, sizeof(int), 0);
   recv(playersockfd, &opTies, sizeof(int), 0);

   printf("%s: %dW/%dL/%dT - %s: %dW/%dL/%dT\n", name, wins, losses, ties,
          opName, opWins, opLosses, opTies);
}

/* Function accepts player's name and password and sends it
   to the server.
*/
int sendNamePass(int playersockfd) {
   char name[21];
   char password[21];
   int nsize, psize, result;
   
   printf("Enter name:\n");
   scanf("%s", name);
   printf("Enter password:\n");
   scanf("%s", password);
   
   psize = strlen(password)+1;
   nsize = strlen(name)+1;
   send(playersockfd, &nsize, sizeof(int), 0);
   send(playersockfd, name, nsize, 0);
   send(playersockfd, &psize, sizeof(int), 0);
   send(playersockfd, password, psize, 0);
   recv(playersockfd, &result, sizeof(int), 0);
   
   // Player was accepted
   if(result == 0) {
      printf("Player registered\n");
      return 1;
   }
   // Incorrect password entered
   else if(result == -2) {
      printf("Incorrect password entered\n");
   }
   // Server not accepting further players
   else { 
      printf("Server full\n");
   }
   return -1;
}

/* Function begins the game and determines which player
   sequence to enter into to.
*/
void playGame(int playersockfd) {
   int playerNum = -1;
   recv(playersockfd, &playerNum, sizeof(int), 0);
   recvNames(playersockfd);
   
   // If player goes first and is an X on board
   if(playerNum == 1) {
      player1(playersockfd);
   }
   
   // If player goes last and is an 0 on board
   else if(playerNum == 2) {
      player2(playersockfd);
   }   
}

/* Function receives the name of player
   and the name of opponent and prints it.
*/
void recvNames(int playersockfd) {
   int nameSize, opNameSize;
   char name[21];
   char opName[21];
 
   recv(playersockfd, &nameSize, sizeof(int), 0);
   recv(playersockfd, name, nameSize, 0);
   recv(playersockfd, &opNameSize, sizeof(int), 0);
   recv(playersockfd, opName, opNameSize, 0);
   
   printf("Your name: %s, Opponent name: %s\n", name, opName);
}

/* The execution of player 1 or the first player
   to connect to the server. Makes move first
   and receives updates from its moves and the
   moves of player 2.
*/
void player1(int playersockfd) {
   int gameStat = -1; // Game not over while -1
   
   // Player 1 makes move until game declared over
   // and break statement is reached
   while(1) {
      printf("Your turn\n");
      makeMove(playersockfd);
      gameStat = recvUpdate(playersockfd);
      checkGameStat(playersockfd, gameStat);     
      // If game is over, after player 1's move
      if(gameStat != -1) { break; }
      
      printf("Opponent's turn\n");
      gameStat = recvUpdate(playersockfd);
      checkGameStat(playersockfd, gameStat);    
      // If game is over, after player 2's move
      if(gameStat != -1) { break; }
   }
}

/* The execution of player 2 or the second player
   to connect to the server. Makes move second
   and receives updates from player 1's moves and
   its own moves.
*/
void player2(int playersockfd) {
   int gameStat = -1; // Game not over while -1
   
   // Player 2 makes move until game declared over
   // and brek statement is reached
   while(1) {
      printf("Opponent's turn\n");
      gameStat = recvUpdate(playersockfd);
      checkGameStat(playersockfd, gameStat);
      // If game is over after player 1's move
      if(gameStat != -1) { break; }

      printf("Your turn\n");      
      makeMove(playersockfd);
      gameStat = recvUpdate(playersockfd);
      checkGameStat(playersockfd, gameStat);
      // If game is over after player 2's move
      if(gameStat != -1) { break; }
   }
}

/* Function receives and returns updated game status after
   a move and calls function to receive the updated board.
*/
int recvUpdate(int playersockfd) {
   int gameStat = -1;
   recv(playersockfd, &gameStat, sizeof(int), 0);
   recvBoard(playersockfd);
   return gameStat;
}

/* Function prints whether the game is over in a win, loss,
   or a draw for associated player.
*/
void checkGameStat(int playersockfd, int gameStat) {

   // If player has lost the game
   if(gameStat == 0) {
       printf("You lost\n");
   }
   
   // If player has won the game
   else if(gameStat == 1) {
       printf("You won\n");
   }
   
   // If game has ended in a draw
   else if(gameStat == 2) {
       printf("Draw\n");
   }
}

/* Function makes a move for player by sending specified
   coordinates to the server.
*/
void makeMove(int playersockfd) {
   int x,y,taken;
   
   // Continue until untaken location is sent
   // and break statement is reached
   while(1) {
      scanf("%d,%d", &x, &y);
      
      // If coordinates go beyond board boundary
      if((x < 0 || x > 2) || (y < 0 || y > 2)) {
         printf("Invalid location choose again\n");
      }
      
      // Coordinates are on the board
      else {
         send(playersockfd, &x, sizeof(int), 0);
         send(playersockfd, &y, sizeof(int), 0);
         recv(playersockfd, &taken, sizeof(int), 0);
         
         // If location is not taken
         if(taken == 1) {        
            break;
         }
         
         // Location is taken
         else {
            printf("Location taken choose again\n");
         }
      }
   }
}

/* Function receives the updated board from the server
   and prints it.
*/
void recvBoard(int playersockfd) {
   char *board;
   char val;
   recv(playersockfd, board, 10, 0);
   
   // Prints the board
   for(int i = 0; i < 3; i++) {
      for(int j = 0; j < 3; j++) {
         val = get(board, i, j);
         printf("%c ", val);       
      }
      printf("\n");
   }
   printf("\n");
}

/* Function gets the symbol at given 
   location on the board.
*/
char get(char *board, int i, int j) {
   return board[i*3 + j];
}
