
// Created by Ruben on 27/06/2023.
//
#include <cstdlib>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <ctime>
#include <math.h>
#define MAX_SIZE 100000000

const int MAX_ORDER = 10;
const size_t MIN_BLOCK_SIZE = 128;
const size_t MAX_BLOCK_SIZE = MIN_BLOCK_SIZE << MAX_ORDER;
int gCookie;
bool initFlag= false;

struct MallocMetadata {
    size_t size;
    bool is_free;
    bool is_maped;
    int cookie;
    MallocMetadata* next;
    MallocMetadata* prev;
};


MallocMetadata* list[11] = {nullptr};
MallocMetadata* mmap_blocks = nullptr;



// get the order of a block
int get_order(size_t size) {
    int order = 0;
    while (size > 128) {
        size >>= 1;
        order++;
    }
    return order;
}


void checkOverflow(MallocMetadata* ptr){
    if(ptr && ptr->cookie != gCookie) exit(0xdeadbeef);
}

// Split a block into two blocks of size/2
void* split_block(MallocMetadata* block, size_t size) {
    if (block== nullptr || block->size < 2*size) {
        return nullptr;
    }
    // Split the block in half
    MallocMetadata* buddy = (MallocMetadata*)((char*)block + size);
    buddy->size = block->size - size;
    buddy->is_free = true;
    buddy->prev = block;
    buddy->next = block->next;

    block->size = size;
    block->is_free = false;
    block->next = buddy;

    if (buddy->next) {
        buddy->next->prev = buddy;
    }

    // Update free blocks list
    size_t order = get_order(buddy->size);
    buddy->prev = nullptr;

    MallocMetadata* curr = list[order];
    MallocMetadata* prev = nullptr;
    while (curr && curr < buddy) {
        prev = curr;
        curr = curr->next;
    }

    if (prev) {
        prev->next = buddy;
    } else {
        list[order] = buddy;
    }

    buddy->next = curr;
    if (curr) {
        curr->prev = buddy;
    }
    return block;
}


//take a block and merge it with its prev and next if they are free
void* merge_blocks_right(MallocMetadata* block) {
   MallocMetadata* buddy = block->next;
   int order = get_order(block->size);

   if(buddy->is_free && buddy->size == block->size && order<10){
       //remove buddy from the list
       if(buddy->prev){
           buddy->prev->next = buddy->next;
       }else{
           list[order] = buddy->next;
       }
       if(buddy->next){
           buddy->next->prev = buddy->prev;
       }
       //merge the blocks
       block->size = block->size*2;
       block->next = buddy->next;
       if(buddy->next){
           buddy->next->prev = block;
       }
       //update the order
       order++;
       list[order+1] = block;
       return merge_blocks_right(block);
     }
     else{
         return block;
     }
}


//take a block and merge it with its prev and next if they are free
void* merge_blocks_left(MallocMetadata* block) {
    MallocMetadata* buddy = block->prev;
    int order = get_order(block->size);

    if(buddy->is_free && buddy->size == block->size && order<10){
        //remove buddy from the list
        if(buddy->prev){
            buddy->prev->next = buddy->next;
        }else{
            list[order] = buddy->next;
        }
        if(buddy->next){
            buddy->next->prev = buddy->prev;
        }
        //merge the blocks
        buddy->size = buddy->size*2;
        buddy->next = block->next;
        if(block->next){
            block->next->prev = buddy;
        }
        //update the order
        order++;
        list[order+1] = buddy;
        return merge_blocks_left(buddy);
    }
    else{
        return block;
    }
}


// merge the block with its prev and next if they are free
void merge_blocks(MallocMetadata* block){
    merge_blocks_left(block);
    merge_blocks_right(block);
}


// Allocates a new memory block of size 'size' bytes using mmap().
void* smmap(size_t size) {
    MallocMetadata* block = (MallocMetadata*)mmap(nullptr, size, PROT_READ, MAP_PRIVATE, -1, 0);
    if (block == (void*)-1) {
        return nullptr;
    }
   // checkOverflow(block);
    block->size = size;
    block->is_free = false;
    block->is_maped = true;
    block->next = mmap_blocks;
    block->prev = nullptr;
    if (mmap_blocks != nullptr) {
        mmap_blocks->prev = block;
    }
    mmap_blocks = block;
    return (void*)(block + 1);
}



// Frees the memory space pointed to by 'ptr' that was allocated using mmap().
void smunmap(void* p) {
    if (p == nullptr) {
        return;
    }
    MallocMetadata* ptr = (MallocMetadata*)p;
    ptr--;
    if (ptr->is_free) {
        return;
    }

    // Remove block from mmap blocks list
    if (ptr->prev != nullptr) {
        ptr->prev->next = ptr->next;
    } else {
        mmap_blocks = ptr->next;
    }
    if (ptr->next != nullptr) {
        ptr->next->prev = ptr->prev;
    }

    munmap(ptr, ptr->size);
}


