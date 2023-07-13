//
// Created by Ruben on 02/07/2023.
//

#include <math.h>
#include <unistd.h>
#include <cstring>
#include <sys/mman.h>

#define MAX_BLOCK_SIZE 100000000
#define MAP_SIZE 131072
#define MAX_ORDER 10
#define NUM_BLOCKS 32
#define MIN_SIZE 128
#define EXIT_MESSAGE 0xdeadbeef

// Struct representing metadata for allocated memory blocks
struct MallocMetadata {
    bool is_free;
    size_t size;
    void* addr;
    int cookie;
    MallocMetadata* next;
    MallocMetadata* prev;
};

// Global variables
bool flagInit = false;
int gCookie = 0;
MallocMetadata* list[MAX_ORDER+1] = {nullptr};
MallocMetadata* headListBlocksAllocated = nullptr;
MallocMetadata* HeadListMapedBlocks = nullptr;

// Function to get the order of a block based on its size
int getOrder(size_t size) {
    int order = 0;
    while (size > MIN_SIZE) {
        size >>= 1;
        order++;
    }
    return order;
}

// Function to check for overflow by comparing the cookie of the metadata
void checkOverflow(MallocMetadata* ptr) {
    if (ptr && ptr->cookie != gCookie) {
        exit(EXIT_MESSAGE);
    }
}

void createMemoryBlock(MallocMetadata* prev, MallocMetadata* curr, void* addr, size_t size) {
    curr->is_free = true;
    curr->size = size;
    curr->addr = addr;
    curr->cookie = gCookie;
    curr->next = nullptr;
    curr->prev = prev;
    if (prev)
        prev->next = curr;
}


// Function to add a metadata block to the corresponding list based on its order
void listAdd(void* metadata_ptr, int order) {
    auto* curr = reinterpret_cast<MallocMetadata*>(metadata_ptr);
    checkOverflow(curr);

    MallocMetadata* last = list[order];
    while (last && last->next && last->next->addr < curr->addr) {
        checkOverflow(last);
        last = last->next;
    }

    curr->prev = last;
    curr->next = last ? last->next : nullptr;

    if (last) {
        last->next = curr;
    } else {
        list[order] = curr;
    }
    if (curr->next) {
        curr->next->prev = curr;
    }
}

// Function to remove a metadata block from the corresponding list
void listRemove(void* metadata_ptr, int order) {
    auto* curr = reinterpret_cast<MallocMetadata*>(metadata_ptr);
    checkOverflow(curr);
    MallocMetadata* prev = curr->prev;
    MallocMetadata* next = curr->next;

    if (!prev && !next) {
        list[order] = nullptr;
    } else {
        if (!prev) {
            list[order] = next;
        } else {
            prev->next = next;
        }
        if (next) {
            next->prev = prev;
        }
    }
}

// Function to split a memory block into smaller blocks
void split(void* ptrMetadata, size_t actual_size, int order) {
    auto* curr = reinterpret_cast<MallocMetadata*>(ptrMetadata);
    int currOrder = order;
    checkOverflow(curr);

    while (actual_size <= curr->size / 2 && currOrder >= 0) {
        currOrder--;

        auto* new_block = reinterpret_cast<MallocMetadata*>(reinterpret_cast<size_t>(ptrMetadata) + (static_cast<size_t>(pow(2, currOrder)) * MIN_SIZE));
        createMemoryBlock(nullptr, new_block, reinterpret_cast<void*>(reinterpret_cast<size_t>(new_block) + sizeof(MallocMetadata)), curr->size >> 1);
        listAdd(reinterpret_cast<void *>(new_block), currOrder);
        curr->size = static_cast<size_t>(pow(2, currOrder)) * MIN_SIZE;
    }
}

