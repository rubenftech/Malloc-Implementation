

#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#define MAX_SIZE 100000000

// size under 64 bytes
struct MallocMetadata {
    size_t size;
    bool is_free;
    bool maped;
    MallocMetadata* next;
    MallocMetadata* prev;
};

int num_free_blocks = 0;
int num_free_bytes = 0;
int num_allocated_blocks = 0;
int num_allocated_bytes = 0;
int num_meta_data_bytes = 0;
int size_meta_data = sizeof(MallocMetadata);

const int MAX_ORDER = 10;
const size_t MIN_BLOCK_SIZE = 128;
const size_t MAX_BLOCK_SIZE = MIN_BLOCK_SIZE << MAX_ORDER; // 128*2^10 = 1024
bool initFlag= false;

MallocMetadata* free_lists[MAX_ORDER + 1] = {nullptr};
MallocMetadata* mmap_blocks = nullptr;


// Helper function to get the order of a block based on its size
int get_order(size_t size) {
    int order = 0;
    while (size > MIN_BLOCK_SIZE) {
        size >>= 1;
        order++;
    }
    return order;
}

//void insertBlockInFreeList(MallocMetadata* ptr) {
//    int order = get_order(ptr->size);
//    if (free_lists[order] == nullptr) {
//        free_lists[order] = ptr;
//        ptr->next = nullptr;
//        ptr->prev = nullptr;
//    } else {
//        free_lists[order]->prev = ptr;
//        ptr->next = free_lists[order];
//        ptr->prev = nullptr;
//        free_lists[order] = ptr;
//    }
//}

// Helper function to split a block into two buddies
void split_block(MallocMetadata* block, int block_order) {
    int buddy_order = block_order - 1;
    size_t buddy_size = MIN_BLOCK_SIZE << buddy_order;

    MallocMetadata* buddy = (MallocMetadata*)((char*)block + buddy_size);
    buddy->size = buddy_size;
    buddy->is_free = true;
    buddy->next = free_lists[buddy_order];
    buddy->prev = nullptr;
    if (free_lists[buddy_order] != nullptr) {
        free_lists[buddy_order]->prev = buddy;
    }
    free_lists[buddy_order] = buddy;
}

// Helper function to merge two buddy blocks
void merge_blocks(MallocMetadata* block, int block_order) {
    int buddy_order = block_order - 1;
    size_t buddy_size = MIN_BLOCK_SIZE << buddy_order;

    MallocMetadata* buddy = (MallocMetadata*)((char*)block - buddy_size);
    if (buddy->is_free && buddy->size == buddy_size) {
        // Remove buddy from free list
        if (buddy->prev != nullptr) {
            buddy->prev->next = buddy->next;
        } else {
            free_lists[buddy_order] = buddy->next;
        }
        if (buddy->next != nullptr) {
            buddy->next->prev = buddy->prev;
        }

        // Merge blocks and update metadata
        block = buddy < block ? buddy : block;
        block->size <<= 1;
        merge_blocks(block, block_order + 1);
    } else {
        // Add the current block to the free list
        block->is_free = true;
        block->next = free_lists[block_order];
        block->prev = nullptr;
        if (free_lists[block_order] != nullptr) {
            free_lists[block_order]->prev = block;
        }
        free_lists[block_order] = block;
    }
}


//create 32 blocks and put them in the list of order
void initOrderList() {
    initFlag = true;
    for (int i = 0; i < 32; ++i) {
        MallocMetadata *newptr = (MallocMetadata *) sbrk(MAX_BLOCK_SIZE);
        if (newptr == (void *) -1) {
            return;
        }
        newptr->size = MAX_BLOCK_SIZE;
        newptr->is_free = true;
        newptr->next = free_lists[MAX_ORDER];
        newptr->prev = nullptr;
        free_lists[MAX_ORDER]=newptr;
    }
}

