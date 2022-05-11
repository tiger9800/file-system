#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include "msg_types.h"

#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

char buf[SECTORSIZE];

static bool* inodemap;
static bool* blockmap;

static void start_user_program(char *argv[]);
static int getFreeInodes(int num_inodes);
static int getFreeBlocks(struct inode* currNode);
static void getTakenBlocks(int num_inodes);
static int init();
static void handleMsg(struct my_msg *msg, int pid);
static void open(struct my_msg *msg, int pid);
static int getInodeNumber(int curr_dir, char *pathname);
static int search(int start_inode, char *pathname);
static struct inode findInode(int curr_num);
static int searchInDirectory(struct inode *curr_inode, char *token);
static int find_entry_in_block(int block, char *token, int num_entries_left);

int main(int argc, char *argv[]) {
    
    if (Register(FILE_SERVER) == ERROR) {
        return ERROR;
    }
    printf("Registered\n");
    if (ReadSector(1, buf) == ERROR) {
        printf("Error\n");
        return ERROR;
    }
    printf("Read first sector\n");
    struct fs_header *header = (struct fs_header*)buf;
    //Now we can get the number of inodes
    int num_inodes = header->num_inodes;
    int num_blocks = header->num_blocks;
    inodemap = malloc(sizeof(bool) * (num_inodes + 1));
    blockmap = malloc(sizeof(bool) * num_blocks);
    if (init() == ERROR) {
        return ERROR;
    }
    printf("Inited\n");
    if(argc > 1)
        start_user_program(argv);
    //return 0;
    int i;
    for (i = 0; i < num_inodes + 1; i++) {
        if (inodemap[i] == false) {
            printf("Inode #%d is occupied\n", i);
        }
    }

    for (i = 0; i < num_blocks; i++) {
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

static int getFreeInodes(int num_inodes) {
    printf("In getFree\n");
    int inodes_per_block = BLOCKSIZE / INODESIZE;

    struct inode* currNode = (struct inode *)buf + 1; //address of the first inode
    printf("Type of root is %d\n", currNode->type);
    inodemap[0] = false;
    int node_count = 1;
    int sector = 1;
    printf("num_inodes is %d\n", num_inodes);
    printf("inodes_per_block is %d\n", inodes_per_block);
    while(node_count <= num_inodes) {
        printf("Before second while_loop: node_count is %d\n", node_count);
        while (node_count / inodes_per_block != sector) {
            printf("node_count is %d\n", node_count);
            if (node_count > num_inodes) {
                break;
            }
            else if(currNode->type == INODE_FREE) {
                inodemap[node_count] = true;
            } else {
                printf("Occupied #%d\n", node_count);
                printf("Type is %d\n", currNode->type);
                inodemap[node_count] = false;
                getFreeBlocks(currNode);
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

static int getFreeBlocks(struct inode *currNode) {
    int num_blocks = currNode->size / BLOCKSIZE;
    if (currNode->size % BLOCKSIZE != 0) {
        num_blocks++;
    }
    int i;
    for (i = 0; i < MIN(NUM_DIRECT, num_blocks); i++) {
        blockmap[currNode->direct[i]] = false;
    }
    if (num_blocks > NUM_DIRECT) {//then we need to search in the indirect block
        int block_to_read = currNode->indirect;
        int indirect_buf[SECTORSIZE/sizeof(int)];
        if (ReadSector(block_to_read, indirect_buf) == ERROR) {
            return ERROR;
        }
        int j;
        for (j = 0; j < (num_blocks - NUM_DIRECT); j++) {
            blockmap[indirect_buf[j]] = false;
        }
    }
    return 0;
}
static void getTakenBlocks(int num_inodes) {
    // Boot block.
    blockmap[0] = false;
    // Inodes' blocks.
    int inode_size = (num_inodes + 1) * INODESIZE;
    int num_blocks = inode_size / BLOCKSIZE;
    if (inode_size % BLOCKSIZE != 0) {
        num_blocks++;
    }
    printf("num_blocks is %d\n", num_blocks);
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
    printf("Read first sector\n");
    struct fs_header *header = (struct fs_header*)buf;
    //Now we can get the number of inodes
    int num_inodes = header->num_inodes;
    int num_blocks = header->num_blocks;
    inodemap = malloc(sizeof(bool) * (num_inodes + 1));
    blockmap = malloc(sizeof(bool) * num_blocks);

    int i;
    for(i = 0; i < num_blocks; i++) {
        blockmap[i] = true;
    }
    printf("Takeb blocks\n");
    getTakenBlocks(num_inodes);
    printf("Done\n");
    if (getFreeInodes(num_inodes) == ERROR) {
        return ERROR;
    }
    return 0;
}

static void handleMsg(struct my_msg *msg, int pid) {
    switch(msg->type) {
        case OPEN:
            open(msg, pid);
            break;
    }
}

/**
 * @brief Open the file/directory and modii fy the message to return the inode number.
 * 
 * @param msg 
 * @param pid 
 */
static void open(struct my_msg *msg, int pid) {
    if (msg->ptr == NULL) {
        msg->numeric = ERROR;
        return;
    }
    char pathname[MAXPATHNAMELEN];
    CopyFrom(pid, pathname, msg->ptr, MAXPATHNAMELEN);
    msg->numeric = getInodeNumber(msg->numeric, pathname);
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

static struct inode findInode(int curr_num) {
    int inodes_per_block = BLOCKSIZE / INODESIZE;
    int block = (curr_num / inodes_per_block) + 1;
    struct inode inode_buf[inodes_per_block];
    ReadSector(block, inode_buf);
    int index = curr_num % inodes_per_block;
    return inode_buf[index];
}

static int searchInDirectory(struct inode *curr_inode, char *token) {
    if (curr_inode == NULL || curr_inode->type != INODE_DIRECTORY) {
        return ERROR;
    }
    int num_entries_left = curr_inode->size / sizeof(struct dir_entry);
    int entries_per_block = BLOCKSIZE / sizeof(struct dir_entry);
    int num_blocks = curr_inode->size / BLOCKSIZE;
    if (curr_inode->size % BLOCKSIZE != 0) {
        num_blocks++;
    }
    int i;
    for (i = 0; i < MIN(NUM_DIRECT, num_blocks); i++) {
        int out_inode = find_entry_in_block(curr_inode->direct[i], token, num_entries_left);
        if (out_inode != ERROR) {
            return out_inode;
        } 
        num_entries_left -= entries_per_block;
    }
    if (num_blocks > NUM_DIRECT) {//then we need to search in the indirect block
        int block_to_read = curr_inode->indirect;
        int indirect_buf[BLOCKSIZE/sizeof(int)];
        if (ReadSector(block_to_read, indirect_buf) == ERROR) {
            return ERROR;
        }
        int j;
        for (j = 0; j < (num_blocks - NUM_DIRECT); j++) {
            int out_inode = find_entry_in_block(indirect_buf[j], token, num_entries_left);
            if (out_inode != ERROR) {
                return out_inode;
            }
            num_entries_left -= entries_per_block;
        }
    }
    return ERROR;
}

static int find_entry_in_block(int block, char *token, int num_entries_left) {
    if (strlen(token) > DIRNAMELEN) {
        fprintf(stderr, "Entry name %s is too long.\n", token);
        return ERROR;
    }
    char entry_name[DIRNAMELEN];
    memset(entry_name, '\0', DIRNAMELEN);
    memcpy(entry_name, token, strlen(token));

    int entries_per_block = BLOCKSIZE/sizeof(struct dir_entry);
    struct dir_entry dir_buf[entries_per_block];
    if (ReadSector(block, dir_buf) == ERROR) {
        return ERROR;
    }
    int i;
    for (i = 0; i < MIN(entries_per_block, num_entries_left); i++) {
        if (dir_buf[i].inum != 0 && strncmp(dir_buf[i].name, entry_name, DIRNAMELEN) == 0) {
            return dir_buf[i].inum;
        }
    }
    return ERROR;
}