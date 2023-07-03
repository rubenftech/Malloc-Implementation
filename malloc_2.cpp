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
//void* smalloc(size_t size) {
//    if (size == 0 || size > MAX_SIZE) {
//        return nullptr;
//    }
//
//    MallocMetadata *last = nullptr;
//    //search for a free block
//    for (MallocMetadata *ptr = first; ptr != nullptr; ptr = ptr->next) {
//        last = ptr;
//        if (ptr->is_free && ptr->size >= size) {
//            ptr->is_free = false;
//            num_free_blocks --;
//            num_free_bytes -= ptr->size;
//            return (char*) ptr + sizeof(MallocMetadata);
//        }
//    }
//
//    //if we got here, we need to allocate a new block
//    MallocMetadata *new_ptr = (MallocMetadata *) sbrk (size + sizeof(MallocMetadata));
//    if (new_ptr == (void *) -1) {
//        return nullptr;
//    }
//
//    new_ptr->size = size;
//    new_ptr->is_free = false;
//    num_allocated_blocks++;
//    num_allocated_bytes += size;
//
//
//    if (first == nullptr) {
//        first = new_ptr;
//    } else {
//        last->next = new_ptr;
//    }
//    new_ptr->next = nullptr;
//    new_ptr->prev = last;
//
//    return (char*) new_ptr + sizeof (MallocMetadata);
//}

void insert(MallocMetadata* node){
    MallocMetadata* first=list;
    if(first==NULL){
        list = node;
    }
    else{
        while(first->next!=NULL){
            first=first->next;
        }
        first->next=node;
    }
    node->prev=first;
    node->next=NULL;
    num_allocated_blocks++;
    num_allocated_bytes += node->size;
    num_meta_data_bytes += sizeof(MallocMetadata);
}

// MAIN FUNCTIONS
void* smalloc(size_t size){
    if(size <= 0 || size>MAX_SIZE) {
        return NULL;
    }

    size_t req_size = size + sizeof(MallocMetadata);
    MallocMetadata* curr= list;
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
    void* prog_brk = sbrk(req_size);
    if(prog_brk == (void*)-1){
        return NULL;
    }

    MallocMetadata* meta = (MallocMetadata*)prog_brk;
    meta->size=size;
    meta->is_free=false;

    MallocMetadata* first=list;
    if(first==NULL){
        list = meta;
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
