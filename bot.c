#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "board_library.h"
#include "event_library.h"

/* Bot player: picks random unlocked cards */
int * board_vision;
playerPick pick;
pthread_t tid[2];
int dim;
int sock_fd;
int gameOn;
int pending;
int end;
int quit;

/* Sets board memory flags to zero */
void initBot(){
  int i;

  for (i = 0; i < dim * dim; i++){
    board_vision[i] = 0;
  }
  pick.x = -1;
  pick.y = -1;
  pending = 0;
}

/* Reads event list from server and marks locked cards */
void readList(int sock_fd, int nrOfEvents){
	int i, x, y;

	//If list isn't empty, get events
	if(nrOfEvents!=0)
	{
		//Alocate memory for the cursor
		node* cursor = (node*) malloc(sizeof(node));
    play_response* resp;

		//Get all events
		for(i=0;i<nrOfEvents;i++)
		{
			//Read event
			read(sock_fd, cursor, sizeof(*cursor));
      resp = &cursor->reply.resp;

    	//If code is +2, memorize plays (mark as already picked, will not be picked by bot)
    	if(resp->code == 2){
        board_vision[dim*resp->play1[0]+resp->play1[1]] = 1;
        board_vision[dim*resp->play2[0]+resp->play2[1]] = 1;
      }
		}
	}
}

/* Reads the max number of points and the player's number
of points and checks if this player has won the game */
void readWinnerLoserInfo(){
	int points, max;
	read(sock_fd, &max, sizeof(max));
	read(sock_fd, &points, sizeof(points));
	if(points==max)
	{
		printf("You won the game: %d correct cards\n", max);
	}
	else
	{
		printf("You lost the game: %d correct cards. \n", points);
	 	printf("The winner had: %d correct cards. \n", max);
	}
}

/* Get random double between 0 and 1 with uniform distribution */
double getRand(){
  return (double) (rand() /(double)RAND_MAX);
}

/* Choose random unrevealed coordinates from uniform distribution */
void nextPick(){
  int x, y;

  /* Pick random card until card chosen is different from either first pick
   or any other card already up */
  while(1){
    x = floor(getRand() * dim);
    y = floor(getRand() * dim);

    if(board_vision[x*dim + y] == 0){
      pick.x = x;
      pick.y = y;
      break;
    }
  }
}

/* Thread function to read messages from server */
void *thread_readServer (void * arg){
	int test;
	play_response resp;
	serverReply* reply = (serverReply*) malloc(sizeof(serverReply));

	//Read board dimension and create bot board
  read(sock_fd, &dim, sizeof(dim));

  board_vision = malloc(dim * dim * sizeof(int));
  initBot();

	//Read number of list events
	read(sock_fd, &nrOfEvents, sizeof(nrOfEvents));

	//Read and print the events from the list to the board
	readList(sock_fd, nrOfEvents);

	//Set quit and end to zero
	quit=0;
	end=0;

	//Read all server replies until the game is over
	while(!quit)
	{
		while(!end)
		{
			//Read either a gameOn message or a serverReply
			if(gameOn == 0)
			{ // Before receiveing the gameOn message, check if there's any pending server reply and catch it
        while(pending > 0){
          test = read(sock_fd, reply, sizeof(*reply));
  				if(test==0 || test==-1)
  				{
  					end = 1;
  					quit = 1;
  					break;
  				}
          printf("Pending message detected.\n");
          pending--;
        }
        printf("Game on pause: waiting for server signal.\n");
        test = read(sock_fd, &gameOn, sizeof(gameOn));
        printf("Game on = %d\n", gameOn);

				if(test==0 || test==-1)
				{
					end = 1;
					quit = 1;
					break;
				}
			}
			//It's a serverReply:
			else
			{
				test = read(sock_fd, reply, sizeof(*reply));
				if(test==0 || test==-1)
				{
					end = 1;
					quit = 1;
					break;
				}

				resp = reply->resp;

				//Process reply acquired
				if(reply->gameOn == 0){
						//Set gameOn to 0
						gameOn=0;
				}

        switch (resp.code) {
					case 0:
						pending--;
						break;
				  case 1:
				    pending++;
				    break;
				  case 3:
				    end = 1;
				  case 2:
            board_vision[dim*resp.play1[0]+resp.play1[1]] = 1;
            board_vision[dim*resp.play2[0]+resp.play2[1]] = 1;
            pending--;
            break;
				  case -2:
				    pending--;
				    break;
					case -1:
						pending--;
						break;
				}
			}
		}

		//If player has quit, skip end of game and new game procedures
		if(quit==1)
			break;

		//---End of Game ---
		printf("Received end of game!\n");

		//Receive winner/loser information
		readWinnerLoserInfo();

		//Wait for restartGame = 1 message from server
		int restartGame;
		read(sock_fd, &restartGame, sizeof(restartGame));
		if(restartGame!=1)
		{
			printf("Restart game error -> Incorrect reply from server\n");
			quit = 1;
			break;
		}
		printf("Received restart game message from server\n");

		//--- New Game ---
	  // Reset bot variables
    initBot();

		//Set end = 0;
		end = 0;
	}

	//Exit
	free(reply);
	pthread_join(tid[1], NULL);
  pthread_exit(NULL);
}

/* Thread function that takes user input and makes players,
then sending them to server */
void *thread_play(void * arg){

	//Gameplay Loop
	while (1)
	{
	   if(gameOn==1 && quit==0 && end==0)
     {
       // Make bot decision
       nextPick();
       printf("Sending pick (%d,%d)\n", pick.x, pick.y);
       write(sock_fd, &pick, sizeof(playerPick));
       sleep(4);
	   }
  }

	//Exit
	pthread_exit(NULL);
}

int main(int argc, char * argv[]){
  struct sockaddr_in server_addr;

  // Seed the random number generator
  srand((unsigned int)time(NULL));

  //Read server address input
  if (argc <2){
    printf("Second argument should be server address.\n");
    exit(-1);
  }

  //Create socket
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd == -1)
  {
    perror("Socket: ");
    exit(-1);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port= htons(3000);
  inet_aton(argv[1], &server_addr.sin_addr);

  //Connect to socket
  if(-1 == connect(sock_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr))){
		printf("Error connecting.\n");
		exit(-1);
  }
  printf("Client connected.\n");

	//Wait for server gameOn reply
	read(sock_fd,&gameOn, sizeof(gameOn));
	printf("GameOn = %d\n", gameOn);

  //If it's the first player, it will receive a second message from the server
	if(gameOn!=1)
	{
		printf("Server error: game not active\n");
		exit(-1);
	}

	//Launch threads
  pthread_create(&tid[0], NULL, thread_readServer , NULL);
	sleep(1);
  pthread_create(&tid[1], NULL, thread_play, NULL);

  pthread_join(tid[1], NULL);
  close(sock_fd);
}
