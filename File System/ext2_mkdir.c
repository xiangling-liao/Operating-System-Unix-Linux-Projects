#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "ext2.h"


unsigned char *disk;
char dirName[EXT2_NAME_LEN+1]="\0";
int find_dir_inode(char *path, char *inodes);
int find_next_dir(char *path, char *dirName);
int* get_inode_bitmap(void* inode_bitmap_block);
int* get_block_bitmap(void* block_bitmap_block);
void set_inode_bitmap(void *inode_bitmap_block, int inode_num);
void set_block_bitmap(void *block_bitmap_block, int block_num);

int main(int argc, char **argv) {

char parentDir[EXT2_NAME_LEN+1];//absolute path on the disk
char dirPath[EXT2_NAME_LEN+1];
char dirName_new[EXT2_NAME_LEN+1];
char *dp;
struct ext2_super_block *sb;
struct ext2_group_desc *group_desc;
struct ext2_dir_entry_2 *entry;
struct ext2_dir_entry_2 *new_dir_entry;
void* inodes;
void* inode_bitmap_block;
void* block_bitmap_block;
int* inode_bitmap;
int* block_bitmap;
int image_fd;
struct ext2_inode* dir_inode;


if(argc != 3) {
        fprintf(stderr, "Usage: %s <image file name> <absolute path on the disk>\n",argv[0]);
        exit(1);
    }
if(argv[2][0] != '/') {
        fprintf(stderr, "Please type in absolute path.\n");
        exit(1);
    }


image_fd = open(argv[1], O_RDWR);

disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, image_fd, 0);
if(disk == MAP_FAILED) {
	perror("mmap");
	exit(1);
}


    sb = (struct ext2_super_block *)(disk + 1024);
    group_desc=(struct ext2_group_desc *)(disk+1024+EXT2_BLOCK_SIZE);
    inodes = disk + EXT2_BLOCK_SIZE *(group_desc->bg_inode_table);
    inode_bitmap_block = disk + EXT2_BLOCK_SIZE * group_desc->bg_inode_bitmap;//loc
    block_bitmap_block = disk + EXT2_BLOCK_SIZE * group_desc->bg_block_bitmap;//loc

strcpy(dirPath,argv[2]);//dir where to put the new dir

if(strcmp(dirPath,"/")){
    if(dirPath[strlen(dirPath)-1]=='/'){
        dirPath[strlen(dirPath)-1]='\0';
        strcpy(argv[2],dirPath);
    }
}

dp=strrchr(argv[2],'/');
dp++;
strcpy(dirName_new,dp);
dp--;
if(dp==strchr(argv[2],'/'))
{    
dp++;
}
*dp='\0';
strcpy(parentDir,argv[2]);

int parent_dir_inode=EXT2_ROOT_INO;
if(strcmp(parentDir,"/")){
    parent_dir_inode=find_dir_inode(parentDir,inodes);//by default =2
   if(parent_dir_inode==-1){
   	fprintf(stderr, "Specified path not found\n"); //not found
    return ENOENT;
   }
}
//check if the new-made dir name has exist in the parent dir
int dir_inode_num=-1;
dir_inode_num=find_dir_inode(dirPath,inodes);
if(dir_inode_num!=-1){
	  fprintf(stderr, "Directory exist!\n"); //not found
    return EEXIST;
   }

//get the inode bitmap & block bitmap
inode_bitmap=get_inode_bitmap(inode_bitmap_block);

block_bitmap=get_block_bitmap(block_bitmap_block);
int i;
   //assign indoe to new dir
for(i=0;i<INODE_NUM;i++)
{
if(inode_bitmap[i]==0)
{
	dir_inode_num=i+1;
	 sb->s_free_inodes_count-=1;
	 group_desc->bg_free_inodes_count-=1;
	 //set up inode bitmap
  set_inode_bitmap(inode_bitmap_block,i);
	break;
}
}
  if (dir_inode_num == -1) {
    fprintf(stderr, "No inode available.\n");
    exit(1);
  }
 
     

     //set up new dir inode
   dir_inode = (struct ext2_inode*) (inodes + (dir_inode_num - 1) * sizeof(struct ext2_inode));
   dir_inode->i_mode=EXT2_S_IFDIR;
   dir_inode->i_size=EXT2_BLOCK_SIZE;
   dir_inode->i_links_count=2;//[this dir and '.']
   dir_inode->i_blocks=2;
   dir_inode->i_dtime=0;
   //assign block to new dir's inode->i_block[0]
   block_bitmap=get_block_bitmap(block_bitmap_block);
   for(i=0;i<BLOCK_NUM;i++){
          if (block_bitmap[i] == 0) {//cuz inode_bitmap[i]->inode i+1
          dir_inode->i_block[0] = i+1;
          set_block_bitmap(block_bitmap_block,i); 
          sb->s_free_blocks_count-=1;
          group_desc->bg_free_blocks_count-=1;
          break;
          }
     }