// Initialize the order list
void initOrderList() {
    initFlag = true;
    for (int i = 0; i < 32; ++i) {
        MallocMetadata *newptr = (MallocMetadata *) sbrk(MAX_BLOCK_SIZE);
        if (newptr == (void *) -1) {
            return;
        }
        srand(time(NULL));
        gCookie = rand();
        newptr->size = MAX_BLOCK_SIZE;
        newptr->is_free = true;
        newptr->is_maped = false;
        newptr->next = list[MAX_ORDER];
        newptr->prev = nullptr;
        list[MAX_ORDER]=newptr;
    }
}


// Searches for a free block with at least 'size' bytes, if size> MAX_SIZE then we use mmap
void* smalloc(size_t size) {
    if (size <= 0) {
        return nullptr;
    }
    if (!initFlag) {
        initOrderList();
    }
    if (size + sizeof (MallocMetadata) > MAX_BLOCK_SIZE) {
        return smmap(size);
    }
    int needed_size= size + sizeof (MallocMetadata);
    int order = get_order(needed_size);

    // Search for a free block in the corresponding order
    for (int i = order; i <= MAX_ORDER; i++) {
        MallocMetadata *block = list[i];
        checkOverflow(block);
        if (block != nullptr) {
            // Remove block from free list
            list[i] = block->next;
            if (block->next != nullptr) {
                block->next->prev = nullptr;
            }

            // Check if the block can be split further
            while (i > order) {
                split_block(block, MIN_BLOCK_SIZE * pow(2, i - 1));
                i--;
            }
            block->is_free = false;

            return (char *) block + sizeof(MallocMetadata);
        }
    }
    return nullptr;
}


// Allocates a new memory block of size 'size' bytes.
void* scalloc(size_t num, size_t size) {
    void* new_ptr = smalloc(num * size);
    if (new_ptr == nullptr) {
        return nullptr;
    }
    memset(new_ptr, 0, num * size);
    return new_ptr;
}


// Frees the memory space pointed to by 'ptr'. If 'ptr' is NULL, no operation is performed.
void sfree(void* p) {

    if (p == nullptr) {
        return;
    }

    MallocMetadata* ptr = (MallocMetadata*)((char*)p - sizeof(MallocMetadata));
    if (ptr->is_free) {
        return;
    }

    if (ptr->is_maped) {
        smunmap(ptr);
        return;
    }

    // Merge the block with its buddy if possible
    merge_blocks(ptr);
    ptr->is_free = true;
}


// Changes the size of the memory block pointed to by 'ptr' to 'size' bytes and returns a pointer to the
void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }

    if (oldp == nullptr) {
        return smalloc(size);
    }

    MallocMetadata* ptr = (MallocMetadata*)((char*)oldp - sizeof(MallocMetadata));
    if (ptr->size >= size) {
        return oldp;
    }

    void* new_ptr = smalloc(size);
    if (new_ptr == nullptr) {
        return nullptr;
    }

    memmove(new_ptr, oldp, ptr->size);
    sfree(oldp);
    return new_ptr;
}

size_t _num_free_blocks() {
    size_t num_blocks=0;
    MallocMetadata* curr;
    for (int i = 0; i < 11; ++i) {
        curr=list[i];
        while(curr){
            if(curr->is_free) num_blocks++;
            curr=curr->next;
        }
    }
    return num_blocks;
}

size_t _num_free_bytes() {
    size_t num_bytes=0;
    MallocMetadata* curr=list[0];
    for (int i = 0; i < 11; ++i) {
        curr=list[i];
        while(curr){
            if(curr->is_free) num_bytes += curr->size;
            curr=curr->next;
        }
    }
    return num_bytes;
}

size_t _num_allocated_blocks() {
    size_t to_return=0;
    MallocMetadata* curr;
    for (int i = 0; i < 11; ++i) {
        curr = list[i];
        while(curr){
            to_return++;
            curr=curr->next;
        }
    }
    curr = mmap_blocks;
    while(curr){
        to_return++;
        curr=curr->next;
    }
    return to_return;
}

size_t _num_allocated_bytes() {
    size_t to_return=0;
    MallocMetadata* curr ;
    for (int i = 0; i < 11; ++i) {
        curr = list[i];
        while(curr){
            to_return+=curr->size;
            curr=curr->next;
        }
    }
    curr = mmap_blocks;
    while(curr){
        to_return+=curr->size;
        curr=curr->next;
    }
    return to_return;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks()*sizeof(MallocMetadata);
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}
