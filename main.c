#include <stdio.h>

#define MAX_BLOCKS 1024
#define MAX_INODES 256
#define BLOCK_SIZE 4096
#define FILE_NAME_LENGTH 255


typedef struct{
    int total_blocks;
    int free_blocks;
    int block_size;
    int inode_count;
    int free_inodes;
}superblock;


typedef struct{
    int inode_id;
    int file_size;
    char file_type;
    int permissions;
    int owner;
    int timestamps[3];
    int data_blocks[12];

}inode;


typedef struct{
    char name[FILE_NAME_LENGTH];
    int inode_number;
}DirectoryEntry;


typedef struct{
    int inode_number;
    DirectoryEntry entries[MAX_INODES]
}Directory;




int main() {

return 0;  
}