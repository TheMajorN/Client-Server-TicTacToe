/*
Group:   Nicholas Baranosky, Myles Spencer, Morgan McGuire
Class:   Operating Systems
Date:    Oct. 18, 2021
Compile: gcc -o server server.c server-thread-2021.c -lpthread
Run:     ./server 17100
   
This program is the server which hosts
and controls the board for tic-tac-toe games.
It accepts connections from many players using
sockets and hosts multiple games using threads.
*/

#include <stdio.h>
#include <stdlib.h>
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
#define PLAYER1 'X'
#define PLAYER2 'O'
#define EMPTY '-'

typedef struct PLAYERRECORD {
   char name[21]; // Up to 20 letters
   char password[21]; // Up to 20 letters
   int wins;
   int losses;
   int ties;
}  PlayerRecord;

typedef struct LOCK {
   pthread_mutex_t lock; // Used to protect scoreboard
}  Lock;

typedef struct GAMECONTEXT {
   int playerXId;            // id of player X
   int playerOId;            // id of player O
   int playerXSockfd;        // sockfd for player X
   int playerOSockfd;        // sockfd for player O
   PlayerRecord *scoreboard;
   Lock *mutex;
}  GameContext;

void playGame(GameContext *game);
void makeMove(int playersockfd, char pSymb, char *board);
char *createBoard(int x, int y);
void printBoard(char *board);
int isTaken(char *board, int msgx, int msgy);
void set(char *board, int msgx, int msgy, char playerSymbol);
int checkWin(char *board, char playerSymbol);
int checkDraw(char *board);
void sendResult(int winner, int loser, int gameStat, char *board);
void sendUpdate(int p1sockfd, int p2sockfd, int gameStat, char *board);
int acceptName(PlayerRecord *scoreboard, int playersockfd, Lock *mutex);
void initScoreboard(PlayerRecord *scoreboard);
void start_subserver(GameContext *game);
void *subserver(void *ptr);
void sendNames(GameContext *game);
void updateGameContext(GameContext *game, int status);
void sendGameContext(GameContext *game);
void sendToPlayer1(GameContext *game);
void sendToPlayer2(GameContext *game);
void printScoreboard(GameContext *game);
void assignXGameContext(GameContext *game, int loc, int playersockfd);
void assignOGameContext(GameContext *game, int loc, int playersockfd);
int serverFull(int playersockfd);
int authenticate(PlayerRecord *scoreboard, int playersockfd, int loc);
void setPassword(PlayerRecord *scoreboard, int playersockfd, int loc);
void acceptPlayer1(GameContext *game, int sockfd);
void acceptPlayer2(GameContext *game, int sockfd);

/* Main function which accepts player connections
   and assigns them a status as either player 1 or
   player 2. Starts the game.
*/
int main(int argc, char *argv[]) {
   int sockfd;   
   
   // Program was run without port
   if(argc != 2) {
      printf("No program port\n");
      exit(1);
   }
   sockfd = start_server(HOST, argv[1], BACKLOG); 
   // Could not establish server connection
   if(sockfd == -1) {
      printf("Start server error\n");
      exit(1);
   }

   Lock *mutex = (Lock*)malloc(sizeof(Lock));
   pthread_mutex_init(&(mutex->lock), NULL);
   PlayerRecord *scoreboard = (PlayerRecord*)malloc(sizeof(PlayerRecord)*10);
   initScoreboard(scoreboard); 

   // Continue accepting players and hosting games until Ctrl + C
   while(1) {
      GameContext *game = (GameContext*)malloc(sizeof(GameContext));
      game->scoreboard = scoreboard;
      game->mutex = mutex;
      acceptPlayer1(game, sockfd);
      acceptPlayer2(game, sockfd);
      start_subserver(game);
   }
}

