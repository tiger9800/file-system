#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include "msg_types.h"
#include "cache_info.h"

#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

const int INODES_PER_BLOCK = BLOCKSIZE/INODESIZE;
const int ENTRIES_PER_BLOCK = BLOCKSIZE/sizeof(struct dir_entry);

static struct block_cache blockCache;
static struct inode_cache inodeCache;
static struct block_cache_entry blockLRU;
static struct inode_cache_entry inodeLRU;

static bool* inodemap;
static bool* blockmap;
static int NUM_BLOCKS;
static int NUM_INODES;

static void start_user_program(char *argv[]);
static int getFreeInodes();
static int changeStateBlocks(struct inode* currNode, bool state);
static void getTakenBlocks();
static int init();
static void handleMsg(struct my_msg *msg, int pid);

static void open(struct my_msg *msg, int pid);
static void create(struct my_msg *msg, int pid);
static void link(struct link_msg *msg, int pid);
static void unlink(struct my_msg *msg, int pid);
static void read(struct my_msg *msg, int pid);
static void write(struct my_msg *msg, int pid);
static void mkdir(struct my_msg *msg, int pid);
static void rmdir(struct my_msg *msg, int pid);
static void seek(struct my_msg *msg, int pid);
static void chdir(struct my_msg *msg, int pid);
static void stat(struct stat_msg *msg, int pid);
static void sync();
static void shutdown();

static int getInodeNumber(int curr_dir, char *pathname);
static int search(int start_inode, char *pathname);
static struct inode accessInode(int curr_num);
static int searchInDirectory(struct inode *curr_inode, char *token);
static int find_entry_in_block(int block, char *token, int num_entries_left);
static int get_num_blocks(int size);
static int fillFreeEntry(int blockNum, int index, int file_inode_num, char *entry_name);
static int createFileInDir(int dir_inode_num, char* file_name, int pref_inum, int type);
static int createNewDirEntry(int dir_inode_num, char* file_name, int pref_inum, int type);
static int writeInodeToDisc(int inode_num, struct inode inode_struct);
static int findFreeInodeNum();
static int createDirEntry(int curr_dir, char *pathname, int pref_inum, int type);
static int writeNewDirEntry(char* file_name, int file_inode_num, int dir_inode_num);
static int updateFreeEntry(struct inode *curr_dir_inode, char *file_name, int file_inode_num);
static int updateFreeEntryInBlock(int blockNum, char* entry_name, int file_inode_num, int num_entries_left);
static int find_free_block();
static int eraseFile(int inode_num);
static int readFromInode(struct inode inode_to_read, int size, int start_pos, char* buf_to_read);
static char* ReadBlock(int block_num, int start_pos, int bytes_left, char* buf_to_read);
static int linkInodes(int curr_dir, char *oldname, char *newname);
static int createRegFile();
static int getParentDir(int curr_dir, char **pathname);
static int unlinkInode(int curr_dir, char *pathname);
static int removeDirEntry(int dir, char *file_name);
static int removeEntryInBlock(int blockNum, char *entry_name, int file_inode_num, int num_entries_left);
static int freeDirEntry(int blockNum, int index);
static int deleteInode(int inode_num);
static int writeToInode(int inode_num, int size, int start_pos, char* buf_to_write);
static int WriteBlock(int block_num, int start_within_block, int bytes_left, char* buf_to_write);
static int createDirectory(int parent_inum);
static int removeDirectory(int curr_dir, char *pathname);
static bool dirIsEmpty(struct inode curr_dir_inode);
static bool dirBlockIsEmpty(int blockNum, int num_entries_left, int start_dir_entry);
static struct Stat* getStat(int curr_dir, char * pathname);
static void initCaches();
static int getHashcode(int num, int size);
static int evictBlock();
static int evictInode();
static int insertBlock(int num);
static int insertInode(int num);
static struct inode_cache_entry *searchInode(int num);
static struct block_cache_entry *searchBlock(int num);
static char * accessBlock(int num);
static int modifyBlock(int num, char *block_buf);
static void printBlockHashQueue(int hashcode);
static void printBlockLRUQueue();
static void printBlockQueues();
static void removeBlockFromHash(struct block_cache_entry *block);
static void removeBlockFromLRU(struct block_cache_entry *block);
static void insertBlockToLRU(struct block_cache_entry *block);
static void insertBlockToHash(struct block_cache_entry *block, int hashcode);
static void updateBlockLRU(struct block_cache_entry *block);
static int fillHoles(int inode_num, int pos);
static void printInodeHashQueue(int hashcode);
static void printInodeLRUQueue();
static void printInodeQueues();
static void removeInodeFromHash(struct inode_cache_entry *inode);
static void removeInodeFromLRU(struct inode_cache_entry *inode);
static void insertInodeToLRU(struct inode_cache_entry *inode);
static void insertInodeToHash(struct inode_cache_entry *inode, int hashcode);
static void updateInodeLRU(struct inode_cache_entry *inode);
static int modifyInode(int num, struct inode new_inode_struct);
static void writeDirtyInodes();
static void writeDirtyBlocks();
static void cleanBlocks(struct inode *inode_struct);
static int fillAndWrite(int inode_num, struct inode inode_to_write, int size, int start_pos, char* buf_to_write);


int main(int argc, char *argv[]) {
    printf("Blocksize: %i\n", BLOCKSIZE);
    printf("# dir_entries per block: %i\n", BLOCKSIZE/(int)sizeof(struct dir_entry));
    if (Register(FILE_SERVER) == ERROR) {
        return ERROR;
    }
    // Now we can get the number of inodes.
    if (init() == ERROR) {
        return ERROR;
    }
    printf("Inited\n");
    if(argc > 1)
        start_user_program(argv);
    // Printing occupied blocks and inodes.
    int i;
    for (i = 0; i < NUM_INODES + 1; i++) {
        if (inodemap[i] == false) {
            printf("Inode #%d is occupied\n", i);
        }
    }

    for (i = 0; i < NUM_BLOCKS; i++) {
        if (blockmap[i] == false) {
            printf("Block #%d is occupied\n", i);
        }
    }

    while (1) {
        struct my_msg msg;
        int pid = Receive(&msg);
        if (pid == ERROR) {
            fprintf(stderr, "Receive() failed.\n");
            continue;
        } else if (pid == 0) {
            fprintf(stderr, "Recieve() failed to avoid deadlock\n");
            // break;
            // Change "break" to "continue."
            Pause();
            continue;
        }
        handleMsg(&msg, pid);
        Reply((void *)&msg, pid);
    }
    return 0;
}

/* Function that starts another process */
static void start_user_program(char *argv[]) {
        int child;
        child = Fork();
        if (child == 0) {
            Exec(argv[1], argv+1);
        }
}

/* Function that initializes the inodemap */
static int getFreeInodes() {
    char *buf = accessBlock(1);
    if (buf == NULL) {
        printf("Error\n");
        return ERROR;
    }
    struct inode *currNode = (struct inode *)buf + 1; //address of the first inode
    // File header is occupied.
    inodemap[0] = false;
    int node_count = 1;
    int sector = 1;

    while(node_count <= NUM_INODES) {
        printf("Before second while_loop: node_count is %d\n", node_count);
        while (node_count / INODES_PER_BLOCK != sector) {
            printf("node_count is %d\n", node_count);
            if (node_count > NUM_INODES) {
                break;
            }
            else if(currNode->type == INODE_FREE) {
                inodemap[node_count] = true;
            } else {
                inodemap[node_count] = false;
                if (changeStateBlocks(currNode, false) == ERROR) {
                    return ERROR;
                }
            }
            currNode++;
            node_count++;
        }
        sector++;
        char *buf = accessBlock(sector);
        if (buf == NULL) {
            printf("Error\n");
            return ERROR;
        }
        currNode = (struct inode *)buf;
    } 
    return 0;    
}

/* Function that changes the states of all the data blocks */
static int changeStateBlocks(struct inode *currNode, bool state) {
    int num_blocks = get_num_blocks(currNode->size);
    int i;
    for (i = 0; i < MIN(NUM_DIRECT, num_blocks); i++) {
        blockmap[currNode->direct[i]] = state;
        if (state == true) currNode->direct[i] = 0;
    }
    if (num_blocks > NUM_DIRECT) {//then we need to search in the indirect block
        int block_to_read = currNode->indirect;
        int *indirect_buf = (int *)accessBlock(block_to_read);
        if (indirect_buf == NULL) {
            return ERROR;
        }
        int j;
        for (j = 0; j < (num_blocks - NUM_DIRECT); j++) {
            blockmap[indirect_buf[j]] = state;
            if (state == true) indirect_buf[j] = 0;
        }
        blockmap[block_to_read] = state;
        if (state == true) block_to_read = 0;
    }
    return 0;
}

/* Function that initializes all occupied blocks to false in the blockmap */
static void getTakenBlocks() {
    // Boot block.
    blockmap[0] = false;
    // Inodes' blocks.
    int inode_size = (NUM_INODES + 1) * INODESIZE;
    int num_blocks = get_num_blocks(inode_size);
    int i;
    for (i = 1; i <= num_blocks; i++) {
        blockmap[i] = false;
    }
}

