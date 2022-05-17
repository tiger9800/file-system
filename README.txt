## DESCRIPTION

This program implements a simple file system for the Yalnix operating system, YFS (Yalnix File System). This file system supports a tree-structured (hierarchical) directory similar in functionality and disk layout to the “classical” Unix file system.
Client processes using the YFS file system send individual file system requests as interprocess communication (IPC) messages to the YFS file server process. The YFS server handles each request sequentially,
sends a reply message back to the specific client process at completion and then waits for another request to arrive from some process at the server.
Our file system handless the following IPC messages:
1. int Open(char *pathname)
2. int Close(int fd)
3. int Create(char *pathname)
4. int Read(int fd, void *buf, int size)
5. int Write(int fd, void *buf, int size)
6. int Seek(int fd, int offset, int whence)
7. int Link(char *oldname, char *newname)
8. int Unlink(char *pathname)
9. int MkDir(char *pathname)
10. int RmDir(char *pathname)
11. int ChDir(char *pathname)
12. int Stat(char *pathname, struct Stat *statbuf)
13. int Sync(void)
14. int Shutdown(void)

## DESIGN NOTES
- We created special data structures that contain the contents of messages (my_msg, link_msg, stat_msg). Each of these structures must be of size 32 bytes.
- We also created data structures representing caches. Inode and block caches have identical layouts. The cache is represented by an array of dummy headers, which initially point to themselves. To add a block to a cache, we first compute its hashcode. Then we insert this block_cache_entry to the circular doubly linked list, specified by some header. 
Since we also want to keep track of the least recently used blocks (potential victims of eviction), we create an LRU header, which will also form a circular doubly linked list (which must contain all blocks in a cache). To make it work, we must put four pointers in each block_cache_entry. Inode cache works in the same way.


## TESTING

- We did a thorough testing of our file system. In particular, this folder includes many tests that show that our file system has an expected behavior when user processes perform IPC calls.

## TO BUILD 

To build Yalnix

- cd to our folder and type "make"

To clean up

- cd to our folder and type "make clean"

## TO RUN
- /clear/courses/comp421/pub/bin/yalnix -lu 1 -ly 1 yfs <program_name> <params>
- To see traces, you can use "-lk 1 -lu 1 -ly 1" after ./yalnix.