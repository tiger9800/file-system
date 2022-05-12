struct my_msg { 
    int type; 
    int numeric1; 
    int numeric2;
    char string[12]; 
    void * ptr; 
};

#define MSG_SIZE 32
#define OPEN 0
#define CREATE 1