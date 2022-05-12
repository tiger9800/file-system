#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include "msg_types.h"

#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define INODES_PER_BLOCK BLOCKSIZE/INODESIZE
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


int main(int argc, char *argv[]) {
    
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
    int NUM_INODES = header->num_inodes;
    int NUM_BLOCKS = header->num_blocks;
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

static void handleMsg(struct my_msg *msg, int pid) {
    switch(msg->type) {
        case OPEN:
            open(msg, pid);
            break;
        case CREATE:
            create(msg, pid);
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
    //find the position of the last /
    int i;
    int last_slash = -1;
    for(i = strlen(pathname) - 1; i >= 0; i--) {
        if(pathname[i] == '/') {
            last_slash = i;
            break;
        }
    }
    int new_file_inode_num;
    if(last_slash == -1) { 
        // We are trying to create a file in the curret directory.
        // msg->numeric1 contains an inode_num of a current directory.
        new_file_inode_num = createFileInDir(msg->numeric1, pathname);
    } else if(last_slash == 0) {
        // We are trying to create in the root directory.
        new_file_inode_num = createFileInDir(ROOTINODE, pathname+1);
    } else {
        // We need to locate the parent direcorty inode.
        char* file_name = pathname + i + 1;
        // Check if slash was the last character in a path name.
        if (file_name[0] == '\0') {
            // Can't use this funtion for creating ".".
            msg->numeric1 = ERROR;
            return;
        }
        pathname[i] = '\0';
        int parent_dir_inode_num = getInodeNumber(msg->numeric1, pathname);
        new_file_inode_num = createFileInDir(parent_dir_inode_num, file_name);
    }
    //Reply with a message that has an inode of the new file or ERROR.
    msg->numeric1 = new_file_inode_num;
    // Reply message also contains a reuse count.
    if(msg->numeric1 != ERROR) {
        msg->numeric2 = findInode(msg->numeric1).reuse;
    }
}

// Returns an inode number of the newly-created or existing file; returns ERROR if failure.
static int createFileInDir(int dir_inode_num, char* file_name) {
    if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
        // Can't create such directory entries manually.
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
        // Erase file contents and return.
        struct inode exist_inode = findInode(inode_num);
        if(exist_inode.type == INODE_DIRECTORY) {
            return ERROR;
        }
        else {//the file is not a directory
            if(eraseFile(inode_num) == ERROR) {
                return ERROR;
            }    
        }
        
    } else {
        // File does not exist in the directory, so we create it from scratch.
        inode_num = create_new_file(dir_inode_num, entry_name);
    }
    return inode_num;
}
// Returns an inode number of the newly-created file or ERROR.
static int create_new_file(int dir_inode_num, char* file_name) {
    // Storage for a new inode.
    struct inode new_node;
    int free_inode_num = findFreeInodeNum(&new_node);
    if(free_inode_num == ERROR) {
        return ERROR;
    }
    // Create a new directory entry with inum = free_inode_num, name == file_name.
    if(createNewDirEntry(file_name, free_inode_num, dir_inode_num) == ERROR) {
        return ERROR;
    }
    // We set inodemap[free_inode_num] to false in this function.
    writeInodeToDisc(free_inode_num, new_node);
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
    writeInodeToDisc(dir_inode_num, curr_dir_inode);
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
    int curr_num = start_inode;
    char *token = strtok(pathname, "/");
    while (token != NULL) {
        struct inode curr_inode = findInode(curr_num);
        // Find a directory entry with name = token.
        curr_num = searchInDirectory(&curr_inode, token);
        if (curr_num == ERROR) {
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