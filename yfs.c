#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

char buf[SECTORSIZE];

// struct free_entity {
//     int num;
//     struct free_entity *next;
// }

// static struct free_entity inode_head = {0, NULL};
// static struct free_entity block_head = {0, NULL};

static bool* inodemap;
static bool* blockmap;

static void start_user_program(char *argv[]);
static void getFreeInodes(int num_inodes);
static void getFreeBlocks(struct inode* currNode);

int main(int argc, char *argv[]) {
    
    Register(FILE_SERVER);
    
    int status = ReadSector(1, buf);
    printf("Read of status: %d\n", status);
    struct fs_header *header = (struct fs_header*)buf;
    //Now we can get the number of inodes
    int num_inodes = header->num_inodes;
    int num_blocks = header->num_blocks;
    inodemap = malloc(sizeof(bool) * num_inodes);
    blockmap = malloc(sizeof(bool) * num_blocks);

    int i;
    for(i = 0; i < num_blocks; i++) {
        blockmap[i] = true;
    }
    getFreeInodes(num_inodes);

    

    if(argc > 1)
        start_user_program(argv);
    return 0;
}

static void start_user_program(char *argv[]) {
        int child;
        child = Fork();
        if (child == 0) {
            Exec(argv[1], argv+1);
        }
}

static void getFreeInodes(int num_inodes) {
    int inodes_per_block = BLOCKSIZE / sizeof(struct inode);
    struct inode* currNode = (struct inode *)buf + 1; //address of the first inode
    int i = 1;
    int node_count = 0;
    while(node_count < num_inodes) {
       
        for (;i<inodes_per_block; i++) {
            if (node_count >= num_inodes) {
                break;
            }
            else if(currNode->type == INODE_FREE) {
                inodemap[node_count] = true;
            } else {
                inodemap[node_count] = false;
                getFreeBlocks(currNode);
            }
            currNode++;
            node_count++;
        }
        i = 0;
    }
        
}

void getFreeBlocks(struct inode *currNode) {
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
        ReadSector(block_to_read, buf);
        int j;
        int* currBlockNum = (int*)buf;
        for (j = 0; j < (num_blocks - NUM_DIRECT); j++) {
            blockmap[currBlockNum[j]] = false;
        }
    }
}
