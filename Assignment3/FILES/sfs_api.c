//Sometimes test2 terminates with segmentation fault for unknown reason,
//but most of the time it ends normally.
//Please run test2 multiple times if segfault happens

#include "disk_emu.h"
#include "sfs_api.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

superblock sb;
inode_table it;
bitmap bm;
root_directory rd;
file_descriptor_table ft;

int filename_is_valid(char *filename) //ok
{
    int len = strlen(filename);
    if (len == 0 || len > MAXFILENAME + 1 + MAXFILEEXTENSION)
        return 0;
    char *dot_pos = strchr(filename, '.');
    if (dot_pos == NULL)
        if (len > MAXFILENAME)
            return 0;
    return 1;
}

int get_fileID(char *filename) //ok
{
    if (!filename_is_valid(filename))
        return -1;
    for (int i = 0; i < FILE_NUM; i++)
        if (strcmp(rd.files[i].filename, filename) == 0)
            return i;
    return -1;
}

int fileID_is_valid(int fileID) //ok
{
    if (fileID < 0 || ft.file_descriptors[fileID].inodeID < 0)
        return 0;
    return 1;
}

int allocate_block() //ok
{
    //loop thru the blocks to find the next free one
    for (int i = bm.next_block_available + 1; i < NUM_BLOCKS; i++)
        if (!bm.block_is_used[i])
        {
            bm.block_is_used[bm.next_block_available] = 1;
            int used = bm.next_block_available;
            bm.next_block_available = i;
            write_blocks(BITMAP_START, NUM_BITMAP_BLOCKS, &bm);
            return used;
        }
    return -1;
}

void release_block(int blockID) //ok
{
    bm.block_is_used[blockID] = 0;
    if (bm.next_block_available > blockID)
        bm.next_block_available = blockID;
    write_blocks(BITMAP_START, NUM_BITMAP_BLOCKS, &bm);
    return;
}

//creates a new inode and returns its id
int new_inode() //ok
{
    int used;
    for (int i = it.next_inode_available + 1; i < NUM_INODES; i++)
        if (!it.inodes[i].is_used)
        {
            it.inodes[it.next_inode_available].is_used = 1;
            used = it.next_inode_available;
            it.next_inode_available = i;
            write_blocks(1, sb.inode_table_length, &it);
            break;
        }
    inode *newInode = &it.inodes[used];
    newInode->size = 0;
    write_blocks(1, sb.inode_table_length, &it);
    return used;
}

//creates a new file descriptor and returns its index in fdt
int new_file_descriptor(int inodeID) //ok
{
    int next_avai = ft.next_fd_available;
    ft.file_descriptors[next_avai].inodeID = inodeID;
    for (int i = ft.next_fd_available; i < FILE_NUM; i++)
        if (ft.file_descriptors[i].inodeID < 0)
        {
            ft.next_fd_available = i;
            break;
        }
    ft.file_descriptors[next_avai].inodeID = inodeID;
    ft.file_descriptors[next_avai].write_pointer = 0;
    ft.file_descriptors[next_avai].read_pointer = 0;
    return next_avai;
}

void mksfs(int fresh) //ok
{
    //file descriptor table
    ft.next_fd_available = 0;
    for (int i = 0; i < FILE_NUM; i++)
    {
        ft.file_descriptors[i].inodeID = -1;
        ft.file_descriptors[i].write_pointer = 0;
        ft.file_descriptors[i].read_pointer = 0;
    }

    if (fresh)
    {
        init_fresh_disk(DISK, BLOCK_SIZE, NUM_BLOCKS);
        //superblock
        sb.magic = 0xACBD0005; //copied from pdf
        sb.block_size = BLOCK_SIZE;
        sb.file_system_size = NUM_BLOCKS;
        sb.inode_table_length = sizeof(inode_table) / BLOCK_SIZE + 1;
        sb.bitmap_length = sizeof(bitmap) / BLOCK_SIZE + 1;
        sb.root_directory = new_inode(); //first call, so set to 0
        int sb_block = allocate_block();
        write_blocks(sb_block, 1, &sb);

        //bitmap
        for (int i = 0; i < NUM_BLOCKS; i++)
            bm.block_is_used[i] = 0;
        bm.next_block_available = 0;
        for (int i = 0; i < NUM_BITMAP_BLOCKS; i++)
            bm.block_is_used[i] = 1;
        write_blocks(BITMAP_START, NUM_BITMAP_BLOCKS, &bm);

        //inode_table
        for (int i = 0; i < NUM_INODES; i++)
        {
            it.inodes[i].is_used = 0;
            it.inodes[i].size = 0;
            it.inodes[i].link_cnt = 0;
            it.inodes[i].ind_pointer = 0;
        }
        it.next_inode_available = 0;
        for (int i = 0; i < sb.inode_table_length; i++)
            allocate_block();
        write_blocks(1, sb.inode_table_length, &it);

        //root directory
        inode *root_inode = &it.inodes[0]; //set the first inode to root dir
        for (int i = 0; i < 12; i++)
            root_inode->data_ptrs[i] = allocate_block();
        for (int i = 0; i < FILE_NUM; i++)
            rd.files[i].inodeID = -1;
        write_blocks(it.inodes[0].data_ptrs[0], sizeof(rd) / BLOCK_SIZE, &rd);
    }
    else
    {
        init_disk(DISK, BLOCK_SIZE, NUM_BLOCKS);
        read_blocks(0, 1, &sb);
        read_blocks(BITMAP_START, sb.bitmap_length, &bm);
        read_blocks(1, sb.inode_table_length, &it);
        read_blocks(it.inodes[0].data_ptrs[0], sizeof(rd) / BLOCK_SIZE, &rd);
    }
}

