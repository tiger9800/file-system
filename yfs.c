#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include "msg_types.h"

#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

const int INODES_PER_BLOCK = BLOCKSIZE/INODESIZE;
const int ENTRIES_PER_BLOCK = BLOCKSIZE/sizeof(struct dir_entry);

char buf[SECTORSIZE];

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
static void read(struct my_msg *msg, int pid);

static int getInodeNumber(int curr_dir, char *pathname);
static int search(int start_inode, char *pathname);
static struct inode findInode(int curr_num);
static int searchInDirectory(struct inode *curr_inode, char *token);
static int find_entry_in_block(int block, char *token, int num_entries_left);
static int get_num_blocks(int size);
static int fillFreeEntry(int blockNum, int index, int file_inode_num, char *entry_name);
static int createFileInDir(int dir_inode_num, char* file_name, int pref_inum);
static int createNewDirEntry(int dir_inode_num, char* file_name, int pref_inum);
static int writeInodeToDisc(int inode_num, struct inode inode_struct);
static int findFreeInodeNum();
static int createDirEntry(int curr_dir, char *pathname, int pref_inum);
static int writeNewDirEntry(char* file_name, int file_inode_num, int dir_inode_num);
static int updateFreeEntry(struct inode *curr_dir_inode, char *file_name, int file_inode_num);
static int updateFreeEntryInBlock(int blockNum, char* entry_name, int file_inode_num, int num_entries_left);
static int find_free_block();
static int eraseFile(int inode_num);
static int readFromInode(struct inode inode_to_read, int size, int start_pos, char* buf_to_read);
static char* ReadBlock(int block_num, int start_pos, int bytes_left, char* buf_to_read);
static int linkInodes(int curr_dir, char *oldname, char *newname);
static int createRegFile(struct inode* new_inode);
static int getParentDir(int curr_dir, char **pathname);
static void unlink(struct my_msg *msg, int pid);
static int unlinkInode(int curr_dir, char *pathname);
static int removeDirEntry(int dir, char *file_name);
static int removeEntryInBlock(int blockNum, char *entry_name, int file_inode_num, int num_entries_left);
static int freeDirEntry(int blockNum, int index);
static int deleteInode(int inode_num);

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
            break;
            // Change "break" to "continue."
            // continue;
        }
        handleMsg(&msg, pid);
        Reply((void *)&msg, pid);
    }
    return 0;
}

static void start_user_program(char *argv[]) {
        int child;
        child = Fork();
        if (child == 0) {
            Exec(argv[1], argv+1);
        }
}

static int getFreeInodes() {
    struct inode *currNode = (struct inode *)buf + 1; //address of the first inode
    // File header is occupied.
    inodemap[0] = false;
    int node_count = 1;
    int sector = 1;
    //TracePrintf(0, "Root node type %i\n", currNode->type);
    //TracePrintf(0, "Root node size %i\n", currNode->size);
    //TracePrintf(0, "sizeof dir_entry %i\n", sizeof(struct dir_entry));

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
        if (ReadSector(sector, buf) == ERROR) {
            printf("Error\n");
            return ERROR;
        }
        currNode = (struct inode *)buf;
    } 
    return 0;    
}

static int changeStateBlocks(struct inode *currNode, bool state) {
    int num_blocks = get_num_blocks(currNode->size);
    int i;
    for (i = 0; i < MIN(NUM_DIRECT, num_blocks); i++) {
        blockmap[currNode->direct[i]] = state;
    }
    if (num_blocks > NUM_DIRECT) {//then we need to search in the indirect block
        int block_to_read = currNode->indirect;
        int indirect_buf[SECTORSIZE/sizeof(int)];
        if (ReadSector(block_to_read, indirect_buf) == ERROR) {
            return ERROR;
        }
        int j;
        for (j = 0; j < (num_blocks - NUM_DIRECT); j++) {
            blockmap[indirect_buf[j]] = state;
        }
        blockmap[block_to_read] = state;
    }
    return 0;
}

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

