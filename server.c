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
#include <math.h>

#include "board_library.h"
#include "event_library.h"

// Max number of players allowed by the server
#define MAXCLI 10

// Pthread types
pthread_t tid[MAXCLI];
pthread_t timeoutID[MAXCLI];
pthread_t t_gamePause;

// Locks to guarantee synchronization to server event list and board
pthread_mutex_t list_lock;
pthread_mutex_t board_lock;
pthread_mutex_t card_lock;
pthread_mutex_t np_lock;

// Flags variables for each player
int players_fd[MAXCLI];
int marker[MAXCLI];
int * play1Array[MAXCLI];
int active_players[MAXCLI];
int nrCorrectPlays[MAXCLI];
colorPlayer * colors;

// Game variables
int dim;
int nplayers;
int gameOn;
int waitRestartGame;

/* Get number of correct plays from each player and update
and return global max */
int getMaxNrCorrectPlays(){
	int i, j, max = 0;
	for(i=0, j=0; j<nplayers; i++)
	{
		if(active_players[i]==1)
		{
			if(nrCorrectPlays[i]>max)
				max = nrCorrectPlays[i];

			j++;
		}
	}
	return max;
}

/* Writes global max and also number of correct plays to every player
participating */
void writePointsToPlayers(){
	int i, j, max;
	max = getMaxNrCorrectPlays();
	for(i=0, j=0; j<nplayers; i++)
	{
		if(active_players[i]==1)
		{
			write(players_fd[i], &max, sizeof(max));
			write(players_fd[i], &nrCorrectPlays[i], sizeof(nrCorrectPlays[i]));
			j++;
		}
	}
}

/* Sets the 2 initial ActivePlayer flags to 1 and the rest to 0
(used in game initial stage) */
void init_activePlayers(){
	int i;
	active_players[0]=1;
	active_players[1]=1;
	// For now only activate first two, set the rest to zero
	for(i=2;i<MAXCLI;i++)
	{
		active_players[i]=0;
	}
}

/* Sets number of correct plays to 0 for each player
(used in game initial stage) */
void init_nrCorrectPlays(){
	int i;
	for(i=0;i<MAXCLI;i++)
		nrCorrectPlays[i]=0;
}

/* Prints event linked list */
void printList(){
	int i;

	node* cursor=head;
	for(i=0;i<nrOfEvents;i++)
	{
		printf("Player %d: code = %d\n", cursor->reply.index, cursor->reply.resp.code);
		cursor=cursor->next;
	}
}

/* Writes, one by one, events in the server list to the player client */
void writeListToPlayer(int index){
	int i;

	//If list isn't empty, write events
	if(nrOfEvents!=0)
	{
		pthread_mutex_lock(&list_lock);
		node* cursor = head;

		//Get all events
		for(i=0;i<nrOfEvents;i++)
		{
			//Write event
			write(players_fd[index], cursor, sizeof(*cursor));
			//Go to next node
			cursor = cursor->next;
		}
		pthread_mutex_unlock(&list_lock);
	}
}

/* Prints cards currently up */
void printCardsUp(){
	int i, j;
	for(i = 0; i < dim ; i++){
		for(j = 0; j < dim; j++){
			printf("%d ", card_up[dim*i+j]);
		}
		printf("\n");
	}
}

/* Writes active players array */
void printActivePlayers(){
	int i;
	printf("Currently active players are ");
	for(i=0;i<MAXCLI;i++)
	{
		printf("%d ", active_players[i]);
	}
	printf("\n");
}

/* Writes a given serverReply structure to all players */
void writeReplyToAllPlayers(serverReply* reply){
	int i, j;
	for(i=0, j=0;j<nplayers;i++)
	{
		if(active_players[i] == 1)
		{
			write(players_fd[i], reply, sizeof(*reply));
			printf("Wrote reply to %d\n", i);
			j++;
		}
	}
	printActivePlayers();
}

/* Generates random rgb color and assigns it to player */
void assignColor(int index, colorPlayer* color){
	/* Generate random r, b and g in the interval ]0,255[
	Check if it's = 0,0,0 or 255,255,255 which is invalid
	Check if that color is too similar to any other color already assigned
	If it is, then generate another random r,b,g and make prior checks again
	until a valid r,b,g is found.
		Assign that rbg to the player indexed by 'index' */

	int r = index;
	r = floor(230 * r / MAXCLI);

	int g = MAXCLI - index;
	g = floor(230 * g / MAXCLI);

	int b = MAXCLI * index;
	b = floor(230 * g / (MAXCLI*MAXCLI));


	if(r==0 && b==0 && g==0)
	{
		r=240;
		b=10;
		g=100;
	}

	if(r<120)
		r+=120;

	color->r = r;
	color->b = b;
	color->g = g;

	printf("Color assigned: %d %d %d\n", r, g, b);
	return;
}

