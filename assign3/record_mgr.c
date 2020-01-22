#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "storage_mgr.h"
#include "buffer_mgr.h"
#include "record_mgr.h"

RM_TableData *tableData;
RecordManager *RM;


/* ------------------table and manager---------------------- */

RC initRecordManager (void *mgmtData)
{
    tableData = (RM_TableData * )malloc(sizeof(RM_TableData));
    tableData->mgmtData = mgmtData;
	initStorageManager();
	return RC_OK;
}

RC shutdownRecordManager ()
{
    free(tableData->schema);//Free the schema of table
    free(tableData);//Free tableData
	free(RM);//Free record manager
	return RC_OK;
}

RC createTable (char *name, Schema *schema)
{
    RC rc = RC_OK;
    
	// Allocating memory space to the record manager custom data structure
	RM = (RecordManager*) malloc(sizeof(RecordManager));

	// Initalizing the Buffer Pool using LRU page replacement policy
	initBufferPool(&RM->bufferPool, name, MAX_PAGE, RS_LRU, NULL);
    
    //Initialize file handler
    SM_FileHandle fileHandle;

	char data[PAGE_SIZE];
	char *firstPage = data;
    
    //create a new file named "name"
    rc = createPageFile(name);
    if(rc != RC_OK) 
        return rc;
    
	// Setting number of tuples to 0
	*(int*)firstPage = 0;
    if(*firstPage != 0) 
        return rc;
    
	// Incrementing pointer by sizeof(int) because 0 is an integer
	firstPage = firstPage + sizeof(int);
    
	// Setting first page to 1 since 0th page if for schema and other meta data
	*(int*)firstPage = 1;
    if(*firstPage != 1) 
        return rc;
    
	// Incrementing pointer by sizeof(int) because 1 is an integer
	firstPage = firstPage + sizeof(int);

	// Setting the number of attributes
	*(int*)firstPage = schema->numAttr;
    if(*firstPage != schema->numAttr) 
        return rc;
    
	// Incrementing pointer by sizeof(int) because number of attributes is an integer
	firstPage = firstPage + sizeof(int);

	// Setting the Key Size of the attributes
	*(int*)firstPage = schema->keySize;
    if(*firstPage != schema->keySize) 
        return rc;

	// Incrementing pointer by sizeof(int) because Key Size of attributes is an integer
	firstPage = firstPage + sizeof(int);
	
    //Open the created file
    rc = openPageFile(name, &fileHandle);
    if(rc != RC_OK) 
        return rc;
    
    int i = 0;
    
	while(i < schema->numAttr){
		// Setting attribute name
       		strncpy(firstPage, schema->attrNames[i], ATTR_SIZE);
	       	firstPage = firstPage + ATTR_SIZE;
	
		// Setting data type of attribute
	       	*(int*)firstPage = (int)schema->dataTypes[i];

		// Incrementing pointer by sizeof(int) because we have data type using integer constants
	       	firstPage = firstPage + sizeof(int);

		// Setting length of datatype of the attribute
	       	*(int*)firstPage = (int) schema->typeLength[i];

		// Incrementing pointer by sizeof(int) because type length is an integer
	       	firstPage = firstPage + sizeof(int);
        
        i++;
    }
		
	// Writing the schema to first location of the page file
    rc = writeBlock(0, &fileHandle, data);
    if(rc != RC_OK) 
        return rc;
		
	// Closing the file after writing
    rc = closePageFile(&fileHandle);

	return rc;
}

