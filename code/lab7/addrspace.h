// addrspace.h 
//	Data structures to keep track of executing user programs 
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"
#include "bitmap.h"
#include "list.h"

#define UserStackSize		1024 	// increase this as necessary!
#define NumProcess 256
#define NumUserProcessFrame 5

class AddrSpace {
  public:
    AddrSpace(OpenFile *executable);	// Create an address space,
					// initializing it with the program
					// stored in the file "executable"
    ~AddrSpace();			// De-allocate an address space

    void InitRegisters();		// Initialize user-level CPU registers,
					// before jumping to user code

    void SaveState();			// Save/restore address space-specific
    void RestoreState();		// info on a context switch 
    void Print(); // print state of memory
    int GetSpaceId();
    void FIFO(int newPage);//swap algorithm
    void readIn(int newPage);//read from disk to mem
    void writeOut(int newPage);//write from mem to disk
  

  private:
    TranslationEntry *pageTable;	//virtual page table
    unsigned int numPages;		// Number of pages in the virtual 
    int spaceId;  // address space
    static BitMap *freeMap,*spaceIdMap; //tool map to allocate
    char swapFileName[20];  //format:"SWAP{spaceId}", it won't be too large
    List *pageQueue;  //queue for the FIFO algorithm
};

#endif // ADDRSPACE_H