/* Function that initializes the file server */
static int init() {
    initCaches();
    char *buf = accessBlock(1);
    if (buf == NULL) {
        printf("Error\n");
        return ERROR;
    }
    struct fs_header *header = (struct fs_header*)buf;
    //Now we can get the number of inodes
    NUM_INODES = header->num_inodes;
    NUM_BLOCKS = header->num_blocks;
    inodemap = malloc(sizeof(bool) * (NUM_INODES + 1));
    blockmap = malloc(sizeof(bool) * NUM_BLOCKS);

    int i;
    for(i = 0; i < NUM_BLOCKS; i++) {
        blockmap[i] = true;
    }
    getTakenBlocks();
    if (getFreeInodes() == ERROR) {
        return ERROR;
    }
    return 0;
}

/* Handle the messages of different kinds */
static void handleMsg(struct my_msg *msg, int pid) {
    switch(msg->type) {
        case OPEN:
            open(msg, pid);
            break;
        case CREATE:
            create(msg, pid);
            break;
        case READ:
            read(msg, pid);
            break;
        case WRITE:
            write(msg, pid);
            break;
        case LINK:
            link((struct link_msg *)msg, pid);
            break;
        case UNLINK:
            unlink(msg, pid);
            break;
        case MKDIR:
            mkdir(msg, pid);
            break;
        case RMDIR:
            rmdir(msg, pid);
            break;
        case SEEK:
            seek(msg, pid);
            break;
        case CHDIR:
            chdir(msg, pid);
            break;
        case STAT:
            stat((struct stat_msg*)msg, pid);
            break;
        case SYNC:
            sync();
            break;
        case SHUTDOWN:
            shutdown();
            break;
    }
}

/* Replies to the message of type STAT */
static void stat(struct stat_msg *msg, int pid) {
    if (msg->pathname == NULL || msg->statbuf == NULL) {
        msg->numeric1 = ERROR;
        return;
    }
    int curr_dir = msg->numeric1;
    if (inodemap[curr_dir]) {
        // This inode number is free.
        msg->numeric1 = ERROR;
        return;
    }

    char pathname[MAXPATHNAMELEN];
    if (CopyFrom(pid, pathname, msg->pathname, msg->size) == ERROR) {
        msg->numeric1 = ERROR;
        return;
    }
    // msg->numeric1 initially contains a current directory.
    // Reply with a message that has an inode number of the opened file or ERROR.

    struct Stat* stats = getStat(curr_dir, pathname);
    if(stats == NULL) {
        msg->numeric1 = ERROR;
    }

    CopyTo(pid, msg->statbuf, stats, sizeof(struct Stat));
    msg->numeric1 = 0;

    free(stats);
    
}

/* Returns a Stat struct filled with useful info about inode */
static struct Stat* getStat(int curr_dir, char * pathname) {
    int inode_num = getInodeNumber(curr_dir, pathname);
    if(inode_num == ERROR) {
        return NULL;
    }
    struct inode statInode = accessInode(inode_num);

    struct Stat* stats = malloc(sizeof(struct Stat));
    if(stats == NULL) {
        return NULL;
    }

    stats->type = statInode.type;
    stats->inum = inode_num;
    stats->size = statInode.size;
    stats->nlink = statInode.nlink;

    return stats;
}

/* Replies to the message of type SEEK */
static void seek(struct my_msg *msg, int pid) {
    (void)pid;
    int file_inode_num = msg->numeric1;
    int curr_pos = msg->numeric2;
    int whence = msg->numeric3;
    int offset = msg->numeric4;

    struct inode file_inode = accessInode(file_inode_num);

    int size = file_inode.size;

    int rel_pos;
    if(whence == SEEK_SET) {
        rel_pos = 0;
    }
    else if(whence == SEEK_CUR) {
        rel_pos = curr_pos;
    }
    else {
        rel_pos = size;
    }
    if(rel_pos + offset < 0) {
        msg->numeric1 = ERROR;
        return;
    }
    msg->numeric1 = rel_pos + offset;
}

/* Replies to the message of type OPEN */
static void open(struct my_msg *msg, int pid) {
    if (msg->ptr == NULL) {
        msg->numeric1 = ERROR;
        return;
    }
    int curr_dir = msg->numeric1;
    if (inodemap[curr_dir]) {
        // This inode number is free.
        msg->numeric1 = ERROR;
        return;
    }
    char pathname[MAXPATHNAMELEN];
    if (CopyFrom(pid, pathname, msg->ptr, msg->numeric5) == ERROR) {
        msg->numeric1 = ERROR;
        return;
    }
    // msg->numeric1 initially contains a current directory.
    // Reply with a message that has an inode number of the opened file or ERROR.
    msg->numeric1 = getInodeNumber(curr_dir, pathname);
    // TracePrintf(0, "getInodeNumber returns %i\n", msg->numeric1);
    // Reply message also contains a reuse count.
    if(msg->numeric1 != ERROR) {
        msg->numeric2 = accessInode(msg->numeric1).reuse;
    } 
}

/* Replies to the message of type CHDIR */
static void chdir(struct my_msg *msg, int pid) {
    if (msg->ptr == NULL) {
        msg->numeric1 = ERROR;
        return;
    }
    int curr_dir = msg->numeric1;
    if (inodemap[curr_dir]) {
        // This inode number is free.
        msg->numeric1 = ERROR;
        return;
    }
    char pathname[MAXPATHNAMELEN];
    if (CopyFrom(pid, pathname, msg->ptr, msg->numeric5) == ERROR) {
        msg->numeric1 = ERROR;
        return;
    }
    // msg->numeric1 initially contains a current directory.
    // Reply with a message that has an inode number of the opened file or ERROR.
    msg->numeric1 = getInodeNumber(curr_dir, pathname);
    // TracePrintf(0, "getInodeNumber returns %i\n", msg->numeric1);
    // Reply message also contains a reuse count.
    if(msg->numeric1 != ERROR) {
        if(accessInode(msg->numeric1).type != INODE_DIRECTORY) {//there cannot be a file with the same name as the directory, so
        //if there is, then the directory does not exist.
            msg->numeric1 = ERROR;
        }
    }
}

/* Replies to the message of type CREATE */
static void create(struct my_msg *msg, int pid) {
    if (msg->ptr == NULL) {
        msg->numeric1 = ERROR;
        return;
    }
    int curr_dir = msg->numeric1;
    if (inodemap[curr_dir]) {
        // This inode number is free.
        msg->numeric1 = ERROR;
        return;
    }
    char pathname[MAXPATHNAMELEN];
    if (CopyFrom(pid, pathname, msg->ptr, msg->numeric5) == ERROR) {
        msg->numeric1 = ERROR;
        return;
    }
    // TracePrintf(0, "Want to create a file with pathname %s\n", pathname);
    // Check that last char of pathname is not "/"
    if (pathname[strlen(pathname) - 1] == '/') {
        msg->numeric1 = ERROR;
        return;
    }
    // The third argument is the desired inum in the dir_entry. 
    // If inum = -1, we want to take a new unique inode number.
    // Function returns an inode number or ERROR.
    msg->numeric1 = createDirEntry(curr_dir, pathname, -1, INODE_REGULAR);
    
    // Reply message also contains a reuse count.
    if(msg->numeric1 != ERROR) {
        msg->numeric2 = accessInode(msg->numeric1).reuse;
    }
}

/* Creates a directory entry in the directory with inum = currr_dir.
   This directory entry contains pathname and pref_inum. 
   If pref_inum = -1, we want to find a free unique inode number.
   type tells what type of the inode is about to be created.
   Possible types are INODE_REGULAR or INODE_DIRECTORY. */
static int createDirEntry(int curr_dir, char *pathname, int pref_inum, int type) {
    int parent_dir = getParentDir(curr_dir, &pathname);
    if (parent_dir == ERROR) {
        return ERROR;
    }
    int new_file_inode_num = createFileInDir(parent_dir, pathname, pref_inum, type);
    return new_file_inode_num;
}

/* Returns an inode number of the parent directory. 
   pathname becomes a name relative to this parent directory.*/
static int getParentDir(int curr_dir, char **pathname) {
    int i;
    int last_slash = -1;
    for(i = strlen(*pathname) - 1; i >= 0; i--) {
        if((*pathname)[i] == '/') {
            last_slash = i;
            break;
        }
    }
    if(last_slash == -1) { 
        return curr_dir;
    } else if(last_slash == 0) {
        (*pathname)++;
        return ROOTINODE;
    } else {
        // We need to locate the parent directory inode.
        char* file_name = *pathname + i + 1;
        // Check if slash was the last character in a path name.
        if (strlen(file_name) == 0) {
            *pathname = ".";
        }
        (*pathname)[i] = '\0';
        int parent_dir_inode_num = getInodeNumber(curr_dir, *pathname);
        if (parent_dir_inode_num == ERROR) {
            return ERROR;
        }
        *pathname = file_name;
        return parent_dir_inode_num;
    }
}