//add '.' & '..' to new dir entry【note:link count will increase accordingly】
     int dir_block_idx=dir_inode->i_block[0];
     new_dir_entry=(void* )disk + EXT2_BLOCK_SIZE * dir_block_idx;
     new_dir_entry->inode=dir_inode_num;
     new_dir_entry->name_len=strlen(".");
     new_dir_entry->file_type=EXT2_FT_DIR;
     strcpy(new_dir_entry->name,".");
     new_dir_entry->rec_len=EXT2_DIR_REC_LEN(strlen("."));   

     new_dir_entry=(void* )new_dir_entry+new_dir_entry->rec_len;
     new_dir_entry->inode=parent_dir_inode;
     new_dir_entry->name_len=strlen("..");
     new_dir_entry->file_type=EXT2_FT_DIR;
     strcpy(new_dir_entry->name,"..");
     new_dir_entry->rec_len=EXT2_BLOCK_SIZE-EXT2_DIR_REC_LEN(strlen(".."));  

//add new dir entry to its parent dir inode block[note:to keep inode->i_size multiple times of EXT2_BLOCK_SIZE]
   int dir_rec_len=EXT2_DIR_REC_LEN(strlen(dirName_new));
   int found=0;
   int parent_blocks_num;
   int dir_num;
  struct ext2_inode* parent_dir=(struct ext2_inode*) (inodes + (parent_dir_inode - 1) * sizeof(struct ext2_inode));

   parent_blocks_num= parent_dir->i_size/EXT2_BLOCK_SIZE;//how many blocks the parent dir inode takes
   int size=0;
   //to check if we can find somewhere in current inode block
  	for (i = 0; i <parent_blocks_num; i++) {
    dir_num=parent_dir->i_block[i];
    entry=(void* )disk + EXT2_BLOCK_SIZE * dir_num;
    while(size < EXT2_BLOCK_SIZE) {
        size += entry->rec_len;     
      if ( size % EXT2_BLOCK_SIZE == 0 && entry->rec_len>= EXT2_DIR_REC_LEN(strlen(dirName_new))+EXT2_DIR_REC_LEN(entry->name_len)) { // need to use next pointer  ???如果当前 block 不能放下下一个 entry
       dir_rec_len=entry->rec_len-EXT2_DIR_REC_LEN(entry->name_len);
       entry->rec_len=EXT2_DIR_REC_LEN(entry->name_len);
       entry=(void*)entry+EXT2_DIR_REC_LEN(entry->name_len);
       found=1; 
        break;
      }
      entry = (void*)entry + entry->rec_len;  //why we mush change entry to a (void*) first
      }
      size=0;
      if(found)
      {
        break;
      }
  
}
//if cannot find, assign a new block to dir inode to store file entry


int block_num;
if(!found){
	block_bitmap=get_block_bitmap(block_bitmap_block);
    for(i=0;i<BLOCK_NUM;i++){
          if (block_bitmap[i] == 0) {//cuz inode_bitmap[i]->inode i+1
          parent_dir->i_block[parent_blocks_num] = i+1;
          block_num=parent_dir->i_block[parent_blocks_num];
          set_block_bitmap(block_bitmap_block,i); 
          sb->s_free_blocks_count-=1;
          group_desc->bg_free_blocks_count-=1;
          break;
          }
     }
    if(parent_dir->i_block[parent_blocks_num]==-1)
     {
      fprintf(stderr,"No available inode for dir.\n");
       exit(1);
     }
     dir_rec_len=EXT2_BLOCK_SIZE;
     entry = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
     entry->inode=dir_inode_num;
     entry->name_len=strlen(dirName_new);
     entry->file_type=EXT2_FT_DIR;
     strcpy(entry->name,dirName_new);
     entry->rec_len=dir_rec_len;    
     parent_dir->i_blocks=parent_dir->i_blocks+2;   
     parent_dir->i_size+=EXT2_BLOCK_SIZE;
}else{
  	 entry->inode=dir_inode_num;
     entry->name_len=strlen(dirName_new);
     entry->file_type=EXT2_FT_DIR;
     strcpy(entry->name,dirName_new);
     entry->rec_len=dir_rec_len;  
     }
