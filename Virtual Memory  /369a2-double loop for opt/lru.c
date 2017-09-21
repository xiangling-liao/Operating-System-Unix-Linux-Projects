#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;
extern node_t *referlist_head;

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {
	//evict the least used one
	node_t *current=referlist_head;
	node_t *prevN=NULL;
	while(current->next!=NULL){
		prevN=current;
		current=current->next;
	}
	prevN->next=NULL;
    int evictFrame=(current->pte->frame)>> PAGE_SHIFT;
    free(current);
    current=NULL;
	return evictFrame;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {
    if(referlist_head==NULL){
	referlist_head=malloc(sizeof(node_t));
	referlist_head->pte=p;
	referlist_head->next=NULL;
	return;
    }else{
    node_t *temp=NULL;
    node_t *prevN=NULL;
    node_t *nextN=NULL;
    node_t *current=referlist_head;
//if the page is in the head, just return
    if(referlist_head->pte->frame==p->frame)
   {
	return;
   }
//else,check if the page is already in the memory
    do{
       if(current->pte->frame !=p->frame){
	   prevN=current;
	   current=current->next;
	   continue;
    }else{
	//if find p's frame
		//at least two element
    nextN=current->next;
    prevN->next=nextN;
    current->next=referlist_head;
    referlist_head=current;
	return;

    }
   }while(current!=NULL);

//if p doesn't exist
    temp=malloc(sizeof(node_t));
    temp->pte=p;
    temp->next=referlist_head;
    referlist_head=temp;
	return;
}
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {
	return;
}