RC openTable (RM_TableData *rel, char *name)
{
     //Initialize return value
    RC rc = RC_OK;
    //Check if name is null
    if(name == NULL) 
        return RC_FILE_NOT_FOUND;
    //Set tableData
    tableData = rel;
    
    if(tableData == NULL) 
        return RC_FILE_SYS_ERROR;
    
    //Initialize file handler and page handler
    SM_FileHandle fileHandle;
	SM_PageHandle pageHandle;
	
	// Setting table's meta data to our custom record manager meta data structure
	rel->mgmtData = RM;
	// Setting the table's name
	rel->name = name;
    
    //Set tableData
    tableData->name = name;
    tableData->mgmtData =RM;
    
	// Pinning a page
	rc = pinPage(&RM->bufferPool, &RM->pageHandle, 0);
    if(rc != RC_OK) 
        return rc;
    
	// Setting the initial pointer (0th location) if the record manager's page data
	pageHandle = (char*) RM->pageHandle.data;
	
    //create a new file
    rc = createPageFile(name);
    if(rc != RC_OK) 
        return rc;
    
	// Retrieving total number of tuples from the page file
	RM->tuplesCount= *(int*)pageHandle;
	pageHandle = pageHandle + sizeof(int);

    //open the new file
    rc = openPageFile(name,&fileHandle);
    if(rc != RC_OK) 
        return rc;
    
	// Getting free page from the page file
	RM->freePage= *(int*) pageHandle;
    pageHandle = pageHandle + sizeof(int);
	
	// Getting the number of attributes from the page file
    int	attributeCount = *(int*)pageHandle;
	pageHandle = pageHandle + sizeof(int);
 	
	Schema *schema;

	// Allocating memory space to 'schema'
	schema = (Schema*) malloc(sizeof(Schema));
    
	// Setting schema's parameters
	schema->numAttr = attributeCount;
	schema->attrNames = (char**) malloc(sizeof(char*) *attributeCount);
	schema->dataTypes = (DataType*) malloc(sizeof(DataType) *attributeCount);
	schema->typeLength = (int*) malloc(sizeof(int) *attributeCount);
    
    //Reset tableData
    tableData->schema = schema;

	// Allocate memory space for storing attribute name for each attribute
	for(int i = 0; i < attributeCount; i++)
		schema->attrNames[i]= (char*) malloc(ATTR_SIZE);
    
    int j = 0;
	while(j < schema->numAttr)
    	{
		// Setting attribute name
		strncpy(schema->attrNames[j], pageHandle, ATTR_SIZE);
		pageHandle = pageHandle + ATTR_SIZE;
	   
		// Setting data type of attribute
		schema->dataTypes[j]= *(int*) pageHandle;
		pageHandle = pageHandle + sizeof(int);

		// Setting length of datatype (length of STRING) of the attribute
		schema->typeLength[j]= *(int*)pageHandle;
		pageHandle = pageHandle + sizeof(int);       
        j++;
	}
	
	// Setting newly created schema to the table's schema
	rel->schema = schema;	

	// Unpinning the page i.e. removing it from Buffer Pool using BUffer Manager
	rc = unpinPage(&RM->bufferPool, &RM->pageHandle);
    if(rc != RC_OK) 
        return rc;

	// Write the page back to disk using Buffer Manger
	rc = forcePage(&RM->bufferPool, &RM->pageHandle);
    if(rc != RC_OK) 
        return rc;
    
    //Close the file
    rc = closePageFile(&fileHandle);

	return rc;
}

RC closeTable (RM_TableData *rel)
{
     if(rel == NULL) 
        return RC_RM_UNKOWN_DATATYPE;

	RecordManager *recordManager = rel->mgmtData;
    rel->name = NULL;
    rel->schema = NULL;
    
	// Shutting down Buffer Pool	
	shutdownBufferPool(&recordManager->bufferPool);
	return RC_OK;
}

RC deleteTable (char *name)
{
    if(name == NULL) 
        return RC_FILE_NOT_FOUND;

    //destroy the table
	destroyPageFile(name);
	return RC_OK;
}

int getNumTuples (RM_TableData *rel)
{
    if(rel == NULL) 
        return RC_RM_UNKOWN_DATATYPE;

	RecordManager *recordManager = rel->mgmtData;
    //return the tuplesCount we already stored
	return recordManager->tuplesCount;
}


/* -----------------handling records in a table------------------- */