void acceptPlayer1(GameContext *game, int sockfd) {
   int player1sockfd, loc;
   while(1) {
      player1sockfd = accept_client(sockfd);
      loc = acceptName(game->scoreboard, player1sockfd, game->mutex);
      if(loc >= 0) { break; } 
   }
   assignXGameContext(game, loc, player1sockfd);
}

void acceptPlayer2(GameContext *game, int sockfd) {
   int player2sockfd, loc;
   while(1) {
      player2sockfd = accept_client(sockfd);
      loc = acceptName(game->scoreboard, player2sockfd, game->mutex);
      if(loc >= 0) { break; }
   }
   assignOGameContext(game, loc, player2sockfd);
}

void assignXGameContext(GameContext *game, int loc, int playersockfd) {
   game->playerXId = loc;   // Location of player X in scoreboard
   game->playerXSockfd = playersockfd;
}

void assignOGameContext(GameContext *game, int loc, int playersockfd) {
   game->playerOId = loc;   // Location of player X in scoreboard
   game->playerOSockfd = playersockfd;
}

/* Function creates thread and passes it the
   game context.
*/
void start_subserver(GameContext *game) {
   pthread_t tsubserver;
   pthread_create(&tsubserver, NULL, subserver, (void *) game);
}

/* Thread function hosts the game for both players
   and prints the final results.
*/
void *subserver(void *ptr) {
   GameContext *game = (GameContext *) ptr;
   int player1 = 1;
   int player2 = 2;
   send(game->playerXSockfd, &player1, sizeof(int), 0);
   send(game->playerOSockfd, &player2, sizeof(int), 0);
   sendNames(game);
   playGame(game);
   sendGameContext(game);
   printScoreboard(game);
}

/* Function prints all the players currently registered on
   the server and their wins, losses, and ties.
*/
void printScoreboard(GameContext *game) {
   pthread_mutex_lock(&(game->mutex->lock));
   printf("Scoreboard:\n");
   int i = 0;
   // While players still exist on scoreboard and end of
   // scoreboard has not been reached
   while(strcmp(game->scoreboard[i].name, "") != 0 && i != 10) {
      printf("Name: %s\n", game->scoreboard[i].name);
      printf("Wins: %d\n", game->scoreboard[i].wins);
      printf("Losses: %d\n", game->scoreboard[i].losses);
      printf("Ties: %d\n\n", game->scoreboard[i].ties);
      i++;
   } 
   pthread_mutex_unlock(&(game->mutex->lock));
}

/* Function sends game context to players.
*/
void sendGameContext(GameContext *game) {
   sendNames(game);
   sendToPlayer1(game);
   sendToPlayer2(game);
}

/* Function sends game stats of player 1 and player 2
   to player 1.
*/
void sendToPlayer1(GameContext *game) {
   pthread_mutex_lock(&(game->mutex->lock));
   send(game->playerXSockfd, &game->scoreboard[game->playerXId].wins,
        sizeof(int), 0);
   send(game->playerXSockfd, &game->scoreboard[game->playerXId].losses,
        sizeof(int), 0);
   send(game->playerXSockfd, &game->scoreboard[game->playerXId].ties,
        sizeof(int), 0);
   send(game->playerXSockfd, &game->scoreboard[game->playerOId].wins,
        sizeof(int), 0);
   send(game->playerXSockfd, &game->scoreboard[game->playerOId].losses,
        sizeof(int), 0);
   send(game->playerXSockfd, &game->scoreboard[game->playerOId].ties,
        sizeof(int), 0);
   pthread_mutex_unlock(&(game->mutex->lock));  
}

