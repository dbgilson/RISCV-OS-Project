#include <vfs.h>
#include <stdint.h>
#include <map.h>
#include <stddef.h>
#include <stdbool.h>
#include <strhelp.h>
#include <minix3.h>
#include <memhelp.h>
#include <ext4.h>

//Directory Tree structure used to cache inodes of the block devices
//for easy access by the file system functions (read, write, open, close).
typedef struct DirTree{
    char name[256];
    int blockdev;
    int fstype;
    VInode *vinode;
    Map *children;
} DirTree;

DirTree dt_root;

//Initialization function of the directory tree root.
void vfs_init(void){
    dt_root.name[0] = '/';
    dt_root.name[1] = '\0';
    dt_root.blockdev = 0; //Mounting Minix3 file system (1 will be Ext4)
    dt_root.fstype = -1;
    dt_root.vinode = NULL;
    dt_root.children = map_new();
}

//Helper function to parse the first name in a path.  This can be
//used to go through a path and get each name.
void vfs_parse_first(char *path, char *first){
    char *p = path;

    if(*p == '/'){
        p++;
        if(*p == '\0'){
            first[0] = '\0';
        }
    }

    strcpy(first, p);
    char *p2 = strchr(first, '/');
    if(p2 != NULL){
        *p2 = '\0';
    }

    return;
}

//Helper function to parse the last name in a path.  This can be
//used to go through a path and get each name.
void vfs_parse_last(char *path, char *last){
    char *p = path;
    char *p2 = strchr(p, '/');

    if(p2 == NULL){
        strcpy(last, path);
    }

    while(p2 != NULL){
        p = p2 + 1;
        p2 = strchr(p, '/');
    }
    strcpy(last, p);
    return;
}

