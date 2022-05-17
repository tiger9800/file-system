struct my_msg { 
    int type; 
    int numeric1; 
    int numeric2;
    int numeric3;
    int numeric4;
    int numeric5;
    void *ptr; 
};

struct link_msg { 
    int type; 
    int numeric1; 
    int oldsize; 
    int newsize;
    void *oldname;
    void *newname; 
};

struct stat_msg {
    int type;
    int numeric1;
    int size;
    char padding[4]; 
    void *pathname;
    void *statbuf; 
};

#define MSG_SIZE 32

#define OPEN 0
#define CREATE 1
#define READ 2
#define LINK 3
#define UNLINK 4
#define WRITE 5
#define MKDIR 6
#define RMDIR 7
#define SEEK 8
#define CHDIR 9
#define STAT 10
#define SYNC 11
#define SHUTDOWN 12
