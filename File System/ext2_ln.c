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

int image_fd;
struct ext2_super_block *sb;
struct ext2_group_desc *group_desc;
struct ext2_dir_entry_2 *entry_parent_tar;
struct ext2_dir_entry_2 *entry_parent_src;
void* inodes;
void* inode_bitmap_block;
void* block_bitmap_block;
int* inode_bitmap;
int* block_bitmap;
char parentDir_tar[EXT2_NAME_LEN+1];//absolute path of target file on the disk
char dirPath_tar[EXT2_NAME_LEN+1];
char dirName_tar[EXT2_NAME_LEN+1];
char *dp_tar;
char parentDir_src[EXT2_NAME_LEN+1];//absolute path of target file on the disk
char dirPath_src[EXT2_NAME_LEN+1];
char dirName_src[EXT2_NAME_LEN+1];
char *dp_src;
int sflag=0;
int i;



if(argc < 4) {
        fprintf(stderr, "Usage: %s <image file name> <source file absolute path> <target file absolute path>\n",argv[0]);
        exit(1);
    }
if(argv[argc-1][0] != '/'||argv[argc-2][0] != '/') {
        fprintf(stderr, "Please type in absolute path.\n");
        exit(1);
    }
 if(!strcmp(argv[argc-1],"/") ||!strcmp(argv[argc-2],"/")){
        fprintf(stderr, "Cannot link directory\n");
        exit(1);
    }
//-s option
 if(argc==5&&!strcmp(argv[2],"-s")){
        sflag=1;
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

//extract target file
strcpy(dirPath_tar,argv[argc-1]);//dir where to put the new dir

if(strcmp(dirPath_tar,"/")){
    if(dirPath_tar[strlen(dirPath_tar)-1]=='/'){
        dirPath_tar[strlen(dirPath_tar)-1]='\0';
    }
}
strcpy(argv[argc-1],dirPath_tar);
dp_tar=strrchr(argv[argc-1],'/');
dp_tar++;
strcpy(dirName_tar,dp_tar);
dp_tar--;
if(dp_tar==strchr(argv[argc-1],'/'))
{    
dp_tar++;
}
*dp_tar='\0';
strcpy(parentDir_tar,argv[argc-1]);

//extract source file

strcpy(dirPath_src,argv[argc-2]);//dir where to put the new dir

if(strcmp(dirPath_src,"/")){
    if(dirPath_src[strlen(dirPath_src)-1]=='/'){
        dirPath_src[strlen(dirPath_src)-1]='\0';
        strcpy(argv[argc-2],dirPath_src);
}
}

dp_src=strrchr(argv[argc-2],'/');
dp_src++;
strcpy(dirName_src,dp_src);
dp_src--;
if(dp_src==strchr(argv[argc-2],'/'))
{    
dp_src++;
}
*dp_src='\0';
strcpy(parentDir_src,argv[argc-2]);


//check if the given path exist
int parent_dir_inode_tar=EXT2_ROOT_INO;
if(strcmp(parentDir_tar,"/")){
parent_dir_inode_tar=find_dir_inode(parentDir_tar,inodes);//by default =2
   if(parent_dir_inode_tar==-1){
   	fprintf(stderr, "Specified target path not found\n"); //not found
    return ENOENT;
   }
}

int parent_dir_inode_src=EXT2_ROOT_INO;
if(strcmp(parentDir_src,"/")){
parent_dir_inode_src=find_dir_inode(parentDir_src,inodes);//by default =2
   if(parent_dir_inode_src==-1){
   	fprintf(stderr, "Specified src path not found\n"); //not found
    return ENOENT;
   }
}

//get the src file inode
int file_inode_src=-1;
struct ext2_inode* parent_dir_block_src = (struct ext2_inode*) (inodes + (parent_dir_inode_src - 1) * sizeof(struct ext2_inode));
int size=0;
int total_size = parent_dir_block_src->i_size;
int block_num_src=parent_dir_block_src->i_block[0];
entry_parent_src = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num_src);
 /* keep track of the bytes read */
