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
static int NUM_BLOCKS;
static int NUM_INODES;

static void start_user_program(char *argv[]);
static int getFreeInodes();
static int changeStateBlocks(struct inode* currNode, bool state);
static void getTakenBlocks();
static int init();
static void handleMsg(struct my_msg *msg, int pid);
static void open(struct my_msg *msg, int pid);
static int getInodeNumber(int curr_dir, char *pathname);
static int search(int start_inode, char *pathname);
static struct inode findInode(int curr_num);
static int searchInDirectory(struct inode *curr_inode, char *token);
static int find_entry_in_block(int block, char *token, int num_entries_left);
static void create(struct my_msg *msg, int pid);

int main(int argc, char *argv[]) {
    
    if (Register(FILE_SERVER) == ERROR) {
        return ERROR;
    }
    //Now we can get the number of inodes
    if (init() == ERROR) {
        return ERROR;
    }
    printf("Inited\n");
    if(argc > 1)
        start_user_program(argv);
    //return 0;
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
    printf("In getFree\n");
    int inodes_per_block = BLOCKSIZE / INODESIZE;

    struct inode* currNode = (struct inode *)buf + 1; //address of the first inode
    //printf("Type of root is %d\n", currNode->type);
    inodemap[0] = false;
    int node_count = 1;
    int sector = 1;
    // printf("num_inodes is %d\n", num_inodes);
    // printf("inodes_per_block is %d\n", inodes_per_block);
    while(node_count <= NUM_INODES) {
        printf("Before second while_loop: node_count is %d\n", node_count);
        while (node_count / inodes_per_block != sector) {
            printf("node_count is %d\n", node_count);
            if (node_count > NUM_INODES) {
                break;
            }
            else if(currNode->type == INODE_FREE) {
                inodemap[node_count] = true;
            } else {
                // printf("Occupied #%d\n", node_count);
                // printf("Type is %d\n", currNode->type);
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
    int num_blocks = currNode->size / BLOCKSIZE;
    if (currNode->size % BLOCKSIZE != 0) {
        num_blocks++;
    }
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
    }
    return 0;
}
static void getTakenBlocks() {
    // Boot block.
    blockmap[0] = false;
    // Inodes' blocks.
    int inode_size = (NUM_INODES + 1) * INODESIZE;
    int num_blocks = inode_size / BLOCKSIZE;
    if (inode_size % BLOCKSIZE != 0) {
        num_blocks++;
    }
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
    for(i = 0; i < num_blocks; i++) {
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
    msg->numeric1 = getInodeNumber(msg->numeric1, pathname);
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
    if(last_slash == -1) {//we are trying to create in the curret directory
        new_file_inode_num = createFileInDir(msg->numeric1, pathname);
    }
    else if(last_slash == 0) {//we are trying to create in the root
        new_file_inode_num = createFileInDir(ROOTINODE, pathname+1);
    } else {//we need to locate the parent direcorty inode 
        char* file_name = pathname + i + 1;
        pathname[i] = '\0';
        int parentInode = getInodeNumber(msg->numeric1, file_name);
        new_file_inode_num = createFileInDir(parentInode, file_name);
    }
    //Reply with a message that has an inode of the new file.
    msg->numeric1 = new_file_inode_num;

    if(msg->numeric1 != ERROR) {
        msg->numeric2 = findInode(msg->numeric1).reuse;
    }
}

static int createFileInDir(int inode_num, char* fine_name) {
    struct inode dir_inode = findInode(inode_num);
    //look through entries and find a an empty entry if it does not exist, then create a new one
    int inode_num = searchInDirectory(dir_node, file_name);
    if(inode_num != ERROR) {
        //erase file contents and return
        struct inode exist_inode = findInode(inode_num);
        if(exist_inode.type == INODE_DIRECTORY) {
            return ERROR;
        }
        else {//the file is not a directory
            if(eraseFile(int inode_num) == ERROR) {
                return ERROR;
            }    
        }
        
    } else {//it does not exist in the directory, so we create it from scratch
        inode_num = create_new_file(dir_inode_num, file_name);
    }
    return inode_num;
}

static int create_new_file(int dir_inode_num, char* file_name) {
    struct inode new_node;
    int free_inode_num = findFreeInodeNum(&new_node);
    if(free_inode_num == ERROR) {
        return ERROR;
    }

    if(createNewDirEntry(file_name, free_inode_num, dir_inode_num) == ERROR) {
        return ERROR;
    }
    writeInodeToDisc(free_inode_num, new_node);
    
    return free_inode_num;

}

static int writeInodeToDisc(int inode_num, struct inode inode_struct) {
    int num_inodes_per_block = BLOCKSIZE/INODESIZE;
    int block_num = inode_num/num_inodes_per_block + 1;
    int index = inode_num % num_inodes_per_block;
    struct inode buf[num_inodes_per_block];
    if(ReadSector(block_num, buf) == ERROR) {
        return ERROR;
    }
    buf[index] = inode_struct;
    if(WriteSector(block_num, buf) == ERROR) {
        return ERROR;
    }
}

static int findFreeInodeNum(struct inode* new_inode) {
    struct inode inode_buf[inodes_per_block];
    int i;
    int free_inode_num = -1;
    for(i = 0; i < NUM_INODES; i++) {
        if(inodemap[i]) {
            free_inode_num = i;
            inodemap[i] = false;
            break;
        }
    }
    if(free_inode_num==-1) {
        return ERROR;
    }
    int inodes_per_block = BLOCKSIZE / INODESIZE;
    int block = (free_inode_num / inodes_per_block) + 1;
    if(ReadSector(block, inode_buf) == ERROR) {
        return ERROR;
    }
    int index = curr_num % inodes_per_block;
    struct inode free_inode = inode_buf[index];
    free_inode.type = INODE_REG;
    free_inode.size = 0;
    free_inode.reuse++;
    free_inode.nlink = 1;
    
    *new_inode = free_inode;
    return free_inode_num;
}

static int createNewDirEntry(char* file_name, int file_inode_num, int dir_inode_num) {
    struct inode curr_dir_inode = findInode(dir_inode_num);

    if (curr_dir_inode == NULL || curr_dir_inode->type != INODE_DIRECTORY) {
        return ERROR;
    }
    int num_entries_left = curr_dir_inode.size / sizeof(struct dir_entry);
    int entries_per_block = BLOCKSIZE / sizeof(struct dir_entry);
    int num_blocks = curr_dir_inode.size / BLOCKSIZE;
    if (curr_dir_inode.size % BLOCKSIZE != 0) {
        num_blocks++;
    }
    int i;
    for (i = 0; i < MIN(NUM_DIRECT, num_blocks); i++) {
        int last_block = curr_dir_inode.direct[i];
        int return_val = updateFreeEntry(curr_dir_inode.direct[i], file_name, file_inode_num, num_entries_left);
        if(return_val != -2) {
            return return_val;
        }
        num_entries_left -= entries_per_block;
    }
    if (num_blocks > NUM_DIRECT) {//then we need to search in the indirect block
        int block_to_read = curr_dir_inode.indirect;
        int indirect_buf[BLOCKSIZE/sizeof(int)];
        if (ReadSector(block_to_read, indirect_buf) == ERROR) {
            return ERROR;
        }
        int j;
        for (j = 0; j < (num_blocks - NUM_DIRECT); j++) {
            int last_block = indirect_buf[j];
            int return_val = updateFreeEntry(indirect_buf[j], file_name, file_inode_num, num_entries_left);
            if(return_val != -2) {
                return return_val;
            }
            num_entries_left -= entries_per_block;
        }
    }

    if (strlen(file_name) > DIRNAMELEN) {
            fprintf(stderr, "Entry name %s is too long.\n", file_name);
            return ERROR;
        }
        char entry_name[DIRNAMELEN];
        memset(entry_name, '\0', DIRNAMELEN);
        memcpy(entry_name, file_name, strlen(file_name));
    }
    //there are no free directories in the directory dir_inode_num
    int max_size = (NUM_DIRECT + BLOCKSIZE/sizeof(int))*BLOCKSIZE;
    if(max_size == curr_dir_inode.size) {
        return ERROR;
    }
    else if(curr_dir_inode.size % BLOCKSIZE != 0){//there is still space in the last block that we can use
        int entries_per_block = BLOCKSIZE/sizeof(struct dir_entry);
        struct dir_entry dir_buf[entries_per_block];
        if(ReadSector(last_block, dir_buf) == ERROR) {
            return ERROR;
        }
        int index = (curr_dir_inode.size % BLOCKSIZE)/(sizeof(struct dir_entry));
        struct dir_entry *last_entry = &dir_buf[index + 1];

        last_entry->inum = file_inode_num;
        last_entry->name = entry_name;
        if(WriteSector(last_block, dir_buf) == ERROR) {
            return ERROR;
        }


    } else {//allocate a new block
        int free_block_num = find_free_block();
        if(free_block_num == ERROR) {
            return ERROR;
        }

        int entries_per_block = BLOCKSIZE/sizeof(struct dir_entry);
        struct dir_entry dir_buf[entries_per_block];

        dir_buf[0].inum = file_inode_num;
        dir_buf[0].name = entry_name;

        if(WriteSector(free_block_num, dir_buf) == ERROR) {
            return ERROR;
        }

        if(num_blocks < NUM_DIRECT) {
            //then, we can put in the last direct block
            curr_dir_inode.direct[num_blocks] = free_block_num; //this is where we put the new block
        } else {//num_blocks >= NUM_DIRECT
            if(num_blocks == NUM_DIRECT) {
                int indirect_block_num = find_free_block();
                curr_dir_inode.indirect = indirect_block_num;
            }
            
            int buf[BLOCKSIZE/sizeof(int)];
            if(ReadSector(curr_dir_inode.indirect, buf) == ERROR) {
                return ERROR;
            }
            buf[num_blocks - NUM_DIRECT] = free_block_num;

            if(WriteSector(curr_dir_inode.indirect, buf) == ERROR) {
                return ERROR;
            }
        }
    }
    curr_dir_inode.size += sizeof(struct dir_entry);//we should have xisted if we reused the old dir_entry
    writeInodeToDisc(dir_inode_num, curr_dir_inode);

}

static int find_free_block() {
    int i;
    for(i = 0; i < NUM_BLOCKS; i++) {
        if(blockmap[i]) {
            blockmap[i] = false;
            return i;
        }
    }
    return ERROR;
}

static int updateFreeEntry(int blockNum, char* file_name, int file_inode_num, int num_entries_left) {
    
    if (strlen(file_name) > DIRNAMELEN) {
        fprintf(stderr, "Entry name %s is too long.\n", file_name);
        return ERROR;
    }
    char entry_name[DIRNAMELEN];
    memset(entry_name, '\0', DIRNAMELEN);
    memcpy(entry_name, file_name, strlen(file_name));

    int entries_per_block = BLOCKSIZE/sizeof(struct dir_entry);
    
    struct dir_entry dir_buf[entries_per_block];
    if (ReadSector(blockNum, dir_buf) == ERROR) {
        return ERROR;
    }
    int i;
    for (i = 0; i < MIN(entries_per_block, num_entries_left); i++) {
        if (dir_buf[i].inum == 0) {
            dir_buf[i].inum = file_inode_num;
            dir_buf[i].name = entry_name;
            WriteSector(blockNum, dir_buf);
            return 0;
        }
    }
    return -2;//returns -2 when no free entries were found.
}


static int eraseFile(int inode_num) {
    int inodes_per_block = BLOCKSIZE / INODESIZE;
    int block = (curr_num / inodes_per_block) + 1;
    struct inode inode_buf[inodes_per_block];
    if(ReadSector(block, inode_buf) == ERROR) {
        return ERROR;
    }
    int index = curr_num % inodes_per_block;
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