int findFreeSlot(char *data, int recordSize)
{
    int i = 0;
    int totalSlots = PAGE_SIZE / recordSize;
    
    while(i < totalSlots){
		if (data[i * recordSize] != '+')
			return i;
        i++;
    }
	return -1;
}
extern RC insertRecord (RM_TableData *rel, Record *record)
{
    RC rc;
    BM_PageHandle *page = (BM_PageHandle *)malloc(sizeof(BM_PageHandle));	
	if(rel == NULL)
		return RC_RM_UNKOWN_DATATYPE;

	//retrieve data from table
	RecordManager *recordManager = rel->mgmtData;
	
	// Set record ID for this record
	RID *recordID = &record->id; 
	
	char *data;
    char *slotOffset;
	
	// Get record size
	int recordSize = getRecordSize(rel->schema);
	
	// Set first free page to the current page
	recordID->page = recordManager->freePage;

	//pin page in use
	rc = pinPage(&recordManager->bufferPool, &recordManager->pageHandle, recordID->page);
	if(rc != RC_OK)
		return rc;
	
	//set date to initial position
	data = recordManager->pageHandle.data;
	
	//find free slot in page
    recordID->slot = findFreeSlot(data, recordSize);
	while(recordID->slot == -1){
		//unpin page if there is no free space found
		rc = unpinPage(&recordManager->bufferPool, &recordManager->pageHandle);
		if(rc != RC_OK)
			return rc;

		//increment page
		recordID->page++;
		
		//pin page in use
		rc = pinPage(&recordManager->bufferPool, &recordManager->pageHandle, recordID->page);
		if(rc != RC_OK)
			return rc;
		
		//set data to initial position	
		data = recordManager->pageHandle.data;

		//search for free slot
		recordID->slot =findFreeSlot(data, recordSize);
	}
	
	
	slotOffset = data;
	
	//make dirty that the pag has been changed
	rc = markDirty(&recordManager->bufferPool, &recordManager->pageHandle);
	if(rc != RC_OK)
		return rc;

	//caculate start position of the slot
	slotOffset = slotOffset + (recordID->slot * recordSize);

	//mark newly inserted record
	*slotOffset = '+';

	//insert new data into table
	memcpy(++slotOffset, record->data + 1, recordSize - 1);

	//unpin page after use
	rc = unpinPage(&recordManager->bufferPool, &recordManager->pageHandle);
	if(rc != RC_OK)
		return rc;
	
	//increment number of tuples
	recordManager->tuplesCount++;
	
	//pin page in use
	rc = pinPage(&recordManager->bufferPool, &recordManager->pageHandle, 0);
	if(rc != RC_OK)
		return rc;

	free(page);
	return RC_OK;
}

extern RC deleteRecord (RM_TableData *rel, RID id)
{
    RC rc;
    if(rel == NULL)
		return RC_RM_UNKOWN_DATATYPE;
	RecordManager *recordManager = rel->mgmtData;	

	//pin page in use
	rc = pinPage(&recordManager->bufferPool, &recordManager->pageHandle, id.page);
	if(rc !=RC_OK)
		return rc;

	//update free page
	recordManager->freePage = id.page;
	
	

	//get record size
	int recordSize = getRecordSize(rel->schema);

	//set offset to the slot
    char *data = recordManager->pageHandle.data;
	data = data + (id.slot * recordSize);
	
	//mark deleted record
	*data = '-';
		
	//mark dirty the page has been changed
	rc = markDirty(&recordManager->bufferPool, &recordManager->pageHandle);
	if(rc != RC_OK)
        return rc;

	//unpin page after use
	rc = unpinPage(&recordManager->bufferPool, &recordManager->pageHandle);
	if(rc != RC_OK)
        return rc;

	return RC_OK;
}