/* first entry in the directory */
int dir_src=0;//dir block index
   while(size < total_size) {
        size += entry_parent_src->rec_len;
        char file_name[EXT2_NAME_LEN+1];
        memcpy(file_name, entry_parent_src->name, entry_parent_src->name_len);
        file_name[entry_parent_src->name_len] = '\0';              /* append null char to the file name */
         if(!strcmp(file_name,dirName_src)){
         	if(entry_parent_src->file_type==EXT2_FT_DIR)
         	{
         		fprintf(stderr,"Source is a directory rather than a file\n");
         		return EISDIR;
         	}
          file_inode_src=entry_parent_src->inode;
         	break;
         }
      if (size%EXT2_BLOCK_SIZE==0) { // need to use next pointer  ???如果当前 block 不能放下下一个 entry    
        dir_src++;
        block_num_src=parent_dir_block_src->i_block[dir_src];
        entry_parent_src = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num_src);
      }else{
         entry_parent_src = (void*) entry_parent_src + entry_parent_src->rec_len; 
      }
  }

if(file_inode_src==-1){
	fprintf(stderr,"Source file does not exist.\n");
         	return ENOENT;
}

struct ext2_inode* file_inode_src_block=(struct ext2_inode*) (inodes + (file_inode_src - 1) * sizeof(struct ext2_inode));
if(sflag==0){
file_inode_src_block->i_links_count+=1;
}


   //traverse the parent_dir_inode_tar list, to check if the dirName_tar has been taken in current dir[pathName]
struct ext2_inode* parent_dir_block_tar = (struct ext2_inode*) (inodes + (parent_dir_inode_tar - 1) * sizeof(struct ext2_inode));
 size=0;
 total_size = parent_dir_block_tar->i_size;
int block_num_tar=parent_dir_block_tar->i_block[0];
entry_parent_tar = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num_tar);
 
int dir_tar=0;//dir block index
   while(size < total_size) {
        size += entry_parent_tar->rec_len;
        char file_name[EXT2_NAME_LEN+1];
        memcpy(file_name, entry_parent_tar->name, entry_parent_tar->name_len);
        file_name[entry_parent_tar->name_len] = '\0';              /* append null char to the file name */
         if(!strcmp(file_name,dirName_tar)){
         	if(entry_parent_tar->file_type==EXT2_FT_DIR)
         	{
         		fprintf(stderr,"Target is a directory rather than a file\n");
         		return EISDIR;
         	}
         	fprintf(stderr,"File copy failed! File name has been used.\n");
         	return (EEXIST);
         }
      if (size%EXT2_BLOCK_SIZE==0) { 
        dir_tar++;
        block_num_tar=parent_dir_block_tar->i_block[dir_tar];
        entry_parent_tar = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num_tar);
      }else{
         entry_parent_tar = (void*) entry_parent_tar + entry_parent_tar->rec_len; 
      }
  }
