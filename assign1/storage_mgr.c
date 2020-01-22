#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "storage_mgr.h"
#include "dberror.h"


/* manipulating page files */
void initStorageManager(void) {
	printf("Initializing the StorageManager!\n");

}

RC createPageFile(char *fileName) {
	FILE *pageFile = fopen(fileName,"w+");//w+ if file has not exist, create a new file
	if(pageFile == NULL) {
		return RC_SM_FOPEN_ERROR;
	}
	// buffer stream
	setbuf(pageFile, NULL);
	// create one string with block size
	char *pageFile_str = (char *) calloc(PAGE_SIZE,sizeof(char));
	strcpy(pageFile_str,"\0");
	// write data to file
	fwrite(pageFile_str,sizeof(char),PAGE_SIZE,pageFile);
	// free cache
	free(pageFile_str);
	// close file 
	fclose(pageFile);
	return RC_OK;
}

RC openPageFile(char *fileName, SM_FileHandle *fHandle) {
	//If file exists, open it for reading and writing
	FILE *pageFile = fopen(fileName,"r+");//r+ if file has not exist, return NULL
	if(pageFile == NULL) {
		return RC_FILE_NOT_FOUND;
	}
	fseek(pageFile,0,SEEK_END);
	// get file size
	long fileSize = ftell(pageFile);
	// assign SM_FileHandle values
	fHandle->fileName = fileName;
    fHandle->totalNumPages = fileSize/PAGE_SIZE;
    fHandle->curPagePos = 0;
    fHandle->mgmtInfo = pageFile;
    return RC_OK;
} 

RC closePageFile(SM_FileHandle *fHandle) {
	if(fclose(fHandle->mgmtInfo)) {
		return RC_FILE_NOT_FOUND;
	} else {
		return RC_OK;
	}
}

RC destroyPageFile(char *fileName) {
	// if file exists then destroy it
	if(remove(fileName)) {
		return RC_FILE_NOT_FOUND;
	}
	return RC_OK;	
}

/* reading blocks from disc */
RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_HANDLE_NOT_INIT;
	if(fHandle->totalNumPages <= pageNum || pageNum < 0) 
        return RC_READ_NON_EXISTING_PAGE;
	/* read a block */
	//read from the pageNum*size
    fseek(fHandle->mgmtInfo, pageNum*PAGE_SIZE, SEEK_SET);//move pointer
    fread(memPage, 1, PAGE_SIZE, fHandle->mgmtInfo);
    fHandle->curPagePos = pageNum;
    return RC_OK;
    
}

int getBlockPos (SM_FileHandle *fHandle)
{
	return fHandle->curPagePos;
}

RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	return readBlock(0, fHandle, memPage);
}

RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	//If read a block before the first page or after the last page, return error
	int cur = getBlockPos(fHandle);
	if(cur - 1 < 0 || cur - 1 > fHandle->totalNumPages)
		return RC_READ_NON_EXISTING_PAGE;
	return readBlock(cur-1, fHandle, memPage);
}

RC readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	//If read a block before the first page or after the last page, return error
	int cur = getBlockPos(fHandle);
	if(cur < 0 || cur > fHandle->totalNumPages)
		return RC_READ_NON_EXISTING_PAGE;
	return readBlock(cur, fHandle, memPage);
}

RC readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	//If read a block before the first page or after the last page, return error
	int cur = getBlockPos(fHandle);
	if(cur + 1 < 0 || cur + 1 > fHandle->totalNumPages)
		return RC_READ_NON_EXISTING_PAGE;
	return readBlock(cur+1, fHandle, memPage);
}

RC readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	fseek(fHandle->mgmtInfo, PAGE_SIZE, SEEK_END);//move pointer
    fread(memPage, 1, PAGE_SIZE, fHandle->mgmtInfo);
	return RC_OK;
}

RC writeBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_HANDLE_NOT_INIT;
	if(fHandle->totalNumPages <= pageNum || pageNum < 0) 
        return RC_READ_NON_EXISTING_PAGE;
	/* write a block */
	int n = strlen(memPage)/PAGE_SIZE + 1;//caculate how many pages users write
	fseek(fHandle->mgmtInfo, pageNum*PAGE_SIZE, SEEK_SET);//move pointer
    fwrite(memPage, 1, PAGE_SIZE, fHandle->mgmtInfo);
    fHandle->curPagePos = pageNum;
	fHandle->totalNumPages = fHandle->totalNumPages + n;//modify the totalpagenumber
    return RC_OK;
}

RC writeCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	int block_position = getBlockPos(fHandle); // get the block position
    int pageNum = block_position/PAGE_SIZE - 1; // get the number of pages in the file
    writeBlock(pageNum, fHandle, memPage); // write data into the blocks
    return RC_OK;
}
RC appendEmptyBlock (SM_FileHandle *fHandle)
{
	char str[] = "\0"; // create a NUL s
    fseek(fHandle->mgmtInfo, 0, SEEK_END); // set the file position to the end of file
    for(int i = 0; i < PAGE_SIZE; i++) { // create a new page in the file
        fwrite(str, 1, 1, fHandle->mgmtInfo);
    }
    fseek(fHandle->mgmtInfo, 0L, SEEK_END);
    int fileSize = ftell(fHandle->mgmtInfo); // get the total size of file
    fHandle->totalNumPages = fileSize/PAGE_SIZE;
    fHandle->curPagePos = fileSize/PAGE_SIZE;
    return RC_OK;
}
RC ensureCapacity (int numberOfPages, SM_FileHandle *fHandle)
{
	if(fHandle->totalNumPages < numberOfPages) {
        int additionalPages = numberOfPages - fHandle->totalNumPages;
        char str[] = "\0";
        fseek(fHandle->mgmtInfo, 0, SEEK_END); // set the file position to the end of file
        for(int i = 0; i < additionalPages*PAGE_SIZE; i++) { // create a new page in the file
            fwrite(str, 1, 1, fHandle->mgmtInfo);
        }
        fseek(fHandle->mgmtInfo, 0L, SEEK_END);
        int fileSize = ftell(fHandle->mgmtInfo); // get the total size of file
        fHandle->totalNumPages = fileSize/PAGE_SIZE;
        fHandle->curPagePos = fileSize/PAGE_SIZE;
    }
    return RC_OK;
}