/* Replies to the message of type READ */
static void read(struct my_msg *msg, int pid) {
    if (msg->ptr == NULL) {
        msg->numeric1 = ERROR;
        return;
    }
    int file_inode_num = msg->numeric1;
    int start_pos = msg->numeric2;
    int reuse_count = msg->numeric3;
    int size_to_read = msg->numeric4;
    if(inodemap[file_inode_num] == true) {
        fprintf(stderr, "The inode is free, so we can't read from it\n");
        msg->numeric1 = ERROR;
        return;
    }
    struct inode file_inode = accessInode(file_inode_num);
    if(file_inode.reuse != reuse_count) {
        fprintf(stderr, "Reuse is different: someone else created a different file in the same inode\n");
        msg->numeric1 = ERROR;
        return;
    }
    char buf_to_read[size_to_read];
    int sizeRead = readFromInode(file_inode, size_to_read, start_pos, buf_to_read);
    if(sizeRead == ERROR) {
        msg->numeric1 = ERROR;
        return;
    }
    if (CopyTo(pid, msg->ptr, buf_to_read, sizeRead) == ERROR) {
        msg->numeric1 = ERROR;
        return;
    }
    msg->numeric1 = sizeRead;
}

/* Attempts to read "size" bytes from "inode_to_read".
   Returns how much was actually read from the file. */
static int readFromInode(struct inode inode_to_read, int size, int start_pos, char* buf_to_read) {
    if (start_pos >= inode_to_read.size) {
        return 0;
    }
    int size_to_read = MIN(inode_to_read.size - start_pos, size);

    if (size_to_read <= 0) {
        return 0;
    }

    int bytes_left = size_to_read;
    int num_blocks = get_num_blocks(inode_to_read.size);
    
    int starting_block = start_pos/BLOCKSIZE;
    int start_within_block = start_pos % BLOCKSIZE;
    int i;
    for (i = starting_block; i < MIN(NUM_DIRECT, num_blocks) && bytes_left > 0; i++) {
        buf_to_read = ReadBlock(inode_to_read.direct[i], start_within_block, bytes_left, buf_to_read);
        if(buf_to_read == NULL){
            return ERROR;
        }
        bytes_left -= (BLOCKSIZE - start_within_block);
        start_within_block = 0;
    }

    if (num_blocks > NUM_DIRECT && bytes_left > 0) {
        // Then we need to search in the indirect block.
        int block_to_read = inode_to_read.indirect;
        int *indirect_buf = (int *)accessBlock(block_to_read);
        if (indirect_buf == NULL) {
            return ERROR;
        }
        int j;
        for (j = i - NUM_DIRECT; j < (num_blocks - NUM_DIRECT) && bytes_left > 0; j++) {
            buf_to_read = ReadBlock(indirect_buf[j], start_within_block, bytes_left, buf_to_read);
            if(buf_to_read == NULL) {
                return ERROR;
            }
            bytes_left -= (BLOCKSIZE - start_within_block);
            start_within_block = 0;
        }
    }
    return size_to_read;
}

/* Attempts to read "size" bytes from block with block_num.
   Takes a starting position and # of bytes that are left to read as parameters.
   Returns how much was actually read from the block. */
static char* ReadBlock(int block_num, int start_pos, int bytes_left, char* buf_to_read) {
    int bytes_to_read = MIN(bytes_left, SECTORSIZE - start_pos);
    if (block_num == 0) {
        char zero_buf[BLOCKSIZE];
        memset(zero_buf, '\0', BLOCKSIZE);
        memcpy(buf_to_read, zero_buf+start_pos, bytes_to_read);
    } else {
        char *buf = accessBlock(block_num);
        if (buf == NULL) {
            return NULL;
        }
        memcpy(buf_to_read, buf+start_pos, bytes_to_read);
    }
    // Advance a buffer by bytes_to_read bytes.
    return buf_to_read + bytes_to_read;
}
/* Creates a new directory entry. Returns an inode number of the newly-created or existing file; returns ERROR if failure. */
static int createFileInDir(int dir_inode_num, char* file_name, int pref_inum, int type) {
    if (strlen(file_name) == 0 || strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
        // Can't create such directory entries manually.
        // TracePrintf(0, "Tried to create an invalid file\n");
        return ERROR;
    }
    if (strlen(file_name) > DIRNAMELEN) {
        fprintf(stderr, "Entry name %s is too long.\n", file_name);
        return ERROR;
    }
    // Format a file name.
    char entry_name[DIRNAMELEN];
    memset(entry_name, '\0', DIRNAMELEN);
    memcpy(entry_name, file_name, strlen(file_name));
    struct inode dir_inode = accessInode(dir_inode_num);

    // Look for this file in the directory.
    int inode_num = searchInDirectory(&dir_inode, entry_name);
    if (inode_num != ERROR) {
        // File with this name already exists in the directory.
        // TracePrintf(0, "File with this name already exists in the directory.\n");
        if (pref_inum == -1 && type == INODE_REGULAR) {
            // Our goal is to create a new file, but it already exists.
            // We need to erase the contents of this existing file.
            // Erase file contents and return.
            struct inode exist_inode = accessInode(inode_num);
            if(exist_inode.type == INODE_DIRECTORY) {
                return ERROR;
            } else {//the file is not a directory
                if(eraseFile(inode_num) == ERROR) {
                    return ERROR;
                }    
            }
        } else {
            // Our goal is to link an oldname to this newname, but a new name already exists in this directory.
            // Or our goal is to create a new directory, which already exists.
            fprintf(stderr, "Such directory entry already exists.\n");
            return ERROR;
        }
        
    } else {
        // TracePrintf(0, "A directory entry does not exist in the directory, so we create it from scratch.\n");
        // A directory entry does not exist in the directory, so we create it from scratch.
        inode_num = createNewDirEntry(dir_inode_num, entry_name, pref_inum, type);
        if(inode_num == ERROR) {
            fprintf(stderr, "Could not create a new directory entry\n");
            return ERROR;
        }
    }
    return inode_num;
}

/* Helper function that creates different types of directory entries.
   Returns an inode number of the newly-created file or ERROR. */
static int createNewDirEntry(int dir_inode_num, char* file_name, int pref_inum, int type) {
    if (pref_inum == -1) {
        // We want to find a new unique inode_num.
        if (type == INODE_REGULAR) {
            // Create a regular file.
            pref_inum = createRegFile();
        } else {
            // Create a directory.
            pref_inum = createDirectory(dir_inode_num);
        }
        if (pref_inum == ERROR) {
            return ERROR;
        }
    }
    struct inode inode_struct = accessInode(pref_inum);

    // Write a new directory entry with inum = pref_inum, name == file_name.
    if(writeNewDirEntry(file_name, pref_inum, dir_inode_num) == ERROR) {
        return ERROR;
    }
    // Increment a number of hard links to this inode_struct.
    inode_struct.nlink++;
    // We set inodemap[pref_inum] to false in this function.
    if (modifyInode(pref_inum, inode_struct) == ERROR) {
        return ERROR;
    }
    return pref_inum;
}

/* Helper function that creates a regular file in the directory.
   Returns an inode number of the newly-created file or ERROR. */
static int createRegFile() {
    int free_inode_num = findFreeInodeNum();
    if (free_inode_num == -1) {
        return ERROR;
    }
    struct inode inode_struct = accessInode(free_inode_num);
    // Initialize an inode fields.
    inode_struct.type = INODE_REGULAR;
    inode_struct.size = 0;
    cleanBlocks(&inode_struct);
    inode_struct.reuse++;
    // Will increment this field when we create a dir_entry. 
    inode_struct.nlink = 0;
    if (modifyInode(free_inode_num, inode_struct) == ERROR) {
        return ERROR;
    }
    inodemap[free_inode_num] = false;
    return free_inode_num;
}
/* Returns a free inode number or ERROR if none exists. */
static int findFreeInodeNum() {
    int i;
    for(i = 0; i < NUM_INODES; i++) {
        if (inodemap[i]) {
            return i;
        }
    }
    return ERROR;
}
/* Performs an actual writing of a new directory entry to the block.     
   Returns 0 on SUCCESS, -1 on ERROR. */
