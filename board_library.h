#include <stdlib.h>

typedef struct board_place{
  char v[3];
} board_place;

typedef struct play_response{
  int code; // 0 - filled on the second play
			      //-10 - filled on the first play
            // 1 - 1st play
            // 2 2nd - same plays
            // 3 END
            // -2 2nd - different
		    // -1 - 5 secs have passed after the 1st play with no picks
  int play1[2];
  int play2[2];
  char str_play1[3], str_play2[3];
} play_response;

extern int * card_up;
extern board_place * board;
extern int dim_board;

char * get_board_place_str(int i, int j);
void init_board(int dim);
play_response board_play (int x, int y, int* play1);
