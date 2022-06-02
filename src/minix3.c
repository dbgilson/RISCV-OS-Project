#include <kmalloc.h>
#include <printf.h>
#include <stat.h>
#include <memhelp.h>
#include <strhelp.h>
#include <minix3.h>
#include <cpage.h>
#include <virtioblk.h>

//This function is a test function developed by Dr. Marz to recursively goes through the inodes 
//given by the Superblock and prints out the absolute path names found at the inodes.
void minix_print(uint32_t inode, const Superblock *sb, char *path){
    void *blockbuf = cpage_znalloc(1);
    Inode ino;
    DirEntry de;
    uint64_t byte, i;
    uint32_t len;
    int ret;

    //Get byte offset to logical block address of the given inode
    byte = OFFSET(inode, sb->imap_blocks, sb->zmap_blocks);
    i    = ALIGN_DOWN_POT(byte, BLOCK_SIZE);

    //Put that data into our buffer
    memset(blockbuf, 0, BLOCK_SIZE);
    if(!minix_disk_rw(blockbuf, i, BLOCK_SIZE, 0)){
        printf("[minix_test]: Unable to read inode %d (%d)\n", inode, ret);
        return;
    }
    i = byte - i;
    memcpy(&ino, blockbuf + i, sizeof(ino));

    //Read out the path given by the input variable
    if (strlen(path) > 0) {
        printf("[minix_test]: %s\n", path);
    }
    //If the file is a directory, it contains a path to other inodes, so
    //recursively use the minix_print function to read out the paths in the filesystem
    if (S_FMT(ino.mode) == S_IFDIR) {
        len = strlen(path);
        strcpy(path + len, "/");
        //First look at first 7 zones (direct pointers) for directory entries
        for (i = 0; i < 7; i++) {
            if (ino.zones[i] == 0) {
                continue;
            }
            byte = ino.zones[i] * BLOCK_SIZE;
            if(!minix_disk_rw(blockbuf, byte, BLOCK_SIZE, 0)){
                printf("[minix_test]: Unable to read inode %d (%d)\n", inode, ret);
                return;
            }
            //If zone[i] != 0, it holds at least 1 DirEntry, so go through the data
            for (i = 128; i < ino.size; i += sizeof(DirEntry)) {
                memcpy(&de, blockbuf + i, sizeof(DirEntry));
                strcpy(path + len + 1, de.name);
                minix_print(de.inode, sb, path);
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
                    strcpy(path + len + 1, de.name);
                    minix_print(de.inode, sb, path);

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
                    strcpy(path + len + 1, de.name);
                    minix_print(de.inode, sb, path);
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
                    strcpy(path + len + 1, de.name);
                    minix_print(de.inode, sb, path);
                }
            }
    }
    cpage_free(blockbuf);
}

//This is a test function (created by Dr. Stephen Marz) to invoke the minix_print function
//seen above and print out all the paths of the minix filesystem
void minix_test(void){
    int ret;
    Superblock sb;
    char *path = (char *)cpage_znalloc(1);
    void *blockbuf = (void *)cpage_znalloc(1);

    //Attempt to read superblock
    if(!minix_disk_rw(blockbuf, BLOCK_SIZE, BLOCK_SIZE, 0)){
        printf("[minix_test]: Unable to read superblock (%d).\n", ret);
        goto out;
    }
    memcpy(&sb, blockbuf, sizeof(sb));
    
    //Check magic value of the file system.  If it is not the minix3 magic,
    //then we can't read from it.
    if (sb.magic != MINIX3_MAGIC) {
        printf("[minix_test]: Magic, This is not a Minix 3 file system.\n");
        printf("Recieved magic is: 0x%08lx\n", sb.magic);
        goto out;
    }
    //Start at root, give the superblock, and start recursively printing
    //the filesystem's paths. 
    minix_print(1, &sb, path);
out:
    cpage_free(path);
    cpage_free(blockbuf);
}