#pragma once

#include <stdint.h>
#include <stat.h>

#define PATH(x)   char x[256]

//This struct will hold file system functions to help access/create file
//system data
typedef struct FileSystem {
    int (*create)(PATH(path), int flags); //Create file with flags 
    int (*stat)(PATH(path), struct stat *stat); //Get file stat information
} FileSystem;

//This structure will hold base Inode information that can
//be used in the DirTree structure for both Minix3 and Ext4 Inodes
typedef struct VInode {
    uint16_t mode;
    uint16_t uid;
    uint16_t gid;
} VInode;

#define VFS_TYPE_MINIX3     1
#define VFS_TYPE_EXT4       2

void vfs_init(void);
void vfs_parse_first(char *path, char *first);
void vfs_parse_last(char *path, char *last);