// Function to merge adjacent memory blocks
void merge(void* metadata_ptr, int order) {
    auto* curr = reinterpret_cast<MallocMetadata*>(metadata_ptr);
    checkOverflow(curr);
    int currOrder = order;
    auto* buddy = reinterpret_cast<MallocMetadata*>(reinterpret_cast<size_t>(curr) ^ curr->size);
    checkOverflow(buddy);

    if (currOrder >= MAX_ORDER || !buddy || !buddy->is_free || buddy->size != curr->size) {
        listAdd(reinterpret_cast<void *>(curr), currOrder);
        return;
    }

    listRemove(reinterpret_cast<void *>(buddy), currOrder);

    if (curr->addr >= buddy->addr) {
        buddy->size *= 2;
        curr = buddy;
    } else {
        curr->size *= 2;
    }

    currOrder++;
    merge(reinterpret_cast<void*>(curr), currOrder);
}

// Initialization function to set up the initial memory blocks
void init() {
    flagInit = true;
    gCookie = rand();

    // Align the starting address of the memory blocks
    size_t alignedAddr = (reinterpret_cast<size_t>(sbrk(0)) + (NUM_BLOCKS * MAP_SIZE - 1)) & ~((NUM_BLOCKS * MAP_SIZE) - 1);
    sbrk(alignedAddr - reinterpret_cast<size_t>(sbrk(0)));

    // Create the first memory block
    auto* last = reinterpret_cast<MallocMetadata*>(sbrk(NUM_BLOCKS * MAP_SIZE));
    createMemoryBlock(nullptr, last, reinterpret_cast<void*>(reinterpret_cast<size_t>(last) + sizeof(MallocMetadata)), MAP_SIZE);
    list[MAX_ORDER] = last;

    // Create the rest of the memory blocks
    void* bottom = reinterpret_cast<void*>(reinterpret_cast<size_t>(last) + MAP_SIZE);
    for (int i = 1; i < NUM_BLOCKS; i++) {
        auto* curr = reinterpret_cast<MallocMetadata*>(bottom);
        createMemoryBlock(last, curr, reinterpret_cast<void*>(reinterpret_cast<size_t>(curr) + sizeof(MallocMetadata)), MAP_SIZE);
        last->next = curr;
        last = curr;
        bottom = reinterpret_cast<void*>(reinterpret_cast<size_t>(bottom) + MAP_SIZE);
    }
}

