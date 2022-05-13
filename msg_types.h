struct my_msg { 
    int type; 
    int numeric1; 
    int numeric2;
    int numeric3;
    int numeric4;
    char string[4]; 
    void * ptr; 
};

#define MSG_SIZE 32

#define OPEN 0
#define CREATE 1
#define READ 2