parent_dir->i_links_count+=1;//[note:dir link count=2+# of subdir]
close(image_fd); 
    return 0;
}


	int find_dir_inode(char *path, char *inodes)
{
    int inode_num=EXT2_ROOT_INO;//2
    int block_num;
    int new_inode_num;
    struct ext2_inode *inode;
    struct ext2_dir_entry_2 *entry;
    int size,total_size;
   while(1)
   {
   new_inode_num = -1;
   inode = (struct ext2_inode*) (inodes + (inode_num - 1) * sizeof(struct ext2_inode));
   if(!strcmp(path,"\0")){
    return inode_num;
   }
   int next=find_next_dir(path,dirName);
   path+=next;//points to next dir

   block_num=inode->i_block[0];
   entry = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
/* read block from disk*/
   size = 0;                                            /* keep track of the bytes read */
   total_size = inode->i_size;
   while(size < total_size) {
   	 size += entry->rec_len;
        char file_name[EXT2_NAME_LEN+1];
        memcpy(file_name, entry->name, entry->name_len);
        file_name[entry->name_len] = '\0';              /* append null char to the file name */
       
        
        if (!strcmp(file_name,dirName)) {
          if(entry->file_type==EXT2_FT_REG_FILE||entry->file_type==EXT2_FT_SYMLINK)
          { 
          fprintf(stderr,"Specified path is not a directory\n");//if is a file or a link
          exit(1);
          }
          new_inode_num = entry->inode;
          break;
        }
      
       if (size%EXT2_BLOCK_SIZE==0) { // need to use next pointer  ???如果当前 block 不能放下下一个 entry
       
        block_num++;
        entry = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
       }else{
        entry = (void*) entry + entry->rec_len;      /* move to the next entry */
       
       }
   }
      if (new_inode_num == -1) {
      return -1;
       } else {
      inode_num = new_inode_num;
      }
 }
}


int find_next_dir(char *path, char *dirName)
{
    if (path[0] != '/') {
    return -1;
  }
  int i, j;
  for (i = 1, j = 0; path[i] != '/'; i++, j++) {
    if (path[i] == '\0') {
      break;
    }
    dirName[j] = path[i];
  }
  dirName[j] = '\0';//next dir name
  return i; //next location where path points to 
}


int* get_inode_bitmap(void *inode_bitmap_block)
{
  int i,byte_num,bit_num;
  int *inode_bitmap=malloc(sizeof(int)*INODE_NUM);
  //inode starts from 1,inode_bitmap[0]->inode 1
  for(i=0;i<INODE_NUM;i++)
  {
byte_num=i/8;
bit_num=i%8;
int *byte=inode_bitmap_block+byte_num;
inode_bitmap[i]=(*byte>>bit_num)&1;
}
return inode_bitmap;
}



int* get_block_bitmap(void* block_bitmap_block)
{
  int i,byte_num,bit_num;
  int *block_bitmap=malloc(sizeof(int)*BLOCK_NUM);
  //block starts from 1,block_bitmap[0]->block 1
  for(i=0;i<BLOCK_NUM;i++)
  {
byte_num=i/8;
bit_num=i%8;
int *byte=block_bitmap_block+byte_num;
block_bitmap[i]=(*byte>>bit_num)&1;
}
return block_bitmap;
}

void set_inode_bitmap(void *inode_bitmap_block, int inode_num) {
  int byte_num,bit_num;
  int *bitmap;
  byte_num=inode_num/8;
  bit_num=inode_num%8;
  bitmap=inode_bitmap_block+byte_num;
  *bitmap= *bitmap|(1<<bit_num);
}

void set_block_bitmap(void *block_bitmap_block, int block_num) {
  int byte_num,bit_num;
  int *bitmap;
  byte_num=block_num/8;
  bit_num=block_num%8;
  bitmap=block_bitmap_block+byte_num;
  *bitmap= *bitmap|(1<<bit_num);
}