// Function to allocate memory
void* smalloc(size_t size) {
    if (size == 0 || size > MAX_BLOCK_SIZE)
        return nullptr;

    if (!flagInit)
        init();

    int needed_size = size + sizeof(MallocMetadata);

    // mapped
    if (needed_size >= MAP_SIZE) {
        void* ptr = mmap(nullptr, needed_size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        auto* ptrNewBlock = reinterpret_cast<MallocMetadata*>(ptr);
        createMemoryBlock(nullptr, ptrNewBlock, reinterpret_cast<void*>(reinterpret_cast<size_t>(ptr) + sizeof(MallocMetadata)), needed_size);
        ptrNewBlock->is_free = false;


        if (!HeadListMapedBlocks) {
            HeadListMapedBlocks = ptrNewBlock;
        } else {
            MallocMetadata* last = HeadListMapedBlocks;
            while (last->next) {
                checkOverflow(last);
                last = last->next;
            }
            ptrNewBlock->prev = last;
            last->next = ptrNewBlock;
        }
        return ptrNewBlock->addr;
    }

    // not mapped
    int order = getOrder(needed_size);
    for (int i = order; i < MAX_ORDER+1; i++) {
        for (MallocMetadata* curr = list[i]; curr; curr = curr->next) {
            checkOverflow(curr);
            if (curr->is_free) {
                curr->is_free = false;
                if (!curr->prev && !curr->next) {
                    list[i] = nullptr;
                } else {
                    if (curr->prev) {
                        curr->prev->next = curr->next;
                    } else {
                        list[i] = curr->next;
                    }
                    if (curr->next) {
                        curr->next->prev = curr->prev;
                    }
                }
                split(reinterpret_cast<void*>(curr), needed_size, i);
                if (!headListBlocksAllocated) {
                    headListBlocksAllocated = curr;
                    curr->prev = nullptr;
                    curr->next = nullptr;
                } else {
                    MallocMetadata* last = headListBlocksAllocated;
                    while (last->next) {
                        last = last->next;
                    }
                    curr->prev = last;
                    curr->next = nullptr;
                    last->next = curr;
                }
                return curr->addr;
            }
        }
    }
    return nullptr;
}

// Function to allocate and zero-initialize memory
void* scalloc(size_t num, size_t size) {
    if (num * size == 0 || num * size > MAX_BLOCK_SIZE)
        return nullptr;

    auto* ptr = reinterpret_cast<MallocMetadata*>(reinterpret_cast<size_t>(smalloc(num * size)) - sizeof(MallocMetadata));
    if (!ptr) {
        return nullptr;
    }
    checkOverflow(ptr);
    std::memset(reinterpret_cast<void*>(reinterpret_cast<size_t>(ptr) + sizeof(MallocMetadata)), 0, num * size);
    return reinterpret_cast<void*>(reinterpret_cast<size_t>(ptr) + sizeof(MallocMetadata));
}

// Function to free allocated memory
void sfree(void* ptr) {
    if (!ptr)
        return;

    auto* curr = reinterpret_cast<MallocMetadata*>(reinterpret_cast<size_t>(ptr) - sizeof(MallocMetadata));
    checkOverflow(curr);

    if (curr->is_free)
        return;

    // If the block is mapped
    if (curr->size > MAP_SIZE) {
        MallocMetadata* prev = curr->prev;
        checkOverflow(prev);
        MallocMetadata* next = curr->next;
        checkOverflow(next);

        munmap(curr, curr->size + sizeof(MallocMetadata));

        if (!prev && !next) {
            HeadListMapedBlocks = nullptr;
        } else {
            if (!prev) {
                HeadListMapedBlocks = next;
            } else {
                prev->next = next;
            }
            if (next) {
                next->prev = prev;
            }
        }
        return;
    }

    // If the block is not mapped
    if (!curr->is_free) {
        curr->is_free = true;
        MallocMetadata* next = curr->next;
        checkOverflow(next);

        MallocMetadata* prev = curr->prev;
        checkOverflow(prev);

        if (!prev && !next) {
            headListBlocksAllocated = nullptr;
        } else {
            if (prev) {
                prev->next = next;
            } else {
                headListBlocksAllocated = next;
            }
            if (next) {
                next->prev = prev;
            }
        }
        merge(curr, getOrder(curr->size));
    }
}

// Helper function to check if a block can be resized using buddies
int checkRealloc(MallocMetadata* curr, size_t size, size_t currBlockSize, int currOrder, bool* isResizable) {
    while (curr) {
        checkOverflow(curr);
        auto* buddy = reinterpret_cast<MallocMetadata*>(reinterpret_cast<size_t>(curr->addr) - sizeof(MallocMetadata) * currBlockSize);
        if (!buddy || !buddy->is_free || buddy->size != currBlockSize) {
            *isResizable = false;
            return -1;
        }
        checkOverflow(buddy);
        if (currBlockSize + buddy->size - sizeof(MallocMetadata) >= size) {
            *isResizable = true;
            return currOrder;
        }
        if (curr->addr < buddy->addr) {
            currBlockSize *= 2;
            currOrder++;
        } else {
            curr = buddy;
            currBlockSize *= 2;
            currOrder++;
        }
    }
    *isResizable = false;
    return -1;
}

// Helper function to resize a block using buddies
void* resizeRealloc(void* metadata_ptr, int order, int mOrder) {
    int currOrder = order;
    auto* curr = reinterpret_cast<MallocMetadata*>(metadata_ptr);
    checkOverflow(curr);

    while (currOrder != mOrder + 1) {
        auto* buddy = reinterpret_cast<MallocMetadata*>(reinterpret_cast<size_t>(curr) * curr->size);
        checkOverflow(buddy);

        listRemove(reinterpret_cast<void *>(buddy), currOrder);

        if (curr->addr >= buddy->addr) {
            buddy->size *= 2;
            curr = buddy;
        } else {
            curr->size *= 2;
        }
        currOrder++;
    }
    return reinterpret_cast<void*>(curr);
}

// Function to reallocate memory
void* srealloc(void* oldptr, size_t size) {
    if (size == 0 || size > MAX_BLOCK_SIZE)
        return nullptr;

    auto* curr = reinterpret_cast<MallocMetadata*>(reinterpret_cast<size_t>(oldptr) - sizeof(MallocMetadata));
    if (!curr)
        return smalloc(size);

    checkOverflow(curr);

    if (curr->size > MAP_SIZE) {
        void* newPtr = smalloc(size);
        if (!newPtr)
            return nullptr;

        std::memmove(newPtr, oldptr, curr->size - sizeof(MallocMetadata));
        sfree(oldptr);
        return newPtr;
    }

    if (size <= curr->size - sizeof(MallocMetadata))
        return oldptr;

    bool isResizable = false;
    int newOrder = checkRealloc(curr, size, curr->size, getOrder(curr->size), &isResizable);

    if (!isResizable) {
        void* newPtr = smalloc(size);
        if (!newPtr)
            return nullptr;

        std::memmove(newPtr, oldptr, curr->size - sizeof(MallocMetadata));
        sfree(oldptr);
        return newPtr;
    } else {
        listRemove(oldptr, getOrder(curr->size));

        auto* pMetadata = reinterpret_cast<MallocMetadata*>(resizeRealloc(curr, getOrder(curr->size), newOrder));
        checkOverflow(pMetadata);

        MallocMetadata* last = headListBlocksAllocated;
        if (!last) {
            headListBlocksAllocated = pMetadata;
        } else {
            while (last->next) {
                last = last->next;
            }
            last->next = pMetadata;
        }

        pMetadata->is_free = false;
        pMetadata->prev = last;
        pMetadata->next = nullptr;
        std::memmove(reinterpret_cast<void*>(reinterpret_cast<size_t>(pMetadata) + sizeof(MallocMetadata)), oldptr, curr->size - sizeof(MallocMetadata));
        return reinterpret_cast<void*>(reinterpret_cast<size_t>(pMetadata) + sizeof(MallocMetadata));
    }
}


// Function to get the number of free blocks
size_t _num_free_blocks() {
    size_t to_return = 0;
    for (auto curr : list) {
        for (; curr; curr = curr->next) {
            checkOverflow(curr);
            to_return++;
        }
    }
    return to_return;
}

// Function to get the number of free bytes
size_t _num_free_bytes() {
    size_t to_return = 0;
    for (auto curr : list) {
        for (; curr; curr = curr->next) {
            checkOverflow(curr);
            to_return += curr->size - sizeof(MallocMetadata);
        }
    }
    return to_return;
}

// Function to get the number of allocated blocks
size_t _num_allocated_blocks() {
    size_t to_return = 0;

    for (MallocMetadata* allocated = headListBlocksAllocated; allocated; allocated = allocated->next) {
        checkOverflow(allocated);
        to_return++;
    }

    for (auto curr : list) {
        for (; curr; curr = curr->next) {
            checkOverflow(curr);
            to_return++;
        }
    }

    for (MallocMetadata* mmap = HeadListMapedBlocks; mmap; mmap = mmap->next) {
        checkOverflow(mmap);
        to_return++;
    }

    return to_return;
}

// Function to get the number of allocated bytes
size_t _num_allocated_bytes() {
    size_t to_return = 0;

    for (MallocMetadata* allocated = headListBlocksAllocated; allocated; allocated = allocated->next) {
        checkOverflow(allocated);
        to_return += allocated->size - sizeof(MallocMetadata);
    }

    for (auto curr : list) {
        for (; curr; curr = curr->next) {
            checkOverflow(curr);
            to_return += curr->size - sizeof(MallocMetadata);
        }
    }

    for (MallocMetadata* mmap = HeadListMapedBlocks; mmap; mmap = mmap->next) {
        checkOverflow(mmap);
        to_return += mmap->size - sizeof(MallocMetadata);
    }

    return to_return;
}

// Function to get the number of metadata bytes
size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(MallocMetadata);
}

// Function to get the size of metadata
size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}
