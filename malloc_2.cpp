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

size_t num_free_blocks=0;
size_t num_free_bytes=0;
size_t num_allocated_blocks=0;
size_t num_allocated_bytes=0;
size_t num_meta_data_bytes=0;

MallocMetadata* list = nullptr;



//Searches for a free block with at least ‘size’ bytes or allocates (sbrk()) one if none are
void* smalloc(size_t size){
    if(size <= 0 || size>MAX_SIZE) {
        return NULL;
    }

    size_t needed_size = size + sizeof(MallocMetadata);
    MallocMetadata* curr= list;
    //search for a free block
    while(curr){
        if(curr->is_free && curr->size >= size){
            curr->is_free=false;
            num_free_blocks--;
            num_free_bytes-=curr->size;
            return (char*)curr + sizeof(MallocMetadata);
        }
        curr=curr->next;
    }

    //if we got here, we need to allocate a new block
    void* newptr = sbrk(needed_size);
    if(newptr == (void*)-1){
        return NULL;
    }

    MallocMetadata* pMetadata = (MallocMetadata*)newptr;
    pMetadata->size=size;
    pMetadata->is_free=false;

    MallocMetadata* first=list;
    if(first==NULL){
        list = pMetadata;
    }
    else{
        while(first->next!=NULL){
            first=first->next;
        }
        first->next=pMetadata;
    }
    pMetadata->prev=first;
    pMetadata->next=NULL;
    num_allocated_blocks++;
    num_allocated_bytes += pMetadata->size;
    num_meta_data_bytes += sizeof(MallocMetadata);
    return (char*)newptr + sizeof(MallocMetadata);
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
    num_free_bytes += ptr->size;
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
    return sizeof (MallocMetadata);
}