//symbolic link:assign node to target file
inode_bitmap=get_inode_bitmap(inode_bitmap_block);
if(sflag==1)
{
	 file_inode_src=-1;
     for(i=0;i<INODE_NUM;i++)
   {
     if(inode_bitmap[i]==0)
     {
     file_inode_src=i+1;
	 sb->s_free_inodes_count-=1;
	 group_desc->bg_free_inodes_count-=1;
	 //set up inode bitmap
     set_inode_bitmap(inode_bitmap_block,i);
	break;
    }
  }
  if (file_inode_src == -1) {
    fprintf(stderr, "No inode available.\n");
    exit(1);
  }

//set up inode talbe,i.e set up new dir inode
   struct ext2_inode*  tar_sym_inode_table = (struct ext2_inode*) (inodes + (file_inode_src - 1) * sizeof(struct ext2_inode));
   tar_sym_inode_table->i_mode=EXT2_S_IFLNK;
   tar_sym_inode_table->i_size=sizeof(dirPath_src);
   tar_sym_inode_table->i_links_count=1;//[this dir and '.']
   tar_sym_inode_table->i_blocks=2;
   tar_sym_inode_table->i_dtime=0;
   //assign block to new dir's inode->i_block[0]
   block_bitmap=get_block_bitmap(block_bitmap_block);
   for(i=0;i<BLOCK_NUM;i++){
          if (block_bitmap[i] == 0) {//cuz inode_bitmap[i]->inode i+1
          tar_sym_inode_table->i_block[0] = i+1;
          set_block_bitmap(block_bitmap_block,i); 
          sb->s_free_blocks_count-=1;
          group_desc->bg_free_blocks_count-=1;
          break;
          }
     }

 //set up tar file block's content
     char* tfile=(void*)disk + EXT2_BLOCK_SIZE * tar_sym_inode_table->i_block[0];
     strcpy(tfile,dirPath_src);
}

  
//add target file entry to its parent dir inode block[to keep inode->i_size multiple times of EXT2_BLOCK_SIZE]
   int file_rec_len_tar=EXT2_DIR_REC_LEN(strlen(dirName_tar));
   int found=0;
   int dir_num_tar;
   dir_tar= parent_dir_block_tar->i_size/EXT2_BLOCK_SIZE;//how many blocks the dir inode takes
   size=0;
   //to check if we can find somewhere in current inode block
  	for (i = 0; i <dir_tar; i++) {
    dir_num_tar=parent_dir_block_tar->i_block[i];
    entry_parent_tar=(void* )disk + EXT2_BLOCK_SIZE * dir_num_tar;
    while(size < EXT2_BLOCK_SIZE) {
        size += entry_parent_tar->rec_len;     
      if ( size % EXT2_BLOCK_SIZE == 0 && entry_parent_tar->rec_len>= EXT2_DIR_REC_LEN(strlen(dirName_tar))+EXT2_DIR_REC_LEN(entry_parent_tar->name_len)) 
      { // need to use next pointer  ???如果当前 block 不能放下下一个 entry
       file_rec_len_tar=entry_parent_tar->rec_len-EXT2_DIR_REC_LEN(entry_parent_tar->name_len);
       entry_parent_tar->rec_len=EXT2_DIR_REC_LEN(entry_parent_tar->name_len);
       entry_parent_tar=(void*)entry_parent_tar+EXT2_DIR_REC_LEN(entry_parent_tar->name_len);
       found=1; 
        break;
      }
      entry_parent_tar = (void*)entry_parent_tar + entry_parent_tar->rec_len;  //why we mush change entry to a (void*) first
      }
      size=0;
      if(found)
      {
        break;
      }
  
}
//if cannot find, assign a new block to dir inode to store file entry
if(!found){
	block_bitmap=get_block_bitmap(block_bitmap_block);
    for(i=0;i<BLOCK_NUM;i++){
          if (block_bitmap[i] == 0) {//cuz inode_bitmap[i]->inode i+1
          parent_dir_block_tar->i_block[dir_tar] = i+1;
          block_num_tar=parent_dir_block_tar->i_block[dir_tar];
          set_block_bitmap(block_bitmap_block,i); 
          sb->s_free_blocks_count-=1;
    	group_desc->bg_free_blocks_count-=1;
          break;
          }
     }
    if(parent_dir_block_tar->i_block[dir_tar]==-1)
     {
      fprintf(stderr,"No available inode for dir.\n");
       exit(1);
     }
     file_rec_len_tar=EXT2_BLOCK_SIZE;
     entry_parent_tar = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE *block_num_tar );
     entry_parent_tar->inode=file_inode_src;
     entry_parent_tar->name_len=strlen(dirName_tar);
     if(sflag==1){
     entry_parent_tar->file_type=EXT2_FT_SYMLINK; 
   }else{
    entry_parent_tar->file_type=EXT2_FT_REG_FILE;
   }
     
     strcpy(entry_parent_tar->name,dirName_tar);
     entry_parent_tar->rec_len=file_rec_len_tar;    
     parent_dir_block_tar->i_blocks=parent_dir_block_tar->i_blocks+2;   
     parent_dir_block_tar->i_size+=EXT2_BLOCK_SIZE;
}else{
  	 entry_parent_tar->inode=file_inode_src;
     entry_parent_tar->name_len=strlen(dirName_tar);
     if(sflag==1){
     entry_parent_tar->file_type=EXT2_FT_SYMLINK; 
   }else{
    entry_parent_tar->file_type=EXT2_FT_REG_FILE;
   }
     strcpy(entry_parent_tar->name,dirName_tar);
     entry_parent_tar->rec_len=file_rec_len_tar;  
     }
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
	//printf("test:enter set_inode_bitmap\n");
  int byte_num,bit_num;
  int *bitmap;
  byte_num=inode_num/8;
  bit_num=inode_num%8;
  bitmap=inode_bitmap_block+byte_num;
  *bitmap= *bitmap|(1<<bit_num);
}

void set_block_bitmap(void *block_bitmap_block, int block_num) {
	//printf("test:enter set_block_bitmap\n");
  int byte_num,bit_num;
  int *bitmap;
  byte_num=block_num/8;
  bit_num=block_num%8;
  bitmap=block_bitmap_block+byte_num;
 *bitmap= *bitmap|(1<<bit_num);
}









