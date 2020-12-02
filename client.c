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

#include "board_library.h"
#include "event_library.h"
#include "UI_library.h"

int sock_fd;
int pending;
int dim;
pthread_t tid[2];
int gameOn;
int end;
int quit;

/* Paints cards (background) on the board, for a given event */
void paintCardsOnBoard(node* cursor){
	play_response* resp = &cursor->reply.resp;
	colorPlayer* color = &cursor->reply.color;

	paint_card(resp->play1[0], resp->play1[1], color->r, color->b, color->g);
	//If code is +2 or -2, write the second card also
	if(resp->code!=1)
		paint_card(resp->play2[0], resp->play2[1], color->r, color->b, color->g);
}

/* Writes cards on the board, for a given event */
void writeCardsOnBoard(node* cursor){
	play_response* resp = &cursor->reply.resp;

	int r, b, g;
	if(resp->code==1)
	{
		r=200;
		b=200;
		g=200;
	}
	else if(resp->code==2)
	{
		r=0;
		b=0;
		g=0;
	}
	else
	{
		r=255;
		b=0;
		g=0;
	}

	//Always write the first card
	write_card(resp->play1[0], resp->play1[1], resp->str_play1, r, b, g);
	//If code is +2 or -2, write the second card also
	if(resp->code!=1)
		write_card(resp->play2[0], resp->play2[1], resp->str_play2, r, b, g);
}

/* Writes and paints the cards on the board based on the list
of events by reading events from server */
void printListToBoard(int sock_fd, int nrOfEvents){
	int i;

	//If list isn't empty, get events
	if(nrOfEvents!=0)
	{
		//Alocate memory for the cursor
		node* cursor = (node*) malloc(sizeof(node));

		//Get all events
		for(i=0;i<nrOfEvents;i++)
		{
			//Read event
			read(sock_fd, cursor, sizeof(*cursor));
			//Paint and write the card/cards accordingly
			paintCardsOnBoard(cursor);
			writeCardsOnBoard(cursor);
		}
	}

}

/* Initializes board window by painting all cards white */
void newBoardWindow(){
	int i, j;

	//Paints all former board cards in white
	for(i=0;i<dim;i++)
	{
		for(j=0;j<dim;j++)
		{
			paint_card(i, j , 255, 255, 255);
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

/* Thread function to read messages from server */
void *thread_readServer (void * arg){
	int test;
	play_response resp;
	serverReply* reply = (serverReply*) malloc(sizeof(serverReply));
	colorPlayer color;

	//Initializes SDL libraries
	if(SDL_Init( SDL_INIT_VIDEO ) < 0) {
		 printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		 exit(-1);
	}
	if(TTF_Init()==-1) {
		printf("TTF_Init: %s\n", TTF_GetError());
		exit(2);
	}

	//Read board dimension
  read(sock_fd, &dim, sizeof(dim));

	//Create the board window that we'll be using - UI_library.c
  create_board_window(300, 300,  dim);

	//Read number of events list
	read(sock_fd, &nrOfEvents, sizeof(nrOfEvents));

	//Read and print the events from the list to the board
	printListToBoard(sock_fd, nrOfEvents);

	//Set quit and end to zero
	quit=0;
	end=0;

	//Read all server replies till game is over
	while(!quit)
	{
		while(!end)
		{
			//Read either a gameOn message or a serverReply
			if(gameOn == 0)
			{
				while(pending > 0){
          test = read(sock_fd, reply, sizeof(*reply));
					paint_card(reply->resp.play1[0], reply->resp.play1[1] , 255, 255, 255);
  				if(test==0 || test==-1)
  				{
  					end = 1;
  					quit = 1;
  					break;
  				}
          pending--;
					printf("Pending message detected.\n");
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
				color = reply->color;
				resp = reply->resp;

				//Process reply acquired
				if(reply->gameOn == 0) {
					gameOn = 0;
				}

				switch (resp.code) {
					case 0:
						paint_card(resp.play1[0], resp.play1[1] , 255, 255, 255);
						pending--;
						break;
				  case 1:
				    paint_card(resp.play1[0], resp.play1[1] , color.r, color.b, color.g);
				    write_card(resp.play1[0], resp.play1[1], resp.str_play1, 200, 200, 200);
						pending++;
				    break;
				  case 3:
				    end = 1;
				  case 2:
				    paint_card(resp.play1[0], resp.play1[1] , color.r, color.b, color.g);
				    write_card(resp.play1[0], resp.play1[1], resp.str_play1, 0, 0, 0);
				    paint_card(resp.play2[0], resp.play2[1] , color.r, color.b, color.g);
				    write_card(resp.play2[0], resp.play2[1], resp.str_play2, 0, 0, 0);
						pending--;
				    break;
				  case -2:
				    paint_card(resp.play1[0], resp.play1[1] , color.r, color.b, color.g);
				    write_card(resp.play1[0], resp.play1[1], resp.str_play1, 255, 0, 0);
				    paint_card(resp.play2[0], resp.play2[1] , color.r, color.b, color.g);
				    write_card(resp.play2[0], resp.play2[1], resp.str_play2, 255, 0, 0);
						pending--;
				    sleep(2);
				    paint_card(resp.play1[0], resp.play1[1] , 255, 255, 255);
				    paint_card(resp.play2[0], resp.play2[1] , 255, 255, 255);
				    break;
					case -1:
						paint_card(resp.play1[0], resp.play1[1] , 255, 255, 255);
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
			printf("Restart game error: ncorrect reply from server\n");
			quit = 1;
			close_board_windows();
			break;
		}
		printf("Received restart game message from server.\n");

		//--- New Game ---
		//New board window
		pending = 0;
		newBoardWindow();
		printf("Created new board window\n");

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
	playerPick* pick = (playerPick*) malloc(sizeof(pick));
  SDL_Event event;
	int done = 0;

	//Gameplay Loop
	while (!done)
	{
		//Get click card event
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				//Case of exit click
				case SDL_QUIT: {
					done = SDL_TRUE;
					close_board_windows();
					break;
				}

				//Case of card click
				case SDL_MOUSEBUTTONDOWN:
				{
					if(gameOn==1 && quit==0 && end==0)
					{
						//Converts event card click coordinates to the corresponding board card location
						get_board_card(event.button.x, event.button.y, &pick->x, &pick->y);
						printf("Click (%d %d): (%d %d)\n", event.button.x, event.button.y, pick->x, pick->y);
    				write(sock_fd, pick, sizeof(*pick));
					}
    		}
    	}
    }
	}
	free(pick);
	close_board_windows();
	//Exit
	pthread_exit(NULL);
}

int main(int argc, char * argv[]){
  struct sockaddr_in server_addr;

  //Read server address input
  if (argc <2){
    printf("Second argument should be server address.\n");
    exit(-1);
  }

  //Create socket
  sock_fd= socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd == -1)
  {
    perror("Socket: ");
    exit(-1);
  }
  server_addr.sin_family = AF_INET;
  server_addr.sin_port= htons(3000);
  inet_aton(argv[1], &server_addr.sin_addr);

  //Connect to socket
  if(-1 == connect(sock_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr)))
  {
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
		printf("Server error: Game not active\n");
		exit(-1);
	}

	//Launch threads
  pthread_create(&tid[0], NULL, thread_readServer , NULL);
	sleep(1);
  pthread_create(&tid[1], NULL, thread_play, NULL);

  pthread_join(tid[1], NULL);
  close(sock_fd);
}
