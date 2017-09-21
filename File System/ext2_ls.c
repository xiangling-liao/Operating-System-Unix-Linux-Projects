#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include "ext2.h"


unsigned char *disk;

char dirName[EXT2_NAME_LEN+1]="\0";
int find_dir_inode(char *path, char *inodes);
int find_next_dir(char *path, char *dirName);

int main(int argc, char **argv) {
char fileName[EXT2_NAME_LEN+1];
struct ext2_dir_entry_2 *entry;
struct ext2_group_desc *group_desc;
void* inodes;
int size,total_size;
int block_num;
int dir_inode=EXT2_ROOT_INO;
int aflag=0;

    if(argc < 2) {
        fprintf(stderr, "Usage: %s <image file name> [-a] <absolute path>\n",argv[0]);
        exit(1);
    }

    if(argv[argc-1][0] != '/') {
        fprintf(stderr, "Please type in an absolute path.\n");
        exit(1);
    }

    if(argc==4&&!strcmp(argv[2],"-a")){
        aflag=1;
    }

    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
	perror("mmap");
	exit(1);
    }

   // struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    group_desc=(struct ext2_group_desc *)(disk+1024+EXT2_BLOCK_SIZE);
    inodes = disk + EXT2_BLOCK_SIZE *(group_desc->bg_inode_table);
   

    strcpy(fileName,argv[argc-1]);//exclude node 2
    if(strcmp(fileName,"/")){

    if(fileName[strlen(fileName)-1]=='/'){
        fileName[strlen(fileName)-1]='\0';
    }
   dir_inode=find_dir_inode(fileName,inodes);
   if(dir_inode==-1)
   {
    return ENOENT;
}
   }
//traverse the inode list
struct ext2_inode* inode = (struct ext2_inode*) (inodes + (dir_inode - 1) * sizeof(struct ext2_inode));
size=0;
total_size = inode->i_size;
block_num=inode->i_block[0];
entry = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
                                           /* keep track of the bytes read */
          /* first entry in the directory */
int i=0;
   while(size < total_size) {
        size += entry->rec_len;
        char file_name[EXT2_NAME_LEN+1];
        memset(file_name, 0, EXT2_NAME_LEN+1);
        memcpy(file_name, entry->name, entry->name_len);
        file_name[entry->name_len] = '\0';              /* append null char to the file name */
        if(strcmp(file_name, "\0") != 0) {
        	 if(aflag==0){
            if(!strcmp(file_name,".")||!strcmp(file_name,".."));
            else
            printf("%s\n",file_name);
        }else{
            printf("%s\n",file_name);
        }
        }
      if ( size % EXT2_BLOCK_SIZE == 0) { // need to use next pointer  [dir inode->i_size must be multiple times of 1024]
        i++;
        block_num=inode->i_block[i];
        entry = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);

      }else{
        entry = (void*) entry + entry->rec_len; 
      }
  }
  close(fd); 
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
    break;
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
            printf("%s\n",dirName);//if is a file or a link
            exit(0);
            }
          new_inode_num = entry->inode;
          break;
        }
      
       if (size%EXT2_BLOCK_SIZE ==0) { // need to use next pointer 
        block_num++;
        entry = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
      }else{
        entry = (void*) entry + entry->rec_len;      /* move to the next entry */   
      }
  }
    if (new_inode_num == -1) {
      printf( "No such file or directory\n"); //not found
      return -1;
    } else {
      inode_num = new_inode_num;
    }
}
return inode_num;
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










