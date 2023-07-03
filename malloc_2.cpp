//
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

int num_free_blocks=0;
int num_free_bytes=0;
int num_allocated_blocks=0;
int num_allocated_bytes=0;
int num_meta_data_bytes=0;
int size_meta_data=sizeof (MallocMetadata);

MallocMetadata* first = 0;

//Searches for a free block with at least ‘size’ bytes or allocates (sbrk()) one if none are
void* smalloc(size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }

    size_t req_size = size + sizeof(MallocMetadata);
    MallocMetadata* curr=first;
    while(curr!=NULL){
        if(curr->is_free && curr->size >= size){
            curr->is_free=false;
            num_free_blocks--;
            num_free_bytes-=curr->size;
            return (char*)curr + sizeof(MallocMetadata);
        }
        curr=curr->next;
    }

    void* prog_brk = sbrk(req_size);
    if(prog_brk==(void*)-1)
    {
        return NULL;
    }

    MallocMetadata* meta = (MallocMetadata*)prog_brk;
    meta->size=size;
    meta->is_free=false;

    MallocMetadata* ptr=first;
    if(first==NULL){
        first = meta;
    }
    else{
        while(first->next!=NULL){
            first=first->next;
        }
        first->next=meta;
    }
    meta->prev=first;
    meta->next=NULL;
    num_allocated_blocks++;
    num_allocated_bytes += meta->size;
    num_meta_data_bytes += sizeof(MallocMetadata);
    return (char*)prog_brk + sizeof(MallocMetadata);
}

//Allocates a new memory block of size ‘size’ bytes.
void* scalloc(size_t num, size_t size){
    void* new_ptr= smalloc(num*size);
    if (new_ptr == nullptr){
        return nullptr;
    }
    std::memset(new_ptr,0,num*size);
    return new_ptr;
}

//Frees the memory space pointed to by ‘ptr’. If ‘ptr’ is NULL, no operation is performed.
void sfree(void* p){
    if (p== nullptr){
        return;
    }
    MallocMetadata* ptr = (MallocMetadata*)((char*) p - sizeof(MallocMetadata));
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
    if (size==0||size>MAX_SIZE){
        return nullptr;
    }

    if (oldp== nullptr){
        return smalloc(size);
    }

    MallocMetadata* ptr = (MallocMetadata*)((char*) oldp - sizeof(MallocMetadata));
    if (ptr->size>=size){
        return oldp;
    }
    void* new_ptr = smalloc(size);
    if (new_ptr== nullptr){
        return nullptr;
    }

    sfree(oldp);
    std::memmove(new_ptr,oldp,ptr->size);
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