//// Allocates a new memory block of size 'size' bytes using mmap().
//void* smmap(size_t size) {
//    if (size == 0 || size > MAX_SIZE) {
//        return nullptr;
//    }
//
//    MallocMetadata* block = (MallocMetadata*)mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, -1, 0);
//    if (block == (void*)-1) {
//        return nullptr;
//    }
//    block->size = size + size_meta_data;
//    block->is_free = false;
//    block->next = mmap_blocks;
//    block->prev = nullptr;
//    if (mmap_blocks != nullptr) {
//        mmap_blocks->prev = block;
//    }
//    mmap_blocks = block;
//
//    num_allocated_blocks++;
//    num_allocated_bytes += size;
//    num_meta_data_bytes += size_meta_data;
//    return (void*)(block + 1);
//}
//
//
// Searches for a free block with at least 'size' bytes
void* smalloc(size_t size) {
    if (size <= 0) {
        return nullptr;
    }

    if (!initFlag){
       initOrderList();
    }

    int order = get_order(size + size_meta_data);
    for (int i = order; i <= MAX_ORDER; i++) {
        MallocMetadata* block = free_lists[i];
        if (block != nullptr) {
            // Remove block from free list
            free_lists[i] = block->next;
            if (block->next != nullptr) {
                block->next->prev = nullptr;
            }

            // Check if the block can be split further
            while (i > order) {
                split_block(block, i);
                i--;
            }

            block->is_free = false;
            num_allocated_blocks++;
            num_allocated_bytes += block->size ;
            return (char*)block + sizeof(MallocMetadata);
        }
    }

    // If no suitable block is found, use mmap to allocate a new block
    if (size > MAX_BLOCK_SIZE) {
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    }
    return nullptr;
}

// Allocates a new memory block of size 'size' bytes.
void* scalloc(size_t num, size_t size) {
    void* new_ptr = smalloc(num * size);
    if (new_ptr != nullptr) {
        memset(new_ptr, 0, num * size);
    }
    return new_ptr;
}

// Frees the memory space pointed to by 'ptr'. If 'ptr' is NULL, no operation is performed.
void sfree(void* p) {
    if (p == nullptr) {
        return;
    }
    MallocMetadata* ptr = (MallocMetadata*)((char*) p - sizeof(MallocMetadata));
    if (ptr->is_free) {
        return;
    }
    ptr->is_free = true;
    num_free_blocks++;
    num_free_bytes += ptr->size;
    merge_blocks(ptr, get_order(ptr->size));
}

// Changes the size of the memory block pointed to by 'ptr' to 'size' bytes and returns a pointer to the
void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }

    if (oldp == nullptr) {
        return smalloc(size);
    }

    MallocMetadata* ptr = (MallocMetadata*)((char*) oldp - sizeof(MallocMetadata));
    size_t old_size = ptr->size - size_meta_data;

    if (ptr->size >= size + size_meta_data) {
        // Check if the block can be split further
        int order = get_order(size + size_meta_data);
        while (ptr->size > MIN_BLOCK_SIZE << order) {
            split_block(ptr, order);
            order++;
        }

        return oldp;
    } else {
        void* new_ptr = smalloc(size);
        if (new_ptr != nullptr) {
            memmove(new_ptr, oldp, old_size);
        }
        sfree(oldp);
        return new_ptr;
    }
}



//// Frees the memory space pointed to by 'ptr' that was allocated using mmap().
//void smunmap(void* p) {
//    if (p == nullptr) {
//        return;
//    }
//    MallocMetadata* ptr = (MallocMetadata*)p;
//    ptr--;
//    if (ptr->is_free) {
//        return;
//    }
//
//    // Remove block from mmap blocks list
//    if (ptr->prev != nullptr) {
//        ptr->prev->next = ptr->next;
//    } else {
//        mmap_blocks = ptr->next;
//    }
//    if (ptr->next != nullptr) {
//        ptr->next->prev = ptr->prev;
//    }
//
//    num_allocated_blocks--;
//    num_allocated_bytes -= ptr->size - size_meta_data;
//    num_meta_data_bytes -= size_meta_data;
//
//    munmap(ptr, ptr->size);
//}

size_t _num_free_blocks() {
    return num_free_blocks;
}

size_t _num_free_bytes() {
    return num_free_bytes;
}

size_t _num_allocated_blocks() {
    return num_allocated_blocks;
}

size_t _num_allocated_bytes() {
    return num_allocated_bytes;
}

size_t _num_meta_data_bytes() {
    return num_meta_data_bytes;
}

size_t _size_meta_data() {
    return size_meta_data;
}
