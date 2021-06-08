/**
 * part2.c 
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
#define FRAMES 256
#define PAGE_MASK 1023

// For the PAGE_MASK we can either take 1047552 (1111 1111 1100 0000 0000) and apply it (and operation) directly to the logical adress, or we can take it as 1023 (0000 0000 0011 1111 1111) and shift bits then apply.

#define PAGE_SIZE 1024
#define OFFSET_BITS 10
#define OFFSET_MASK 1023

#define LOGICAL_MEMORY_SIZE  PAGES * PAGE_SIZE
#define PHYSICAL_MEMORY_SIZE FRAMES * PAGE_SIZE

// Max number of characters per line of input file to read.
#define BUFFER_SIZE 10

int fifo_next_frame = 0;

struct node {
  int data;
  int age;
  struct node *next;
};

typedef struct node node_t;

node_t *list_head;
int list_size = 0;

void list_init(){
  list_head = malloc(sizeof(node_t));
  list_head->age = 0;
  list_head->next = NULL;
}

int list_remove() {
  node_t *current = list_head;
  node_t *min_prev = list_head;

  int min_age = -1;
  while(current->next != NULL){
      if(min_age<current->next->age){
          min_age = current->next->age;
          min_prev = current;
      }
      current = current->next;
  }

  node_t *temp = min_prev->next->next;
  int value = min_prev->next->data;
  free(min_prev->next);
  min_prev->next = temp;
  list_size--;
  return value;
}

void list_increment_age(){
  node_t *current = list_head; 
  while(current->next != NULL){
    current->next->age++;
    current = current->next;
  }
}

int list_add(int i){
  list_increment_age();
  node_t *current = list_head; 
  while(current->next != NULL){
      current = current->next;
      if(current->data == i){
        current->age = 0;
        return 0; 
      }
  }
  
  node_t *new_node = malloc(sizeof(node_t));
  new_node->age = 0;
  new_node->data = i;
  new_node->next = NULL;
  current->next = new_node;
  list_size++;

  if(list_size>FRAMES)
    return list_remove();

  return 0;
}

struct tlbentry {
  int logical;
  int physical;
};

// Creating program mode variable to understand which algorithm is going to used.
int program_mode = 0;

// TLB is kept track of as a circular array, with the oldest element being overwritten once the TLB is full.
struct tlbentry tlb[TLB_SIZE];
// number of inserts into TLB that have been completed. Use as tlbindex % TLB_SIZE for the index of the next TLB line to use.
int tlbindex = 0;

// pagetable[logical_page] is the physical page number for logical page. Value is -1 if that logical page isn't yet in the table.
int pagetable[PAGES];

signed char main_memory[PHYSICAL_MEMORY_SIZE];

// Pointer to memory mapped backing file
signed char *backing;

int max(int a, int b){
  if (a > b)
    return a;
  return b;
}

/* Returns the physical address from TLB or -1 if not present. */
int search_tlb(int logical_page) {
    for(int i = 0; i < TLB_SIZE; i++) {
      if(tlb[i].logical == logical_page) return tlb[i].physical;
    }

    return -1;
}

/*
  Replace with new page if the given frame is used, 
  if frame is never used by a page, add it to tlb.
*/
void add_to_tlb(int page, int frame) {
   for(int i = 0; i < TLB_SIZE; i++) {
      if(tlb[i].physical == frame) {
          tlb[i].logical = page;
          return;
      }
    }
  tlb[tlbindex % TLB_SIZE].logical = page;
  tlb[tlbindex % TLB_SIZE].physical = frame;
  tlbindex++;
}

int fifo_replacement(){
  int return_value = -1;
  for(int i =0;i<PAGES; i++){
    if(pagetable[i] == fifo_next_frame){
        return_value = i;
        break;
    }
  }
  
  return return_value;
}

int lru_replacement(){
  return list_remove();
}


//Function for replacement procedure.
//Works according to the user selection -p (FIFO or LRU)
//Returns a frame to be used by the page
int replacement(int page){
    int old_page = 0;
    
    if(program_mode) old_page = lru_replacement();
    else old_page = fifo_replacement();

    int frame =  pagetable[old_page];
    //Remove old page from page table
    pagetable[old_page] = -1;
   
    return frame;
}

int main(int argc, const char *argv[]){
      if (argc != 5) {
        fprintf(stderr, "Usage ./virtmem backingstore input -p 0/1\n");
        exit(1);
      }

      if (strcmp(argv[3], "-p")) {
        fprintf(stderr, "Please indicate the program flag: ./virtmem backingstore input -p 0/1");
        exit(1);
      }

      if (strcmp(argv[4], "0") && strcmp(argv[4], "1")) {
        fprintf(stderr, "Please select a valid program mode such as 0 or 1: ./virtmem backingstore input -p 0/1");
        exit(1);
      }

      program_mode = atoi(argv[4]);
    
      list_init();

      const char *backing_filename = argv[1]; 
      int backing_fd = open(backing_filename, O_RDONLY);
      backing = mmap(0, LOGICAL_MEMORY_SIZE, PROT_READ, MAP_PRIVATE, backing_fd, 0); 
      
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
      int num_free_frame = FRAMES;
     
      // Declaring pointers for the further memcpy call.
      signed char *transfer_location_in_main_memory = 0;
      signed char *data_location_in_backing_store = 0;
      
      while (fgets(buffer, BUFFER_SIZE, input_fp) != NULL) {
        total_addresses++;
        int logical_address = atoi(buffer);

        // Calculate the page offset and logical page number from logical_address */
        int offset = logical_address & OFFSET_MASK;
        int page = (logical_address >> OFFSET_BITS) & PAGE_MASK;
        ///////
    
        int frame = search_tlb(page);
      
        // TLB hit
        if (frame != -1) {
          tlb_hits++;
         
        // TLB miss
        } else {
          frame = pagetable[page];
          
          // Page fault
          if (frame == -1) {
          
            page_faults++;
            
            if(num_free_frame > 0) {
              frame = FRAMES - num_free_frame;
              num_free_frame--;  
            }
            else{
              frame = replacement(page);
            } 
            fifo_next_frame = (fifo_next_frame+1)%FRAMES;

            transfer_location_in_main_memory = main_memory + (frame * PAGE_SIZE);

            data_location_in_backing_store = backing + (page * PAGE_SIZE);

            memcpy(transfer_location_in_main_memory, data_location_in_backing_store, PAGE_SIZE);

            pagetable[page] = frame;
          }
          add_to_tlb(page, frame);
        }

        //If LRU is selected, reset age of referenced page to 0 
        if(program_mode) {
          list_add(page);
        } 
             
        int physical_address = (frame << OFFSET_BITS) | offset;
        signed char value = main_memory[frame * PAGE_SIZE + offset];
        
        printf("Virtual address: %d Physical address: %d Value: %d\n", logical_address, physical_address, value);
      }
      
      printf("Number of Translated Addresses = %d\n", total_addresses);
      printf("Page Faults = %d\n", page_faults);
      printf("Page Fault Rate = %.3f\n", page_faults / (1. * total_addresses));
      printf("TLB Hits = %d\n", tlb_hits);
      printf("TLB Hit Rate = %.3f\n", tlb_hits / (1. * total_addresses));
      
      return 0;
}