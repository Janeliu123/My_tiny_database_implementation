#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

#include "storage_mgr.h"
#include "dberror.h"
#include "buffer_mgr.h"
#include "dt.h"

typedef unsigned long long BigInt; //ulonglong

//setup global mutex for lock
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*--------------- Buffer Manager Interface Pool Handling --------------*/
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, 
		const int numPages, ReplacementStrategy strategy,
		void *stratData)
{
    
	//initialize our buffer pool
	bm->pageFile = pageFileName;
	bm->numPages = numPages;
	bm->strategy = strategy;

	//initialize Our Pages, with numPages pages. 
	BM_PageHandle* PageFrameArray = calloc(numPages,sizeof(BM_PageHandle));

	//make it as an empty page
	for (int i = 0; i < numPages; ++i)
	{
		PageFrameArray[i].pageNum = NO_PAGE;
		PageFrameArray[i].data = calloc(PAGE_SIZE,sizeof(char));
		PageFrameArray[i].fixCounts = 0;
		PageFrameArray[i].NumReadIO = 0;
		PageFrameArray[i].NumWriteIO = 0;
		PageFrameArray[i].isDirty = false;
		PageFrameArray[i].HitCount = 0;
		PageFrameArray[i].HitTime = -1;
	}

	//store our array in mgmtData for futher uses
	bm->mgmtData = PageFrameArray;

	//clock tick
	bm->stratData = calloc(1,sizeof(BigInt));


	return RC_OK;
}

RC shutdownBufferPool(BM_BufferPool *const bm)
{
    RC ret;

	//for readable code
	BM_PageHandle* PageFrameArray = (BM_PageHandle*)bm->mgmtData;

	//check if there's any pinned page
	for (int i = 0; i < bm->numPages; ++i)
	{
		if (PageFrameArray[i].fixCounts != 0)
		{
			return RC_BM_SHUTDOWN_PINNED_PAGE;
		}
	}

	//lock
	pthread_mutex_lock(&mutex);

	ret = forceFlushPool(bm);

	//unlock 
	pthread_mutex_unlock(&mutex);
	if (ret != RC_OK)
	{
		printf("forceFlushPool failed in shutdownBufferPool\n");
		return ret;
	}

	//free data
	for (int i = 0; i < bm->numPages; ++i)
	{
		free(PageFrameArray[i].data);
	}
	free(PageFrameArray);
	free(bm->stratData);

	return RC_OK;

}

RC forceFlushPool(BM_BufferPool *const bm)
{
   BM_PageHandle* PageFrameArray = (BM_PageHandle*)bm->mgmtData;

	RC ret;

	for (int i = 0; i < bm->numPages; ++i)
	{
		if (PageFrameArray[i].isDirty == true && PageFrameArray[i].fixCounts == 0)
		{
			SM_FileHandle fHandle;

			ret = openPageFile(bm->pageFile, &fHandle);

			if (ret != RC_OK)
				return RC_FILE_SYS_ERROR;

			//WRITE back to file
			ret = writeBlock(PageFrameArray[i].pageNum, &fHandle, PageFrameArray[i].data);

			if (ret != RC_OK)
				return RC_FILE_SYS_ERROR;

			//increase stat
			PageFrameArray[i].NumWriteIO++;

			//since written to disk and fix count = 0, mark it as non dirty
			PageFrameArray[i].isDirty = false;

			//cleap up
			ret = closePageFile(&fHandle);

			if (ret != RC_OK)
				return RC_FILE_SYS_ERROR;

		}
	}

	return RC_OK;
}

/*---------- Buffer Manager Interface Access Pages -----------*/

RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	//Handle Out Our Page Array
	BM_PageHandle* PageArray = bm->mgmtData;

	for (int i = 0; i < bm->numPages; ++i)
	{
		if (PageArray[i].pageNum == page->pageNum)
		{
			PageArray[i].isDirty = true;
			page->isDirty = true;
			return RC_OK;
		}
	}

	return RC_BM_NON_EXISTING_PAGE;
}


RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
    RC ret;

	//Handle Out Our Page Array
	BM_PageHandle* PageArray = bm->mgmtData;

	for (int i = 0; i < bm->numPages; ++i) 
	{
		if (PageArray[i].pageNum == page->pageNum)
		{
			SM_FileHandle fHandle;

			ret = openPageFile(bm->pageFile, &fHandle);
			if (ret != RC_OK)
			{
				printf("forcePage openPageFile failed\n");
				return ret;
			}
				
			ret = writeBlock(PageArray[i].pageNum, &fHandle, page->data);
			if (ret != RC_OK)
			{
				printf("forcePage writeBlock failed\n");
				return ret;
			}
				
			ret = closePageFile(&fHandle);
			if (ret != RC_OK)
			{
				printf("forcePage closePageFile\n");
				return ret;
			}

			return RC_OK;

		}
	}


	return RC_BM_NON_EXISTING_PAGE;
}

RC ReplacePage(BM_BufferPool * const bm, int const replaceIndex, BM_PageHandle * const page, int const pageNum, char const * pageContent) {
 
    RC ret;

    BM_PageHandle* PageArray = (BM_PageHandle*)bm->mgmtData;

    if (PageArray[replaceIndex].isDirty == true)
    {
 
        ret = forcePage(bm, &PageArray[replaceIndex]);
 
        if (ret != RC_OK)
            return ret;
 
        //successfully write to disk, increase io count.
        PageArray[replaceIndex].NumWriteIO++;
 
        PageArray[replaceIndex].isDirty = false;
    }
    memmove(PageArray[replaceIndex].data, pageContent, PAGE_SIZE * sizeof(char));
    //modify our pool first
    PageArray[replaceIndex].NumReadIO += 1;
    PageArray[replaceIndex].fixCounts = 1;
    PageArray[replaceIndex].pageNum = pageNum;
    PageArray[replaceIndex].HitCount++;
    PageArray[replaceIndex].HitTime = (*((BigInt*)bm->stratData))++;//clock_tick++;
    //direct mem address assign to user supply buffer
    page->data = PageArray[replaceIndex].data;
    page->pageNum = pageNum; 
    return RC_OK;
 
}
int FindPage_Clock(BM_PageHandle* PageFrameArray, int maxIndex)
{
    int i = 0;
    //using temporary array so actually array won't get effected
    BM_PageHandle* tmpArray = malloc(sizeof(BM_PageHandle) * (maxIndex + 1));
    memmove(tmpArray, PageFrameArray, sizeof(BM_PageHandle) * (maxIndex + 1));
 
    while (true) {
 
        //skip pinned page.
        if (tmpArray[i].fixCounts != 0)
        {
 
            if (i != maxIndex)
                i++;
            else
                i = 0;
            continue;
        }
        //found correct shit return it and clean temp array
        if (tmpArray[i].HitCount == 0)
        {
            free(tmpArray);
            return i;
        }
        //do core part of second chance algrorithm
        if (tmpArray[i].HitCount != 0)
        {
            tmpArray[i].HitCount--;
            if (i != maxIndex)
                i++;
            else
                i = 0;
            continue;
        }
    }
}
 
int FindPage_LFU(BM_PageHandle* PageFrameArray, int maxIndex)
{
    //LFU algorithm
    int leastHitCount = PageFrameArray[0].HitCount;
 
    int leastIndex = -1;
 
    for (int i = 0; i <= maxIndex; ++i)
    {
        if (PageFrameArray[i].fixCounts != 0)
            continue;
 
        if (PageFrameArray[i].HitCount <= leastHitCount) {
            leastHitCount = PageFrameArray[i].HitCount;
        }
    }
 
    for (int i = 0; i <= maxIndex; ++i)
    {
        if (PageFrameArray[i].fixCounts != 0)
            continue;
 
        if (PageFrameArray[i].HitCount == leastHitCount) {
            leastIndex = i;
            break;
        }
    }
 
    return leastIndex;
}
 
 
int FindPage_LRU(BM_PageHandle* PageFrameArray, int maxIndex)
{
    //LRU algorithm
    int leastHitTime = PageFrameArray[0].HitTime;
 
    int leastIndex = -1;
 
    for (int i = 0; i <= maxIndex; ++i) 
    {
        if (PageFrameArray[i].fixCounts != 0)
            continue;
 
        if (PageFrameArray[i].HitTime <= leastHitTime) {
            leastIndex = i;
            leastHitTime = PageFrameArray[i].HitTime;
        }
    } 
    return leastIndex;
}
 
