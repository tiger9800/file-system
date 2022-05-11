struct my_msg { 
    int type; 
    int numeric; 
    char string[16]; 
    void * ptr; 
};

#define MSG_SIZE 32
#define OPEN 0