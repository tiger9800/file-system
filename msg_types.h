struct my_msg { 
    int type; 
    int numeric1; 
    int numeric2;
    int numeric3;
    int numeric4;
    char padding[4]; 
    void *ptr; 
};

struct link_msg { 
    int type; 
    int numeric1; 
    char padding[8]; 
    void *oldname;
    void *newname; 
};

#define MSG_SIZE 32

#define OPEN 0
#define CREATE 1
#define READ 2
#define LINK 3
#define UNLINK 4
#define WRITE 5
