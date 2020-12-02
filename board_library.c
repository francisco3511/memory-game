#include <stdlib.h>
#include "board_library.h"
#include <stdio.h>
#include <string.h>

int dim_board;
board_place * board;
int n_corrects;
int * card_up;

int linear_conv(int i, int j){
  return j*dim_board+i;
}
char * get_board_place_str(int i, int j){
  return board[linear_conv(i, j)].v;
}

void init_board(int dim){
  int count  = 0;
  int i, j;
  char * str_place;

  dim_board = dim;
  card_up = (int *)malloc(sizeof(int) * dim * dim);

  n_corrects = 0;
  board = malloc(sizeof(board_place)* dim *dim);

  for( i=0; i < (dim_board*dim_board); i++){
    board[i].v[0] = '\0';
    card_up[i] = 0;
  }

	char c1, c2;
  for (c1 = 'a' ; c1 < ('a'+dim_board); c1++){
    for (c2 = 'a' ; c2 < ('a'+dim_board); c2++){
      do{
        i = random()% dim_board;
        j = random()% dim_board;
        str_place = get_board_place_str(i, j);
        printf("%d %d -%s-\n", i, j, str_place);
      } while(str_place[0] != '\0');
      str_place[0] = c1;
      str_place[1] = c2;
      str_place[2] = '\0';
      do{
        i = random()% dim_board;
        j = random()% dim_board;
        str_place = get_board_place_str(i, j);
        printf("%d %d -%s-\n", i, j, str_place);
      }while(str_place[0] != '\0');
      str_place[0] = c1;
      str_place[1] = c2;
      str_place[2] = '\0';
      count += 2;
      if (count == dim_board*dim_board)
        return;
    }
  }
}

play_response board_play(int x, int y, int* play1)
{
  play_response resp;
  resp.code =10;
  if(card_up[linear_conv(x,y)] == 1)
	{
		//If it's the second play that is locked then we need to store the first play
		//on the resp structure
		if(play1[0]!=-1)
		{
			resp.code = 0;
			resp.play1[0]= play1[0];
      resp.play1[1]= play1[1];
      printf("Filled card: on second play.\n");
		}
		//Else, we clicked on a locked card in the first play
		else{
			resp.code = -10;
      printf("Filled card: on first play.\n");
		}
  }
	else{
    if(play1[0]== -1){
        printf("First play.\n");
        resp.code = 1; // First play made

        play1[0]=x;
        play1[1]=y;
        resp.play1[0]= play1[0];
        resp.play1[1]= play1[1];
        strcpy(resp.str_play1, get_board_place_str(x, y));
        card_up[linear_conv(x,y)] = 1; // Flip first card play up
      }
		else{
        char * first_str = get_board_place_str(play1[0], play1[1]);
        char * secnd_str = get_board_place_str(x, y);

        resp.play1[0]= play1[0];
        resp.play1[1]= play1[1];
        strcpy(resp.str_play1, first_str);
        resp.play2[0]= x;
        resp.play2[1]= y;
        strcpy(resp.str_play2, secnd_str);

        if (strcmp(first_str, secnd_str) == 0)
				{
          printf("Correct play.\n");

          strcpy(first_str, "");
          strcpy(secnd_str, "");

          // Set both revealed cards up
          card_up[linear_conv(play1[0], play1[1])] = 1;
          card_up[linear_conv(x,y)] = 1;

          n_corrects+=2;
          if (n_corrects == dim_board* dim_board)
              resp.code=3;
          else
            resp.code=2;
        }
				else
				{
          printf("Incorrect play.\n");
          card_up[linear_conv(x, y)] = 1; // Set card revealed up (to be turned down 2secs later)
          resp.code = -2;
        }
        play1[0]= -1;
      }
    }
  return resp;
}
