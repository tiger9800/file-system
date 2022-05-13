#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include "msg_types.h"

#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

const int INODES_PER_BLOCK =  BLOCKSIZE/INODESIZE;
const int ENTRIES_PER_BLOCK =  BLOCKSIZE/sizeof(struct dir_entry);

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

static int getInodeNumber(int curr_dir, char *pathname);
static int search(int start_inode, char *pathname);
static struct inode findInode(int curr_num);
static int searchInDirectory(struct inode *curr_inode, char *token);
static int find_entry_in_block(int block, char *token, int num_entries_left);
static int get_num_blocks(int size);
static int fillFreeEntry(int blockNum, int index, int file_inode_num, char *entry_name);
static int createFileInDir(int dir_inode_num, char* file_name);
static int create_new_file(int dir_inode_num, char* file_name);
static int writeInodeToDisc(int inode_num, struct inode inode_struct);
static int findFreeInodeNum(struct inode* new_inode);
static int createNewDirEntry(char* file_name, int file_inode_num, int dir_inode_num);
static int updateFreeEntry(struct inode *curr_dir_inode, char *file_name, int file_inode_num);
static int updateFreeEntryInBlock(int blockNum, char* entry_name, int file_inode_num, int num_entries_left);
static int find_free_block();
static int eraseFile(int inode_num);
static int readFromInode(struct inode inode_to_read, int size, int start_pos, char* buf_to_read);
static char* ReadBlock(int block_num, int start_pos, int bytes_left, char* buf_to_read);
static void read(struct my_msg *msg, int pid);


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
                changeStateBlocks(currNode, false);
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
    }
}

static void open(struct my_msg *msg, int pid) {
    if (msg->ptr == NULL) {
        msg->numeric1 = ERROR;
        return;
    }
    char pathname[MAXPATHNAMELEN];
    CopyFrom(pid, pathname, msg->ptr, MAXPATHNAMELEN);
    TracePrintf(0, "pathname after CopyFrom = %s\n", pathname);
    // msg->numeric1 initially contains a current directory.
    // Reply with a message that has an inode number of the opened file or ERROR.
    msg->numeric1 = getInodeNumber(msg->numeric1, pathname);
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
    char pathname[MAXPATHNAMELEN];
    CopyFrom(pid, pathname, msg->ptr, MAXPATHNAMELEN);
    //TracePrintf(0, "PathName after copyFrom %s\n", pathname);
    //find the position of the last /
    int i;
    int last_slash = -1;
    for(i = strlen(pathname) - 1; i >= 0; i--) {
        if(pathname[i] == '/') {
            last_slash = i;
            break;
        }
    }
    //TracePrintf(0, "Location of the last slash: %i\n", last_slash);
    int new_file_inode_num;
    if(last_slash == -1) { 
        // We are trying to create a file in the curret directory.
        // msg->numeric1 contains an inode_num of a current directory.
        //TracePrintf(0, "No slash\n");
        if (strlen(pathname) == 0) {
            msg->numeric1 = ERROR;
            return;
        }
        new_file_inode_num = createFileInDir(msg->numeric1, pathname);
    } else if(last_slash == 0) {
        // We are trying to create in the root directory.
        if (strlen(pathname + 1) == 0) {
            msg->numeric1 = ERROR;
            return;
        }
        new_file_inode_num = createFileInDir(ROOTINODE, pathname+1);
        //TracePrintf(0, "Parent\n");
    } else {
        // We need to locate the parent direcorty inode.
        char* file_name = pathname + i + 1;
        // Check if slash was the last character in a path name.
        if (strlen(file_name) == 0) {
            // Can't use this funtion for creating ".".
            msg->numeric1 = ERROR;
            return;
        }
        pathname[i] = '\0';
        int parent_dir_inode_num = getInodeNumber(msg->numeric1, pathname);
        new_file_inode_num = createFileInDir(parent_dir_inode_num, file_name);
    }
    TracePrintf(0, "new_file_inode_num is %i\n", new_file_inode_num);
    //Reply with a message that has an inode of the new file or ERROR.
    msg->numeric1 = new_file_inode_num;
    
    // Reply message also contains a reuse count.
    if(msg->numeric1 != ERROR) {
        msg->numeric2 = findInode(msg->numeric1).reuse;
    }
}

