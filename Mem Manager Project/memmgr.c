/*
  Cody Bollinger
  Virtual Memory Manager Project
  CPSC 351 - Fall 2020
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#define ARGC_ERROR 1
#define FILE_ERROR 2
#define BUFLEN 256
#define PAGE_TABLE_SIZE 256
#define PAGE_SIZE 256
#define FRAME_SIZE 256
#define FRAMES 256
#define TLB_SIZE 16

// Struct Declarations and Struct Data Structures
struct TLB_ENTRY {
  int pageNumber;
  int frameNumber;
  bool isEmpty;
};
struct pageTableEntry {
  int pageNumber;
  int pageFrame;
};
struct pageTableEntry pageTable[PAGE_TABLE_SIZE];                     // Page Table Array of 'pageTableEntry' objects
struct TLB_ENTRY TLB[TLB_SIZE];                                       // TLB Array of 'TLB_ENTRY' objects
int PHYSICAL_MEMORY[FRAMES][FRAME_SIZE];                              // Physical Memory - for storing reads from Backing store
unsigned freePageFrames = 128;                                        // PHYSICAL_MEMORY free page frames
//-------------------------------------------------------------------
// Function Declarations
unsigned getpage(unsigned x);                                         // Get page from logical address
unsigned getoffset(unsigned x);                                       // Get offset from logical address
void add_TLB_entry(unsigned page, unsigned frame);                    // Add entry to TLB (FIFO replacement algorithm)
//-------------------------------------------------------------------
// Function Defintions
unsigned getpage(unsigned x) { return ((0xFF00 & x) >> 8); }

unsigned getoffset(unsigned x) { return x & 0xFF; }

void add_TLB_entry(page, frame) {
  // Count Current TLB Entries
  int countEntries = 0;
  bool duplicate = false;
  unsigned counter = 0;
  while (counter < TLB_SIZE) {
    if (!TLB[counter].isEmpty) {
      countEntries++;
    }
    if (TLB[counter].pageNumber == page) {
      duplicate = true;
    }
    counter++;
  }
  // IF TLB has room, simply insert new entry at first available index
  struct TLB_ENTRY newEntry;
  if (countEntries < TLB_SIZE && !duplicate) {
    newEntry.pageNumber = page;
    newEntry.frameNumber = frame;
    newEntry.isEmpty = false;
    TLB[countEntries] = newEntry;
  }
  // FIFO Algorithm
  // Else if TLB is full -> shift all elements 1 index to the left and insert new
  // element at the end of the TLB. (index[0] is overwritten)
  else if (countEntries == TLB_SIZE && !duplicate) {
    counter = 0;
    while (counter < TLB_SIZE - 1) {
      TLB[counter] = TLB[counter + 1];
      counter++;
    }
    newEntry.pageNumber = page;
    newEntry.frameNumber = frame;
    newEntry.isEmpty = false;
    TLB[TLB_SIZE - 1] = newEntry;
  }
}
//-------------------------------------------------------------------

int main(int argc, const char* argv[]) {
  /*
    FILE pointers
    - "addresses.txt" -> contains logical addresses
    - "correct.txt" -> contains logical address, translated physical address, signed value
    - "BACKING_STORE.bin" -> In case of page fault, 256 byte page will be read to physical memory
  */
  FILE* fadd = fopen("addresses.txt", "r");
  if (fadd == NULL) { fprintf(stderr, "Could not open file: 'addresses.txt'\n");  exit(FILE_ERROR);  }
  FILE* fcorr = fopen("correct.txt", "r");
  if (fcorr == NULL) { fprintf(stderr, "Could not open file: 'correct.txt'\n");  exit(FILE_ERROR);  }
  FILE* backingStore = fopen ("BACKING_STORE.bin" , "rb");
  if (backingStore == NULL) { fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");  exit(FILE_ERROR);  }
  //---------------------------------------------------------------------------------------------------------------
  // STATISTICS Variables
  // - for calculating TLB hit rate and page fault rate at the end of program execution.
  unsigned countPageFault = 0;
  unsigned countTLBHits = 0;
  float totalAddresses = 1000;
  int lineCount = 0;
  //---------------------------------------------------------------------------------------------------------------
  // ADDRESS TRANSLATION Variables
  unsigned   page, offset, physical_add, frame;       // extracted through address translation.
  unsigned   logic_add;                               // read from file address.txt
  unsigned   virt_add, phys_add;                      // read from file correct.txt
  unsigned   availableFrame = 0, availablePage = 0;   // keep track of next available for page table/memory
  signed     value;                                   // stored value from correct.txt
  signed     getValue;                                // value of byte from physical address to compare to ^ value
  char buf[BUFLEN];                                   // buffer for reading from correct.txt
  //---------------------------------------------------------------------------------------------------------------
  // Begin Address translation - while end of file(address.txt) has not been reached
  while (feof(fadd) == 0) {
    // read line from file correct.txt
    fscanf(fcorr, "%s %s %d %s %s %d %s %d", buf, buf, &virt_add, buf, buf, &phys_add, buf, &value);
    // read line from file address.txt 'logical addresses' and store in var logic_add  
    fscanf(fadd, "%d", &logic_add);
    lineCount++;

    int page   = getpage(logic_add);            // Getting PAGE from logical address
    int offset = getoffset(logic_add);          // Getting offset from logical address
    bool TLBhit = false, pageFault = true;      // bool flags
    unsigned counter;                           // Loop counter
    /*
      1. Using 'page', TLB is consulted first. IF page is in TLB -> TLB hit, extract frame.
        - IF page is not found in TLB, TLB miss and Page Table must be consulted.
      2. Using 'page', Page Table is consulted. IF page is found -> No page fault, extract frame.
        - IF page is not found, Page Fault occurs and Backing store must be read into memory.
      3. In case of Page Fault, 256 byte page will be read into physical memory. FIFO page
         replacement once PHYSICAL_MEMORY fills up.
    */
    counter = 0;
    while (counter < TLB_SIZE) {          // 1. TLB is consulted FIRST
      if (TLB[counter].pageNumber == page) {
        TLBhit = true;
        countTLBHits++;
        frame = TLB[counter].frameNumber;
        break;
      }
      else { counter++; }
    }
    if (!TLBhit) {                       // 2. IF TLB MISS -> Search Page Table
      counter = 0;
      while (counter < availablePage) {
        if (pageTable[counter].pageNumber == page) {
          frame = pageTable[counter].pageFrame;
          pageFault = false;
          break;
        }
        else { counter++; }
      }
      if (pageFault) {                  // 3. IF Page Fault Occurred -> read from Backing Store
        countPageFault++;
        char backingStoreBuf[BUFLEN];
        fseek(backingStore, page * PAGE_SIZE, SEEK_SET);
        fread(backingStoreBuf, sizeof(char), PAGE_SIZE, backingStore);
        counter = 0;

        // Read into Physical Memory
        // IF there is still room in PHYSICAL_MEMORY -> insert into empty space.
        while (counter < PAGE_SIZE) {
          PHYSICAL_MEMORY[availableFrame][counter] = backingStoreBuf[counter];
          counter++;
        }
        //UPDATE Page Table and Increment to next available page/frame
        frame = availableFrame;
        pageTable[availableFrame].pageNumber = page;
        pageTable[availableFrame].pageFrame = availableFrame;
        availablePage++;
        availableFrame++;

        // FIFO ALGORITHM IS STILL WORK IN PROGRESS -----------------------
        // PROGRAM ONLY WORKS ON 256 FRAME SIZE
        // PHASE 2 of the project -> incomplete

        // No more room in PHYSICAL_MEMORY -> FIFO replacement
        // index '0' is overwritten. Write to last index.
        /*
        else {
          // Shift elements
          //availableFrame = 0;
          int columnCounter = 0;
          int rowCounter = 0;
          int lastframe = 127;
          while (rowCounter < lastframe) {
            for (int column = 0; column < PAGE_SIZE; column++) {
              PHYSICAL_MEMORY[rowCounter][column] = PHYSICAL_MEMORY[rowCounter + 1][column];
            }
            pageTable[columnCounter].pageNumber = page;
            pageTable[columnCounter].pageFrame = rowCounter;
            rowCounter++;
          }

          // Elements are shifted -> NOW reading backingstore into last frame.
          counter = 0;
          while (counter < PAGE_SIZE) {
            PHYSICAL_MEMORY[lastframe][counter] = backingStoreBuf[counter];
            counter++;
          }
          frame = lastframe;
          pageTable[lastframe].pageNumber = page;
          pageTable[lastframe].pageFrame = lastframe;
        }
        */
      }
    }
    // Update TLB  
    add_TLB_entry(page, frame);
    // Get stored value from memory and calculate physical address
    getValue = PHYSICAL_MEMORY[frame][offset];
    physical_add = frame++ * FRAME_SIZE + offset;
    // Confirm translated address and retrieved value match values in 'correct.txt'
    assert(phys_add == physical_add);
    assert(getValue == value);
    // Print results and a '\n' every 5 lines
    printf("logical: %5u (page: %3u, offset: %3u) ---> physical: %5u ---> Value: %d -- passed\n", logic_add, page, offset, physical_add, getValue);
    if (lineCount % 5 == 0) { printf("\n"); }
  }
  // CLOSE FILES
  fclose(fcorr);
  fclose(fadd);
  fclose(backingStore);
  
  // Calculate Stats 
  float tlb_hit_rate = countTLBHits / totalAddresses;
  float page_fault_rate = countPageFault / totalAddresses;
  printf("\nNumber of 'TLB hits' = %d, TLB hit rate: %.4f percent\n", countTLBHits, tlb_hit_rate);
  printf("Number of 'Page Faults' = %d, Page-Fault rate: %.4f percent\n", countPageFault, page_fault_rate);
  printf("\nALL logical ---> physical assertions PASSED!\n");
  printf("\n\t\t...done.\n");
  return 0;
}