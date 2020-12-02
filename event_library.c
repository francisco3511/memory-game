#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "board_library.h"
#include "event_library.h"

int nrOfEvents;
node* head;

void addNodeToList(serverReply* reply)
{
	//Create the new node to be added to the list
	node* newNode = (node*) malloc(sizeof(node));
	newNode->reply = *reply;
	newNode->next = NULL;

	//If the list is empty, the head will point to this node
	if(head==NULL)
		head=newNode;

	//Else, the list is not empty
	else{
		//Start the cursor at the head of the list
		node* cursor = head;

		//Go to the last element
		while(cursor->next!=NULL)
	   cursor = cursor->next;

		//Add the newNode at the end of the list
		cursor->next = newNode;
	}

	//Increment nrOfEvents
	++nrOfEvents;
	printf("nrOfEvents = %d\n",nrOfEvents);
}

void removeNode(node* cursor, node* former)
{
  //if we want to remove the first element of the list
	if(cursor==head){
		//if there is more than one element in the list, update head
		if(head->next != NULL)
			head=cursor->next;
		  //Else the element to be removed was the only element in the list
		else
		  head=NULL;
	}
	//else it's not the first element of the list
	else{
		//make adequate connections
		former->next=cursor->next;
	}

	//Always free the node removed at the end
	free(cursor);
}

void removeFromList(serverReply reply)
{
	node* cursor = head;
	node* former;

	//Check if list is empty
	if(head==NULL)
		return;

	//Go through list
	while(cursor!=NULL)
  {
		//If code is either 2, -2 or 1 (in timeout thread) then remove 1st play from the list
		if(cursor->reply.resp.code==1 &&
				cursor->reply.index==reply.index &&
					cursor->reply.resp.play1[0] == reply.resp.play1[0] &&
						cursor->reply.resp.play1[1] == reply.resp.play1[1])
		{
			//Remove it
			removeNode(cursor, former);
			//Decrement nrOfEvents
			--nrOfEvents;
			printf("nrOfEvents = %d\n",nrOfEvents);
		}

		//If code is -2, then do additionally:
		if(reply.resp.code==-2)
		{
			//Find the reply from the list
			if(cursor->reply.resp.code==-2 &&
					cursor->reply.index==reply.index &&
						cursor->reply.resp.play2[0] == reply.resp.play2[0] &&
							cursor->reply.resp.play2[1] == reply.resp.play2[1])
			{
				//Remove it
				removeNode(cursor, former);
				//Decrement nrOfEvents
				--nrOfEvents;
				printf("Number Of Events = %d\n",nrOfEvents);
			}
		}

		//Update pointers cursor and former
		former = cursor;
		cursor = cursor->next;
	}
}

void deleteList()
{
	//Check if list is empty
	if(head==NULL)
		return;

	//Go through list
	node* cursor = head;
	node* aux;
	while(cursor!=NULL)
  {
			aux=cursor;
			cursor=cursor->next;
			free(aux);
	}

	head = NULL;
}