//This function populates the Directory Tree by initially going through each of the filesystems.  It starts with Minix 3
//and will then move to the Ext4 file system.  I have not actually implemented the caching system as I'm trying to figure out
//the best way to do that, so it just gets to the Inodes as of now.
void vfs_cache_inodes(void){
    //Initialize Directory Tree Root
    vfs_init();

    //Start caching with minix filesystem (similar steps to minix_print
    //test to go through initial paths to find directory entries and cache them)
    vfs_init();
    int ret;
    Superblock sb;
    char *path = (char *)cpage_znalloc(1);
    void *blockbuf = (void *)cpage_znalloc(1);
    Inode ino;
    DirEntry de;
    uint64_t byte, i;
    uint32_t len;

    //Get superblock of minix file system
    if(!minix_disk_rw(blockbuf, BLOCK_SIZE, BLOCK_SIZE, 0)){
        printf("[minix_test]: Unable to read superblock (%d).\n", ret);
        goto out;
    }
    memcpy(&sb, blockbuf, sizeof(sb));
    if (sb.magic != MINIX3_MAGIC) {
        printf("[minix_test]: This is not a Minix 3 file system.\n");
        goto out;
    }

    //Not doing inode 0 (unused) or 1 (root), so go through the rest of the inodes (similar process
    //to minix_print function, so see that function for more code comments)
    for(int j = 2; j < sb.num_inodes-1; j++){
        byte = OFFSET(j, sb.imap_blocks, sb.zmap_blocks);
        i    = ALIGN_DOWN_POT(byte, BLOCK_SIZE);
        memset(blockbuf, 0, BLOCK_SIZE);
        if(!minix_disk_rw(blockbuf, i, BLOCK_SIZE, 0)){
            printf("[minix_cache]: Unable to read inode %d (%d)\n", j, ret);
            return;
        }

        i = byte - i;
        memcpy(&ino, blockbuf + i, sizeof(ino));
        if (strlen(path) > 0) {
            printf("[minix_cache]: %s\n", path);
        }

        //If inode is a directory, cache it into the tree
        if(S_FMT(ino.mode) == S_IFDIR){
            len = strlen(path);
            strcpy(path + len, "/");
            //First look at direct pointers
            for (i = 0; i < 7; i++){
                if (ino.zones[i] == 0){
                    continue;
                }
                byte = ino.zones[i] * BLOCK_SIZE;
                if (!minix_disk_rw(blockbuf, byte, BLOCK_SIZE, 0)){
                    printf("[minix_cache]: Unable to read inode %d (%d)\n", j, ret);
                    return;
                }
                for (i = 128; i < ino.size; i += sizeof(DirEntry)){
                    memcpy(&de, blockbuf + i, sizeof(DirEntry));
                    //We are at a directory entry, cache it to tree
                    if(de.inode != 0){
                        
                    }
                }
            }
            uint64_t addr;
            //I know this isn't quite how you do the indirect pointers, but I think it's a start
            if (ino.zones[ZONE_INDIR] != 0){
                // Indirect
                byte = ino.zones[ZONE_INDIR] * BLOCK_SIZE;
                minix_disk_rw(blockbuf, byte, BLOCK_SIZE, 0);

                memcpy(addr, blockbuf, sizeof(addr));
                byte = addr * BLOCK_SIZE;
                minix_disk_rw(blockbuf, byte, BLOCK_SIZE, 0);

                for (i = 128; i < ino.size; i += sizeof(DirEntry)){
                    memcpy(&de, blockbuf + i, sizeof(DirEntry));
                    //We are at a directory entry, cache it to tree
                    if(de.inode != 0){
                        
                    }
                }
            }
            if (ino.zones[ZONE_DINDR] != 0){
                // Doubly-indirect
                byte = ino.zones[ZONE_INDIR] * BLOCK_SIZE;
                minix_disk_rw(blockbuf, byte, BLOCK_SIZE, 0);

                memcpy(addr, blockbuf, sizeof(addr));
                byte = addr * BLOCK_SIZE;
                minix_disk_rw(blockbuf, byte, BLOCK_SIZE, 0);

                memcpy(addr, blockbuf, sizeof(addr));
                byte = addr * BLOCK_SIZE;
                minix_disk_rw(blockbuf, byte, BLOCK_SIZE, 0);

                for (i = 128; i < ino.size; i += sizeof(DirEntry)){
                    memcpy(&de, blockbuf + i, sizeof(DirEntry));
                    //We are at a directory entry, cache it to tree
                    if(de.inode != 0){
                        
                    }
                }
            }
            if (ino.zones[ZONE_TINDR] != 0){
                // Triply-indirect
                byte = ino.zones[ZONE_INDIR] * BLOCK_SIZE;
                minix_disk_rw(blockbuf, byte, BLOCK_SIZE, 0);

                memcpy(addr, blockbuf, sizeof(addr));
                byte = addr * BLOCK_SIZE;
                minix_disk_rw(blockbuf, byte, BLOCK_SIZE, 0);

                memcpy(addr, blockbuf, sizeof(addr));
                byte = addr * BLOCK_SIZE;
                minix_disk_rw(blockbuf, byte, BLOCK_SIZE, 0);

                memcpy(addr, blockbuf, sizeof(addr));
                byte = addr * BLOCK_SIZE;
                minix_disk_rw(blockbuf, byte, BLOCK_SIZE, 0);

                for (i = 128; i < ino.size; i += sizeof(DirEntry)){
                    memcpy(&de, blockbuf + i, sizeof(DirEntry));
                    //We are at a directory entry, cache it to tree
                    if(de.inode != 0){
                        
                    }
                }
            }
        }
    }

out:
cpage_free(path);
cpage_free(blockbuf);
    //Now for Ext4 (Leaving commented out as I'm needing to fix some function declarations before
    //it is completely compilable)
    /*
    Groupdesc bg;
    uint64_t size;
    Superblock_ext4 sb4;
    Inode_ext2 *inodes  = NULL;
    size = ALIGN_UP_POT(sizeof(sb), 512);

    //Block offset of superblock is 1024, so start from that offset and read the size of a superblock (ext4)
    if(!ext4_disk_rw(blockbuf, 1024, size, 0)){
        printf("[ext4_test]: Unable to read Superblock_ext4 (%d).\n", ret);
        goto out;
    }
    memcpy(&sb4, blockbuf, sizeof(sb));

    //Check the magic number of the superblock.  If not ext4, then it doesn't work.
    if (sb4.s_magic != EXT4_MAGIC) {
        printf("[ext4_test]: This is not a Ext4 file system (magic: 0x%04x).\n", sb4.s_magic);
        goto out;
    }
    else {
        printf("[ext4_test]: (Label: %.16s, Block size: %u)\n", sb4.s_volume_name, 1024 << sb4.s_log_block_size);
    }

    //Get the first block group located right after the first superblock
    size = ALIGN_UP_POT(sizeof(bg), 512);
    if(!ext4_disk_rw(blockbuf, 2048, size, 0)){
        printf("[ext4_test]: Unable to read block group\n");
        goto out;
    }
    memcpy(&bg, blockbuf, sizeof(bg));

    //Get byte offset into the inode table in order to start reading from them
    byte = BLOCK_OFFSET(INODE_TABLE_BLOCK(&bg), BLOCK_SIZE(&sb4));
    size = sb4.s_inodes_per_group * sb4.s_inode_size;
    size = ALIGN_UP_POT(size, 512);
    blockbuf= cpage_znalloc(1);
    inodes = (Inode_ext2 *)blockbuf;

    //Attempt to get inode data into buffer
    if (!ext4_disk_rw((void *)inodes, byte, BLOCK_SIZE(sb4), 0)) {
        printf("[ext4_test]: Error reading inode table.\n");
        goto out2;
    }

    //We are looking at the root inode, so offset to the root (2nd Inode)
    Inode_ext2 *root = inodes + EXT2_ROOT_INO;
    printf("[ext4_test]: Root inode: mode: %o, blocks: %u\n", root->i_mode, root->i_blocks);

    //See if i_block is interpreted as extent header
    if (root->i_flags & EXT4_EXTENTS_FL) {
        //Attempt to read out extent data of the inode block
        Extentheader *eh = (Extentheader *)root->i_block;
        if (eh->eh_magic != EXT4_EXTENT_MAGIC) {
            printf("[ext4_test]: Error reading root inode extent.\n");
            goto out2;
        }
        printf("[ext4_test]: Root extent: magic: 0x%04x, depth: %u, entries: %u\n", eh->eh_magic, eh->eh_depth, eh->eh_entries);

        //If the depth of the extent is 0, we are at the leaf node, and therefore the extent is right after the extent header
        if (eh->eh_depth == 0) {
            printf("[ext4_test]: Extents are fully in i_block\n");
            Extent *ee = (void *)root->i_block + sizeof(Extentheader);

            //Get block data location from ee_start, ee_len, and ee_block
            printf("[ext4_test]: Extent: starts: %u, len: %u, block: %u\n", ee->ee_start, ee->ee_len, ee->ee_block);
            size = 1024 << sb4.s_log_block_size;
            byte = BLOCK_OFFSET(ee->ee_start, size);
            if(!ext4_disk_rw(blockbuf, byte, size, 0)){
                printf("[ext4_test]: Unable to read directory entries.\n");
                goto out2;
            }
            //Start reading out directory entries.  Once we find 0xde00 in file_type_name_len, 
            //we hit the tail tail of the directory entires and there are no more.
            Direntry *dirents = blockbuf;
            do {
                if (dirents->file_type_name_len == 0xde00) {
                    printf("[ext4_test]: Dirent tail\n");
                    break;
                }
                printf("[ext4_test]: Dirent: %u (type: 0x%x) ('%.256s')\n", dirents->inode, dirents->file_type_name_len >> 8, dirents->name);
                dirents = (void *)dirents + dirents->rec_len;
            } while (1);
        }
    }
    else {
        printf("[ext4_test]: Root uses block pointers.\n");
    }

out2:
    cpage_free(inodes);
    cpage_free(blockbuf);
    */
}

//File system functions to add!!!

//int vfs_open(const *char path, int stat);
//int vfs_close(int fd);
//bool vfs_read(char *buff, int fd, int size);
//bool vfs_write(char *buff, int fd, int size);
//int vfs_create(PATH(path), int flags);
//int vfs_stat(PATH(path), struct stat *stat);