static int init() {
    if (ReadSector(1, buf) == ERROR) {
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

    //initalize root struct
    return 0;
}

static void handleMsg(struct my_msg *msg, int pid) {
    //TracePrintf(0, "Message type %i\n", msg->type);
    //TracePrintf(0, "Numeric1 %i\n", msg->numeric1);
    //TracePrintf(0, "Numeric2 %i\n", msg->numeric2);
    //TracePrintf(0, "Addr of path (ptr) %p\n", msg->ptr);
    switch(msg->type) {
        case OPEN:
            //TracePrintf(0, "Handling OPEN  Message\n");
            open(msg, pid);
            break;
        case CREATE:
            //TracePrintf(0, "Handling CREATE Message\n");
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
    }
}

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
    CopyFrom(pid, pathname, msg->ptr, MAXPATHNAMELEN);
    // msg->numeric1 initially contains a current directory.
    // Reply with a message that has an inode number of the opened file or ERROR.
    msg->numeric1 = getInodeNumber(curr_dir, pathname);
    // Reply message also contains a reuse count.
    if(msg->numeric1 != ERROR) {
        msg->numeric2 = findInode(msg->numeric1).reuse;
    } 
}


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
    CopyFrom(pid, pathname, msg->ptr, MAXPATHNAMELEN);
    TracePrintf(0, "Want to create a file with pathname %s\n", pathname);
    // Check that last char of pathname is not "/"
    if (pathname[strlen(pathname) - 1] == '/') {
        msg->numeric1 = ERROR;
        return;
    }
    // The third argument is the desired inum in the dir_entry. 
    // If inum = -1, we want to take a new unique inode number.
    // Function returns an inode number or ERROR.
    msg->numeric1 = createDirEntry(curr_dir, pathname, -1);
    
    // Reply message also contains a reuse count.
    if(msg->numeric1 != ERROR) {
        msg->numeric2 = findInode(msg->numeric1).reuse;
    }
}

static int createDirEntry(int curr_dir, char *pathname, int pref_inum) {
    int parent_dir = getParentDir(curr_dir, &pathname);
    if (parent_dir == ERROR) {
        return ERROR;
    }
    int new_file_inode_num = createFileInDir(parent_dir, pathname, pref_inum);
    return new_file_inode_num;
}

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
    struct inode file_inode = findInode(file_inode_num);
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
    CopyTo(pid, msg->ptr, buf_to_read, sizeRead);
    msg->numeric1 = sizeRead;
}

