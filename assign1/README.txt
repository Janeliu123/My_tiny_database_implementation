README
==================================================
GROUP-5 :

A20426786 Jiajia Liu
A20409838 Peng Guo
A20434064 Xuanyi Zhu
==================================================
FILE LIST :

Makefile		dberror.c		storage_mgr.c		test_assign1_1.c
README.txt		dberror.h		storage_mgr.h		test_helper.h
==================================================
STPES :

1. Input the command : cd assign1
2. Input the command : make
3. Waiting for compiling
4. Input the command : ./test1
5. See the printing information of this project
6. If you want to delete the compiled file, please type the command : make clean
==================================================
SOLUTION DESCRIPTION :

1. MANIPULATING PAGE FILES

    1) initStorageManager(void)
    - Initializing the StorageManager.

    2) createPageFile(char *fileName)
    - Create one new empty page file for both of writing and reading.

    3) openPageFile(char *fileName, SM_FileHandle *fHandle)
    - Open the existing pageFile and store relative contents to fHandle.

    4) closePageFile(SM_FileHandle *fHandle)
    - Close the existing PageFile.

    5) destroyPageFile(char *fileName)
    - Delete the existing PageFile.
--------------------------------------------------
2.Reading Blocks from A Page File

    1) readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
    - Validate the page number.
    - Judge if this file avaliable.
    - SEEK_SET, moves the file pointer to begining of the file.
    - Read the block from file on a disk.
    - Change the current page number.

    2) getBlockPos (SM_FileHandle *fHandle)
    - Get the curpagepos from the structure which pointed by fHandle.

    3) readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
    - Read the block from the begining to the position of PAGE_SIZE.

    4) readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
    - Get the current position by using getBlockPos().
    - Judge if the previous block(current-1) exist, if not, return "RC_READ_NON_EXISTING_PAGE".
    - If this block exist, read the block from file on a disk.

    5) readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
    - Get the current position by using getBlockPos().
    - Judge if this block exist, if not, return "RC_READ_NON_EXISTING_PAGE".
    - If this block exist, read the block from file on a disk.

    6) readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
    - Get the current position by using getBlockPos().
    - Judge if the next block(current+1) exist, if not, return "RC_READ_NON_EXISTING_PAGE".
    - If this block exist, read the block from file on a disk.

    7) readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
    - Move pointer to the position which is one page long away from the end of file.
    - Read the block from this position to the end.
--------------------------------------------------
3. Writing Blocks to A Page File

    1) writeBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
    - Validate the page number.
    - Judge if this file avaliable.
    - SEEK_SET, moves the file pointer to begining of the file.
    - Write the file from memory block using the memory pointer to the file on a disk. 
    - Update the current page position.
    - Update the total page number.

    2) writeCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage) 
    - Get the current position.
    - Set current page position as the page where the data needs to be written.

    3) appendEmptyBlock (SM_FileHandle *fHandle)
    - This function creates a memory block equal to page size and pointer addressing this block.
    - Seek the file write pointer to the end of the file and add an empty page.
    - Update the total number of pages in the file with the new number of pages.
    - Update the current page number and the pointer point this new page.

    4) ensureCapacity (int numberOfPages, SM_FileHandle *fHandle)
    - This function checks whether the file has the number of pages as required.
    - If this file contains less pages than the required then it calculates the number of pages required to meet the required limit.
    - Call appendEmptyBlock and append the new pages to the file.
    - Add missing pages, and the adding number is equal to how much it lacks(for loop).
--------------------------------------------------
4. Define new error in "dberror.h"

    #define RC_SM_FOPEN_ERROR 5