static int writeNewDirEntry(char* file_name, int file_inode_num, int dir_inode_num) {
    struct inode curr_dir_inode = accessInode(dir_inode_num);
    if (curr_dir_inode.type != INODE_DIRECTORY) {
        fprintf(stderr, "The wrong type of parent directory.\n");
        return ERROR;
    }

    int last_block;
    if ((last_block = updateFreeEntry(&curr_dir_inode, file_name, file_inode_num)) < 0) {
        // Returns either SUCCESS (-2) or ERROR (-1).
        if (last_block == -2) {
            // We found a free entry and successfully updated it.
            return 0;
        } else {
            // We found a free entry but failed to update it.
            return ERROR;
        }
    }

    // There are no free directory entries in the directory dir_inode_num.
    int max_size = (NUM_DIRECT + BLOCKSIZE/sizeof(int)) * BLOCKSIZE;
    if (max_size == curr_dir_inode.size) {
        // Directory reached its maximum size.
        fprintf(stderr, "Directory reached its maximum size.\n");
        return ERROR;
    } else if(curr_dir_inode.size % BLOCKSIZE != 0){
        // There is still space in the last block that we can use.
        int index = (curr_dir_inode.size % BLOCKSIZE)/(sizeof(struct dir_entry));
        if (fillFreeEntry(last_block, index, file_inode_num, file_name) == ERROR) {
            fprintf(stderr, "fillFreeEntry failed.\n");
            return ERROR;
        }
    } else {
        // Allocate a new block.
        int free_block_num = find_free_block();
        if(free_block_num == ERROR) {
            fprintf(stderr, "No enough free blocks\n");
            return ERROR;
        }
        // Fill the first entry in this newly-allocated block.
        if (fillFreeEntry(free_block_num, 0, file_inode_num, file_name) == ERROR) {
            fprintf(stderr, "fillFreeEntry failed.\n");
            return ERROR;
        }
        int num_blocks = get_num_blocks(curr_dir_inode.size);
        // Look for a place where we can put this newly-allocated block.
        if(num_blocks < NUM_DIRECT) {
            // We can put free_block_num in the last direct block.
            curr_dir_inode.direct[num_blocks] = free_block_num; 
        } else {
            if(num_blocks == NUM_DIRECT) {
                curr_dir_inode.indirect = find_free_block();
                if (curr_dir_inode.indirect == ERROR) {
                    return ERROR;
                }
            }
            int *indirect_buf = (int *)accessBlock(curr_dir_inode.indirect);
            if(indirect_buf == NULL) {
                return ERROR;
            }
            indirect_buf[num_blocks - NUM_DIRECT] = free_block_num;
            if(modifyBlock(curr_dir_inode.indirect, (char *)indirect_buf) == ERROR) {
                return ERROR;
            }
        }
        // After all error-checkings, can finally change bitmap for new block and (possibly) new indirect block.
        blockmap[free_block_num] = false;
        if (num_blocks == NUM_DIRECT) blockmap[curr_dir_inode.indirect] = false;
    }
    // Must increment a size of the directory.
    curr_dir_inode.size += sizeof(struct dir_entry);
    if (modifyInode(dir_inode_num, curr_dir_inode) == ERROR) {
        return ERROR;
    }
    return 0;
}
/* Attepts to find an existing free directory entry in the curr_dir_inode.
   Returns -2 if such free enty is found; ERROR if some error occured; a number of existing last block in the directory if no free entry was found. */
static int updateFreeEntry(struct inode *curr_dir_inode, char *file_name, int file_inode_num) {
    int num_entries_left = curr_dir_inode->size / sizeof(struct dir_entry);
    int num_blocks = get_num_blocks(curr_dir_inode->size);
    int i;
    int last_block = 0;
    // Search in the direct blocks.
    for (i = 0; i < MIN(NUM_DIRECT, num_blocks); i++) {
        last_block = curr_dir_inode->direct[i];
        int return_val = updateFreeEntryInBlock(last_block, file_name, file_inode_num, num_entries_left);
        if(return_val != -3) {
            return return_val;
        }
        num_entries_left -= ENTRIES_PER_BLOCK;
    }
    // Search in the indirect blocks.
    if (num_blocks > NUM_DIRECT) {
        int block_to_read = curr_dir_inode->indirect;
        int *indirect_buf = (int *)accessBlock(block_to_read);
        if (indirect_buf == NULL) {
            return ERROR;
        }
        int j;
        for (j = 0; j < (num_blocks - NUM_DIRECT); j++) {
            last_block = indirect_buf[j];
            int return_val = updateFreeEntryInBlock(last_block, file_name, file_inode_num, num_entries_left);
            if(return_val != -3) {
                return return_val;
            }
            num_entries_left -= ENTRIES_PER_BLOCK;
        }
    }
    // No free entry was found. Return the last allocated block in this directory.
    return last_block;
}

/* Attepts to find a free block. 
   Sets its contents to 0s, which makes implementation of Read and Write easier.
   Returns the number of free block if found; ERROR if some error occured. */
static int find_free_block() {
    int i;
    for(i = 0; i < NUM_BLOCKS; i++) {
        if(blockmap[i]) {
            // Always fill a new block with zeros!
            char zero_buf[BLOCKSIZE];
            memset(zero_buf, '\0', BLOCKSIZE);
            if (modifyBlock(i, zero_buf) == ERROR) {
                return ERROR;
            }
            return i;
        }
    }
    return ERROR;
}

/* Attepts to find an allocated free dir_entry in a given block. 
   Returns -2 on SUCCESS, -1 on ERROR, -3 if no free entry was found in the block. */
static int updateFreeEntryInBlock(int blockNum, char* entry_name, int file_inode_num, int num_entries_left) {  
    struct dir_entry *dir_buf = (struct dir_entry *)accessBlock(blockNum);
    if (dir_buf == NULL) {
        return ERROR;
    }
    int i;
    for (i = 0; i < MIN(ENTRIES_PER_BLOCK, num_entries_left); i++) {
        if (dir_buf[i].inum == 0) {
            if (fillFreeEntry(blockNum, i, file_inode_num, entry_name) == ERROR) {
                return ERROR;
            }
            // Success!
            return -2;
        }
    }
    // No free entry was found.
    return -3;
}

/* Erases the contents of a regular file. 
   Returns 0 on SUCCESS, -1 on ERROR. */
static int eraseFile(int inum) {
    struct inode fileInode = accessInode(inum);
    if (changeStateBlocks(&fileInode, true) == ERROR) {
        return ERROR;
    }
    fileInode.size = 0;
    if(modifyInode(inum, fileInode) == ERROR) {
       return ERROR;
    }
    return 0;
}


/* Returns an inode number of a file with pathname relative to a directory 
   with inum = curr_dir. Returns an inode number on SUCCESS, -1 on ERROR. */
static int getInodeNumber(int curr_dir, char *pathname) {
    if (pathname[0] == '/') {
        // Pathname is absolute.
        // Remove leading /'s.
        while (pathname[0] == '/') {
            pathname++;
        }
        return search(ROOTINODE, pathname);
    } else {
        // Pathname is relative.
        return search(curr_dir, pathname);
    }
}

/* Performs an iterative search of a file with a given name.
   Returns an inode number on SUCCESS, -1 on ERROR. */
static int search(int start_inode, char *pathname) {
    int curr_num = start_inode;
    char *token = strtok(pathname, "/");
    while (token != NULL) {
        struct inode curr_inode = accessInode(curr_num);
        // Find a directory entry with name = token.
        curr_num = searchInDirectory(&curr_inode, token);
        if (curr_num == ERROR) {
            return ERROR;
        }
        token = strtok(NULL, "/");
    }
    return curr_num;
}
/* Returns an inode_num of the file we are looking for in the given directory
   or ERROR if the file is not there. */
static int searchInDirectory(struct inode *curr_inode, char *token) {
    if (curr_inode == NULL || curr_inode->type != INODE_DIRECTORY) {
        return ERROR;
    }
    if (strlen(token) > DIRNAMELEN) {
        fprintf(stderr, "Entry name %s is too long.\n", token);
        return ERROR;
    }
    // Format a file name.
    char entry_name[DIRNAMELEN];
    memset(entry_name, '\0', DIRNAMELEN);
    memcpy(entry_name, token, strlen(token));

    int num_entries_left = curr_inode->size / sizeof(struct dir_entry);
    int num_blocks = get_num_blocks(curr_inode->size);
    int i;
    // Search in direct blocks.
    for (i = 0; i < MIN(NUM_DIRECT, num_blocks); i++) {
        int out_inode = find_entry_in_block(curr_inode->direct[i], entry_name, num_entries_left);
        if (out_inode != ERROR) {
            return out_inode;
        } 
        num_entries_left -= ENTRIES_PER_BLOCK;
    }
    // Search in indirect blocks.
    if (num_blocks > NUM_DIRECT) {
        int block_to_read = curr_inode->indirect;
        int *indirect_buf = (int *)accessBlock(block_to_read);
        if (indirect_buf == NULL) {
            return ERROR;
        }
        int j;
        for (j = 0; j < (num_blocks - NUM_DIRECT); j++) {
            int out_inode = find_entry_in_block(indirect_buf[j], entry_name, num_entries_left);
            if (out_inode != ERROR) {
                return out_inode;
            }
            num_entries_left -= ENTRIES_PER_BLOCK;
        }
    }
    return ERROR;
}

/* Attempts to find a dir_entry in a block.
   Returns an inode number of a file we are looking for or ERROR. */
static int find_entry_in_block(int block, char *entry_name, int num_entries_left) {
    struct dir_entry *dir_buf = (struct dir_entry *)accessBlock(block);
    if (dir_buf == NULL) {
        return ERROR;
    }
    int i;
    for (i = 0; i < MIN(ENTRIES_PER_BLOCK, num_entries_left); i++) {
        if (dir_buf[i].inum != 0 && strncmp(dir_buf[i].name, entry_name, DIRNAMELEN) == 0) {
            return dir_buf[i].inum;
        }
    }
    return ERROR;
}

/* Returns a number of blocks that "size" of bytes occupied. */
static int get_num_blocks(int size) {
    int num_blocks = size / BLOCKSIZE;
    if (size % BLOCKSIZE != 0) {
        num_blocks++;
    }
    return num_blocks;
}

/* Fills a free dir_entry in a block at specified index. */
static int fillFreeEntry(int blockNum, int index, int file_inode_num, char *entry_name) {
    struct dir_entry *dir_buf = (struct dir_entry *)accessBlock(blockNum);
    if (dir_buf == NULL) {
        return ERROR;
    }

    dir_buf[index].inum = file_inode_num;
    memset(dir_buf[index].name, '\0', DIRNAMELEN);
    int size = MIN(strlen(entry_name) + 1, DIRNAMELEN);
    memcpy(dir_buf[index].name, entry_name, size);
    if (modifyBlock(blockNum, (char *)dir_buf) == ERROR) {
        return ERROR;
    }
    return 0;
}

