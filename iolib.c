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

int curr_dir_inode = ROOTINODE;

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
    struct my_msg new_msg = {OPEN, curr_dir_inode, 0, 0, 0, "", pathname};
    assert(sizeof(struct my_msg) == 32);
    if (Send((void*)&new_msg, -FILE_SERVER) == ERROR) {
        return ERROR;
    }
    //now new_msg has numeric changed
    if(new_msg.numeric1 == ERROR) {
        return ERROR;
    }
    open_files[curr_fd].inode_num = new_msg.numeric1;
    open_files[curr_fd].pos = 0;
    //we need to return reuse count, so we can compare on subseqeuent reads/writes
    open_files[curr_fd].reuse = new_msg.numeric2;
    return curr_fd;
}

int Close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR;
    }
    // Set all fields of file_indo to 0.
    if (open_files[fd].inode_num == 0) {
        return ERROR;
    }
    open_files[fd].inode_num = 0;
    open_files[fd].pos = 0;
    open_files[fd].reuse = 0;
    return 0;
}

int Create(char *pathname) {
    if(pathname == NULL || strlen(pathname) == 0 || strlen(pathname) > MAXPATHNAMELEN - 1) {
        return ERROR;
    }
    int curr_fd = getSmallestFD();
    if(curr_fd == ERROR) {
        TracePrintf(0, "MAX is achieved: %i\n", MAX_OPEN_FILES);
        return ERROR;
    }

    struct my_msg new_msg = {CREATE, curr_dir_inode, 0, 0, 0, "", pathname};
    assert(sizeof(struct my_msg) == 32);
    if (Send((void*)&new_msg, -FILE_SERVER) == ERROR) {
        return ERROR;
    }
    if(new_msg.numeric1 == ERROR) {
        return ERROR;
    }
    open_files[curr_fd].inode_num = new_msg.numeric1;
    open_files[curr_fd].pos = 0;
    //we need to return reuse count, so we can compare on subseqeuent reads/writes
    open_files[curr_fd].reuse = new_msg.numeric2;
    return curr_fd;    
}


int Read(int fd, void * buf, int size) {
    (void)fd;
    (void)buf;
    (void)size;
    if(fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR;
    }
    struct file_info file_to_read = open_files[fd];
    if(file_to_read.inode_num == 0) {//if file is not open
        return ERROR;
    }
    if(buf == NULL) {
        return ERROR;
    }
    if(size < 0) {
        return ERROR;
    }
    if(size == 0) {
        return 0;
    }
    TracePrintf(0, "pos = %i\n", file_to_read.pos);
    struct my_msg new_msg = {READ, file_to_read.inode_num, file_to_read.pos, file_to_read.reuse, size, "", buf};
    assert(sizeof(struct my_msg) == 32);
    if (Send((void*)&new_msg, -FILE_SERVER) == ERROR) {
        return ERROR;
    }
    if(new_msg.numeric1 == ERROR) {
        return ERROR;
    }
    open_files[fd].pos += new_msg.numeric1;
    //numeric1 is going to be size
    //position should be moved by numeric1
    return new_msg.numeric1;    
}

int Write(int fd, void * buf, int size) {
    if(fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR;
    }
    struct file_info file_to_write = open_files[fd];
    if(file_to_write.inode_num == 0) {//if file is not occupied
        return ERROR;
    }
    if(buf == NULL) {
        return ERROR;
    }
    if(size < 0) {
        return ERROR;
    }
    if(size == 0) {
        return 0;
    }

    struct my_msg new_msg = {WRITE, file_to_write.inode_num, file_to_write.pos, file_to_write.reuse, size, "", buf};
    assert(sizeof(struct my_msg) == 32);
    if (Send((void*)&new_msg, -FILE_SERVER) == ERROR) {
        return ERROR;
    }
    if(new_msg.numeric1 == ERROR) {
        return ERROR;
    }
    open_files[fd].pos += new_msg.numeric1;
    //numeric1 is going to be size
    //position should be moved by numeric1
    return new_msg.numeric1;       
}

int Seek(int fd, int offset, int whence) {
    (void)fd;
    (void)offset;
    (void)whence;
    return 0;    
}

int Link(char * oldname, char * newname) {
    if(oldname == NULL || strlen(oldname) == 0 || strlen(oldname) > MAXPATHNAMELEN - 1) {
        return ERROR;
    }
    if(newname == NULL || strlen(newname) == 0 || strlen(newname) > MAXPATHNAMELEN - 1) {
        return ERROR;
    }

    struct link_msg new_msg = {LINK, curr_dir_inode, "", oldname, newname};
    assert(sizeof(struct link_msg) == 32);
    if (Send((void*)&new_msg, -FILE_SERVER) == ERROR) {
        return ERROR;
    }
    // new_msg.numeric1 contains a return status.
    return new_msg.numeric1;    
}

int Unlink(char * pathname) {
    if(pathname == NULL || strlen(pathname) == 0 || strlen(pathname) > MAXPATHNAMELEN - 1) {
        return ERROR;
    }

    struct my_msg new_msg = {UNLINK, curr_dir_inode, 0, 0, 0, "", pathname};
    assert(sizeof(struct my_msg) == 32);
    if (Send((void*)&new_msg, -FILE_SERVER) == ERROR) {
        return ERROR;
    }
    // new_msg.numeric1 contains a return status.
    return new_msg.numeric1;      
}

int ReadLink(char * pathname, char * buf, int len) {
    (void)pathname;
    (void)buf;
    (void)len;
    return 0;    
}

int MkDir(char * pathname) {
    if(pathname == NULL || strlen(pathname) == 0 || strlen(pathname) > MAXPATHNAMELEN - 1) {
        return ERROR;
    }

    struct my_msg new_msg = {MKDIR, curr_dir_inode, 0, 0, 0, "", pathname};
    assert(sizeof(struct my_msg) == 32);
    if (Send((void*)&new_msg, -FILE_SERVER) == ERROR) {
        return ERROR;
    }
    // new_msg.numeric1 contains a return status.
    return new_msg.numeric1;     
}

int RmDir(char * pathname) {
    if(pathname == NULL || strlen(pathname) == 0 || strlen(pathname) > MAXPATHNAMELEN - 1) {
        return ERROR;
    }

    struct my_msg new_msg = {RMDIR, curr_dir_inode, 0, 0, 0, "", pathname};
    assert(sizeof(struct my_msg) == 32);
    if (Send((void*)&new_msg, -FILE_SERVER) == ERROR) {
        return ERROR;
    }
    // new_msg.numeric1 contains a return status.
    return new_msg.numeric1;      
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
    for(i = 0; i < MAX_OPEN_FILES; i++) {
        if(open_files[i].inode_num == 0) {
            return i;
        }
    }
    return ERROR;//no available file descriptors.
}