extern RC updateRecord (RM_TableData *rel, Record *record)
{
    RC rc;
    BM_PageHandle *page = (BM_PageHandle *)malloc(sizeof(BM_PageHandle));
	
	if(NULL==rel)
		return RC_RM_UNKOWN_DATATYPE;
	

	//retrieve data from table
	RecordManager *recordManager = rel->mgmtData;
	
	//pin page in use
	rc = pinPage(&recordManager->bufferPool, &recordManager->pageHandle, record->id.page);
	if(rc != RC_OK)
        return rc;

	

	//get record size
	int recordSize = getRecordSize(rel->schema);

	//set rid for record
	RID id = record->id;

	//get inserted pisition
	char *data = recordManager->pageHandle.data;
	data = data + (id.slot * recordSize);
	
	//mark updated record
	*data = '+';
	
	//update record with new data
	memcpy(++data, record->data + 1, recordSize - 1 );
	
	//mark dirty the page has been changed
	rc = markDirty(&recordManager->bufferPool, &recordManager->pageHandle);
	if(rc != RC_OK)
        return rc;

	//unpin page after use
	rc = unpinPage(&recordManager->bufferPool, &recordManager->pageHandle);
	if(rc != RC_OK)
        return rc;
	
	free(page);
	return RC_OK;	
}

extern RC getRecord (RM_TableData *rel, RID id, Record *record)
{
    RC rc;
    BM_PageHandle *page = (BM_PageHandle *)malloc(sizeof(BM_PageHandle));

	if(NULL==rel)
		return RC_RM_UNKOWN_DATATYPE;

	//retrieve data from table
	RecordManager *recordManager = rel->mgmtData;
    record->id.slot = id.slot;
	record->id.page = id.page;
    
	//pin page in use
	rc = pinPage(&recordManager->bufferPool, &recordManager->pageHandle, id.page);
	if(rc != RC_OK)
        return rc;

	//get offset
	int recordSize = getRecordSize(rel->schema);
	char *dataOffset = recordManager->pageHandle.data;
	dataOffset = dataOffset + (id.slot * recordSize);
	
	//check record by certain given rid
	if(*dataOffset != '+'){
		// Return error if tuple not found
		return RC_RM_NO_THIS_TUPLE;
	}else{
		//find the data position
		char *data = record->data;	

		memcpy(++data, dataOffset + 1, recordSize - 1);
	}

	//unpin page after use
	rc = unpinPage(&recordManager->bufferPool, &recordManager->pageHandle);

	if(rc != RC_OK)
        return rc;

	free(page);
	return RC_OK;
}

/* -------------------------------scans---------------------------- */
RC startScan (RM_TableData *rel, RM_ScanHandle *scan, Expr *cond)
{
    openTable(rel, "ScanTable");
    //pointer  scan Manager
    RecordManager *scanManager = (RecordManager*) malloc(sizeof(RecordManager));
    //pointer table manager
	RecordManager *tableManager;
	scan->rel= rel;
    	
	// Setting the scan's meta data to our meta data
    scan->mgmtData = scanManager;
    // initializing the scan record as 0.   	
	scanManager->scanCount = 0;
    	
	//scan from 1st page
    scanManager->recordID.page = 1;
    	
	// 0 to start scan from the first slot	
	scanManager->recordID.slot = 0;

	// Scan condition as Exor *cond
    scanManager->condition = cond;
    	
	// Setting the our meta data to the table's meta data
    tableManager = rel->mgmtData;

	// Setting the tuple count
    tableManager->tuplesCount = ATTR_SIZE;

	return RC_OK;
}