/* Replies to the message of type LINK. */
static void link(struct link_msg *msg, int pid) {
    if (msg->oldname == NULL || msg->newname == NULL) {
        msg->numeric1 = ERROR;
        return;
    }
    int curr_dir = msg->numeric1;
    if (inodemap[curr_dir]) {
        // This inode number is free.
        msg->numeric1 = ERROR;
        return;
    }
    char old[MAXPATHNAMELEN];
    char new[MAXPATHNAMELEN];
    if (CopyFrom(pid, old, msg->oldname, msg->oldsize) == ERROR || CopyFrom(pid, new, msg->newname, msg->newsize) == ERROR) {
        msg->numeric1 = ERROR;
        return;
    }
    // Reply with a message that contains return status.
    msg->numeric1 = linkInodes(curr_dir, old, new);
}

/* Performs linking of two inodes. */
static int linkInodes(int curr_dir, char *oldname, char *newname) {
    int inode_num = getInodeNumber(curr_dir, newname);
    if (inode_num != ERROR) {
        fprintf(stderr, "File with newname already exists.\n");
        return ERROR;
    }
    // Get parent directories.
    int old_dir = getParentDir(curr_dir, &oldname);
    int new_dir = getParentDir(curr_dir, &newname);
    if (old_dir == ERROR || new_dir == ERROR) {
        fprintf(stderr, "Either oldname or newname has an invalid parent directory.\n");
        return ERROR;
    }
    int old_inode_num = getInodeNumber(old_dir, oldname);
    if (old_inode_num == ERROR) {
        fprintf(stderr, "Oldname file does not exists.\n");
        return ERROR;
    }

    struct inode old_inode = accessInode(old_inode_num);
    if (old_inode.type != INODE_REGULAR) {
        fprintf(stderr, "Oldname has a wrong type.\n");
        return ERROR;
    }

    int ret_inode_num = createDirEntry(new_dir, newname, old_inode_num, INODE_REGULAR);
    if (ret_inode_num != old_inode_num) {
        return ERROR;
    } else {
        return 0;
    }
}

/* Replies to the message of type UNLINK. */
static void unlink(struct my_msg *msg, int pid) {
    if (msg->ptr == NULL) {
        msg->numeric1 = ERROR;
        return;
    }
    int curr_dir = msg->numeric1;
    if (inodemap[curr_dir]) {
        // This inode number is free.
        msg->numeric1 = ERROR;
        return;
    }
    char pathname[MAXPATHNAMELEN];
    if (CopyFrom(pid, pathname, msg->ptr, msg->numeric5) == ERROR) {
        msg->numeric1 = ERROR;
        return;
    }
    // Reply with a message that contains return status.
    msg->numeric1 = unlinkInode(curr_dir, pathname);
}

/* Deletes a corresponding directory entry.
   If the nlink of a given inode drops to 0, we delete this inode. */
static int unlinkInode(int curr_dir, char *pathname) {
    // Get parent directory.
    int parent_dir = getParentDir(curr_dir, &pathname);
    if (parent_dir == ERROR) {
        fprintf(stderr, "File has an invalid parent directory.\n");
        return ERROR;
    }
    int inode_num = getInodeNumber(parent_dir, pathname);
    if (inode_num == ERROR) {
        fprintf(stderr, "This file does not exists.\n");
        return ERROR;
    }

    struct inode inode_struct = accessInode(inode_num);
    if (inode_struct.type != INODE_REGULAR) {
        fprintf(stderr, "File has a wrong type.\n");
        return ERROR;
    }
    int nlink = removeDirEntry(parent_dir, pathname);
    if (nlink == ERROR) {
        return ERROR;
    } else if (nlink == 0) {
        if (deleteInode(inode_num) == ERROR) {
            return ERROR;
        }
    }
    return 0;
}

/* Removes a corresponding directory entry in specified directory. */
static int removeDirEntry(int dir, char *file_name) {
     if (strlen(file_name) == 0 || strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
        // Can't delete such directory entries manually.
        return ERROR;
    }
    int file_inode_num = getInodeNumber(dir, file_name);
    if (file_inode_num == ERROR) {
        fprintf(stderr, "This file does not exists.\n");
        return ERROR;
    }
    struct inode inode_struct = accessInode(file_inode_num);
    struct inode curr_dir_inode = accessInode(dir);

    int num_entries_left = curr_dir_inode.size / sizeof(struct dir_entry);

    int num_blocks = get_num_blocks(curr_dir_inode.size);

    bool removed = false;
    int i;
    // Search in direct blocks.
    for (i = 0; i < MIN(NUM_DIRECT, num_blocks) && removed == false; i++) {
        int return_val = removeEntryInBlock(curr_dir_inode.direct[i], file_name, file_inode_num, num_entries_left);
        if(return_val == ERROR) {
            return ERROR;
        } else if (return_val == 0) {
            removed = true;
        }
        num_entries_left -= ENTRIES_PER_BLOCK;
    }
    // Search in indirect blocks.
    if (num_blocks > NUM_DIRECT && removed == false) {
        int block_to_read = curr_dir_inode.indirect;
        int *indirect_buf = (int *)accessBlock(block_to_read);
        if (indirect_buf == NULL) {
            return ERROR;
        }
        int j;
        for (j = 0; j < (num_blocks - NUM_DIRECT) && removed == false; j++) {
            int return_val = removeEntryInBlock(indirect_buf[j], file_name, file_inode_num, num_entries_left);
            if(return_val == ERROR) {
                return ERROR;
            } else if (return_val == 0) {
                removed = true;
            }
            num_entries_left -= ENTRIES_PER_BLOCK;
        }
    }
    if (removed == true) {
        // We succesfully removed a dir_entry.
        // Decrement a number of nlinks to the given file_inode_num.
        inode_struct.nlink--;
        if(modifyInode(file_inode_num, inode_struct) == ERROR) {
            return ERROR;
        }
        return inode_struct.nlink;
    }
    return ERROR;
}

/* Removes a corresponding directory entry in a specified block. 
   Returns 0 on SUCCESS, -1 on ERROR, -2 if no such entry was found. */
static int removeEntryInBlock(int blockNum, char *entry_name, int file_inode_num, int num_entries_left) {
    struct dir_entry *dir_buf = (struct dir_entry *)accessBlock(blockNum);
    if (dir_buf == NULL) {
        return ERROR;
    }
    int i;
    for (i = 0; i < MIN(ENTRIES_PER_BLOCK, num_entries_left); i++) {
        if (dir_buf[i].inum == file_inode_num && strncmp(dir_buf[i].name, entry_name, DIRNAMELEN) == 0) {
            if (freeDirEntry(blockNum, i) == ERROR) {
                return ERROR;
            }
            // Success!
            return 0;
        }
    }
    // No such entry was found.
    return -2;
}

/* Frees a directory entry. Returns 0 on success and -1 of ERROR. */
static int freeDirEntry(int blockNum, int index) {
    struct dir_entry *dir_buf = (struct dir_entry *)accessBlock(blockNum);
    if (dir_buf == NULL) {
        return ERROR;
    }
    dir_buf[index].inum = 0;
    memset(dir_buf[index].name, '\0', DIRNAMELEN);
    if (modifyBlock(blockNum, (char *)dir_buf) == ERROR) {
        return ERROR;
    }
    return 0;
}

/* Deletes an inode. Returns 0 on success and -1 of ERROR. */
static int deleteInode(int inode_num) {
    struct inode inode_struct = accessInode(inode_num);
    if (changeStateBlocks(&inode_struct, true) == ERROR) {
        return ERROR;
    }
    inode_struct.type = INODE_FREE;
    inode_struct.nlink = 0;
    inode_struct.size = 0;
    if (modifyInode(inode_num, inode_struct) == ERROR) {
        return ERROR;
    }
    inodemap[inode_num] = true;
    return 0;
}

/* Replies to the message of type WRITE. */
static void write(struct my_msg *msg, int pid) {
    TracePrintf(0, "max number of blocks is %i\n", (NUM_DIRECT + BLOCKSIZE/sizeof(int)));
    if (msg->ptr == NULL) {
        msg->numeric1 = ERROR;
        return;
    }
    int file_inode_num = msg->numeric1;
    int start_pos = msg->numeric2;
    int reuse_count = msg->numeric3;
    int size_to_write = msg->numeric4;
    if(inodemap[file_inode_num] == true) {
        fprintf(stderr, "The inode is free, so we can't read from it\n");
        msg->numeric1 = ERROR;
        return;
    }
    struct inode file_inode = accessInode(file_inode_num);
    if(file_inode.reuse != reuse_count) {
        fprintf(stderr, "Reuse is different: someone else created a different file in the same inode\n");
        msg->numeric1 = ERROR;
        return;
    }
    if(file_inode.type != INODE_REGULAR) {
        fprintf(stderr, "Wrong type of file.\n");
        msg->numeric1 = ERROR;
        return;
    }
    char buf_to_write[size_to_write];
    if (CopyFrom(pid, buf_to_write, msg->ptr, size_to_write) == ERROR) {
        msg->numeric1 = ERROR;
        return;
    }
    msg->numeric1 = fillAndWrite(file_inode_num, file_inode, size_to_write, start_pos, buf_to_write);
}

