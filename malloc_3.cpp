//
// Created by Ruben on 27/06/2023.
//
#include <unistd.h>
#include <cstring>
#include <iostream>
#define MAX_SIZE 100000000
#define MAX_ORDER 10
#define FREEBLOCKS 32

struct MallocMetadata {
    size_t size;
    bool is_free;

    MallocMetadata* next;
    MallocMetadata* prev;
};
bool flag_init=false;

int num_free_blocks=0;
int num_free_bytes=0;
int num_allocated_blocks=0;
int num_allocated_bytes=0;
int num_meta_data_bytes=0;
int size_meta_data=sizeof (MallocMetadata);

MallocMetadata* first = 0;


void initList() {
    flag_init=true;

    MallocMetadata* prev = nullptr;
    MallocMetadata* curr = first;
    for (int i=0; i<FREEBLOCKS; i++) {
        curr = (MallocMetadata *) sbrk(size_meta_data);
        curr->is_free = true;
        curr->size = 128 * 1024;
        curr->next = nullptr;
        curr->prev = prev;
        prev->next = curr;
        prev = curr;
    }
}






//Searches for a free block with at least ‘size’ bytes or allocates (sbrk()) one if none are
void* smalloc(size_t size) {
    if (size == 0||size>MAX_SIZE) {
        return nullptr;
    }
    if (!flag_init){
        initList();
        }

    //search for a free block
    for (MallocMetadata *ptr = first; ptr != nullptr; ptr = ptr->next) {
        if (ptr->is_free && ptr->size >= size) {
            ptr->is_free = false;
            num_allocated_blocks++;
            num_allocated_bytes += size;
            num_free_blocks--;
            num_free_bytes -= size;
            return (void*) (ptr + sizeof (MallocMetadata));
        }
    }
}

//Allocates a new memory block of size ‘size’ bytes.
void* scalloc(size_t num, size_t size){
    void* new_ptr= smalloc(num*size);
    std::memset(new_ptr,0,num*size);
    return new_ptr;
}

//Frees the memory space pointed to by ‘ptr’. If ‘ptr’ is NULL, no operation is performed.
void sfree(void* p){
    if (p== nullptr){
        return;
    }
    MallocMetadata* ptr = (MallocMetadata*) p;
    ptr -= sizeof (MallocMetadata);
    if (ptr->is_free){
        return;
    }
    ptr->is_free=true;
    num_free_blocks++;
    num_free_bytes+=ptr->size;
    return;
}

//Changes the size of the memory block pointed to by ‘ptr’ to ‘size’ bytes and returns a pointer to the
void* srealloc(void* oldp, size_t size){
    if (oldp== nullptr){
        return smalloc(size);
    }
    if (size==0||size>MAX_SIZE){
        sfree(oldp);
        return nullptr;
    }
    MallocMetadata* ptr = (MallocMetadata*) oldp;
    ptr -= sizeof (MallocMetadata);
    if (ptr->size>=size){
        return oldp;
    }
    void* new_ptr = smalloc(size);
    if (new_ptr== nullptr){
        return nullptr;
    }
    std::memmove(new_ptr,oldp,ptr->size);
    sfree(oldp);
    return new_ptr;
}


size_t _num_free_blocks(){
    return num_free_blocks;
}

size_t _num_free_bytes(){
    return num_free_bytes;
}

size_t _num_allocated_blocks(){
    return num_allocated_blocks;
}

size_t _num_allocated_bytes(){
    return num_allocated_bytes;
}

size_t _num_meta_data_bytes(){
    return num_meta_data_bytes;
}

size_t _size_meta_data(){
    return size_meta_data;
}