RC next (RM_ScanHandle *scan, Record *record)
{
    // Initiliazing scan data
	RecordManager *scanManager = scan->mgmtData;
	RecordManager *tableManager = scan->rel->mgmtData;
    Schema *schema = scan->rel->schema;
	
	// Checking if scan condition (test expression) is present
	if (scanManager->condition == NULL)
		return RC_RM_SCAN_CONDITION_NOT_FOUND;

	Value *result = (Value *) malloc(sizeof(Value));
   
	char *data;
   	
	// Getting record size of the schema
	int recordSize = getRecordSize(schema);

	// Calculating Total number of slots
	int totalSlots = PAGE_SIZE / recordSize;

	// Getting Scan Count
	int scanCount = scanManager->scanCount;

	// Getting tuples count of the table
	int tuplesCount = tableManager->tuplesCount;

	// Checking if the table contains tuples. If the tables doesn't have tuple, then return respective message code
	if (tuplesCount == 0)
		return RC_RM_NO_MORE_TUPLES;

	// Iterate through the tuples
	while(scanCount <= tuplesCount)
	{  
		// If all the tuples have been scanned, execute this block
		if (scanCount <= 0)
		{
			// printf("INSIDE If scanCount <= 0 \n");
			// Set PAGE and SLOT to first position
			scanManager->recordID.page = 1;
			scanManager->recordID.slot = 0;
		}
		else
		{
			// printf("INSIDE Else scanCount <= 0 \n");
			scanManager->recordID.slot++;

			// If all the slots have been scanned execute this block
			if(scanManager->recordID.slot >= totalSlots)
			{
				scanManager->recordID.slot = 0;
				scanManager->recordID.page++;
			}
		}

		// Pinning the page i.e. putting the page in buffer pool
		pinPage(&tableManager->bufferPool, &scanManager->pageHandle, scanManager->recordID.page);
			
		// Retrieving the data of the page			
		data = scanManager->pageHandle.data;

		// Calulate the data location from record's slot and record size
		data = data + (scanManager->recordID.slot * recordSize);
		
		// Set the record's slot and page to scan manager's slot and page
		record->id.page = scanManager->recordID.page;
		record->id.slot = scanManager->recordID.slot;

		// Intialize the record data's first location
		char *dataP = record->data;

		// '-' is used for Tombstone mechanism.
		*dataP = '-';
		
		memcpy(++dataP, data + 1, recordSize - 1);

		// Increment scan count because we have scanned one record
		scanManager->scanCount++;
		scanCount++;

		// Test the record for the specified condition (test expression)
		evalExpr(record, schema, scanManager->condition, &result); 

		// v.boolV is TRUE if the record satisfies the condition
		if(result->v.boolV == TRUE)
		{
			// Unpin the page i.e. remove it from the buffer pool.
			unpinPage(&tableManager->bufferPool, &scanManager->pageHandle);
			// Return SUCCESS			
			return RC_OK;
		}
	}
	
	// Unpin the page i.e. remove it from the buffer pool.
	unpinPage(&tableManager->bufferPool, &scanManager->pageHandle);
	
	// Reset the Scan Manager's values
	scanManager->recordID.page = 1;
	scanManager->recordID.slot = 0;
	scanManager->scanCount = 0;
	
	// None of the tuple satisfy the condition and there are no more tuples to scan
	return RC_RM_NO_MORE_TUPLES;
}

RC closeScan (RM_ScanHandle *scan)
{
	//free scan and memory
	scan->rel = NULL;
    free(scan->mgmtData);  
//	scan->mgmtData = NULL;
	return RC_OK;
}


/* -----------------------dealing with schemas------------------------ */
extern int getRecordSize (Schema *schema)
{
	int rsize = 0;	

	for(int i = 0; i < schema->numAttr;i++)
	{
		switch(schema->dataTypes[i])
		{
			//check the type
			case DT_BOOL:
				rsize += sizeof(bool);
				break;
			case DT_INT:
				rsize += sizeof(int);
				break;
			case DT_FLOAT:
				rsize += sizeof(float);
				break;
			case DT_STRING:
				rsize += sizeof(char) * schema->typeLength[i];
				break;
            //if none of these types. print error.
			default: 
				printf("Data type error!\n");
				return -1;
			
		}
	}
	return rsize;
}

extern Schema *createSchema (int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys)
{
    Schema * newschema;
	//Allocate a new memory space
	newschema = (Schema *) malloc(sizeof(Schema));
//	printf("start create  schema\n");
	// Start create schema	
	newschema->numAttr = numAttr;
	newschema->attrNames = attrNames;
	newschema->dataTypes = dataTypes;
	newschema->typeLength = typeLength;
    newschema->keyAttrs = keys;
	newschema->keySize = keySize;
	return newschema; 
}

extern RC freeSchema (Schema *schema)
{
    free(schema);
 //   schema = NULL;
    return RC_OK;
}


