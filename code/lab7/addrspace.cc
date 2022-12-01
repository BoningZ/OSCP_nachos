// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

BitMap *AddrSpace::freeMap=new BitMap(NumPhysPages);
BitMap *AddrSpace::spaceIdMap=new BitMap(NumProcess);

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;
    pageQueue=new List();

    //allocate spaceId
    ASSERT(spaceIdMap->NumClear()>0);
    spaceId=spaceIdMap->Find();
    sprintf(swapFileName,"SWAP%d",spaceId);

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size=numPages*PageSize;
    fileSystem->Remove(swapFileName);
    fileSystem->Create(swapFileName,size);


    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
   
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
	pageTable[i].physicalPage = -1;
	pageTable[i].valid = FALSE;
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
	pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    }
    


// then, copy in the code and data segments into memory
    if (noffH.code.size > 0) {
        Segment seg=noffH.code;
        int startPos=seg.virtualAddr;
        int endPos=startPos+seg.size;
        int firstVPage=startPos/PageSize,lastVPage=divRoundUp(endPos,PageSize);
        int curAddr=seg.inFileAddr;//read from this addr of file
        for(int vPage=firstVPage;vPage<=lastVPage;vPage++){
            if(!pageTable[vPage].valid)FIFO(vPage);
            int pagePos=pageTable[vPage].physicalPage*PageSize;//physical pos
            if(vPage==firstVPage)pagePos+=startPos%PageSize;
            int curSize=(vPage==lastVPage?(endPos%PageSize):PageSize)-(vPage==firstVPage?(startPos%PageSize):0);//size
            executable->ReadAt(&machine->mainMemory[pagePos],curSize,curAddr);
            printf("read code(infile addr:%d length:%d) to vpage:%d\n",curAddr,curSize,vPage);
            curAddr+=curSize;
        }
    }
    if (noffH.initData.size > 0) {
        Segment seg=noffH.initData;
        int startPos=seg.virtualAddr;
        int endPos=startPos+seg.size;
        int firstVPage=startPos/PageSize,lastVPage=divRoundUp(endPos,PageSize);
        int curAddr=seg.inFileAddr;//read from this addr of file
        for(int vPage=firstVPage;vPage<=lastVPage;vPage++){
            if(!pageTable[vPage].valid)FIFO(vPage);
            int pagePos=pageTable[vPage].physicalPage*PageSize;//physical pos
            if(vPage==firstVPage)pagePos+=startPos%PageSize;
            int curSize=(vPage==lastVPage?endPos%PageSize:PageSize)-(vPage==firstVPage?startPos%PageSize:0);//size
            executable->ReadAt(&machine->mainMemory[pagePos],curSize,curAddr);
            curAddr+=curSize;
        }
    }

    /*OpenFile *swapFile=fileSystem->Open(swapFileName);
    if(swapFile==NULL){
        printf("Unable to open swap file %s\n",swapFileName);
        return;
    }
    if(noffH.code.size>0){
        Segment seg=noffH.code;
        char tmpBuff[seg.size];
        executable->ReadAt(tmpBuff,seg.size,seg.inFileAddr);
        swapFile->WriteAt(tmpBuff,seg.size,seg.virtualAddr);
        printf("vSpace: code start at:%d length:%d\n",seg.virtualAddr,seg.size);
    }
    if(noffH.initData.size>0){
        Segment seg=noffH.initData;
        char tmpBuff[seg.size];
        executable->ReadAt(tmpBuff,seg.size,seg.inFileAddr);
        swapFile->WriteAt(tmpBuff,seg.size,seg.virtualAddr);
        printf("vSpace: initData start at:%d length:%d\n",seg.virtualAddr,seg.size);
    }
    delete swapFile;*/


    Print();

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    for(int i=0;i<numPages;i++)freeMap->Clear(pageTable[i].physicalPage);
    spaceIdMap->Clear(spaceId);
   delete [] pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}


//----------------------------------------------------------------------
// AddrSpace::Print
// 	Print the state of memory, to see how program using
//----------------------------------------------------------------------
void
AddrSpace::Print(){
    printf("SpaceId:%d\n",spaceId);
    printf("page table dump: %d pages in total\n",numPages);
    printf("============================================\n");
    printf("\tVirtPage, \tPhysPage\n");
    for(int i=0;i<numPages;i++)
        printf("\t%d, \t\t%d\n",pageTable[i].virtualPage,pageTable[i].physicalPage);
    printf("============================================\n");
}

int
AddrSpace::GetSpaceId(){
    return spaceId;
}

TranslationEntry*
AddrSpace::getPageTable(){
    return pageTable;
}

void 
AddrSpace::readIn(int newPage){
    OpenFile *swapFile=fileSystem->Open(swapFileName);
    if(swapFile==NULL){
        printf("Unable to open swap file %s\n",swapFileName);
        return;
    }
    swapFile->ReadAt(&(machine->mainMemory[pageTable[newPage].physicalPage]),PageSize,newPage*PageSize);
    delete swapFile;
    printf("virtual page:%d has been read into mem\n",newPage);
}

void
AddrSpace::writeOut(int oldPage){
    printf("trying to swap virtual page:%d into disk...\t",oldPage);
    if(pageTable[oldPage].dirty){
        printf("Dirty! It will be written into disk\n");
        OpenFile *swapFile=fileSystem->Open(swapFileName);
        if(swapFile==NULL){
            printf("Unable to open swap file %s\n",swapFileName);
            return;
        }
        swapFile->WriteAt(&(machine->mainMemory[pageTable[oldPage].physicalPage]),PageSize,oldPage*PageSize);
        stats->numPageWriteOuts++;
        delete swapFile;
    }else{
        printf("Clean! No need to write into disk\n");
    }
}

void
AddrSpace::FIFO(int newPage){
    printf("start to swap, current pageQueue size:%d\n",pageQueue->GetSize());
    pageQueue->Append((void*)newPage);
    int oldPage=-1;
   
    if(pageQueue->GetSize()>NumUserProcessFrame)
        oldPage=(int)pageQueue->Remove();
    printf("page swapping...\n");
    if(oldPage!=-1)printf("\tout:vNum: %d, physPage:%d\n",oldPage,pageTable[oldPage].physicalPage);
    printf("\tin:vNum: %d\n",newPage);

    if(oldPage!=-1){//need to swap out an old page
        writeOut(oldPage);
        pageTable[oldPage].valid=false;
        pageTable[newPage].physicalPage=pageTable[oldPage].physicalPage;   
    }
    else pageTable[newPage].physicalPage=freeMap->Find();//limit not reached

    pageTable[newPage].valid=true;
    pageTable[newPage].dirty=false;
    pageTable[newPage].readOnly=false;
    
    readIn(newPage);
    Print();
}