int FindPage_FIFO(BM_PageHandle* PageFrameArray,int maxIndex,int curIndex, int nextIndex)
{
    //simple fifo algorithm
 
    if (PageFrameArray[curIndex].fixCounts != 0)
    {
        return FindPage_FIFO(PageFrameArray, maxIndex, curIndex+1, nextIndex);
    }
 
    if (PageFrameArray[nextIndex].fixCounts != 0)
    {
        return FindPage_FIFO(PageFrameArray, maxIndex, curIndex, nextIndex + 1);
    }
  
    if (nextIndex > maxIndex)
    {
        return FindPage_FIFO(PageFrameArray, maxIndex, curIndex, 0);
    }
 
    if (PageFrameArray[curIndex].NumReadIO > PageFrameArray[nextIndex].NumReadIO && PageFrameArray[nextIndex].fixCounts == 0)
    {
        return nextIndex;
    }
 
    return curIndex;
}
 
 
RC pinPage(BM_BufferPool * const bm, BM_PageHandle * const page, const PageNumber pageNum)
{
    RC ret;
    //ensure our file is big enough
    SM_FileHandle fHandle;
 
    ret = openPageFile(bm->pageFile, &fHandle);
    ret = ensureCapacity(pageNum+1, &fHandle);
    ret = closePageFile(&fHandle);
    if (ret != RC_OK)
        return ret;
    
    //initialize
    page->pageNum = NO_PAGE;
    page->data = NULL;
    page->fixCounts = 0;
    page->isDirty = false;
    page->HitCount = 0;
    page->HitTime = -1;
 
    //Handle Out Our Page Array
    BM_PageHandle* PageArray = (BM_PageHandle*)bm->mgmtData;
 
    //Check if we already have a cached page
    for (int i = 0; i < bm->numPages; ++i) 
    {
        //if so return it.
        if (PageArray[i].pageNum == pageNum)
        {
            PageArray[i].fixCounts++;
            PageArray[i].HitCount++;
            PageArray[i].HitTime = (*((BigInt*)bm->stratData))++;//clock_tick++;
 
            page->data = PageArray[i].data;
            page->pageNum = pageNum;
            return RC_OK;
        }   
    }
 
    ret = openPageFile(bm->pageFile, &fHandle);
 
    if (ret != RC_OK)
    {
        printf("pinPage openPageFile failed\n");
        return ret;
    }
         
    //alloc a temporary buffer in stack
    char pageContent[PAGE_SIZE] = { 0 };
 
    //read specific block
    ret = readBlock(pageNum, &fHandle, pageContent);
 
    //error handling
    if (ret != RC_OK)
    {
        printf("pinPage readBlock failed.\n");
        return ret;
    }
 
    //close our file, No resource leak 
    ret = closePageFile(&fHandle);
 
    //error handling
    if (ret != RC_OK)
    {
        printf("pinPage closePageFile failed\n");
        return ret;
    }
    //find if there're any unused page
    for (int i = 0; i < bm->numPages; ++i)
	{
		//found page num == -1! 
		if (PageArray[i].pageNum == NO_PAGE)
		{
			memmove(PageArray[i].data, pageContent, PAGE_SIZE * sizeof(char));

			//modify our pool first
			PageArray[i].fixCounts++;
			PageArray[i].NumReadIO++;
			PageArray[i].pageNum = pageNum;
			PageArray[i].HitCount++;
			PageArray[i].HitTime = (*((BigInt*)bm->stratData))++;//clock_tick++;

			//direct mem address assign to user supply buffer
			page->data = PageArray[i].data;
			page->pageNum = pageNum;

			return RC_OK;
		}
	}
    //check if there're no more removable page
    for (int i = 0; i < bm->numPages; ++i)
    {
        //found page then break
        if (PageArray[i].fixCounts == 0)
        {
            break;
        }
        return RC_BM_PAGE_OVERFLOW;
    }
         
    switch (bm->strategy)
    {
        case RS_FIFO:
 
            {
                //get correct replace index using FIFO
                int i = FindPage_FIFO(PageArray, bm->numPages - 1, 0, 1);
                ret = ReplacePage(bm, i, page, pageNum, pageContent);
                return ret;
 
            }   
 
            break;
 
        case RS_LRU:
 
        {
             
            //get correct replace index using LRU
            int i = FindPage_LRU(PageArray, bm->numPages - 1);
            ret = ReplacePage(bm, i, page, pageNum, pageContent);
            return ret;
 
        }
 
 
            break;
        case RS_CLOCK:
 
        {
 
            //get correct replace index using Clock
            int i = FindPage_Clock(PageArray, bm->numPages - 1);
            ret = ReplacePage(bm, i, page, pageNum, pageContent);
            return ret;
 
        }
 
            break;
        case RS_LFU:
        {
 
            //get correct replace index using Clock
            int i = FindPage_LFU(PageArray, bm->numPages - 1);
            ret = ReplacePage(bm, i, page, pageNum, pageContent);
            return ret;
 
        }
 
            break;
        case RS_LRU_K:
            break;
    }
 
    return ret;
}

RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	BM_PageHandle* PageArray = bm->mgmtData;

	for (int i = 0; i < bm->numPages; ++i)
	{
		if (PageArray[i].pageNum == page->pageNum)
		{
			PageArray[i].fixCounts--;

			return RC_OK;
		}
	}

	return RC_BM_NON_EXISTING_PAGE;
}

/*---------------- Statistics Interface --------------------*/
PageNumber *getFrameContents (BM_BufferPool *const bm)
{
    //possible memory leaks here. depends on test
    int* PageNumberArray = malloc(sizeof(int) * bm->numPages);
 
    for (int i = 0; i < bm->numPages; ++i)
    {
        PageNumberArray[i] = ((BM_PageHandle*)bm->mgmtData)[i].pageNum;
    }
 
    return PageNumberArray;
}

bool * getDirtyFlags(BM_BufferPool * const bm) {
    bool* DirtyFlag_Arr = malloc(sizeof(bool) * bm->numPages);
    for (int i = 0; i < bm->numPages; ++i){
        DirtyFlag_Arr[i] = ((BM_PageHandle*)bm->mgmtData)[i].isDirty;
    }
    return DirtyFlag_Arr;
}

int * getFixCounts(BM_BufferPool * const bm) {
    int* FixCountsArr = malloc(sizeof(int) * bm->numPages);
    for(int i = 0; i < bm->numPages;++i) {
        FixCountsArr[i] = ((BM_PageHandle*)bm->mgmtData)[i].fixCounts;
    }
    return FixCountsArr;
}

int getNumReadIO(BM_BufferPool * const bm) {
    int TotalNumReadIO = 0;
    for(int i = 0; i < bm->numPages; ++i) {
        TotalNumReadIO += ((BM_PageHandle*)bm->mgmtData)[i].NumReadIO;
    }
    return TotalNumReadIO;
}

int getNumWriteIO(BM_BufferPool *const bm) {
    int TotalNumWriteIO = 0;
    for (int i = 0; i < bm->numPages; ++i) {
        TotalNumWriteIO += ((BM_PageHandle*)bm->mgmtData)[i].NumWriteIO;
    }
    return TotalNumWriteIO;
}
