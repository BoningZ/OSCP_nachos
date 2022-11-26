// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"
#include "math.h"
#include "time.h"


FileHeader::FileHeader(){
     memset(dataSectors, 0, sizeof(dataSectors));
}

int
FileHeader::numSectors(){
    return ceil((double)numBytes/(double)SectorSize);
}

void 
FileHeader::SetModifiedTime(int newTime){
    modifiedTime=newTime;
}


bool
FileHeader::Extend(int newNumBytes){
    if(newNumBytes<numBytes)return false;//wrong param
    if(newNumBytes==numBytes)return true;//no need to change
    int newNumSectors=ceil((double)newNumBytes/(double)SectorSize);
    if(newNumSectors==numSectors()){//same num of sectors
        numBytes=newNumBytes;
        return true;
    }
    int deltaSectors=newNumSectors-numSectors();
    OpenFile *openFile=new OpenFile(0);
    BitMap *bitMap=new BitMap(NumDirect);
    bitMap->FetchFrom(openFile);
    //disk is full or file is too big
    if(newNumSectors>NumDirect||deltaSectors>bitMap->NumClear()){
        printf("disk is full/ file is too big\n");
        printf("old size:%dB--new size:%dB\n",numBytes,newNumBytes);
        printf("new sectors:%d   delta:%d   direct:%d   clear:%d\n",newNumSectors,deltaSectors,NumDirect,bitMap->NumClear());
        bitMap->Print();
        return false;
    }
    //allocate
    for(int i=numSectors();i<newNumSectors;i++)dataSectors[i]=bitMap->Find();
    bitMap->WriteBack(openFile);
    numBytes=newNumBytes;
    modifiedTime=(int)time(NULL);//current timestamp(sec)
    return true;
}

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{ 
    numBytes = fileSize;
    if (freeMap->NumClear() < numSectors())
	return FALSE;		// not enough space

    for (int i = 0; i < numSectors(); i++)
	dataSectors[i] = freeMap->Find();
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
    for (int i = 0; i < numSectors(); i++) {
	ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
	freeMap->Clear((int) dataSectors[i]);
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
    modifiedTime=(int)time(NULL);
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    return(dataSectors[offset / SectorSize]);
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.\nFile size: %d.\nFile blocks:", numBytes);
    for (i = 0; i < numSectors(); i++)printf("%d ", dataSectors[i]);

    if(modifiedTime){//only normal file can have modified time
        char s[100];
        time_t tmpTime=(time_t)modifiedTime;
        strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &*localtime(&tmpTime));  
        printf("\nLast modified time:%s", s);  
    }

    printf("\nFile contents:\n");
    for (i = k = 0; i < numSectors(); i++) {
	synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n"); 
    }
    delete [] data;
}