/* Writes "size" bytes to the inode with inode_num from buf_to_write, 
   starting at start_pos. Returns a number of bytes actually written. */
static int writeToInode(int inode_num, int size, int start_pos, char* buf_to_write) {
    int max_size = (NUM_DIRECT + BLOCKSIZE/(sizeof(int)))*BLOCKSIZE;
    struct inode inode_to_write = accessInode(inode_num);
    //determine how much we can write by subtracting what we have written from what we can write
    int left_to_write = max_size - inode_to_write.size;

    int size_to_write = MIN(left_to_write, size);
    if (size_to_write < 0) {
        return ERROR;
    } else if (size_to_write == 0) {
        return 0;
    }

    int bytes_left = size_to_write;

    int num_blocks = get_num_blocks(inode_to_write.size);
    
    int starting_block = start_pos/BLOCKSIZE;
    int start_within_block = start_pos % BLOCKSIZE;
    int i;
    // Write to direct blocks.
    for (i = starting_block; i < NUM_DIRECT && bytes_left > 0; i++) {
        // Allocate a new block if needed.
        if (i >= num_blocks || inode_to_write.direct[i] == 0) {
            int free_block_num = find_free_block();
            if(free_block_num == ERROR) {
                return size_to_write - bytes_left;
            }
            inode_to_write.direct[i] = free_block_num;
        }
        // Write to a block.
        int bytes_written = WriteBlock(inode_to_write.direct[i], start_within_block, bytes_left, buf_to_write);
        if (bytes_written == ERROR) {
            return size_to_write - bytes_left;
        }
        inode_to_write.size += bytes_written;
        if (modifyInode(inode_num, inode_to_write) == ERROR) {
            return size_to_write - bytes_left;
        }
        buf_to_write += bytes_written;
        bytes_left -= bytes_written;
        start_within_block = 0;
        blockmap[inode_to_write.direct[i]] = false;
    }
     // Write to indirect blocks.
    if (bytes_left > 0) {
        // Allocate an indirect block if needed.
        if(num_blocks <= NUM_DIRECT) {
            inode_to_write.indirect = find_free_block();
            if(inode_to_write.indirect == ERROR) {
                return size_to_write - bytes_left;
            }
        }
        // Now we definetely have the indirect block, so we can start writing into it.
        int *indirect_buf = (int *)accessBlock(inode_to_write.indirect);
        if (indirect_buf == NULL) {
            return size_to_write - bytes_left;
        }
        int j;
        for (j = i - NUM_DIRECT; bytes_left > 0; j++) {
            // Allocate a new block if needed.
            if (j + NUM_DIRECT >= num_blocks || indirect_buf[j] == 0) {
                int free_block_num = find_free_block();
                if (free_block_num == ERROR) {
                    return size_to_write - bytes_left;
                }
                indirect_buf[j] = free_block_num;
            } 
            int bytes_written = WriteBlock(indirect_buf[j], start_within_block, bytes_left, buf_to_write);
            if (bytes_written == ERROR) {
                return size_to_write - bytes_left;
            }
            inode_to_write.size += bytes_written;
            if (modifyInode(inode_num, inode_to_write) == ERROR) {
                return size_to_write - bytes_left;
            }
            buf_to_write += bytes_written;
            bytes_left -= bytes_written;
            start_within_block = 0;
            if (modifyBlock(inode_to_write.indirect, (char *)indirect_buf) == ERROR) {
                return size_to_write - bytes_left;
            }
            blockmap[indirect_buf[j]] = false;
            blockmap[inode_to_write.indirect] = false;
        }
    }
    return size_to_write;
}

/* Fills the holes caused by SEEK operation. */
static int fillHoles(int inode_num, int pos) {
    struct inode inode_to_write = accessInode(inode_num);
    int max_size = (NUM_DIRECT + BLOCKSIZE/(sizeof(int)))*BLOCKSIZE;
    int end = MIN(max_size, pos);
    // Find an index of first block that is partially in hole.
    int first_partial_block = get_num_blocks(inode_to_write.size) - 1;
    // Find an index of the last block that is partially in hole.
    int last_partial_block = get_num_blocks(end) - 1;
    // Fill all non-allocated blocks with zeros.
    int i;
    for (i = first_partial_block + 1; i < NUM_DIRECT && i <= last_partial_block; i++) {
        inode_to_write.direct[i] = 0;
    }
    if (last_partial_block >= NUM_DIRECT) {
        if(first_partial_block < NUM_DIRECT) {
            // Allocate an indirect block.
            inode_to_write.indirect = find_free_block();
            if (inode_to_write.indirect == ERROR) {
                return ERROR;
            }
            blockmap[inode_to_write.indirect] = false;
        }
        int *indirect_buf = (int *)accessBlock(inode_to_write.indirect);
        if (indirect_buf == NULL) {
            return ERROR;
        }
        int j;
        for (j = i - NUM_DIRECT; j <= last_partial_block - NUM_DIRECT; j++) {
            indirect_buf[j] = 0;
        }
        if (modifyBlock(inode_to_write.indirect, (char *)indirect_buf) == ERROR) {
            return ERROR;
        }
    }
    inode_to_write.size = end;
    if (modifyInode(inode_num, inode_to_write) == ERROR) {
        return ERROR;
    }
    return 0;
}

/* Writes to a specific block. */
static int WriteBlock(int block_num, int start_within_block, int bytes_left, char* buf_to_write) {
    char *buf = accessBlock(block_num);
    if(buf == NULL) {
        return ERROR;
    }
    int bytes_to_write = MIN(bytes_left, SECTORSIZE - start_within_block);
    memcpy(buf + start_within_block, buf_to_write, bytes_to_write);
    // Advance a buffer by bytes_to_read bytes.
    if(modifyBlock(block_num, buf) == ERROR) {
        return ERROR;
    }
    return bytes_to_write;
}

/* Fills the wholes and tries to write "size" bytes to the inode.
   Returns how much was actually write to the file or ERROR.*/
static int fillAndWrite(int inode_num, struct inode inode_to_write, int size, int start_pos, char* buf_to_write) {
    TracePrintf(0, "In fillAndWrite\n");
    // Fill the holes.
    if (start_pos > inode_to_write.size) {
        if (fillHoles(inode_num, start_pos) == ERROR) {
            return ERROR;
        }
    }
    int sizeWritten = -1;
    // Write a buffer.
    if ((sizeWritten = writeToInode(inode_num, size, start_pos, buf_to_write)) == ERROR) {
        return ERROR;
    }
    return sizeWritten;
}

/* Replies to a message of type MKDIR. */
static void mkdir(struct my_msg *msg, int pid) {
    if (msg->ptr == NULL) {
        msg->numeric1 = ERROR;
        return;
    }
    int curr_dir = msg->numeric1;
    if (inodemap[curr_dir]) {
        // This inode number is free.
        TracePrintf(0, "Inode is not valid\n");
        msg->numeric1 = ERROR;
        return;
    }
    char pathname[MAXPATHNAMELEN];
    if (CopyFrom(pid, pathname, msg->ptr, msg->numeric5) == ERROR) {
        TracePrintf(0, "Couldn't copy a pathname\n");
        msg->numeric1 = ERROR;
        return;
    }
    // The third argument is the desired inum in the dir_entry. 
    // If inum = -1, we want to take a new unique inode number.
    // Function returns an inode number or ERROR.
    int ret_val = createDirEntry(curr_dir, pathname, -1, INODE_DIRECTORY);
    if (ret_val == ERROR) {
        msg->numeric1 = ERROR;
    } else {
        msg->numeric1 = 0;
    }
}

/* Creates an inode of type INODE_DIRECTORY. */
static int createDirectory(int parent_inum) {
    int free_inode_num = findFreeInodeNum();
    if (free_inode_num == -1) {
        fprintf(stderr, "No enough free inodes\n");
        return ERROR;
    }
    struct inode inode_struct = accessInode(free_inode_num);
    // Initialize an inode fields.
    inode_struct.type = INODE_DIRECTORY;
    inode_struct.size = 0;
    cleanBlocks(&inode_struct);
    inode_struct.reuse++;
    inode_struct.nlink = 1; // Contains link to "."
    if (modifyInode(free_inode_num, inode_struct) == ERROR) {
        return ERROR;
    }
    // Put "." to a newly-created directory.
    if(writeNewDirEntry(".", free_inode_num, free_inode_num) == ERROR) {
        TracePrintf(0, "writeNewDirEntry for . failed\n");
        return ERROR;
    }

    // Put ".." to a newly-created directory.
    if(writeNewDirEntry("..", parent_inum, free_inode_num) == ERROR) {
        TracePrintf(0, "writeNewDirEntry for .. failed\n");
        return ERROR;
    }
    // Increase nlink of parent directory because of ".."
    struct inode parent_dir_struct = accessInode(parent_inum);
    parent_dir_struct.nlink++;
    if (modifyInode(parent_inum, parent_dir_struct) == ERROR) {
        return ERROR;
    }
    inodemap[free_inode_num] = false;
    return free_inode_num;
}

/* Sets all block_nums to 0. Enforces better security. */
static void cleanBlocks(struct inode *inode_struct) {
    int i;
    for (i = 0; i < NUM_DIRECT; i++) {
        inode_struct->direct[i] = 0;
    }
    inode_struct->indirect = 0;
}