/* Timeout thread function: waits 5 seconds after first pick
and if not, removes */
void* thread_timeoutTest (void* arg){
	serverReply* reply = (serverReply*) arg;

	//Store the value that the timeout marker had when this thread was launched
	int threadMarker = marker[reply->index];

	//Sleep for 5 seconds
	sleep(5);

	//Check if the player hasn't picked a second card after 5 seconds
	if(threadMarker==marker[reply->index])
	{
		//Change code to -1
		reply->resp.code=-1;

		//Remove event from list
		pthread_mutex_lock(&list_lock);
		removeFromList(*reply);
		pthread_mutex_unlock(&list_lock);

		// Flip card down
		pthread_mutex_lock(&card_lock);
		card_up[linear_conv(reply->resp.play1[0],reply->resp.play1[1])] = 0;
		pthread_mutex_unlock(&card_lock);

		printCardsUp();
		//Send reply
		writeReplyToAllPlayers(reply);

		//Set play1[0] to -1 (to signal that the next play will once again be a first pick)
		play1Array[reply->index][0]=-1;
		printf("Timeout reached.\n");
	}

	pthread_exit(NULL);
}

/* Pauses game by setting gameOn to 0 and writing serverReply to
the active player left */
void pauseGame(){
		int i;
		gameOn=0;
		//Write a server reply with gameOn=0 for the active player
		for(i=0; i<MAXCLI;i++)
		{
			if(active_players[i] == 1)
			{
				serverReply* reply = (serverReply*) malloc(sizeof(serverReply));
				reply->gameOn = gameOn;
				play_response* resp = (play_response*) malloc(sizeof(play_response));
				reply->resp = *resp;
				reply->resp.code=100;
				write(players_fd[i],reply, sizeof(*reply));
				break;
			}
		}
}

void reinitializePlayersMarkers(){
	int i, j;
	for(i=0, j=0; j<nplayers;i++)
	{
			if(active_players[i]==1)
			{
				//Reinitialize timeout marker
				marker[i] = 0;

				//Reinitialize play1[i][0] to -1
				play1Array[i][0] = -1;

				j++;
			}
	}
}

