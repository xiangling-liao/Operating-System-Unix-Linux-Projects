#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;
/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int clock_evict() {
	//referlist_head points to the oldest page
	node_t *prevN=NULL;
	node_t *nextN=NULL;
	node_t *current=referlist_head;
	node_t *temp=NULL;
	
	int evictFrame;
	//to check if the page is referred recently, if no, evict it
	while(current->pte->frame&PG_REF){
		current->pte->frame=(current->pte->frame)^ PG_REF;
		prevN=current;
	    current=current->next;
	    nextN=current->next;
	}

	//free the space for evicted node
	if(current==referlist_head){
		while(current->next!=referlist_head){
			current=current->next;
		}
	 evictFrame=referlist_head->pte->frame;
	 current->next=referlist_head->next;
	  temp=referlist_head;  //store the pointer to the original list head to free later
	  referlist_head=referlist_head->next;
	  free(temp);
	  temp=NULL;
	  return evictFrame>> PAGE_SHIFT;
	}else{
	prevN->next=nextN;
	referlist_head=nextN;
    evictFrame=current->pte->frame;
    free(current);
    current=NULL;
     
	return evictFrame>> PAGE_SHIFT;
	}


}

/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {

    if(referlist_head==NULL){
	referlist_head=malloc(sizeof(node_t));
	referlist_head->pte=p;
	referlist_head->next=referlist_head;
	return;
    }else{
    node_t *current=referlist_head;
    node_t *prevN=NULL;
	do{
		if(current->pte->frame !=p->frame){    //if the page not found, continue
			prevN=current;
	        current=current->next;
	        continue;
        }else{
	     (current->pte)->frame=(current->pte)->frame | PG_REF;   //if the page found, set ref bit=1, and return
	     return;   //if we need to set ref bit???
        }
	}while(current!=referlist_head);

    node_t *temp=NULL;	    //else add the new page node
    temp=malloc(sizeof(node_t));
    temp->pte=p;
    temp->next=referlist_head;
    prevN->next=temp;
    return;

}	
}

/* Initialize any data structures needed for this replacement
 * algorithm. 
 */
void clock_init() {
	//printf("hello");
	return;
}