/* Function sends game stats of player 2 and player 1
   to player 2.
*/
void sendToPlayer2(GameContext *game) {
   pthread_mutex_lock(&(game->mutex->lock));
   send(game->playerOSockfd, &game->scoreboard[game->playerOId].wins,
        sizeof(int), 0);
   send(game->playerOSockfd, &game->scoreboard[game->playerOId].losses,
        sizeof(int), 0);
   send(game->playerOSockfd, &game->scoreboard[game->playerOId].ties,
        sizeof(int), 0);
   send(game->playerOSockfd, &game->scoreboard[game->playerXId].wins,
        sizeof(int), 0);
   send(game->playerOSockfd, &game->scoreboard[game->playerXId].losses,
        sizeof(int), 0);
   send(game->playerOSockfd, &game->scoreboard[game->playerXId].ties,
        sizeof(int), 0);
   pthread_mutex_unlock(&(game->mutex->lock));
}

/* Function sends names of player 1 and player 2
   to both player 1 and player 2.
*/
void sendNames(GameContext *game) {
   int p1size = strlen(game->scoreboard[game->playerXId].name)+1;
   int p2size = strlen(game->scoreboard[game->playerOId].name)+1;
   
   // player 1 and player 2 receive their own name
   send(game->playerXSockfd, &p1size, sizeof(int), 0);
   send(game->playerOSockfd, &p2size, sizeof(int), 0);
   send(game->playerXSockfd, game->scoreboard[game->playerXId].name,
        p1size, 0);
   send(game->playerOSockfd, game->scoreboard[game->playerOId].name,
        p2size, 0);
   
   // player 1 and player 2 receive each others name
   send(game->playerXSockfd, &p2size, sizeof(int), 0);
   send(game->playerOSockfd, &p1size, sizeof(int), 0);
   send(game->playerOSockfd, game->scoreboard[game->playerXId].name,
        p1size, 0);
   send(game->playerXSockfd, game->scoreboard[game->playerOId].name,
        p2size, 0);
}

/* Function accepts names from players
   and registers them in the scoreboard if it is not full.
   Also returns the location of player on scoreboard.
*/
int acceptName(PlayerRecord *scoreboard, int playersockfd, Lock *mutex) {
   char name[21];
   int size, loc, comp, result;
   recv(playersockfd, &size, sizeof(int), 0);
   recv(playersockfd, name, size, 0);
   
   pthread_mutex_lock(&(mutex->lock));
   // Loop checks to see if player name is already registered
   for(loc = 0; loc < 10; loc++) {
      comp = strcmp(name, scoreboard[loc].name);
      // If name is already on scoreboard
      if(comp == 0) {
         if(authenticate(scoreboard,playersockfd,loc) == -2) { return -2; }
         printf("Player already on scoreboard\n\n");
         result = 0;
         send(playersockfd, &result, sizeof(int), 0);
         pthread_mutex_unlock(&(mutex->lock));
         return loc;
      }
   }
   // Loop checks to see if empty location in scoreboard exists
   for(loc = 0; loc < 10; loc++) {
      // If empty space in scoreboard, place player there
      if(strcmp(scoreboard[loc].name, "") == 0) {
         strcpy(scoreboard[loc].name, name); 
         setPassword(scoreboard,playersockfd,loc);
         printf("Player placed on board\n\n");
         result = 0;
         send(playersockfd, &result, sizeof(int), 0);
         pthread_mutex_unlock(&(mutex->lock)); 
         return loc;
      }
   }
   pthread_mutex_unlock(&(mutex->lock));
   return(serverFull(playersockfd));
}

int authenticate(PlayerRecord *scoreboard, int playersockfd, int loc) {
   int size, result;
   int incorrect = -2;
   char password[21];
   
   recv(playersockfd, &size, sizeof(int), 0);
   recv(playersockfd, password, size, 0);
   if(strcmp(scoreboard[loc].password, password) != 0) {
      printf("Incorrect password received\n");
      send(playersockfd, &incorrect, sizeof(int), 0);
      return -2;
   }
   printf("Correct password received\n");
   return 0;

}

void setPassword(PlayerRecord *scoreboard, int playersockfd, int loc) {
   int size;
   char password[21];
   
   recv(playersockfd, &size, sizeof(int), 0);
   recv(playersockfd, password, size, 0);
   strcpy(scoreboard[loc].password, password);
   printf("Password received and set\n");
}