void *thread_func (void * arg){
  playerPick* pick = (playerPick*) malloc(sizeof(playerPick));
  int index = (int) arg;
	int test;
	serverReply* reply;

	//Alocate memory for server reply
	reply = (serverReply *) malloc(sizeof(serverReply));

	//Assign gameOn value to reply
	reply->gameOn = gameOn;

	//Assign player index to reply
	reply->index = index;

	//Assign color to player and store it in the reply structure
	assignColor(index, &reply->color);

	//Allocate play1[index][0] memory
	play1Array[index] = (int*) malloc(2*sizeof(int));

	//Send board dimension to player
  write(players_fd[index], &dim, sizeof(dim));

	//Send nrOfEvents to player
	write(players_fd[index],&nrOfEvents, sizeof(nrOfEvents));

	//Write list to player
	writeListToPlayer(index);

	//Initialize timeout marker
	marker[index] = 0;

	//Initialize play1[index][0] to -1
	play1Array[index][0] = -1;

	int quit = 0, end = 0;

	//Gameplay (multiple matches) loop
	while(!quit)
	{
		//One match loop
		while(!end)
		{
		  //Read player picks
		  test = read(players_fd[index], pick, sizeof(*pick));
			if(test==0 || test==-1)
			{
				end = 1;
				quit = 1;
				break;
			}

		  //Sends the board_x and board_y location of the card chosen to function board_play
		  //Stores the struct play_response 'resp' accordingly to the play made

			pthread_mutex_lock(&board_lock);
		  play_response resp = board_play(pick->x, pick->y, play1Array[index]);
			pthread_mutex_unlock(&board_lock);

			//Store resp in server reply
			reply->resp = resp;

		  //Act on resp.code
		  switch (resp.code) {
				case 0: // Card choosen was filled (on second play)
					//Remove event from list
					pthread_mutex_lock(&list_lock);
					removeFromList(*reply);
					pthread_mutex_unlock(&list_lock);

					// Flip card down
					pthread_mutex_lock(&card_lock);
					card_up[linear_conv(reply->resp.play1[0],reply->resp.play1[1])] = 0;
					pthread_mutex_unlock(&card_lock);

					//Increment marker
					marker[index]++;

					//Write server reply to all available players
					writeReplyToAllPlayers(reply);
					//Set play1[0] to -1 (to signal that the next play will once again be a first pick)
					play1Array[index][0]=-1;
					printCardsUp();
					break;
		    case 1: // First pick was made
					//Add event to list
					pthread_mutex_lock(&list_lock);
					addNodeToList(reply);
					pthread_mutex_unlock(&list_lock);

					//Increment marker
					marker[index]++;
					//Write server reply to all avaiable players
					writeReplyToAllPlayers(reply);
					//Create timeout thread
					pthread_create(&timeoutID[index], NULL, thread_timeoutTest , (void*) reply);
					printCardsUp();
		      break;
		    case 3: // End of game reached
		      end = 1;
		    case 2: // Second play: correct
					//Increment nrCorrectPlays array
					nrCorrectPlays[index]+=2;

					pthread_mutex_lock(&list_lock);
					//Remove first play from list
					removeFromList(*reply);
					//Add event to list
					addNodeToList(reply);
					pthread_mutex_unlock(&list_lock);

					//Increment marker
					marker[index]++;
					//Write server reply to all avaiable players
					writeReplyToAllPlayers(reply);
					printCardsUp();
		      break;
		    case -2: // Second play: incorrect
					pthread_mutex_lock(&list_lock);
					//Remove first play from the list
					removeFromList(*reply);
					//Add event to list
					addNodeToList(reply);
					pthread_mutex_unlock(&list_lock);

					//Increment marker
					marker[index]++;
					//Write server reply to all avaiable players
		      writeReplyToAllPlayers(reply);
					//Wait 2 seconds
					sleep(2);

					pthread_mutex_lock(&list_lock);
					//Remove incorrect play from list
					removeFromList(*reply);
					pthread_mutex_unlock(&list_lock);

					// Flip cards down
					pthread_mutex_lock(&card_lock);
					card_up[linear_conv(reply->resp.play1[0],reply->resp.play1[1])] = 0;
					card_up[linear_conv(reply->resp.play2[0],reply->resp.play2[1])] = 0;
					pthread_mutex_unlock(&card_lock);
					printCardsUp();
		      break;
		  }
		}

		//If player has quit, skip end of game and new game procedures
		if(quit==1)
			break;

		//Activate restartGame process marker (new players will have to wait till
		//new game is started)
		waitRestartGame=1;
		printf("End of game: player %d!\n", index);

		//Send winner/loser information to players
		writePointsToPlayers();

		//Delete list
		deleteList();
		//set nrOfEvents to zero
		nrOfEvents = 0;

		//Wait 10 seconds
		printf("Waiting 10 seconds...\n");
		sleep(10);

		//Reinitialize the board with dimension dim
		printf("Reinitializing board.\n");
		free(board);
		free(card_up);
		init_board(dim);

		//Reinitialize all active players markers
		reinitializePlayersMarkers();

		//Reinitialize nrCorrectPlays array
		init_nrCorrectPlays();

		//Send restartGame = 1 message to players to restart game
		printf("Sending restartGame messages\n");
		int k, j, restartGame = 1;
		for(k=0, j=0;j<nplayers;k++)
		{
			if(active_players[k] == 1)
			{
				test=write(players_fd[k], &restartGame, sizeof(restartGame));
				if(test==0 || test==-1)
				{
					printf("Error restarting the game.\n");
				}
				j++;
			}
		}

		//Deactivate restartGame process marker
		waitRestartGame=0;

		//Set end=0
		end = 0;
		printf("New game begins.\n");
	}

	// PLAYER QUIT
	//Deactivate position of active_players array
	active_players[index]=0;
	printActivePlayers();

	//Decrement number of players
	pthread_mutex_lock(&np_lock);
	--nplayers;
	pthread_mutex_unlock(&np_lock);

	printf("Number of players: %d\n", nplayers);

	//If there's a timeout thread, wait for it to finish before closing this player's thread
	pthread_join(timeoutID[index], NULL);

	//Free dinamically alocated memory
	free(play1Array[index]);
	free(reply);
	free(pick);

	//Pause game if less than 2 players now
	if(nplayers<2){
		printf("Less than 2 players connected: game will pause.\n");
		pauseGame();
	}

	printf("Player with index %d exited.\n", index);
	//Close socket
	shutdown(players_fd[index],SHUT_RDWR);
	close(players_fd[index]);
	//Exit thread
	pthread_exit(NULL);
}

