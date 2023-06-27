//
// Created by Ruben on 27/06/2023.
//
#include <unistd.h>
#define MAX_SIZE 100000000

void* smalloc(size_t size){
    if (size == 0){
        return nullptr;
    }
    if (size>MAX_SIZE){
        return nullptr;
    }
    else{
        void* ptr = sbrk(size);
        if (ptr == (void*)-1){
            return nullptr;
        }
        return ptr;
    }
}