/* Replies to a message of tyoe RMDIR. */
static void rmdir(struct my_msg *msg, int pid) {
    if (msg->ptr == NULL) {
        msg->numeric1 = ERROR;
        return;
    }
    int curr_dir = msg->numeric1;
    if (inodemap[curr_dir]) {
        // This inode number is free.
        msg->numeric1 = ERROR;
        return;
    }
    char pathname[MAXPATHNAMELEN];
    if (CopyFrom(pid, pathname, msg->ptr, msg->numeric5) == ERROR) {
        msg->numeric1 = ERROR;
        return;
    }
    // Function returns 0 or ERROR.
    msg->numeric1 = removeDirectory(curr_dir, pathname);
}

/* Removes a directory. Returns 0 on SUCCESS or ERROR.*/
static int removeDirectory(int curr_dir, char *pathname) {
    // Get parent directory.
    int parent_dir = getParentDir(curr_dir, &pathname);
    if (parent_dir == ERROR) {
        fprintf(stderr, "File has an invalid parent directory.\n");
        return ERROR;
    }
    int inode_num = getInodeNumber(parent_dir, pathname);
    if (inode_num == ERROR) {
        fprintf(stderr, "This directory does not exists.\n");
        return ERROR;
    }
    // Check that it's not a root directory.
    if (inode_num == ROOTINODE) {
        fprintf(stderr, "Root directory cannot be deleted.\n");
        return ERROR;
    }
    // It must be a directory.
    struct inode inode_struct = accessInode(inode_num);
    if (inode_struct.type != INODE_DIRECTORY) {
        fprintf(stderr, "File has a wrong type.\n");
        return ERROR;
    }

    // Check that there are no entries in the directory other than "." and ".."
    if (!dirIsEmpty(inode_struct)) {
        fprintf(stderr, "Directory is not empty.\n");
        return ERROR;
    }

    struct inode parent_dir_struct = accessInode(parent_dir);
    // "." and corresponding link will be removed when we call deleteInode
    // ".." entry will be removed automatically. We need to decrement a link to parent directory manually.
    parent_dir_struct.nlink--;
    if (modifyInode(parent_dir, parent_dir_struct) == ERROR) {
        return ERROR;
    }
    // Must remove dir_entry from parent directory.
    if (removeDirEntry(parent_dir, pathname) == ERROR) {
        return ERROR;
    }
    // Free inode and its blocks.
    if (deleteInode(inode_num) == ERROR) {
        return ERROR;
    }
    return 0;
}

/* Checks whether directory contains only "." and "..". 
   Returns true if empty, false otherwise.*/
static bool dirIsEmpty(struct inode curr_dir_inode) {
    int num_entries_left = curr_dir_inode.size / sizeof(struct dir_entry);
    int num_blocks = get_num_blocks(curr_dir_inode.size);
    int i;
    // All dir_entries must have inum = 0 (except for the first two entries: "." and "..")
    // Start iteration from 2 (skip 0, 1)
    int start_dir_entry = 2;
    // Search in direct blocks.
    for (i = 0; i < MIN(NUM_DIRECT, num_blocks); i++) {
        if (dirBlockIsEmpty(curr_dir_inode.direct[i], num_entries_left, start_dir_entry) == false) {
            return false;
        }
        num_entries_left -= ENTRIES_PER_BLOCK;
        start_dir_entry = 0;
    }
    // Search in indirect blocks.
    if (num_blocks > NUM_DIRECT) {
        int block_to_read = curr_dir_inode.indirect;
        int *indirect_buf = (int *)accessBlock(block_to_read);
        if (indirect_buf == NULL) {
            return ERROR;
        }
        int j;
        for (j = 0; j < (num_blocks - NUM_DIRECT); j++) {
            if (dirBlockIsEmpty(indirect_buf[j], num_entries_left, start_dir_entry) == false) {
                return false;
            }
            num_entries_left -= ENTRIES_PER_BLOCK;
        }
    }
    return true;
}

/* Checks whether a block of directory contains no dir_entries other than
   "." and "..". Returns true if empty, false otherwise.*/
static bool dirBlockIsEmpty(int blockNum, int num_entries_left, int start_dir_entry) {
    struct dir_entry *dir_buf = (struct dir_entry *)accessBlock(blockNum);
    if (dir_buf == NULL) {
        return ERROR;
    }
    int i;
    for (i = start_dir_entry; i < MIN(ENTRIES_PER_BLOCK, num_entries_left); i++) {
        if (dir_buf[i].inum != 0) {
            return false;
        }
    }
    return true;
}

/* Returns a hashcode for the caches.*/
static int getHashcode(int num, int size) {
    return num % size;
}

/* Evicts a block according to the LRU princinple.*/
static int evictBlock() {
    struct block_cache_entry *to_evict = blockLRU.lru_prev;
    // Delete from LRU circular doubly-linked list.
    removeBlockFromLRU(to_evict);
    // Delete from hash circular doubly-linked list.
    removeBlockFromHash(to_evict);
    if (to_evict->dirty) {
        // Write a block to the disk.
        if(WriteSector(to_evict->num, to_evict->data) == ERROR) {
            return ERROR;
        }
    }
    free(to_evict);
    blockCache.count--;
    return 0;
}

/* Writes inode to a block cache. */
static int writeInodeToDisc(int inode_num, struct inode inode_struct) {
    int block_num = inode_num/INODES_PER_BLOCK + 1;
    int index = inode_num % INODES_PER_BLOCK;
    struct inode *buf = (struct inode*)accessBlock(block_num);
    if(buf == NULL) {
        return ERROR;
    }
    buf[index] = inode_struct;
    if(modifyBlock(block_num, (char *)buf) == ERROR) {
        return ERROR;
    }
    // Make sure that this inode is "occupied" and won't be overwritten.
    inodemap[inode_num] = false;
    return 0;
}

/* Evicts an inode according to the LRU princinple. */
static int evictInode() {
    struct inode_cache_entry *to_evict = inodeLRU.lru_prev;
    TracePrintf(0, "Victim has inum = %i\n", to_evict->num);
    // Delete from LRU circular doubly-linked list.
    removeInodeFromLRU(to_evict);
    // Delete from hash circular doubly-linked list.
    removeInodeFromHash(to_evict);
    TracePrintf(0, "Inode queues after eviction:\n");
    printInodeQueues();
    if (to_evict->dirty) {
        // Write inode to the cached block.
        if (writeInodeToDisc(to_evict->num, to_evict->inode_struct) == ERROR) {
            return ERROR;
        }
    }
    free(to_evict);
    inodeCache.count--;
    return 0;
}

/* Inserts a block cache entry to the block cache. */
static int insertBlock(int num) {
    // Check that entry is not in a cache.
    struct block_cache_entry *block_entry = searchBlock(num);
    if (block_entry != NULL) {
        return ERROR;
    }
    if (blockCache.count >= BLOCK_CACHESIZE) {
        // Evict block!!
        if (evictBlock() == ERROR) {
            return ERROR;
        }
    }
    int hashcode = getHashcode(num, BLOCK_CACHESIZE);

    char block_buf[BLOCKSIZE];
    if(ReadSector(num, block_buf) == ERROR) {
        return ERROR;
    }

    struct block_cache_entry *block = malloc(sizeof(struct block_cache_entry));
    block->num = num;
    memcpy(block->data, block_buf, BLOCKSIZE);
    block->dirty = false;
    // Insert block in hash.
    insertBlockToHash(block, hashcode);
    // Insert in LRU.
    insertBlockToLRU(block);
    blockCache.count++;
    return 0;
}

/* Inserts an inode cache entry to the inode cache. */
static int insertInode(int num) {
    // Check that entry is not in a cache.
    struct inode_cache_entry *inode_entry = searchInode(num);
    if (inode_entry != NULL) {
        // Inode is already inserted.
        return ERROR;
    }
    if (inodeCache.count >= INODE_CACHESIZE) {
        // Evict an inode.
        if (evictInode() == ERROR) {
            return ERROR;
        }
    }
    int hashcode = getHashcode(num, INODE_CACHESIZE);
    int block = (num / INODES_PER_BLOCK) + 1;
    struct inode *inode_buf = (struct inode *)accessBlock(block);
    int index = num % INODES_PER_BLOCK;
    inode_entry = malloc(sizeof(struct inode_cache_entry));
    inode_entry->num = num;
    inode_entry->inode_struct = inode_buf[index];
    inode_entry->dirty = false;
    // Insert inode in hash.
    insertInodeToHash(inode_entry, hashcode);
    // Insert in LRU.
    insertInodeToLRU(inode_entry);
    inodeCache.count++;
    return 0;
}

/* Searches for an inode cache entry in the inode cache. */
static struct inode_cache_entry *searchInode(int num) {
    int hashcode = getHashcode(num, INODE_CACHESIZE);
    struct inode_cache_entry *curr = inodeCache.cache[hashcode].hash_next;
    while (curr->num != 0) {
        if (num == curr->num) {
            return curr;
        }
        curr = curr->hash_next;
    }
    return NULL;
}
/* Searches for an block cache entry in the block cache. */
static struct block_cache_entry *searchBlock(int num) {
    int hashcode = getHashcode(num, BLOCK_CACHESIZE);
    struct block_cache_entry *curr = blockCache.cache[hashcode].hash_next;
    while (curr->num != 0) {
        if (num == curr->num) {
            return curr;
        }
        curr = curr->hash_next;
    }
    return NULL;
}

