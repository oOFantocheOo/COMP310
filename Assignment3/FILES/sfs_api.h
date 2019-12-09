#ifndef _INCLUDE_SFS_API_H_
#define _INCLUDE_SFS_API_H_

#include <stdint.h>
#include <stdlib.h>

//Here are my assumptions
#define MAXFILENAME 16
#define MAXFILEEXTENSION 3
#define BLOCK_SIZE 1024
#define NUM_BLOCKS 300
#define NUM_INODES 120
#define FILE_NUM 100
#define NUM_BITMAP_BLOCKS (sizeof(bitmap) / BLOCK_SIZE + 1)
#define BITMAP_START (NUM_BLOCKS - NUM_BITMAP_BLOCKS)
#define DISK "my_disk"

typedef struct
{
    int magic;
    int block_size;
    int file_system_size;
    int inode_table_length;
    int bitmap_length;
    int bitmap_start;
    int directory_walker_index;
    int root_directory; //set to be 0
} superblock;

typedef struct
{
    int is_used;
    int size;
    int link_cnt;
    int data_ptrs[12];
    int ind_pointer;
} inode;

typedef struct
{
    int block_is_used[NUM_BLOCKS];
    int next_block_available;
} bitmap;

typedef struct
{
    inode inodes[NUM_INODES];
    int next_inode_available;
} inode_table;

typedef struct
{
    int inodeID;
    int read_pointer;
    int write_pointer;
} file_descriptor;

typedef struct
{
    file_descriptor file_descriptors[FILE_NUM];
    int next_fd_available;
} file_descriptor_table;

typedef struct
{
    int inodeID;
    char filename[MAXFILENAME + 1 + MAXFILEEXTENSION];
} directory_entry;

typedef struct
{
    directory_entry files[FILE_NUM];
    int next_entry_available;
} root_directory;

void mksfs(int fresh);
int sfs_getnextfilename(char *fname);
int sfs_getfilesize(const char *path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_frseek(int fileID, int loc);
int sfs_fwseek(int fileID, int loc);
int sfs_fwrite(int fileID, const char *buf, int length);
int sfs_fread(int fileID, char *buf, int length);
int sfs_remove(char *file);
#endif
