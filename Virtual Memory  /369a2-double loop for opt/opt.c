#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"
#include "sim.h"

extern unsigned memsize;

extern int debug;

char *tracefile;

pgdir_entry_t pgdir[PTRS_PER_PGDIR];
tracenode_t *current_trace;

/* Page to evict is chosen using the optimal (aka MIN) algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {
	int max_distance=-1;
	int evict_frame_num=-1;
	tracenode_t *current=curr;
	int j;
	int i;
	//initialize the distance for each frame to 0
	for(j=0;j<memsize;j++){
	coremap[j].distance=0;
	}
	//find distance for each frame in memory
	for(j=0;j<memsize;j++){
		current=curr->next;
		while(current!=NULL){
			if(current->vaddr!=coremap[j].vaddr>>PAGE_SHIFT)//first time detected;if no long exist in the tracefile in the future, distance stay at 0
		{
			coremap[j].distance++;
		}else{
			break;
		}
		if(coremap[j].distance==0) {
		return j;}
		current=current->next;
	}
	}

	// if every page is used in the future, pick the one with the largest distance to evict
	for(i=0;i<memsize;i++){
		if(coremap[i].distance>max_distance){
			max_distance=coremap[i].distance;
            evict_frame_num=i;
		}
	} 
	
	return evict_frame_num;	
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref(pgtbl_entry_t *p) {
	//move the head for tracefile to next
	curr=curr->next;
	return;
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init(){
	char buf[MAXLINE];
	FILE *tfp=stdin;
    addr_t vaddr;  
    char type;
   //read tracefile and save each vaddr into a linked list
	if(tracefile != NULL) {
		if((tfp = fopen(tracefile, "r")) == NULL) {
			perror("Error opening tracefile:");
			exit(1);
		}
	}
	
	while(fgets(buf, MAXLINE, tfp) != NULL) {
		if(buf[0] != '=') {		
		sscanf(buf, "%c %lx",&type,&vaddr);
		if(tracelist_head==NULL){
			tracelist_head = malloc(sizeof(tracenode_t));
			tracelist_head->vaddr=vaddr>>PAGE_SHIFT;
			tracelist_head->next=NULL;
			current_trace=tracelist_head;
		} else{
		tracenode_t *temp=NULL;
		temp=malloc(sizeof(tracenode_t));
		temp->next=NULL;
		temp->vaddr=vaddr>>PAGE_SHIFT;
		current_trace->next=temp;
		current_trace=temp;
		}
		} else {
			continue;
		}
	}
	curr=tracelist_head;

	return;
}