int serverFull(int playersockfd) {
   int result = -1;
   printf("Server full\n");
   send(playersockfd, &result, sizeof(int), 0);
   return -1;
}

/* Function intializes the scoreboard.
*/
void initScoreboard(PlayerRecord *scoreboard) {
  // Loop assigns each place on scoreboard as empty
   for(int i = 0; i < 10; i++) {
      strcpy(scoreboard[i].name, "");
      scoreboard[i].wins = 0;
      scoreboard[i].losses = 0;
      scoreboard[i].ties = 0;
   }
}

/* Function creates the board, handles the moves of players 
   in the correct sequence, sends updated board to players, 
   and determines if game is over in a win, loss, or draw.
*/
void playGame(GameContext *game) {
   char *board = createBoard(3,3);
   int gameStat = -1;  // Game not over while -1   
   // Game ends once a win, loss, or draw occurs which means
   while(1) {
       makeMove(game->playerXSockfd, PLAYER1, board);
       gameStat = checkWin(board, PLAYER1);     
       // If player 1 has won the game
       if(gameStat == 1) {
          updateGameContext(game, 1);
          sendResult(game->playerXSockfd, game->playerOSockfd, gameStat, board);
          free(board);
          break;
       }
       gameStat = checkDraw(board);      
       // If game has ended in a draw
       if(gameStat == 2) {      
          updateGameContext(game, 3);
          sendResult(game->playerXSockfd, game->playerOSockfd, gameStat, board);
          free(board);
          break;
       }
       // No win or draw yet, update both players
       sendUpdate(game->playerXSockfd, game->playerOSockfd, gameStat, board); 
       makeMove(game->playerOSockfd, PLAYER2, board);
       gameStat = checkWin(board, PLAYER2);
       // If player 2 has won the game
       if(gameStat == 1) {
          updateGameContext(game, 2);
          sendResult(game->playerOSockfd, game->playerXSockfd, gameStat, board);
          free(board);
          break;
       }
       // No win or draw yet, update both players
       sendUpdate(game->playerXSockfd, game->playerOSockfd, gameStat, board);
   }
}

/* Function updates the game context for a given player
   based on a win, loss. or tie.
*/
void updateGameContext(GameContext *game, int status) {
   pthread_mutex_lock(&(game->mutex->lock));
   // If player 1 or X has won
   if(status == 1) {
      game->scoreboard[game->playerXId].wins++;
      game->scoreboard[game->playerOId].losses++;
   }
   // If player 2 or O has won
   else if(status == 2) {
      game->scoreboard[game->playerOId].wins++;
      game->scoreboard[game->playerXId].losses++;
   }
   // If game has ended in a draw
   else if(status == 3) {
      game->scoreboard[game->playerXId].ties++;
      game->scoreboard[game->playerOId].ties++;
   }
   pthread_mutex_unlock(&(game->mutex->lock));
}

/* Function send indication that game is continuing and also
   sends the updated board.
*/
void sendUpdate(int p1sockfd, int p2sockfd, int gameStat, char *board) {
   send(p1sockfd, &gameStat, sizeof(int), 0);
   send(p1sockfd, board, strlen(board)+1, 0);
   send(p2sockfd, &gameStat, sizeof(int), 0);
   send(p2sockfd, board, strlen(board)+1, 0);
}

/* Function marks the player's specified location
   on the board as long as it has not already been
   taken.
*/
void makeMove(int playersockfd, char pSymb, char *board) {
   int x,y,taken;
   
   // Accepts input for coordinates until 
   // untaken board location is sent
   while(1) {
      recv(playersockfd, &x, sizeof(int), 0);
      recv(playersockfd, &y, sizeof(int), 0);
      taken = isTaken(board, x, y);
      
      // If location on the board is taken
      if(taken == 0) {
         send(playersockfd, &taken, sizeof(int), 0);
      }
      // Location on the board is not taken, can be marked
      else { break; }
   }
   set(board, x, y, pSymb);
   send(playersockfd, &taken, sizeof(int), 0);
}

