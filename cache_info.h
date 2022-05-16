#include <stdbool.h>
struct inode_cache_entry { 
    int num; 
    struct inode inode_struct; 
    bool dirty; 
    struct inode_cache_entry *hash_next;
    struct inode_cache_entry *hash_prev;
    struct inode_cache_entry *lru_next;
    struct inode_cache_entry *lru_prev;
};

struct block_cache_entry { 
    int num; 
    char data[BLOCKSIZE];  
    bool dirty;
    struct block_cache_entry *hash_next;
    struct block_cache_entry *hash_prev;
    struct block_cache_entry *lru_next;
    struct block_cache_entry *lru_prev;
};

struct inode_cache { 
    struct inode_cache_entry cache[INODE_CACHESIZE];
    int count;
};

struct block_cache { 
    struct block_cache_entry cache[BLOCK_CACHESIZE];
    int count;
};

#define BLOCK 0
#define INODE 1