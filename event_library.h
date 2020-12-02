#include <stdlib.h>

typedef struct playerPick{
  int x;
  int y;
} playerPick;

typedef struct colorPlayer{
  int r;
  int b;
  int g;
} colorPlayer;

typedef struct serverReply{
	int gameOn;
	int index;
  struct colorPlayer color;
  struct play_response resp;
} serverReply;

typedef struct node{
	struct serverReply reply;
	struct node* next;
}node;

extern int nrOfEvents;
extern node* head;

void addNodeToList(serverReply* reply);
void removeNode(node* cursor, node* former);
void removeFromList(serverReply reply);
void deleteList();