/* Initializes the headers for block and inode queues. */
static void initCaches() {
    // Init block hash headers.
    int i;
    for (i = 0; i < BLOCK_CACHESIZE; i++) {
        blockCache.cache[i].num = 0;
        blockCache.cache[i].hash_next = &(blockCache.cache[i]);
        blockCache.cache[i].hash_prev = &(blockCache.cache[i]);
    }
    // Init inode hash headers.
    for (i = 0; i < INODE_CACHESIZE; i++) {
        inodeCache.cache[i].hash_next = &(inodeCache.cache[i]);
        inodeCache.cache[i].hash_prev = &(inodeCache.cache[i]);
    }
    // Init block lru header.
    blockLRU.num = 0;
    blockLRU.lru_next = &blockLRU;
    blockLRU.lru_prev = &blockLRU;
    // Init inode lru header.
    inodeLRU.num = 0;
    inodeLRU.lru_next = &inodeLRU;
    inodeLRU.lru_prev = &inodeLRU;
    printBlockQueues();
    printInodeQueues();
}

static void printBlockHashQueue(int hashcode) {
    // Init block hash headers.
    struct block_cache_entry *curr = blockCache.cache[hashcode].hash_next;
    int i = 1;
    while (curr->num != 0) {
        TracePrintf(0, "%i. Block#%i\n", i++, curr->num);
        curr = curr->hash_next;
    }
}

static void printBlockLRUQueue() {
    // Init block hash headers.
    struct block_cache_entry *curr = blockLRU.lru_next;
    int i = 1;
    while (curr->num != 0) {
        TracePrintf(0, "%i. Block#%i\n", i++, curr->num);
        curr = curr->lru_next;
    }
}

static void printBlockQueues() {
    int i;
    for (i = 0; i < BLOCK_CACHESIZE; i++) {
        TracePrintf(0, "Printing block hash queue for hashcode = %i\n", i);
        printBlockHashQueue(i);
    }
    TracePrintf(0, "Printing block lru queue\n");
    printBlockLRUQueue();
}

static void printInodeHashQueue(int hashcode) {
    // Init inode hash headers.
    struct inode_cache_entry *curr = inodeCache.cache[hashcode].hash_next;
    int i = 1;
    while (curr->num != 0) {
        TracePrintf(0, "%i. Inode#%i\n", i, curr->num);
        curr = curr->hash_next;
        i++;
    }
}

static void printInodeLRUQueue() {
    struct inode_cache_entry *curr = inodeLRU.lru_next;
    int i = 1;
    while (curr->num != 0) {
        TracePrintf(0, "%i. Inode#%i\n", i, curr->num);
        curr = curr->lru_next;
        i++;
    }
}

static void printInodeQueues() {
    int i;
    for (i = 0; i < INODE_CACHESIZE; i++) {
        TracePrintf(0, "Printing inode hash queue for hashcode = %i\n", i);
        printInodeHashQueue(i);
    }
    TracePrintf(0, "Printing inode lru queue\n");
    printInodeLRUQueue();
}

/* Function used for all accesses of inodes. */
static struct inode accessInode(int num) {
    struct inode_cache_entry *inode_entry = searchInode(num);
    if (inode_entry == NULL) {
        TracePrintf(0, "Inode %i is not in the cache. Insert it.\n", num);
        if (insertInode(num) == ERROR) {
            TracePrintf(0, "Some error occured in insertInode.\n");
        }
        inode_entry = searchInode(num);
    } else {
        TracePrintf(0, "Inode %i is in the cache. Update its LRU position.\n", num);
        updateInodeLRU(inode_entry);
    }
    return inode_entry->inode_struct;
}

/* Function used for all accesses of blocks. */
static char * accessBlock(int num) {
    struct block_cache_entry *block_entry = searchBlock(num);
    if (block_entry == NULL) {
        TracePrintf(0, "Block is not in the cache. Insert it.\n");
        if (insertBlock(num) == ERROR) {
            return NULL;
        }
        block_entry = searchBlock(num);
    } else {
        TracePrintf(0, "Block is in the cache. Update its LRU position.\n");
        updateBlockLRU(block_entry);
        TracePrintf(0, "Print updated LRU queue after update\n");
        printBlockLRUQueue();
    }
    printBlockQueues();
    return block_entry->data;
}
/* Function used for all modifications of blocks. */
static int modifyBlock(int num, char *block_buf) {
    struct block_cache_entry *block_entry = searchBlock(num);
    if (block_entry == NULL) {
        TracePrintf(0, "Block is not in the cache. Insert it.\n");
        if (insertBlock(num) == ERROR) {
            return ERROR;
        }
        block_entry = searchBlock(num);
        // Now block_entry is in the cache with malloced buffer for data.
        memcpy(block_entry->data, block_buf, BLOCKSIZE);
    }
    // No need to do memcpy if block was already in the cache 
    // (the pointers of dest and src would be the same)
    block_entry->dirty = true;
    return 0;
}

/* Function used for all modifications of inodes. */
static int modifyInode(int num, struct inode new_inode_struct) {
    struct inode_cache_entry *inode_entry = searchInode(num);
    if (inode_entry == NULL) {
        TracePrintf(0, "Inode is not in the cache. Insert it.\n");
        if (insertInode(num) == ERROR) {
            return ERROR;
        }
        inode_entry = searchInode(num);
    }
    // Now inode_entry is in the cache with malloced memory for struct inode.
    inode_entry->inode_struct = new_inode_struct;
    inode_entry->dirty = true;
    return 0;
}

static void removeBlockFromHash(struct block_cache_entry *block) {
    block->hash_next->hash_prev = block->hash_prev;
    block->hash_prev->hash_next = block->hash_next;
}

static void removeBlockFromLRU(struct block_cache_entry *block) {
    block->lru_next->lru_prev = block->lru_prev;
    block->lru_prev->lru_next = block->lru_next;
}

static void insertBlockToLRU(struct block_cache_entry *block) {
    block->lru_next = blockLRU.lru_next;
    block->lru_prev = &blockLRU;
    blockLRU.lru_next->lru_prev = block;
    blockLRU.lru_next = block;
}

static void insertBlockToHash(struct block_cache_entry *block, int hashcode) {
    block->hash_next = blockCache.cache[hashcode].hash_next;
    block->hash_prev = &blockCache.cache[hashcode];
    blockCache.cache[hashcode].hash_next->hash_prev = block;
    blockCache.cache[hashcode].hash_next = block;
}

static void updateBlockLRU(struct block_cache_entry *block) {
    // Know that block_entry was already in the cache.
    // Remove block from LRU.
    removeBlockFromLRU(block);
    // Make this block most recent in LRU.
    insertBlockToLRU(block);
}

static void removeInodeFromHash(struct inode_cache_entry *inode) {
    inode->hash_next->hash_prev = inode->hash_prev;
    inode->hash_prev->hash_next = inode->hash_next;
}

static void removeInodeFromLRU(struct inode_cache_entry *inode) {
    inode->lru_next->lru_prev = inode->lru_prev;
    inode->lru_prev->lru_next = inode->lru_next;
}

static void insertInodeToLRU(struct inode_cache_entry *inode) {
    inode->lru_next = inodeLRU.lru_next;
    inode->lru_prev = &inodeLRU;
    inodeLRU.lru_next->lru_prev = inode;
    inodeLRU.lru_next = inode;
}

static void insertInodeToHash(struct inode_cache_entry *inode, int hashcode) {
    inode->hash_next = inodeCache.cache[hashcode].hash_next;
    inode->hash_prev = &inodeCache.cache[hashcode];
    inodeCache.cache[hashcode].hash_next->hash_prev = inode;
    inodeCache.cache[hashcode].hash_next = inode;
}

static void updateInodeLRU(struct inode_cache_entry *inode) {
    // Know that block_entry was already in the cache.
    // Remove block from LRU.
    removeInodeFromLRU(inode);
    // Make this block most recent in LRU.
    insertInodeToLRU(inode);
}

/* Replies to a message of type SYNC. */
static void sync() {
    // Write all dirty inodes to the block cache.
    writeDirtyInodes();
    // Write all dirty blocks to the block cache.
    writeDirtyBlocks();
}

/* Replies to a message of type SHUTDOWN. */
static void shutdown() {
    // Write all dirty inodes and blocks.
    sync();
    fprintf(stdout, "Shutting down a server...\n");
    Exit(0);
}

/* Writes all dirty inodes to a block cache. */
static void writeDirtyInodes() {
    struct inode_cache_entry *curr = inodeLRU.lru_next;
    while (curr->num != 0) {
        if (curr->dirty) {
            writeInodeToDisc(curr->num, curr->inode_struct);
        }
        curr = curr->lru_next;
    }
}

/* Writes all dirty blocks to a disk. */
static void writeDirtyBlocks() {
    struct block_cache_entry *curr = blockLRU.lru_next;
    while (curr->num != 0) {
        if (curr->dirty) {
            WriteSector(curr->num, curr->data);
        }
        curr = curr->lru_next;
    }
}