int sfs_getnextfilename(char *fname) //ok
{
    for (int i = sb.directory_walker_index; i < FILE_NUM; i++) //for every possible file's index
        if (rd.files[i].inodeID >= 0)                          //if it's a file
        {
            strcpy(fname, rd.files[i].filename);
            sb.directory_walker_index = i + 1;
            return 1;
        }
    return 0;
}

int sfs_getfilesize(const char *path) //ok
{
    int fileID = get_fileID((char *)path);
    if (fileID < 0)
        return -1;
    return it.inodes[ft.file_descriptors[fileID].inodeID].size;
}

int sfs_fopen(char *name) //ok
{
    if (!filename_is_valid(name))
        return -1;
    int fileID = get_fileID(name);
    if (fileID < 0) //file does not exist
    {
        int inodeID = new_inode();
        int rdID = rd.next_entry_available;
        rd.files[rdID].inodeID = inodeID;
        for (int i = rd.next_entry_available + 1; i < FILE_NUM; i++)
            if (rd.files[i].inodeID < 0)
            {
                rd.next_entry_available = i;
                break;
            }
        strncpy(rd.files[rdID].filename, name, strlen(name));
        int idx = new_file_descriptor(inodeID);
        write_blocks(((inode *)(&it.inodes[0]))->data_ptrs[0], (sizeof(rd) / BLOCK_SIZE) + 1, &rd);
        return idx;
    }
    else //file exists
    {
        int inodeID = rd.files[fileID].inodeID;
        int idx = -1;

        for (int i = 0; i < FILE_NUM; i++) //search the file
            if (ft.file_descriptors[i].inodeID == inodeID)
                idx = i;

        if (idx >= 0) //if opened
            return idx;
        return new_file_descriptor(inodeID);
    }
}

int sfs_fclose(int fileID) //ok
{
    if (!fileID_is_valid(fileID))
        return -1;
    ft.file_descriptors[fileID].inodeID = -1;
    ft.file_descriptors[fileID].write_pointer = 0;
    ft.file_descriptors[fileID].read_pointer = 0;
    if (fileID < ft.next_fd_available)
        ft.next_fd_available = fileID;
    return 0;
}

int sfs_frseek(int fileID, int loc) //ok
{
    if (!fileID_is_valid(fileID))
        return -1;
    ft.file_descriptors[fileID].read_pointer = loc;
    return 0;
}

int sfs_fwseek(int fileID, int loc) //ok
{
    if (!fileID_is_valid(fileID))
        return -1;
    ft.file_descriptors[fileID].write_pointer = loc;
    return 0;
}

int sfs_fwrite(int fileID, const char *buf, int length) //ok
{
    if (!fileID_is_valid(fileID))
        return -1;
    inode *cur = &it.inodes[ft.file_descriptors[fileID].inodeID];
    file_descriptor *f = &ft.file_descriptors[fileID];

    int blocks_used;
    if (cur->size == 0)
        blocks_used = 0;
    else
        blocks_used = cur->size / BLOCK_SIZE + 1;

    int stop = ((f->write_pointer + length) / BLOCK_SIZE + 1);
    int new_blocks = stop - blocks_used;

    if (cur->link_cnt + new_blocks >= 12) //unable to handle indirect pointers
        return length;
    for (int i = cur->link_cnt; i < cur->link_cnt + new_blocks; i++)
        cur->data_ptrs[i] = allocate_block();
    cur->link_cnt += new_blocks;
    int start_block = (f->write_pointer) / BLOCK_SIZE;
    int res = length;
    char temp_buffer[BLOCK_SIZE];
    for (int i = start_block; i < stop; i++)
    {
        int block = cur->data_ptrs[i];
        read_blocks(block, 1, (void *)temp_buffer);
        char *temp = temp_buffer;
        int flag = f->write_pointer % BLOCK_SIZE;
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if (length == 0)
                break;
            if (i >= flag)
            {
                memcpy(temp, buf, 1);
                temp++;
                buf++;
                length--;
                f->write_pointer++;
                cur->size++;
            }
        }
        write_blocks(block, 1, (void *)temp_buffer);
    }
    cur->link_cnt += new_blocks;
    write_blocks(1, sb.inode_table_length, &it);
    return res;
}

int sfs_fread(int fileID, char *buf, int length) //ok
{
    if (!fileID_is_valid(fileID))
        return -1;
    inode *i = &it.inodes[ft.file_descriptors[fileID].inodeID];
    file_descriptor *f = &ft.file_descriptors[fileID];

    int res = 0;
    if (f->read_pointer + length > i->size)
        res = i->size - f->read_pointer;
    else
        res = length;

    char tmp[BLOCK_SIZE];
    if (res < BLOCK_SIZE) //only handles one block's data
    {
        int start = (f->read_pointer) % BLOCK_SIZE;
        int block_start = i->data_ptrs[0];
        read_blocks(block_start, 1, tmp);
        memcpy(buf, tmp + start, res);
        f->read_pointer += res;
        return res;
    }
    return length;
}

int sfs_remove(char *file) //ok
{
    int fileID = get_fileID(file);
    if (fileID < 0 || ft.file_descriptors[fileID].inodeID != -1) //if not valid or file not closed
        return -1;
    rd.files[fileID].inodeID = -1;
    if (fileID < rd.next_entry_available)
        rd.next_entry_available = fileID;
    int curID = rd.files[fileID].inodeID;
    write_blocks(((inode *)(&it.inodes[0]))->data_ptrs[0], (sizeof(rd) / BLOCK_SIZE) + 1, &rd);
    if (curID > 0)
        release_block(curID);
    return fileID;
}
