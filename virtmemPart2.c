/**
 * virtmem.c 
 */

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define TLB_SIZE 16
#define PAGES 1024
#define PAGE_MASK 1023

#define PAGE_SIZE 1024
#define OFFSET_BITS 10
#define OFFSET_MASK 1023

#define MEMORY_SIZE PAGES * PAGE_SIZE

// Max number of characters per line of input file to read.
#define BUFFER_SIZE 10

struct tlbentry {
unsigned char logical;
unsigned char physical;
};

// TLB is kept track of as a circular array, with the oldest element being overwritten once the TLB is full.
struct tlbentry tlb[TLB_SIZE];
// number of inserts into TLB that have been completed. Use as tlbindex % TLB_SIZE for the index of the next TLB line to use.
int tlbindex = 0;

// pagetable[logical_page] is the physical page number for logical page. Value is -1 if that logical page isn't yet in the table.
int pagetable[PAGES];

signed char main_memory[MEMORY_SIZE];

// Pointer to memory mapped backing file
signed char *backing;

int max(int a, int b)
{
if (a > b)
return a;
return b;
}

/* Returns the physical address from TLB or -1 if not present. */
int search_tlb(unsigned char logical_page) {
for(int i = 0; i < TLB_SIZE; i++) {
if(tlb[i].logical == logical_page) {
return tlb[i].physical;
}
}
return -1;
}

/* Adds the specified mapping to the TLB, replacing the oldest mapping (FIFO replacement). */
void add_to_tlb(unsigned char logical, unsigned char physical) {
tlb[tlbindex % TLB_SIZE] = (struct tlbentry) {logical, physical};
tlbindex++;
}

/* Finds the least recently used page in the TLB and removes it (LRU replacement). */
void remove_lru_tlb(void) {
int min_use_index = 0;
int min_use_count = tlb[0].use_count;
for(int i = 1; i < TLB_SIZE; i++) {
if(tlb[i].use_count < min_use_count) {
min_use_index = i;
min_use_count = tlb[i].use_count;
}
}
tlb[min_use_index] = (struct tlbentry) {0, 0};
}

int main(int argc, const char *argv[])
{
if (argc != 3) {
fprintf(stderr, "Usage ./virtmem backingstore input\n");
exit(1);
}

const char *backing_filename = argv[1];
int backing_fd = open(backing_filename, O_RDONLY);
backing = mmap(0, MEMORY_SIZE, PROT_READ, MAP_PRIVATE, backing_fd, 0);
const char *input_filename = argv[2];
FILE *input_fp = fopen(input_filename, "r");

// Fill page table entries with -1 for initially empty table.
int i;
for (i = 0; i < PAGES; i++) {
pagetable[i] = -1;
}

// Character buffer for reading lines of input file.
char buffer[BUFFER_SIZE];

// Data we need to keep track of to compute stats at end.
int total_addresses = 0;
int tlb_hits = 0;
int page_faults = 0;

// Number of the next unallocated physical page in main memory
unsigned char free_page = 0;

// Page replacement policy
int policy;
if(argc == 4) {
policy = atoi(argv[3]);
} else {
policy = 0; // default to FIFO replacement
}

while (fgets(buffer, BUFFER_SIZE, input_fp) != NULL) {
total_addresses++;
int logical_address = atoi(buffer);
  /* TODO 
/ Calculate the page offset and logical page number from logical_address */
int offset = logical_address & OFFSET_MASK;
int logical_page = logical_address >> OFFSET_BITS;
///////

int physical_page = search_tlb(logical_page);
// TLB hit
if (physical_page != -1) {
    tlb_hits++;
} else {
    // TLB miss
    physical_page = pagetable[logical_page];
    // Page fault
    if(physical_page == -1) {
        page_faults++;
        // Check if we have free page frames
        if(free_page < PAGES) {
            physical_page = free_page;
            free_page++;
        } else {
            // No free page frames, replace a page according to the specified policy
            if(policy == 0) {
                // FIFO replacement
                physical_page = tlbindex % TLB_SIZE;
            } else if(policy == 1) {
                // LRU replacement
                remove_lru_tlb();
            } else {
                fprintf(stderr, "Error: Invalid page replacement policy specified.\n");
                exit(1);
            }
        }
        // Load page from backing store
        memcpy(main_memory + physical_page * PAGE_SIZE, backing + logical_page * PAGE_SIZE, PAGE_SIZE);
        // Update page table
        pagetable[logical_page] = physical_page;
    }
    // Add mapping to TLB
    add_to_tlb(logical_page, physical_page);
}

int physical_address = (physical_page << OFFSET_BITS) | offset;
signed char value = main_memory[physical_address];
printf("Virtual address: %d Physical address: %d Value: %d\n", logical_address, physical_address, value);
}

fclose(input_fp);
close(backing_fd);

// Print statistics
printf("Number of Translated Addresses = %d\n", total_addresses);
printf("Page Faults = %d\n", page_faults);
printf("Page Fault Rate = %.3f\n", (float)page_faults / (float)total_addresses);
printf("TLB Hits = %d\n", tlb_hits);
printf("TLB Hit Rate = %.3f\n", (float)tlb_hits / (float)total_addresses);

return 0;
}
  
