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
#include <time.h>
#include "ext2.h"


unsigned char *disk;
char dirName[EXT2_NAME_LEN+1]="\0";
int find_dir_inode(char *path, char *inodes);
int find_next_dir(char *path, char *dirName);
void set_inode_bitmap_back(void *inode_bitmap_block, int inode_num);
void set_block_bitmap_back(void *block_bitmap_block, int block_num);

int main(int argc, char **argv) {
char parentDir[EXT2_NAME_LEN+1] = {0};
char fileName_name[EXT2_NAME_LEN+1] = {0};//real file name to delete
char pathName[EXT2_NAME_LEN+1] = {0};//absolute path on the disk
char *fp;
struct ext2_super_block *sb;
struct ext2_group_desc *group_desc;
struct ext2_dir_entry_2 *entry;
void* inodes;
void* inode_bitmap_block;
void* block_bitmap_block;
int image_fd;
int i;

if(argc != 3) {
        fprintf(stderr, "Usage: %s <image file name> <target file/link>\n",argv[0]);
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

    strcpy(pathName,argv[argc-1]);//dir where to put the new dir

if(strcmp(pathName,"/")){
    if(pathName[strlen(pathName)-1]=='/'){
        pathName[strlen(pathName)-1]='\0';
        strcpy(argv[argc-1],pathName);
    }
}

fp=strrchr(argv[argc-1],'/');
fp++;
strcpy(fileName_name,fp);
fp--;
if(fp==strchr(argv[argc-1],'/'))
{    
fp++;
}
*fp='\0';
strcpy(parentDir,argv[argc-1]);

  int dir_inode=EXT2_ROOT_INO;
  if(strcmp(parentDir,"/")){
     dir_inode=find_dir_inode(parentDir,inodes);//by default =2
     if(dir_inode==-1){
        fprintf(stderr, "Specified path not found\n"); 
        return ENOENT;
     }
}

 //traverse the inode list, to find the file inode 
int file_inode_num=-1;
struct ext2_inode* inode = (struct ext2_inode*) (inodes + (dir_inode - 1) * sizeof(struct ext2_inode));
int size=0;
int total_size = inode->i_size;
int block_num=inode->i_block[0];
entry = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
 /* keep track of the bytes read */
/* first entry in the directory */
int dir=0;//dir block index
   while(size < total_size) {
        size += entry->rec_len;
        char file_name[EXT2_NAME_LEN+1] = {0};
        memcpy(file_name, entry->name, entry->name_len);
        file_name[entry->name_len] = '\0';              /* append null char to the file name */
         if(!strcmp(file_name,fileName_name)){
            if(entry->file_type==EXT2_FT_DIR)
            {
                fprintf(stderr,"Source is a directory rather than a file\n");
                return EISDIR;
            }
           file_inode_num=entry->inode; 
           break;
         }
      if (size%EXT2_BLOCK_SIZE==0) { // need to use next pointer  ???如果当前 block 不能放下下一个 entry    
        dir++;
        block_num=inode->i_block[dir];
        entry = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
      }else{
         entry = (void*) entry + entry->rec_len; 
      }
  }

if(file_inode_num==-1){
  fprintf(stderr,"File does not exist.\n");
          return ENOENT;
}


  //remove file/link from dir entry
 
    entry->inode=0;
    entry->name_len=0;
    entry->file_type=0;
    strcpy(entry->name,"\0");
   

//if link count for an inode=0, then get back the inode and blocks
  struct ext2_inode* file_inode = (struct ext2_inode*) (inodes + (file_inode_num - 1) * sizeof(struct ext2_inode));
  if((--file_inode->i_links_count)==0){
    //get back the inode
   
    set_inode_bitmap_back(inode_bitmap_block,file_inode_num-1);
    sb->s_free_inodes_count+=1;
     group_desc->bg_free_inodes_count+=1;

     //get back the block
     int file_blocks_num=file_inode->i_blocks/2;
     int nsize = file_blocks_num > 12 ? 12 : file_blocks_num; 
    
     for(i=0;i< nsize;i++){
          set_block_bitmap_back(block_bitmap_block,file_inode->i_block[i]-1); 
          sb->s_free_blocks_count+=1;
          group_desc->bg_free_blocks_count+=1;
     }
     if(file_blocks_num>12){
        int* indirect_block = (void *)disk + EXT2_BLOCK_SIZE *file_inode->i_block[12];
        int *block;
        for (i = 0; i <file_blocks_num-12;i++ ) {
          block=(int *)(indirect_block+i*sizeof(int *));
          set_block_bitmap_back(block_bitmap_block,(*block)-1); 
          sb->s_free_blocks_count+=1;
          group_desc->bg_free_blocks_count+=1;
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


void set_inode_bitmap_back(void *inode_bitmap_block, int inode_num) {
    //printf("test:enter set_inode_bitmap\n");
  int byte_num,bit_num;
  int *bitmap;
  byte_num=inode_num/8;
  bit_num=inode_num%8;
  bitmap=inode_bitmap_block+byte_num;
  *bitmap= *bitmap&(0<<bit_num);
}

void set_block_bitmap_back(void *block_bitmap_block, int block_num) {
    //printf("test:enter set_block_bitmap\n");
  int byte_num,bit_num;
  int *bitmap;
  byte_num=block_num/8;
  bit_num=block_num%8;
  bitmap=block_bitmap_block+byte_num;
 *bitmap= *bitmap&(0<<bit_num);
}
