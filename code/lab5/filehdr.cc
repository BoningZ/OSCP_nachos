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
     dataSectors[LastIndex]=-1;
}

int
FileHeader::numSectors(){
    return ceil((double)numBytes/(double)SectorSize);
}

void 
FileHeader::SetModifiedTime(int newTime){
    modifiedTime=newTime;
}

int
FileHeader::GetModifiedTime(){
    return modifiedTime;
}

int
FileHeader::NumBytes(bool includingFrag){
    return includingFrag?numSectors()*SectorSize:numBytes;
}

//lab4基础上加上二级索引的逻辑
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
    BitMap *bitMap=new BitMap(NumSectors);
    bitMap->FetchFrom(openFile);
    //disk is full or file is too big
    if(newNumSectors>=NumDirect+NumDirect2||deltaSectors>bitMap->NumClear()){
        printf("disk is full/ file is too big\n");
        printf("old size:%dB--new size:%dB\n",numBytes,newNumBytes);
        printf("new sectors:%d   delta:%d   direct:%d+%d   clear:%d\n",newNumSectors,deltaSectors,NumDirect,NumDirect2,bitMap->NumClear());
        bitMap->Print();
        return false;
    }
    //allocate
    for(int i=numSectors();i<newNumSectors&&i<LastIndex;i++)dataSectors[i]=bitMap->Find();
    if(newNumSectors>=NumDirect){//修改后的文件大小需要扇区数量多于一级索引表的大小：需要扩展二级索引
        int dataSectors2[NumDirect2],start=0;
        if(dataSectors[LastIndex]!=-1){//已经扩展了二级索引
            //从硬盘将A位置的内容读入B地址（存的是扇区号）
            synchDisk->ReadSector(dataSectors[LastIndex],(char*)dataSectors2);
            start=numSectors()-NumDirect+1;
        }else dataSectors[LastIndex]=bitMap->Find(); //未扩展二级索引  
        //allocate for level 2
        for(int i=start;i<=newNumSectors-NumDirect;i++)dataSectors2[i]=bitMap->Find();
        synchDisk->WriteSector(dataSectors[LastIndex],(char*)dataSectors2);
    }
    bitMap->WriteBack(openFile);
    numBytes=newNumBytes;
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

//分配空间：新增二级索引的逻辑
bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{ 
    numBytes = fileSize;
    if (freeMap->NumClear() < numSectors())return FALSE;//not enough disk space
    if(NumDirect+NumDirect2<=numSectors())return false;//not enough file indices

    for (int i = 0; i < numSectors()&&i<LastIndex; i++)
	    dataSectors[i] = freeMap->Find();
    if(numSectors()<LastIndex)dataSectors[LastIndex]=-1;//no need level 2
    else{//需要二级索引
        dataSectors[LastIndex]=freeMap->Find();
        int dataSectors2[NumDirect2];
        for(int i=0;i<=numSectors()-NumDirect;i++)
            dataSectors2[i]=freeMap->Find();
        synchDisk->WriteSector(dataSectors[LastIndex],(char*)dataSectors2);
    }
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

//释放空间：新增二级索引的逻辑
void 
FileHeader::Deallocate(BitMap *freeMap)
{
    for (int i = 0; i < numSectors()&&i<LastIndex; i++) {
	ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
	freeMap->Clear((int) dataSectors[i]);
    }
    if(dataSectors[LastIndex]!=-1){//需要释放二级索引的空间
        int dataSectors2[NumDirect2];
        synchDisk->ReadSector(dataSectors[LastIndex],(char*)dataSectors2);
        freeMap->Clear((int)dataSectors[LastIndex]);
        for(int i=0;i<=numSectors()-NumDirect;i++)
            freeMap->Clear((int)dataSectors2[i]);
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
//访问时需调用 offset：文件的第几个字节 返回文件对应的块号
//offset：偏移量
int
FileHeader::ByteToSector(int offset)
{
    if(offset/SectorSize<LastIndex)//不在二级索引中
        return(dataSectors[offset / SectorSize]);
    else{//在二级索引中
        int dataSectors2[NumDirect2];
        synchDisk->ReadSector(dataSectors[LastIndex],(char*)dataSectors2);
        return dataSectors2[offset/SectorSize-LastIndex];
    }
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
//打印单个文件信息 新增关于二级索引的逻辑
void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    bool level2=dataSectors[LastIndex]!=-1;
    int dataSectors2[NumDirect2];
    if(level2)
        synchDisk->ReadSector(dataSectors[LastIndex],(char*)dataSectors2);
    

    printf("FileHeader contents.\nFile size: %d.\nFile blocks:", numBytes);
    for (i = 0; i < numSectors()&&i<LastIndex; i++)printf("%d ", dataSectors[i]);
    if(level2){
        printf("  (level2)");
        for(i=0;i<=numSectors()-NumDirect;i++)printf("%d ",dataSectors2[i]);
    }

    if(modifiedTime){//only normal file can have modified time
        char s[100];
        time_t tmpTime=(time_t)modifiedTime;
        strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &*localtime(&tmpTime));  
        printf("\nLast modified time:%s", s);  
    }

    printf("\nFile contents:\n");
    for (i = k = 0; i < numSectors(); i++) {
        if(i<LastIndex)synchDisk->ReadSector(dataSectors[i], data);
        else synchDisk->ReadSector(dataSectors2[i-LastIndex],data);
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