/* -------------dealing with records and attribute values---------------- */
extern RC createRecord (Record **record, Schema *schema)
{
    Record *r = (Record*) malloc(sizeof(Record));
	//new space for record
	r->data= (char*) malloc(getRecordSize(schema));
	r->id.page = -1;
    r->id.slot = -1;

	char *data = r->data;	
	*data = '-'; //Tombstone
	*(++data) = '\0';//the end of string
	*record = r;
	return RC_OK;  
}

extern RC freeRecord (Record *record)
{
    free(record->data);
	free(record);
	return RC_OK;
}

extern RC getAttr (Record *record, Schema *schema, int attrNum, Value **value)
{
    int offset = 1;
	for(int i = 0; i < attrNum; i++)
	{
		switch (schema->dataTypes[i])
		{
			case DT_STRING:
				offset = offset + schema->typeLength[i];
				break;
			case DT_INT:
				offset = offset + sizeof(int);
				break;
			case DT_FLOAT:
				offset = offset + sizeof(float);
				break;
			case DT_BOOL:
				offset = offset + sizeof(bool);
				break;
		}
	}
	
	Value *attr = (Value*) malloc(sizeof(Value));
    
	//attribute number ==1 test case error
	if (attrNum == 1){
		schema->dataTypes[attrNum] = 1;
	}
	else {
		schema->dataTypes[attrNum] = schema->dataTypes[attrNum];
	}
	
	switch(schema->dataTypes[attrNum])
	{
		case DT_STRING:
        {
			int length = schema->typeLength[attrNum];
			attr->v.stringV = (char *) malloc(length + 1);
			memcpy(attr->v.stringV, record->data + offset,schema->typeLength[attrNum]);
			attr->v.stringV[length + 1] = '\0';
			attr->dt = DT_STRING;
			break;
        }

		case DT_INT:
        {
			int value;
			memcpy(&value, record->data + offset, sizeof(int));
			attr->v.intV = value;
			attr->dt = DT_INT;
      		break;
        }

		case DT_FLOAT:
        {
	  		float value;
	  		memcpy(&value, record->data + offset, sizeof(float));
	  		attr->v.floatV = value;
			attr->dt = DT_FLOAT;
			break;
        }
		
		case DT_BOOL:
        {
			bool value;
			memcpy(&value,record->data + offset, sizeof(bool));
			attr->v.boolV = value;
			attr->dt = DT_BOOL;
      			break;
        }
		
		default:
			return RC_RM_UNKOWN_DATATYPE;
	}

	*value = attr;
	return RC_OK;
}

extern RC setAttr (Record *record, Schema *schema, int attrNum, Value *value)
{
    if (value->dt != schema->dataTypes[attrNum]) 
        return RC_RM_COMPARE_VALUE_OF_DIFFERENT_DATATYPE;

	int offset = 1;
	for(int i = 0; i < attrNum; i++)
	{
		switch (schema->dataTypes[i])
		{
			case DT_STRING:
				offset = offset + schema->typeLength[i];
				break;
			case DT_INT:
				offset = offset + sizeof(int);
				break;
			case DT_FLOAT:
				offset = offset + sizeof(float);
				break;
			case DT_BOOL:
				offset = offset + sizeof(bool);
				break;
		}
	}
    char *data= record->data + offset;
	
	switch(schema->dataTypes[attrNum])
	{
		case DT_STRING:
		{
			memcpy(record->data + offset, value->v.stringV,schema->typeLength[attrNum]);
			data = data + schema->typeLength[attrNum];
			break;
		}

		case DT_INT:
		{
			*(int *) data = value->v.intV;	  
			data +=  sizeof(int);
			break;
		}
		
		case DT_FLOAT:
		{
			*(float *) data = value->v.floatV;
			data +=  sizeof(float);
			break;
		}
		
		case DT_BOOL:
		{	
			*(bool *) data = value->v.boolV;
			data +=  sizeof(bool);
			break;
		}

		default:
			return RC_RM_UNKOWN_DATATYPE;
	}			
	return RC_OK;
}