static void read(struct my_msg *msg, int pid) {
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
        for (j = 0; j < (num_blocks - NUM_DIRECT) && bytes_left > 0; j++) {
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
    memcpy(buf_to_read, buf+start_pos, MIN(bytes_left, SECTORSIZE));
    return buf_to_read;
}

// Returns an inode number of the newly-created or existing file; returns ERROR if failure.
static int createFileInDir(int dir_inode_num, char* file_name) {
    if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
        // Can't create such directory entries manually.
        //TracePrintf(0, "Tried to create an invalid file\n");
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
    //TracePrintf(0, "Entry name %s\n", entry_name);
    struct inode dir_inode = findInode(dir_inode_num);

    // Look through entries and find an empty entry. If it does not exist, then create a new one.
    int inode_num = searchInDirectory(&dir_inode, entry_name);
    //TracePrintf(0, "Inode num after the search in directory %i\n", inode_num);
    if (inode_num != ERROR) {
        // File with this name already exists in the directory.
        // Erase file contents and return.
        struct inode exist_inode = findInode(inode_num);
        if(exist_inode.type == INODE_DIRECTORY) {
            return ERROR;
        }
        else {//the file is not a directory
            //TracePrintf(0, "File exists, so we are erasing it !!!!!\n");
            if(eraseFile(inode_num) == ERROR) {
                return ERROR;
            }    
        }
        
    } else {
        // File does not exist in the directory, so we create it from scratch.
        //TracePrintf(0, "There is no file like this\n");
        inode_num = create_new_file(dir_inode_num, entry_name);
        if(inode_num == ERROR) {
            //TracePrintf(0, "Receive an error from create_new_file\n");
            fprintf(stderr, "Could not create a new file\n");
            return ERROR;
        }
        //TracePrintf(0, "Here is the inode_num of the newly created file %i\n", inode_num);
    }
    return inode_num;
}
// Returns an inode number of the newly-created file or ERROR.
static int create_new_file(int dir_inode_num, char* file_name) {
    // Storage for a new inode.
    struct inode new_node;
    int free_inode_num = findFreeInodeNum(&new_node);
    //TracePrintf(0, "Here is the free_inode_num %i\n", free_inode_num);
    if(free_inode_num == ERROR) {
        return ERROR;
    }
    // Create a new directory entry with inum = free_inode_num, name == file_name.
    if(createNewDirEntry(file_name, free_inode_num, dir_inode_num) == ERROR) {
        //TracePrintf(0, "Create new dir entry returned an error\n");
        return ERROR;
    }
    // We set inodemap[free_inode_num] to false in this function.
    writeInodeToDisc(free_inode_num, new_node);
    TracePrintf(0, "create_new_file is successfull, free_inode_num is %i\n", free_inode_num);
    return free_inode_num;
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

// Returns a free inode number.
static int findFreeInodeNum(struct inode* new_inode) {
    struct inode inode_buf[INODES_PER_BLOCK];
    int i;
    int free_inode_num = -1;
    for(i = 0; i < NUM_INODES; i++) {
        if(inodemap[i]) {
            free_inode_num = i;
            break;
        }
    }
    //TracePrintf(0, "free inode num %i\n", free_inode_num);
    //TracePrintf(0,"NUM_INODES: %i\n", NUM_INODES);
    if (free_inode_num == -1) {
        return ERROR;
    }
    int block = (free_inode_num / INODES_PER_BLOCK) + 1;
    if(ReadSector(block, inode_buf) == ERROR) {
        return ERROR;
    }
    int index = free_inode_num % INODES_PER_BLOCK;
    // Initialize an inode fields.
    inode_buf[index].type = INODE_REGULAR;
    inode_buf[index].size = 0;
    inode_buf[index].reuse++;
    inode_buf[index].nlink = 1;
    *new_inode = inode_buf[index];
    return free_inode_num;
}

// Returns 0 on SUCCESS, -1 on ERROR
static int createNewDirEntry(char* file_name, int file_inode_num, int dir_inode_num) {
    //TracePrintf(0, "dir_inode_num in CreateNewDirEntry: %i\n", dir_inode_num);
    struct inode curr_dir_inode = findInode(dir_inode_num);
    //TracePrintf(0, "inode type of curr_dir_inode %i\n", curr_dir_inode.type);
    if (curr_dir_inode.type != INODE_DIRECTORY) {
        //TracePrintf(0, "dir_inode_num is not a directory\n");
        return ERROR;
    }

    int last_block;
    if ((last_block = updateFreeEntry(&curr_dir_inode, file_name, file_inode_num)) <= 0) {
        // Either SUCCESS (we found a free entry and modified it) or ERROR.
        //TracePrintf(0, "There was a free entry found in a existing block, so we update it\n");
        //TracePrintf(0, "Last block: %i\n", last_block);
        return last_block;
    }
    //TracePrintf(0, "Positive last block %i\n", last_block);

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
        TracePrintf(0, "Allocate a new block!!!!!!!!!\n");
        int free_block_num = find_free_block();
        TracePrintf(0, "free_block_num: %i\n", free_block_num);
        if(free_block_num == ERROR) {
            return ERROR;
        }
        // Fill the first entry in this newly-allocated block.
        if (fillFreeEntry(free_block_num, 0, file_inode_num, file_name) == ERROR) {
            TracePrintf(0, "fillFreeEntry returned -1\n");
            return ERROR;
        }
        TracePrintf(0, "fillFreeEntry is successful\n");
        int num_blocks = get_num_blocks(curr_dir_inode.size);
        TracePrintf(0, "num_blocks: %i\n", num_blocks);
        if(num_blocks < NUM_DIRECT) {
             TracePrintf(0, "Direct block to be used\n");
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
    writeInodeToDisc(dir_inode_num, curr_dir_inode);
    TracePrintf(0, "createNewDirEntry is successful\n");
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

    changeStateBlocks(fileInode, true);
    fileInode->size = 0;
    if(WriteSector(block, inode_buf) == ERROR) {
        return ERROR;
    }
    return 0;
}



static int getInodeNumber(int curr_dir, char *pathname) {
    TracePrintf(0, "pathname in getInodeNumber = %s\n", pathname);
    TracePrintf(0, "Inside of getInodeNumber\n");
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
    TracePrintf(0, "==================\n");
    TracePrintf(0, "Start doing search for pathname = %s\n", pathname);
    int curr_num = start_inode;
    char *token = strtok(pathname, "/");
    while (token != NULL) {
        TracePrintf(0, "Current token is %s\n", token);
        struct inode curr_inode = findInode(curr_num);
        // Find a directory entry with name = token.
        curr_num = searchInDirectory(&curr_inode, token);
        if (curr_num == ERROR) {
            TracePrintf(0, "curr_num is ERROR!!!!\n");
            return ERROR;
        }
        token = strtok(NULL, "/");
    }
    TracePrintf(0, "Search returns inode_num = %d\n", curr_num);
    return curr_num;
}


static struct inode findInode(int inode_num) {
    int block = (inode_num / INODES_PER_BLOCK) + 1;
    //TracePrintf(0, "Block %i for inode_num %i\n", block, inode_num);
    struct inode inode_buf[INODES_PER_BLOCK];
    ReadSector(block, inode_buf);
    int index = inode_num % INODES_PER_BLOCK;
    //TracePrintf(0, "Index %i for inode_num %i\n", index, inode_num);
    //TracePrintf(0, "INODES_PER_BLOCK %i\n", INODES_PER_BLOCK);
    //TracePrintf(0, "Type of the inode with num %i is %i\n", inode_num, inode_buf[index].type);
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