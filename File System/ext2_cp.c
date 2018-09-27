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
int find_dir_inode(char *path, char *inodes);
int find_next_dir(char *path, char *dirName);
int* get_inode_bitmap(void* inode_bitmap_block);
int* get_block_bitmap(void* block_bitmap_block);
void set_inode_bitmap(void *inode_bitmap_block, int inode_num);
void set_block_bitmap(void *block_bitmap_block, int block_num);

char dirName[EXT2_NAME_LEN+1]="\0";
int main(int argc, char **argv) {

int file_fd,image_fd;
char fileName[EXT2_NAME_LEN+1];//source file path including file name
char fileName_name[EXT2_NAME_LEN+1];
char pathName[EXT2_NAME_LEN+1];//absolute path on the disk
char *fp;
struct ext2_super_block *sb;
struct ext2_group_desc *group_desc;
struct ext2_dir_entry_2 *entry;
void* inodes;
int file_size;
int file_blocks;
void* inode_bitmap_block;
void* block_bitmap_block;
int* inode_bitmap;
int* block_bitmap;
int dir_inode=EXT2_ROOT_INO;
int size,total_size;
int block_num;
int file_inode_idx;
int i;
struct ext2_inode* file_inode;
int indirect=0;




    if(argc != 4) {
        fprintf(stderr, "Usage: %s <image file name1> <source file> <absolute path on the disk>\n",argv[0]);
        exit(1);
    }

    if(argv[argc-1][0] != '/') {
        fprintf(stderr, "Please type in absolute path.\n");
        exit(1);
    }
    
    if(!strcmp(argv[argc-2],"/")){
        fprintf(stderr, "Cannot copy directory\n");
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

   if ((file_fd = open(argv[2], O_RDONLY)) == -1) {
    perror("open file");
    exit(ENOENT);
    }

    file_size = lseek(file_fd, 0, SEEK_END);
    //how many blocks the file needed in the disk
    if(file_size % EXT2_BLOCK_SIZE ==0)
    {
    	file_blocks = file_size/EXT2_BLOCK_SIZE;
    }else
    {
    	file_blocks = file_size/EXT2_BLOCK_SIZE +1;
    }
    
/*    if((file_blocks-12)>(1024/sizeof(int)))
    {
	  fprintf(stderr,"Copy failed! Double indirect is needed.\n");
    exit(1);
    }

    if((file_blocks + sb->s_free_blocks_count)>sb->s_blocks_count)
    {
    	fprintf(stderr, "Copy failed! No enough space for the file.\n");
    	exit(1);
    }*/
    	
    	

  strcpy(fileName,argv[2]);//
  
    if(fileName[strlen(fileName)-1]=='/'){
        fileName[strlen(fileName)-1]='\0';
}


  if((fp=strrchr(fileName,'/'))){
  		fp++;//file name extracted from source file path
  	}
  else {
  		fp = fileName;  	
  }
  	
  strcpy(fileName_name, fp);
  strcpy(pathName, argv[3]);
   if(strcmp(pathName,"/")){
       if(pathName[strlen(pathName)-1]=='/'){
        pathName[strlen(pathName)-1]='\0';
       }
    dir_inode=find_dir_inode(pathName,inodes);//by default =2
    if(dir_inode==-1){
   	fprintf(stderr, "No such file or directory\n"); //not found\n"); 
    return ENOENT;
    }
   }

   //traverse the inode list, to check if the fileName has been taken in current dir[pathName]
struct ext2_inode* inode = (struct ext2_inode*) (inodes + (dir_inode - 1) * sizeof(struct ext2_inode));
size=0;
total_size = inode->i_size;
block_num=inode->i_block[0];
entry = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
 /* keep track of the bytes read */
/* first entry in the directory */
int dir=0;//dir block index
   while(size < total_size) {
 size += entry->rec_len;
        char file_name[EXT2_NAME_LEN+1];
        memcpy(file_name, entry->name, entry->name_len);
        file_name[entry->name_len] = '\0';              /* append null char to the file name */
         if(!strcmp(file_name,fileName_name)){
         	fprintf(stderr,"File copy failed! File name has been used.\n");
         	exit(1);
         }
      if (size%EXT2_BLOCK_SIZE==0) { // need to use next pointer  ???如果当前 block 不能放下下一个 entry    
        dir++;
        block_num=inode->i_block[dir];
        entry = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
      }else{
         entry = (void*) entry + entry->rec_len; 
      }
  }
  
  //get the inode bitmap 
inode_bitmap=get_inode_bitmap(inode_bitmap_block);
  //assign inode to the file
file_inode_idx=-1;
for(i=0;i<INODE_NUM;i++)
{
if(inode_bitmap[i]==0)
{
	file_inode_idx=i+1;
	break;
}
}
  if (file_inode_idx == -1) {
    fprintf(stderr, "No inode available.\n");
    exit(1);
  }
 
//set up inode bitmap
  sb->s_free_inodes_count-=1;
    	group_desc->bg_free_inodes_count-=1;
    
  set_inode_bitmap(inode_bitmap_block,file_inode_idx-1);


  //set up inode table
  file_inode = (struct ext2_inode*) (inodes + (file_inode_idx - 1) * sizeof(struct ext2_inode));
  file_inode->i_mode=EXT2_S_IFREG;
  file_inode->i_size=file_size;
  file_inode->i_links_count=1;
  file_inode->i_blocks=file_blocks * 2;
  file_inode->i_dtime=0;
  //????file_inode->i_block[15]

  
//add file entry to its parent dir inode block[to keep inode->i_size multiple times of EXT2_BLOCK_SIZE]
   int file_rec_len=EXT2_DIR_REC_LEN(strlen(fp));
   int found=0;
   int dir_num;
   dir= inode->i_size/EXT2_BLOCK_SIZE;//how many blocks the dir inode takes
   size=0;
   total_size = inode->i_size;
   //to check if we can find somewhere in current inode block
  	for (i = 0; i <dir; i++) {
    dir_num=inode->i_block[i];
    entry=(void* )disk + EXT2_BLOCK_SIZE * dir_num;
    while(size < EXT2_BLOCK_SIZE) {
        size += entry->rec_len;     
      if ( size % EXT2_BLOCK_SIZE == 0 && entry->rec_len>= EXT2_DIR_REC_LEN(strlen(fp))+EXT2_DIR_REC_LEN(entry->name_len)) { // need to use next pointer  ???如果当前 block 不能放下下一个 entry
       file_rec_len=entry->rec_len-EXT2_DIR_REC_LEN(entry->name_len);
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



if(!found){
	block_bitmap=get_block_bitmap(block_bitmap_block);
    for(i=0;i<BLOCK_NUM;i++){
          if (block_bitmap[i] == 0) {//cuz inode_bitmap[i]->inode i+1
          inode->i_block[dir] = i+1;
          block_num=inode->i_block[dir];
          set_block_bitmap(block_bitmap_block,i); 
          sb->s_free_blocks_count-=1;
    	group_desc->bg_free_blocks_count-=1;
          break;
          }
     }
    if(inode->i_block[dir]==-1)
     {
      fprintf(stderr,"No available inode for dir.\n");
       exit(1);
     }
     file_rec_len=EXT2_BLOCK_SIZE;
     entry = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE *block_num );
     entry->inode=file_inode_idx;
     entry->name_len=strlen(fp);
     entry->file_type=EXT2_FT_REG_FILE;
     strcpy(entry->name,fp);
     entry->rec_len=file_rec_len;    
     inode->i_blocks=inode->i_blocks+2;   
     inode->i_size+=EXT2_BLOCK_SIZE;
}else{
  	 entry->inode=file_inode_idx;
     entry->name_len=strlen(fp);
     entry->file_type=EXT2_FT_REG_FILE;
     strcpy(entry->name,fp);
     entry->rec_len=file_rec_len;  
     }
  

//assign blocks to the file
if(file_blocks>12)
{
	indirect=1;
}
int j=0;
block_bitmap=get_block_bitmap(block_bitmap_block);
for (i = 0; i < BLOCK_NUM;i++ ) {
    if (block_bitmap[i] == 0) {
      file_inode->i_block[j]=i+1;
     set_block_bitmap(block_bitmap_block,i); //tell at the beginning, if there is space for file
     sb->s_free_blocks_count-=1;
    	group_desc->bg_free_blocks_count-=1;
     j++;
     if(j==12)
     {
     	break;
     }
    }
  }

  //assign block to single indirect 
 block_bitmap=get_block_bitmap(block_bitmap_block);
if(indirect){
for (i = 0; i < BLOCK_NUM;i++ ) {
    if (block_bitmap[i] == 0) {
      file_inode->i_block[12]=i+1;
     set_block_bitmap(block_bitmap_block,i);
     sb->s_free_blocks_count-=1;
    	group_desc->bg_free_blocks_count-=1;
     break; 
}
}
}


int* indirect_block = (void *)disk + EXT2_BLOCK_SIZE *file_inode->i_block[12];
int *block;

//assign blocks[file_blocks-12] pointed by single indirect
block_bitmap=get_block_bitmap(block_bitmap_block);
j=0;
for (i = 0; i <BLOCK_NUM;i++ ) {
    if (block_bitmap[i] == 0) {
      block=(int *)(indirect_block+i*sizeof(int*));
      *block=i+1; 
     set_block_bitmap(block_bitmap_block,i); //tell at the beginning, if there is space for file
     sb->s_free_blocks_count-=1;
    	group_desc->bg_free_blocks_count-=1;
     j++;
     if(j==file_blocks-12)
     {
     	break;

    }
  }
}


//copy the file content to the disk
unsigned char* file = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, file_fd, 0);
int bytes_left=file_size;
int copied=0;
for (i = 0; i < 12; i++) {
     block_num = file_inode->i_block[i];
    if (bytes_left< EXT2_BLOCK_SIZE) {
      memcpy(disk + EXT2_BLOCK_SIZE * block_num,
             file + copied, bytes_left);
      bytes_left = 0;
      break;
    } else {
      memcpy(disk + EXT2_BLOCK_SIZE * block_num,
             file + copied, EXT2_BLOCK_SIZE);
      bytes_left -= EXT2_BLOCK_SIZE;
      copied += EXT2_BLOCK_SIZE;
    }
  }

if(indirect){
for (i = 0; i < file_blocks-12; i++) {
     block_num = indirect_block[i];
    if (bytes_left< EXT2_BLOCK_SIZE) {
      memcpy(disk + EXT2_BLOCK_SIZE * block_num,
             file + copied, bytes_left);
      bytes_left = 0;
      break;
    } else {
      memcpy(disk + EXT2_BLOCK_SIZE * (block_num + 1),
             file + copied, EXT2_BLOCK_SIZE);
      bytes_left -= EXT2_BLOCK_SIZE;
      copied += EXT2_BLOCK_SIZE;
    }
  }
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
            fprintf(stderr,"Specified path is not a directory\n");
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
int *byte=(int*)(inode_bitmap_block+byte_num);
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
//fail, sb etc :how to release resources