void *thread_newPlayers (void * arg){
	int sock_fd = (int) arg;

	//Permanently accept new player requests
	while(1){
			//Check if game is full of players
			while(nplayers == MAXCLI)
					sleep(5); //Sleep for 5 seconds, then check again

			//Then, the game isn't full of players:
		  if(nplayers < MAXCLI){
					int index, test, temp_fd;
					printf("Number of players is %d.\n", nplayers);

					//Accept new player connection
					temp_fd = accept(sock_fd, NULL, NULL);

					//Find an available position to put the socket in
					for(index=0;index<MAXCLI;index++){
						if(active_players[index]==0){
							players_fd[index]=temp_fd;
							break;
						}
					}

					//Error checking
					if(players_fd[index]==-1 && players_fd[index]==0)
					{
						printf("Socket opening error: player index %d.\n", index);
					}
				  printf("Client connected.\n");

					//If we're at the restart game period of 10 secs, block until game has restarted
					while(waitRestartGame==1)
					{
						sleep(1);
					}

					//Activate player position in active_players array
					active_players[index] = 1;
					//Increment number of players
					pthread_mutex_lock(&np_lock);
					nplayers++;
					pthread_mutex_unlock(&np_lock);

					printActivePlayers();

					//If the game was paused, but now is going to be restarted:
					if(gameOn==0 && nplayers >= 2	)
					{
						gameOn=1;
						int k,j;

						//Write gameOn=1 to all active players
						for(k=0, j=0;j<nplayers;k++)
						{
							if(active_players[k] == 1)
							{
								test=write(players_fd[k], &gameOn, sizeof(gameOn));

								if(test==0 || test==-1)
									printf("GameOn write error.\n");
								j++;
							}
						}
					}

					//Else, the game is going on, write gameOn=1 to the new player
					else
					{
						test=write(players_fd[index],&gameOn, sizeof(gameOn));
						if(test==0 || test==-1)
							printf("gameOn write error\n");

					}
					//Create thread for the new player
					pthread_create(&tid[index], NULL, thread_func, (void*) index);
			}
	}
	pthread_exit(NULL);
}

int main(int argc, char** argv){
  struct sockaddr_in local_addr;
  pthread_t t_newPlayers;
  int msg_ret, i, j, error;

	// Initialize mutexes
	if(error = pthread_mutex_init(&list_lock, NULL))
		exit(1);

	if(error = pthread_mutex_init(&board_lock, NULL))
		exit(1);

	if(error = pthread_mutex_init(&card_lock, NULL))
		exit(1);

	if(error = pthread_mutex_init(&np_lock, NULL))
		exit(1);

  //Check argv
  if(argc<2){
    printf("Not enough parameters on command line.\n");
    exit(-1);
  }

  //Parse parameter to dimension of board (int)
  sscanf(argv[1], "%d", &dim);

	//Check argv
  if(dim % 2){
    printf("Pick even board dimension.\n");
    exit(-1);
  }

  //Create and initialize socket
  int sock_fd= socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd == -1){
    perror("Socket: ");
    exit(-1);
  }
  local_addr.sin_family = AF_INET;
  local_addr.sin_port= htons(3000);
  local_addr.sin_addr.s_addr= INADDR_ANY;

  //Bind socket
  int err = bind(sock_fd, (struct sockaddr *)&local_addr, sizeof(local_addr));
  if(err == -1) {
    perror("Bind: ");
    exit(-1);
  }
  printf("Socket created and binded.\n");

  //Listen to socket
  listen(sock_fd, 5);

  //Print waiting for players status
	nplayers=0;
  printf("Waiting for players...\n");

  //Accept player request
  players_fd[0]= accept(sock_fd, NULL, NULL);
  printf("Client connected.\n");
	nplayers++;

	//Accept second player request
  players_fd[1]= accept(sock_fd, NULL, NULL);
  printf("Client connected.\n");
	nplayers++;

	//Initialize the board with dimension dim
  init_board(dim);

	//Initialize list
	head=NULL;
	nrOfEvents=0;

	//Initialize active players array
	init_activePlayers();
	printActivePlayers();

	//Initialize nrCorrectPlays array
	init_nrCorrectPlays();

	//Send gameOn messages to first two players
	gameOn = 1;
	write(players_fd[0],&gameOn, sizeof(gameOn));
	write(players_fd[1],&gameOn, sizeof(gameOn));

	//Set waitRestartGame marker to 0
	waitRestartGame=0;

  //Create thread for the two first players
  pthread_create(&tid[0], NULL, thread_func, (void*) 0);
	pthread_create(&tid[1], NULL, thread_func, (void*) 1);

	//Create thread that looks for new players
	pthread_create(&t_newPlayers, NULL, thread_newPlayers, (void *) sock_fd);

	//Wait for thread join on the nplayer threads created for the players
	int flag;
	while(1){
			flag = 0;
			for(i=0; i<MAXCLI;i++){
					if(active_players[i]==1){
						pthread_join(tid[i], NULL);
						flag=1;
						break;
					}
			}
			if(flag==0 && nplayers==0)
			{
				printf("Game over.\n");
				break;
			}
	}

	// Destroy mutexes, free list and close socket
	pthread_mutex_destroy(&list_lock);
	pthread_mutex_destroy(&board_lock);
	pthread_mutex_destroy(&card_lock);
	pthread_mutex_destroy(&np_lock);
	deleteList();
	close(sock_fd);
}
