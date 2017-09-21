#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;


/* Page to evict is chosen using the fifo algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int fifo_evict() {
	//evict the oldest page by find the end of the linked list
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
 * needed by the fifo algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void fifo_ref(pgtbl_entry_t *p) {
	//if the linked list is null
    if(referlist_head==NULL){
	referlist_head=malloc(sizeof(node_t));
	referlist_head->pte=p;
	referlist_head->next=NULL;
	return;
    }else{
	//else put the new page in the head
    node_t *temp=NULL;
    node_t *current=referlist_head;
    while(current!=NULL){
    if(current->pte->frame !=p->frame){
	current=current->next;
	continue;
    }else{
	return;
    }
    }
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
void fifo_init() {
	return;
}
