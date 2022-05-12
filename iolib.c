#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <comp421/filesystem.h>

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "msg_types.h"


struct file_info {
    short inode_num;
    unsigned int pos;
    int reuse;
};

struct file_info open_files[MAX_OPEN_FILES];

int curr_dir = ROOTINODE;

static int getSmallestFD();

int Open(char *pathname) {
    //pick the file descriptor
    if(pathname == NULL || strlen(pathname) == 0 || strlen(pathname) > MAXPATHNAMELEN - 1) {
        return ERROR;
    }
    int curr_fd = getSmallestFD();
    if(curr_fd == ERROR) {
        return ERROR;
    }
    struct my_msg new_msg = {OPEN, curr_dir, 0, "", &pathname};
    assert(sizeof(struct my_msg) == 32);
    if (Send((void*)&new_msg, -FILE_SERVER) == ERROR) {
        return ERROR;
    }
    //now new_msg has numeric changed
    if(new_msg->numeric == ERROR) {
        return ERROR;
    }
    open_files[fd].inode_num = new_msg->numeric1;
    open_files[fd].pos = 0;
    //we need to return reuse count, so we can compare on subseqeuent reads/writes
    open_files[fd].reuse = new_msg->numeric2;
    return 0;
}

int Close(int fd) {
    (void)fd;
    return 0;
}

int Create(char *pathname) {
    if(pathname == NULL || strlen(pathname) == 0 || strlen(pathname) > MAXPATHNAMELEN - 1) {
        return ERROR;
    }
    int curr_fd = getSmallestFD();
    if(curr_fd == ERROR) {
        return ERROR;
    }

    struct my_msg new_msg = {CREATE, curr_dir, 0, "", &pathname};
    assert(sizeof(struct my_msg) == 32);
    if (Send((void*)&new_msg, -FILE_SERVER) == ERROR) {
        return ERROR;
    }
    open_files[fd].inode_num = new_msg->numeric1;
    open_files[fd].pos = 0;
    //we need to return reuse count, so we can compare on subseqeuent reads/writes
    open_files[fd].reuse = new_msg->numeric2;
    return 0;    
}


int Read(int fd, void * buf, int size) {
    (void)fd;
    (void)buf;
    (void)size;
    return 0;    
}

int Write(int fd, void * buf, int size) {
    (void)fd;
    (void)buf;
    (void)size;
    return 0;    
}

int Seek(int fd, int offset, int whence) {
    (void)fd;
    (void)offset;
    (void)whence;
    return 0;    
}

int Link(char * oldname, char * newname) {
    (void)oldname;
    (void)newname;
    return 0;    
}

int Unlink(char * pathname) {
    (void)pathname;
    return 0;    
}

int ReadLink(char * pathname, char * buf, int len) {
    (void)pathname;
    (void)buf;
    (void)len;
    return 0;    
}

int MkDir(char * pathname) {
    (void)pathname;
    return 0;    
}

int RmDir(char * pathname) {
    (void)pathname;
    return 0;    
}

int ChDir(char * pathname) {
    (void)pathname;
    return 0;    
}

int Stat(char * pathname, struct Stat * statbuf) {
    (void)pathname;
    (void)statbuf;
    return 0;    
}

int Sync() {
    return 0;
}

int Shutdown() {
    return 0;   
}

static int getSmallestFD() {
    int i;
    for(int i = 0; i < MAX_OPEN_FILES; i++) {
        if(open_files[i].inode_num == 0) {
            return i;
        }
    }
    return ERROR;//no available file descriptors.
}