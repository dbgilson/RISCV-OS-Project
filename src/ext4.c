#include <kmalloc.h>
#include <printf.h>
#include <stat.h>
#include <memhelp.h>
#include <ext4.h>
#include <cpage.h>
#include <virtioblk.h>

//Made by Dr. Marz, gets the extent header from a given block
static struct extent_header *get_extent_header(uint32_t blocks[]){
    struct extent_header *ehdr = (struct extent_header *)blocks;
    return ehdr;
}

//Made by Dr. Marz, calculates the number of block groups of the superblock
static uint64_t NUM_BLOCK_GROUPS(const Superblock_ext4 *sb){
    return (sb->s_blocks_count - 1) / sb->s_blocks_per_group + 1;
}

//Made by Dr. Marz, calculates the block offest given a block and blocksize
static uint64_t BLOCK_OFFSET(uint64_t block, uint32_t blocksize){
    return 1024 + (block - 1) * blocksize;
}

//Made by Dr. Marz, calculates the block of the inode given the groupdescriptor
static uint64_t INODE_TABLE_BLOCK(const Groupdesc *bg){
    uint64_t itab_lo = bg->bg_inode_table;
    uint64_t itab_hi = bg->bg_inode_table_hi;
    return itab_lo | (itab_hi << 32);
}

//Made by Dr. Marz, calculates the block size of the system using the superblock's s_log_block_size to 
//multiply the block size.
static uint64_t BLOCK_SIZE(const Superblock_ext4 *sb){
    return 1024 << sb->s_log_block_size;
}

//This function is a test function developed by Dr. Marz to print the directory entries of the 
//root inode.  I am unable to get it working as of now due to by ext4_disk_rw messing up, which in turn
//won't let us read the data from hdd3.dsk, which was the ext4test.dsk.
void ext4_print_root(const Groupdesc *bg, const Superblock_ext4 *sb){
    uint64_t byte;
    uint64_t offset;
    uint64_t size;
    uint64_t i;
    int ret;
    void *blockbuf = NULL;
    Inode_ext2 *inodes  = NULL;

    //Get byte offset into the inode table in order to start reading from them
    byte = BLOCK_OFFSET(INODE_TABLE_BLOCK(bg), BLOCK_SIZE(sb));
    size = sb->s_inodes_per_group * sb->s_inode_size;
    size = ALIGN_UP_POT(size, 512);
    blockbuf= cpage_znalloc(1);
    inodes = (Inode_ext2 *)blockbuf;

    //Attempt to get inode data into buffer
    if (!ext4_disk_rw((void *)inodes, byte, BLOCK_SIZE(sb), 0)) {
        printf("[ext4_test]: Error reading inode table.\n");
        goto out;
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
            goto out;
        }
        printf("[ext4_test]: Root extent: magic: 0x%04x, depth: %u, entries: %u\n", eh->eh_magic, eh->eh_depth, eh->eh_entries);

        //If the depth of the extent is 0, we are at the leaf node, and therefore the extent is right after the extent header
        if (eh->eh_depth == 0) {
            printf("[ext4_test]: Extents are fully in i_block\n");
            Extent *ee = (void *)root->i_block + sizeof(Extentheader);

            //Get block data location from ee_start, ee_len, and ee_block
            printf("[ext4_test]: Extent: starts: %u, len: %u, block: %u\n", ee->ee_start, ee->ee_len, ee->ee_block);
            size = 1024 << sb->s_log_block_size;
            byte = BLOCK_OFFSET(ee->ee_start, size);
            if(!ext4_disk_rw(blockbuf, byte, size, 0)){
                printf("[ext4_test]: Unable to read directory entries.\n");
                goto out;
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

out:
    cpage_free(inodes);
    cpage_free(blockbuf);
}

//This function is a test function developed by Dr. Marz made to invoke ext4_print_root() by first getting
//the superblock data of the filesystem (hdd3.dsk is the ext4test.dsk, just renamed)
void ext4_test(void){
    uint64_t size;
    int ret;
    Superblock_ext4 sb;
    Groupdesc bg;
    char *path = cpage_znalloc(1);
    void *blockbuf = cpage_znalloc(1);
    size = ALIGN_UP_POT(sizeof(sb), 512);

    //Block offset of superblock is 1024, so start from that offset and read the size of a superblock (ext4)
    if(!ext4_disk_rw(blockbuf, 1024, size, 0)){
        printf("[ext4_test]: Unable to read Superblock_ext4 (%d).\n", ret);
        goto out;
    }
    memcpy(&sb, blockbuf, sizeof(sb));

    //Check the magic number of the superblock.  If not ext4, then it doesn't work.
    if (sb.s_magic != EXT4_MAGIC) {
        printf("[ext4_test]: This is not a Ext4 file system (magic: 0x%04x).\n", sb.s_magic);
        goto out;
    }
    else {
        printf("[ext4_test]: (Label: %.16s, Block size: %u)\n", sb.s_volume_name, 1024 << sb.s_log_block_size);
    }

    //Get the first block group located right after the first superblock
    size = ALIGN_UP_POT(sizeof(bg), 512);
    if(!ext4_disk_rw(blockbuf, 2048, size, 0)){
        printf("[ext4_test]: Unable to read block group\n");
        goto out;
    }
    memcpy(&bg, blockbuf, sizeof(bg));

    //Test reading of block device by using ext4_print_root
    ext4_print_root(&bg, &sb);
out:
    cpage_free(path);
    cpage_free(blockbuf);
}