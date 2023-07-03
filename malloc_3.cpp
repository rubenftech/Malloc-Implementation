
// Created by Ruben on 27/06/2023.
//
#include <unistd.h>
#include <cstring>
#define MAX_SIZE 100000000

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

size_t num_free_blocks=0;
size_t num_free_bytes=0;
size_t num_allocated_blocks=0;
size_t num_allocated_bytes=0;
size_t num_meta_data_bytes=0;

MallocMetadata* list[11] = {nullptr};



void* split_block(MallocMetadata* block, size_t size) {
    if (block->size < 2*size) {
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
    size_t order = 0;
    size_t block_size = size;
    while (block_size >>= 1) {
        order++;
    }
    buddy->next = list[order];
    if (list[order]) {
        list[order]->prev = buddy;
    }
    list[order] = buddy;

    return block;
}

int get_order(size_t size) {
    int order = 0;
    while (size > 128) {
        size >>= 1;
        order++;
    }
    return order;
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

void merge_blocks(MallocMetadata* block){
    merge_blocks_right(block);
    merge_blocks_left(block);
}
// Searches for a free block with at least 'size' bytes or allocates (sbrk()) one if none are found
void* smalloc(size_t size) {
    if (size <= 0 || size > MAX_SIZE) {
        return nullptr;
    }

    size_t needed_size = size + sizeof(MallocMetadata);
    size_t order = 0;
    size_t block_size = 128;

    // Find the smallest block size that can fit the requested size
    while (block_size < needed_size) {
        order++;
        block_size <<= 1;
    }

    // Search for a free block in the corresponding order
    MallocMetadata* curr = list[order];
    while (curr) {
        if (curr->size >= needed_size) {
            // Remove the block from the free blocks list
            if (curr->prev) {
                curr->prev->next = curr->next;
            } else {
                list[order] = curr->next;
            }

            if (curr->next) {
                curr->next->prev = curr->prev;
            }

            // Split the block if necessary
            void* block = split_block(curr, needed_size);
            if (!block) {
                curr->is_free = false;
                num_free_blocks--;
                num_free_bytes -= curr->size;
                return (char*)curr + sizeof(MallocMetadata);
            }

            return block;
        }
        curr = curr->next;
    }

    // If we reached here, we need to allocate a new block
    void* newptr = sbrk(needed_size);
    if (newptr == (void*)-1) {
        return nullptr;
    }

    MallocMetadata* pMetadata = (MallocMetadata*)newptr;
    pMetadata->size = needed_size;
    pMetadata->is_free = false;
    pMetadata->prev = nullptr;
    pMetadata->next = nullptr;

    num_allocated_blocks++;
    num_allocated_bytes += pMetadata->size;
    num_meta_data_bytes += sizeof(MallocMetadata);
    return (char*)newptr + sizeof(MallocMetadata);
}

// Allocates a new memory block of size 'size' bytes.
void* scalloc(size_t num, size_t size) {
    void* new_ptr = smalloc(num * size);
    if (new_ptr == nullptr) {
        return nullptr;
    }
    std::memset(new_ptr, 0, num * size);
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

    // Merge the block with its buddy if possible
    merge_blocks(ptr);

    ptr->is_free = true;
    num_free_blocks++;
    num_free_bytes += ptr->size;
    return;
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

    std::memmove(new_ptr, oldp, ptr->size);
    sfree(oldp);
    return new_ptr;
}

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
    size_t to_return=0;
    MallocMetadata* curr = list[0];
    for (int i = 0; i < 10; ++i) {
        curr = list[i];
        while(curr){
            to_return+=curr->size;
            curr=curr->next;
        }
    }
    return to_return;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks()*sizeof(MallocMetadata);
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}