/* Function sends results of game to winning player and losing player
   or sends both players draw result depending on gameStat.
*/
void sendResult(int winner, int loser, int gameStat, char *board) {
   int lose = 0; // Sent to player that loses the game
   
   // If gameStat is 1, the game has been won by a player
   if(gameStat == 1) {
      send(winner, &gameStat, sizeof(int), 0);
      send(winner, board, strlen(board)+1, 0);
      send(loser, &lose, sizeof(int), 0); 
      send(loser, board, strlen(board)+1, 0);
   }
   // If gameStat is 2, the game has ended in a draw
   else if(gameStat == 2) {
      send(winner, &gameStat, sizeof(int), 0);
      send(winner, board, strlen(board)+1, 0);
      send(loser, &gameStat, sizeof(int), 0);
      send(loser, board, strlen(board)+1, 0);
   }
}

/* Function gets the symbol at given
   location on the board.
*/ 
char get(char *board, int i, int j) {
   return board[i*3 + j];
}

/* Function determines if location on board is taken.
   If it is returns a 0, if not returns a 1.
*/
int isTaken(char *board, int msgx, int msgy) {
   char loc = get(board, msgx, msgy);
   
   // Location not taken
   if(loc == EMPTY) {
      return 1;
   }
   // Location is taken
   else {
      return 0;
   } 
}   

/* Function marks location on the board with 
   specified player's symbol.
*/ 
void set(char *board, int msgx, int msgy, char playerSymbol) {
   board[msgx*3 + msgy] = playerSymbol;
}

/* Function creates the board used for the 
   tic-tac-toe game.
*/
char *createBoard(int x, int y) {
   char *board = (char*)malloc(x*y+1);
   
   // Intialize each board location as empty
   for(int i = 0; i < x * y; i++) {
     board[i] = EMPTY;
   }
   return board;
}

/* Check to see if game has been won. Returns 1 if it has,
   and -1 if it hasn't.
*/ 
int checkWin(char *board, char playerSymbol) {
   if(board[0] == playerSymbol && board[4] == playerSymbol
     && board[8] == playerSymbol) { // Checks specific board location
      return 1;
   }
   if(board[2] == playerSymbol && board[4] == playerSymbol
     && board[6] == playerSymbol) { // Checks specific board location
      return 1;
   }
   if(board[0] == playerSymbol && board[3] == playerSymbol
     && board[6] == playerSymbol) { // Checks specific board location
      return 1;
   }
   if(board[2] == playerSymbol && board[5] == playerSymbol
     && board[8] == playerSymbol) { // Checks specific board location
      return 1; 
   }
   if(board[1] == playerSymbol && board[4] == playerSymbol
     && board[7] == playerSymbol) { // Checks specific board location
      return 1;
   }
   int row = 0;
   while(row < 7) {   // Checks row of board for win
      if(board[row] == playerSymbol && board[row+1] == playerSymbol
        && board[row+2] == playerSymbol) {
         return 1;
      }
     row = row + 3; // Add 3 to current row to get next row
   }
   return -1;
}

/* Function checks board for a draw
*/
int checkDraw(char *board) {
   int status = 2; // Status is a draw when 2
   
   // Iterates through each board location
   for(int i = 0; i < 10; i++) {
   
      // If a empty location on board exists, no draw
      if(board[i] == EMPTY) {
         status = -1;  // Game is not over
         return status;
      }
   }
   return status;
}

/* Print the tic-tac-toe board.
*/
void printBoard(char *board) {
   char val;
   for(int i = 0; i < 3; i++) {
      for(int j = 0; j < 3; j++) {
         val = get(board, i, j);
         printf("%c ", val);
      }
      printf("\n");
   }
}