//returns how much was actually read from the file
static int readFromInode(struct inode inode_to_read, int size, int start_pos, char* buf_to_read) {
    int size_to_read = MIN(inode_to_read.size - start_pos, size);

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

    if (num_blocks > NUM_DIRECT && bytes_left > 0) {//then we need to search in the indirect block
        int block_to_read = inode_to_read.indirect;
        int indirect_buf[BLOCKSIZE/sizeof(int)];
        if (ReadSector(block_to_read, indirect_buf) == ERROR) {
            return ERROR;
        }
        int j;
        for (j = starting_block - NUM_DIRECT; j < (num_blocks - NUM_DIRECT) && bytes_left > 0; j++) {
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

static char* ReadBlock(int block_num, int start_pos, int bytes_left, char* buf_to_read) {
    if(ReadSector(block_num, buf)) {
        return NULL;
    }
    int bytes_to_read = MIN(bytes_left, SECTORSIZE - start_pos);
    memcpy(buf_to_read, buf+start_pos, bytes_to_read);
    // Advance a buffer by bytes_to_read bytes.
    return buf_to_read + bytes_to_read;
}

// Returns an inode number of the newly-created or existing file; returns ERROR if failure.
static int createFileInDir(int dir_inode_num, char* file_name, int pref_inum) {
    TracePrintf(0, "In createFileInDir\n");
    TracePrintf(0, "filename is %s\n", file_name);
    if (strlen(file_name) == 0 || strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
        // Can't create such directory entries manually.
        TracePrintf(0, "Tried to create an invalid file\n");
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
    struct inode dir_inode = findInode(dir_inode_num);

    // Look through entries and find an empty entry. If it does not exist, then create a new one.
    int inode_num = searchInDirectory(&dir_inode, entry_name);
    if (inode_num != ERROR) {
        // File with this name already exists in the directory.
        if (pref_inum == -1) {
            // Our goal is to create a new file, but it already exists.
            // We need to erase the contents of this existing file.
            // Erase file contents and return.
            struct inode exist_inode = findInode(inode_num);
            if(exist_inode.type == INODE_DIRECTORY) {
                return ERROR;
            } else {//the file is not a directory
                if(eraseFile(inode_num) == ERROR) {
                    return ERROR;
                }    
            }
        } else {
            // Our goal is to link an oldname to this newname, but a new name already exists in this directory.
            fprintf(stderr, "New file already exists.\n");
            return ERROR;
        }
        
    } else {
        // A directory entry does not exist in the directory, so we create it from scratch.
        inode_num = createNewDirEntry(dir_inode_num, entry_name, pref_inum);
        if(inode_num == ERROR) {
            fprintf(stderr, "Could not create a new directory entry\n");
            return ERROR;
        }
    }
    return inode_num;
}
// Returns an inode number of the newly-created file or ERROR.
static int createNewDirEntry(int dir_inode_num, char* file_name, int pref_inum) {
    // Storage for a new inode.
    struct inode inode_struct;
    if (pref_inum == -1) {
        // We want to find a new unique inode_num and create a regular file in it.
        pref_inum = createRegFile(&inode_struct);
        if(pref_inum == ERROR) {
            return ERROR;
        }
    } else {
        // We already have an inode we want to store in the dir_entry.
        inode_struct = findInode(pref_inum);
    }

    // Write a new directory entry with inum = pref_inum, name == file_name.
    if(writeNewDirEntry(file_name, pref_inum, dir_inode_num) == ERROR) {
        return ERROR;
    }
    // Increment a number of hard links to this inode_struct.
    inode_struct.nlink++;
    TracePrintf(0, "Number of links is %i\n", inode_struct.nlink);
    // We set inodemap[pref_inum] to false in this function.
    if (writeInodeToDisc(pref_inum, inode_struct) == ERROR) {
        return ERROR;
    }
    return pref_inum;
}

static int writeInodeToDisc(int inode_num, struct inode inode_struct) {
    int block_num = inode_num/INODES_PER_BLOCK + 1;
    int index = inode_num % INODES_PER_BLOCK;
    struct inode buf[INODES_PER_BLOCK];
    if(ReadSector(block_num, buf) == ERROR) {
        return ERROR;
    }
    buf[index] = inode_struct;
    if(WriteSector(block_num, buf) == ERROR) {
        return ERROR;
    }
    // Make sure that this inode is "occupied" and won't ve overwritten.
    inodemap[inode_num] = false;
    return 0;
}

static int createRegFile(struct inode* new_inode) {
    int free_inode_num = findFreeInodeNum();
    if (free_inode_num == -1) {
        return ERROR;
    }
    struct inode inode_struct = findInode(free_inode_num);
    // Initialize an inode fields.
    inode_struct.type = INODE_REGULAR;
    inode_struct.size = 0;
    inode_struct.reuse++;
    // Will increment this field when we create a dir_entry. 
    inode_struct.nlink = 0;
    *new_inode = inode_struct;
    return free_inode_num;
}

// Returns a free inode number or ERROR if none exists.
static int findFreeInodeNum() {
    int i;
    for(i = 0; i < NUM_INODES; i++) {
        if (inodemap[i]) {
            return i;
        }
    }
    return ERROR;
}

// Returns 0 on SUCCESS, -1 on ERROR
static int writeNewDirEntry(char* file_name, int file_inode_num, int dir_inode_num) {
    struct inode curr_dir_inode = findInode(dir_inode_num);
    if (curr_dir_inode.type != INODE_DIRECTORY) {
        return ERROR;
    }

    int last_block;
    if ((last_block = updateFreeEntry(&curr_dir_inode, file_name, file_inode_num)) <= 0) {
        // Either SUCCESS (we found a free entry and modified it) or ERROR.
        return last_block;
    }

    // There are no free directory entries in the directory dir_inode_num.
    int max_size = (NUM_DIRECT + BLOCKSIZE/sizeof(int)) * BLOCKSIZE;
    if (max_size == curr_dir_inode.size) {
        // Directory reached its maximum size.
        return ERROR;
    } else if(curr_dir_inode.size % BLOCKSIZE != 0){
        // There is still space in the last block that we can use.
        int index = (curr_dir_inode.size % BLOCKSIZE)/(sizeof(struct dir_entry));
        if (fillFreeEntry(last_block, index, file_inode_num, file_name) == ERROR) {
            return ERROR;
        }
    } else {
        // Allocate a new block.
        int free_block_num = find_free_block();
        if(free_block_num == ERROR) {
            return ERROR;
        }
        // Fill the first entry in this newly-allocated block.
        if (fillFreeEntry(free_block_num, 0, file_inode_num, file_name) == ERROR) {
            return ERROR;
        }
        int num_blocks = get_num_blocks(curr_dir_inode.size);
        if(num_blocks < NUM_DIRECT) {
            //then, we can put in the last direct block
            curr_dir_inode.direct[num_blocks] = free_block_num; //this is where we put the new block
        } else {//num_blocks >= NUM_DIRECT
            if(num_blocks == NUM_DIRECT) {
                curr_dir_inode.indirect = find_free_block();
                if (curr_dir_inode.indirect == ERROR) {
                    return ERROR;
                }
            }
            int indirect_buf[BLOCKSIZE/sizeof(int)];
            if(ReadSector(curr_dir_inode.indirect, indirect_buf) == ERROR) {
                return ERROR;
            }
            indirect_buf[num_blocks - NUM_DIRECT] = free_block_num;

            if(WriteSector(curr_dir_inode.indirect, indirect_buf) == ERROR) {
                return ERROR;
            }
        }
        // After all error-checkings, can finally change bitmap for new block and (possibly) new indirect block.
        blockmap[free_block_num] = false;
        if (num_blocks == NUM_DIRECT) blockmap[curr_dir_inode.indirect] = false;
    }
    // Must increment a size of the directory.
    curr_dir_inode.size += sizeof(struct dir_entry);//we should have existed if we reused the old free dir_entry
    if (writeInodeToDisc(dir_inode_num, curr_dir_inode) == ERROR) {
        return ERROR;
    }
    return 0;
}

// Returns 0 if such free enty is found; ERROR if some error occured; a number of existing last block in the directory if no free entry was found.
static int updateFreeEntry(struct inode *curr_dir_inode, char *file_name, int file_inode_num) {
    int num_entries_left = curr_dir_inode->size / sizeof(struct dir_entry);
    int num_blocks = get_num_blocks(curr_dir_inode->size);
    int i;
    int last_block = -1;
    for (i = 0; i < MIN(NUM_DIRECT, num_blocks); i++) {
        last_block = curr_dir_inode->direct[i];
        int return_val = updateFreeEntryInBlock(last_block, file_name, file_inode_num, num_entries_left);
        if(return_val != -2) {
            return return_val;
        }
        num_entries_left -= ENTRIES_PER_BLOCK;
    }
    if (num_blocks > NUM_DIRECT) {//then we need to search in the indirect block
        int block_to_read = curr_dir_inode->indirect;
        int indirect_buf[BLOCKSIZE/sizeof(int)];
        if (ReadSector(block_to_read, indirect_buf) == ERROR) {
            return ERROR;
        }
        int j;
        for (j = 0; j < (num_blocks - NUM_DIRECT); j++) {
            last_block = indirect_buf[j];
            int return_val = updateFreeEntryInBlock(last_block, file_name, file_inode_num, num_entries_left);
            if(return_val != -2) {
                return return_val;
            }
            num_entries_left -= ENTRIES_PER_BLOCK;
        }
    }
    // No free entry was found.
    return last_block;
}

static int find_free_block() {
    int i;
    for(i = 0; i < NUM_BLOCKS; i++) {
        if(blockmap[i]) {
            // Will do it later in a function after all error-checkings.
            // blockmap[i] = false;
            return i;
        }
    }
    return ERROR;
}

// Returns 0 on SUCCESS, -1 on ERROR, -2 if no free entry was found in the block.
static int updateFreeEntryInBlock(int blockNum, char* entry_name, int file_inode_num, int num_entries_left) {  
    struct dir_entry dir_buf[ENTRIES_PER_BLOCK];
    if (ReadSector(blockNum, dir_buf) == ERROR) {
        return ERROR;
    }
    int i;
    for (i = 0; i < MIN(ENTRIES_PER_BLOCK, num_entries_left); i++) {
        if (dir_buf[i].inum == 0) {
            if (fillFreeEntry(blockNum, i, file_inode_num, entry_name) == ERROR) {
                return ERROR;
            }
            return 0;
        }
    }
    return -2;//returns -2 when no free entries were found.
}


static int eraseFile(int curr_num) {
    int block = (curr_num / INODES_PER_BLOCK) + 1;
    struct inode inode_buf[INODES_PER_BLOCK];
    if(ReadSector(block, inode_buf) == ERROR) {
        return ERROR;
    }
    int index = curr_num % INODES_PER_BLOCK;
    struct inode *fileInode = &inode_buf[index];

    if (changeStateBlocks(fileInode, true) == ERROR) {
        return ERROR;
    }
    fileInode->size = 0;
    if(WriteSector(block, inode_buf) == ERROR) {
        return ERROR;
    }
    return 0;
}



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

static int search(int start_inode, char *pathname) {
    TracePrintf(0, "Start of search with pathname = %s!!!!!!!!\n", pathname);
    int curr_num = start_inode;
    char *token = strtok(pathname, "/");
    while (token != NULL) {
        struct inode curr_inode = findInode(curr_num);
        // Find a directory entry with name = token.
        TracePrintf(0, "curr_num = %i\n", curr_num);
        TracePrintf(0, "token = %s\n", token);
        curr_num = searchInDirectory(&curr_inode, token);
        if (curr_num == ERROR) {
            TracePrintf(0, "Error in searchInDirectory!!!\n");
            return ERROR;
        }
        token = strtok(NULL, "/");
    }
    return curr_num;
}


static struct inode findInode(int inode_num) {
    int block = (inode_num / INODES_PER_BLOCK) + 1;
    struct inode inode_buf[INODES_PER_BLOCK];
    ReadSector(block, inode_buf);
    int index = inode_num % INODES_PER_BLOCK;
    return inode_buf[index];
}

// Returns an inode_num of the file we are looking for in the given directory or ERROR if the file is not there.
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
    for (i = 0; i < MIN(NUM_DIRECT, num_blocks); i++) {
        int out_inode = find_entry_in_block(curr_inode->direct[i], entry_name, num_entries_left);
        if (out_inode != ERROR) {
            return out_inode;
        } 
        num_entries_left -= ENTRIES_PER_BLOCK;
    }
    if (num_blocks > NUM_DIRECT) {//then we need to search in the indirect block
        int block_to_read = curr_inode->indirect;
        int indirect_buf[BLOCKSIZE/sizeof(int)];
        if (ReadSector(block_to_read, indirect_buf) == ERROR) {
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

static int find_entry_in_block(int block, char *entry_name, int num_entries_left) {
    struct dir_entry dir_buf[ENTRIES_PER_BLOCK];
    if (ReadSector(block, dir_buf) == ERROR) {
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

static int get_num_blocks(int size) {
    int num_blocks = size / BLOCKSIZE;
    if (size % BLOCKSIZE != 0) {
        num_blocks++;
    }
    return num_blocks;
}

static int fillFreeEntry(int blockNum, int index, int file_inode_num, char *entry_name) {
    struct dir_entry dir_buf[ENTRIES_PER_BLOCK];
    if (ReadSector(blockNum, dir_buf) == ERROR) {
        return ERROR;
    }
    dir_buf[index].inum = file_inode_num;
    memcpy(dir_buf[index].name, entry_name, DIRNAMELEN);
    if (WriteSector(blockNum, dir_buf) == ERROR) {
        return ERROR;
    }
    return 0;
}


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
    CopyFrom(pid, old, msg->oldname, MAXPATHNAMELEN);
    CopyFrom(pid, new, msg->newname, MAXPATHNAMELEN);
    // Reply with a message that contains return status.
    msg->numeric1 = linkInodes(curr_dir, old, new);
}

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

    struct inode old_inode = findInode(old_inode_num);
    if (old_inode.type != INODE_REGULAR) {
        fprintf(stderr, "Oldname has a wrong type.\n");
        return ERROR;
    }

    int ret_inode_num = createDirEntry(new_dir, newname, old_inode_num);
    if (ret_inode_num != old_inode_num) {
        TracePrintf(0, "createDirEntry returned %i\n", ret_inode_num);
        TracePrintf(0, "but old_inode_num = %i\n", old_inode_num);
        return ERROR;
    } else {
        return 0;
    }
}

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
    CopyFrom(pid, pathname, msg->ptr, MAXPATHNAMELEN);
    // Reply with a message that contains return status.
    msg->numeric1 = unlinkInode(curr_dir, pathname);
}

static int unlinkInode(int curr_dir, char *pathname) {
    TracePrintf(0, "================\n");
    TracePrintf(0, "About to unlink: %s in directory#%i\n", pathname, curr_dir);
    // Get parent directory.
    int parent_dir = getParentDir(curr_dir, &pathname);
    if (parent_dir == ERROR) {
        fprintf(stderr, "File has an invalid parent directory.\n");
        return ERROR;
    }
    TracePrintf(0, "parent_dir = %i\n", parent_dir);
    TracePrintf(0, "pathname = %s\n", pathname);
    int inode_num = getInodeNumber(parent_dir, pathname);
    TracePrintf(0, "inode_num = %i\n", inode_num);
    if (inode_num == ERROR) {
        fprintf(stderr, "This file does not exists.\n");
        return ERROR;
    }

    struct inode inode_struct = findInode(inode_num);
    TracePrintf(0, "A type of inode is %i\n", inode_struct.type);
    if (inode_struct.type != INODE_REGULAR) {
        fprintf(stderr, "File has a wrong type.\n");
        return ERROR;
    }
    TracePrintf(0, "About to do removeDirEntry\n");
    int nlink = removeDirEntry(parent_dir, pathname);
    TracePrintf(0, "nlink = %i\n", nlink);
    if (nlink == ERROR) {
        return ERROR;
    } else if (nlink == 0) {
        TracePrintf(0, "About to do deleteInode\n");
        if (deleteInode(inode_num) == ERROR) {
            return ERROR;
        }
        TracePrintf(0, "Success for deleteInode :)\n");
    }
    return 0;
}

static int removeDirEntry(int dir, char *file_name) {
    int file_inode_num = getInodeNumber(dir, file_name);
    if (file_inode_num == ERROR) {
        fprintf(stderr, "This file does not exists.\n");
        return ERROR;
    }
    struct inode inode_struct = findInode(file_inode_num);
    TracePrintf(0, "Before removal:  inode_struct.nlink = %i\n", inode_struct.nlink);
    struct inode curr_dir_inode = findInode(dir);

    int num_entries_left = curr_dir_inode.size / sizeof(struct dir_entry);

    int num_blocks = get_num_blocks(curr_dir_inode.size);

    int i;
    for (i = 0; i < MIN(NUM_DIRECT, num_blocks); i++) {
        int return_val = removeEntryInBlock(curr_dir_inode.direct[i], file_name, file_inode_num, num_entries_left);
        if(return_val == ERROR) {
            return ERROR;
        } else if (return_val == 0) {
            inode_struct.nlink--;
            if(writeInodeToDisc(file_inode_num, inode_struct) == ERROR) {
                return ERROR;
            }
            TracePrintf(0, "After removal:  inode_struct.nlink = %i\n", inode_struct.nlink);
            return inode_struct.nlink;
        }
        num_entries_left -= ENTRIES_PER_BLOCK;
    }
    if (num_blocks > NUM_DIRECT) {//then we need to search in the indirect block
        int block_to_read = curr_dir_inode.indirect;
        int indirect_buf[BLOCKSIZE/sizeof(int)];
        if (ReadSector(block_to_read, indirect_buf) == ERROR) {
            return ERROR;
        }
        int j;
        for (j = 0; j < (num_blocks - NUM_DIRECT); j++) {
            int return_val = removeEntryInBlock(indirect_buf[j], file_name, file_inode_num, num_entries_left);
            if(return_val == ERROR) {
                return ERROR;
            } else if (return_val == 0) {
                inode_struct.nlink--;
                if(writeInodeToDisc(file_inode_num, inode_struct) == ERROR) {
                    return ERROR;
                }
                TracePrintf(0, "After removal:  inode_struct.nlink = %i\n", inode_struct.nlink);
                return inode_struct.nlink;
            }
            num_entries_left -= ENTRIES_PER_BLOCK;
        }
    }
    return ERROR;
}

// Returns 0 if SUCCESS, -1 if ERROR, -2 if not found
static int removeEntryInBlock(int blockNum, char *entry_name, int file_inode_num, int num_entries_left) {
    struct dir_entry dir_buf[ENTRIES_PER_BLOCK];
    if (ReadSector(blockNum, dir_buf) == ERROR) {
        return ERROR;
    }
    int i;
    for (i = 0; i < MIN(ENTRIES_PER_BLOCK, num_entries_left); i++) {
        if (dir_buf[i].inum == file_inode_num && strncmp(dir_buf[i].name, entry_name, DIRNAMELEN) == 0) {
            if (freeDirEntry(blockNum, i) == ERROR) {
                return ERROR;
            }
            return 0;
        }
    }
    return -2;//returns -2 when no free entries were found.
}

static int freeDirEntry(int blockNum, int index) {
    struct dir_entry dir_buf[ENTRIES_PER_BLOCK];
    if (ReadSector(blockNum, dir_buf) == ERROR) {
        return ERROR;
    }
    dir_buf[index].inum = 0;
    memset(dir_buf[index].name, '\0', DIRNAMELEN);
    if (WriteSector(blockNum, dir_buf) == ERROR) {
        return ERROR;
    }
    return 0;
}

static int deleteInode(int inode_num) {
    struct inode inode_struct = findInode(inode_num);
    inode_struct.type = INODE_FREE;
    inode_struct.size = 0;
    if (changeStateBlocks(&inode_struct, true) == ERROR) {
        return ERROR;
    }
    if (writeInodeToDisc(inode_num, inode_struct) == ERROR) {
        return ERROR;
    }
    inodemap[inode_num] = true;
    return 0;
}

static void write(struct my_msg *msg, int pid) {
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
    struct inode file_inode = findInode(file_inode_num);
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
    CopyFrom(pid, buf_to_write, msg->ptr, size_to_write);

    int sizeWrite = fillAndWrite(file_inode_num, file_inode, size_to_write, start_pos);
    if(sizeWrite == ERROR) {
        msg->numeric1 = ERROR;
        return;
    }
    msg->numeric1 = sizeWrite;
}

static int writeToInode(struct inode* inode_to_write, int size, int start_pos, char* buf_to_write) {

    int max_size = (NUM_DIRECT + BLOCKSIZE/(sizeof(int)))*BLOCKSIZE;
    //determine how much we can write by subtracting what we have written from what we can write
    int left_to_write = max_size - inode_to_write.size;

    int size_to_write = MIN(left_to_write, size);


    int bytes_left = size_to_write;

    int num_blocks = get_num_blocks(inode_to_read.size);
    
    int starting_block = start_pos/BLOCKSIZE;
    int start_within_block = start_pos % BLOCKSIZE;
    int i;
    for (i = starting_block; i < NUM_DIRECT && bytes_left > 0; i++) {

        if(starting_block >= num_blocks) {//then we do not need to allocate a new block
               
        }
        
        int bytes_written -= WriteBlock(inode_to_write.direct[i], start_within_block, bytes_left, buf_to_write);
        if(bytes_written == ERROR) {
            return size_to_write - bytes_left;
        }
        inode_to_write.size += bytes_written;

        buf_to_write+=bytes_written;

        bytes_left -= bytes_written;
        
        start_within_block = 0;
    }

    if (bytes_left > 0) {//then we need to search in the indirect block
        
        int indirect_buf[BLOCKSIZE/sizeof(int)];
        if(num_blocks <= NUM_DIRECT) {
            inode_to_write.indirect = find_free_block();
            if(inode_to_write.indirect == ERROR) {
                return size_to_write - bytes_left;
            }
        }
        //now we definetely have the indirect block, so we can start writing into it
        if (ReadSector(block_to_read, indirect_buf) == ERROR) {
            return size_to_write - bytes_left;
        }
        int j;
        for (j = starting_block - NUM_DIRECT; bytes_left > 0; j++) {
            
            if(starting_block >= num_blocks) {
                //we need to allocate a new block
                //and put it into indirect_buf[j]
            } 
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

static int WriteBlock(int block_num, int start_within_block, int bytes_left, char* buf_to_write) {
    
    if(ReadSector(block_num, buf) == ERROR) {
        return ERROR;
    }
    int bytes_to_write = MIN(bytes_left, SECTORSIZE - start_within_block);
    memcpy(buf + start_within_block, buf_to_write, bytes_to_write);
    // Advance a buffer by bytes_to_read bytes.
    if(WriteSector(block_num, buf) == ERROR) {
        return ERROR;
    }
    return bytes_to_write;
}



//returns how much was actually write to the file
static int fillAndWrite(int inode_num, struct inode inode_to_write, int size, int start_pos, char* buf_to_write) {
    // Fill the holes.
    int bytes_to_fill = start_pos - inode_to_write.size;
    if (bytes_to_fill > 0) {
        char hole_buf[bytes_to_fill];
        memset(hole_buf, '\0', bytes_to_fill);
        if (writeToInode(inode_to_write, bytes_to_fill, inode_to_write.size, hole_buf) == ERROR) {
            return ERROR;
        }
    }
    int sizeWritten = -1;
    // Write a buffer.
    if ((sizeWritten = writeToInode(inode_to_write, size, start_pos, buf_to_write)) == ERROR) {
        return ERROR;
    }
    inode_to_write.size+=sizeWritten;
    writeInodeToDisc(inode_num, inode_to_write);
    return sizeWritten;
}

static char* ReadBlock(int block_num, int start_pos, int bytes_left, char* buf_to_read) {
    if(ReadSector(block_num, buf)) {
        return NULL;
    }
    int bytes_to_read = MIN(bytes_left, SECTORSIZE - start_pos);
    memcpy(buf_to_read, buf+start_pos, bytes_to_read);
    // Advance a buffer by bytes_to_read bytes.
    return buf_to_read + bytes_to_read;
}
