#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>

#include "storage_mgr.h"
#include "buffer_mgr.h"
#include "record_mgr.h"
#include "btree_mgr.h"


/*--------------------init and shutdown index manager--------------------*/
RC initIndexManager (void *mgmtData)
{
    printf("++++++++++++++++Init++++++++++++++++++\n");
    return RC_OK;
}

RC shutdownIndexManager ()
{
    printf("+++++++++++++++Shutdown++++++++++++++++\n");
    return RC_OK;
}

/*----------------------Accessibility Functions-------------------------*/
storeArray *Init(int size) //init an array to store value, it contain value's size.
{
    storeArray *arr = (storeArray *) malloc(sizeof(storeArray)); 
    arr->elems = (int *) malloc(sizeof(int)*size);
    arr->size = size;
    arr->fill = 0;
    return arr;
}

BT_Node *createBTNode(int size, int isLeaf, int pageNum) //creat a new node
{
    BT_Node *node = (BT_Node *) malloc(sizeof(BT_Node));
    node->isLeaf = isLeaf;
    node->size = size;
    node->pageNum = pageNum;
    node->vals = Init(size);
    node->right = NULL;
    node->left = NULL;
    node->parent = NULL;
    if (isLeaf) 
    {
        node->leafRIDPages = Init(size);
        node->leafRIDSlots = Init(size);
    }else {
        node->childrenPages = Init(size + 1);
        node->children = (BT_Node *) malloc(sizeof(BT_Node) * (size+1));
    }
    return node;
}

RC writeBtreeHeader(BTreeHandle *tree)//modify the information of b_tree header
{//if the tree is null, in many our functions, add a new node to root, then give the tree this header
    RC rc;
    BM_BufferPool *bm = tree->mgmtData;
    BM_PageHandle *page = (BM_PageHandle *) malloc(sizeof(BM_PageHandle)); 
    if (RC_OK != (rc = pinPage(bm, page, 0))) 
    {
        free(page);
        return rc;
    }
    char *ptr = page->data;
    int *pos = (int *)ptr;
    pos[0] = tree->size;
    pos[1] = tree->keyType;
    pos[2] = tree->whereIsRoot;
    pos[3] = tree->numNodes;
    pos[4] = tree->numEntries;
    pos[5] = tree->depth;
    pos[6] = tree->nextPage;
    rc = markDirty(bm, page);
    rc = unpinPage(bm, page);
    forceFlushPool(bm);
    free(page);
    return rc;
}

int BinarySearch(storeArray *arr, int elem, int *fitOn) //binary search to find the node
{
    int first = 0;
    int last = arr->fill - 1;
    if (last < 0) // empty array
    { 
            (*fitOn) = first;
            return -1;
    }
    int position;
    while(1) 
    {
            position = (first + last) / 2;
            if (elem == arr->elems[position]) 
            {
                while(position && elem == arr->elems[position - 1]) 
                { // judge if there is the same element infront of this element
                    position--;
                }
                (*fitOn) = position;
                return position;
            }
            if (first >= last) 
            {
                if (elem > arr->elems[first]) 
                    first++;
                (*fitOn) = first; // must fit here
                return -1; // not found
            }
            if (elem > arr->elems[position]) 
            {
                first = position + 1;
            }else {
                last = position - 1;
            }
    }
}

int Search(storeArray *arr, int elem, int *fitOn)//loop lookup the key
{
    int num_elems = arr->fill - 1;
    if(num_elems < 0)//empty
    {
        (*fitOn) = 0;
        return -1;
    }
    int i=0;
    //int *ptr = arr->elems;
    while(1)
    {
        if(i > num_elems) return-1;
        if(elem == arr->elems[i])
        {
            (*fitOn) = i;
            return i;
        }
        i++;
    }
}

int InsertAt(storeArray *arr, int elem, int index) //given a location and insert the node
{
    if (arr->size > arr->fill && index <= arr->fill) 
    {
        if (index != arr->fill) 
        { // insert at the end
            for (int i = arr->fill; i > index; i--) 
            {
                arr->elems[i] = arr->elems[i - 1];
            }
        }
        arr->elems[index] = elem;
        arr->fill++;
        return index;
    }
    return -1; // not more place, or invalid index
}

RC readNode(BT_Node **node, BTreeHandle *tree, int pageNum) //read a node
{
    RC rc;
    BM_PageHandle *page = (BM_PageHandle *) malloc(sizeof(BM_PageHandle));
    if (RC_OK!=(rc = pinPage(tree->mgmtData, page, pageNum))) 
    {
        free(page);
        return rc;
    }

    int isLeaf;
    int *ptr = (int *)page->data;
    isLeaf = *ptr;
    int filled;
    filled = *(++ptr);
    ptr = (int *)(page->data + 40); // skip header bytes of the node
    BT_Node *node1 = createBTNode(tree->size, isLeaf, pageNum);
    int value, i;
    if (!isLeaf) 
    {
        int childPage;
        for (i = 0; i < filled; i++)
        {
            childPage = *ptr;
            value = *(++ptr);
            ptr++;
            InsertAt(node1->vals, value, i);
            InsertAt(node1->childrenPages, childPage, i);
        }
        childPage = *ptr;
        InsertAt(node1->childrenPages, childPage, i);
    }else {
        int ridPage, ridSlot;
        for (i = 0; i < filled; i++) 
        {
            ridPage = *ptr;
            ridSlot = *(++ptr);
            value = *(++ptr);
            ptr++;
            InsertAt(node1->vals, value, i);
            InsertAt(node1->leafRIDPages, ridPage, i);
            InsertAt(node1->leafRIDSlots, ridSlot, i);
        }
    }
    rc = unpinPage(tree->mgmtData, page);
    free(page);
    *node = node1;
    return rc;
}

RC writeNode(BT_Node *node, BTreeHandle *tree) //write to node
{
    RC rc;
    BM_PageHandle *page = (BM_PageHandle *) malloc(sizeof(BM_PageHandle));
    rc = pinPage(tree->mgmtData, page, node->pageNum);
    if (rc != RC_OK) 
    {
        free(page);
        return rc;
    }
    int *ptr = (int *)page->data;
    *ptr = node->isLeaf;
    *(++ptr) = node->vals->fill;
    ptr = (int *)(page->data + 40); // skip header bytes of the node
    int i;
    if (!node->isLeaf) 
    {
        for (i = 0; i < node->vals->fill; i++) 
        {
            *ptr = node->childrenPages->elems[i];
            *(++ptr) = node->vals->elems[i];
            ptr++;
        }
        *ptr = node->childrenPages->elems[i];
    }else {
        for (i = 0; i < node->vals->fill; i++) 
        {
            *ptr = node->leafRIDPages->elems[i];
            *(++ptr) = node->leafRIDSlots->elems[i];
            *(++ptr) = node->vals->elems[i];
            ptr++;
        }
    }
    rc = markDirty(tree->mgmtData, page);
    rc = unpinPage(tree->mgmtData, page);
    forceFlushPool(tree->mgmtData);
    free(page);
    return rc;
}

RC loadBtreeNodes(BTreeHandle *tree, BT_Node *root, BT_Node **leftOnLevel, int level) 
{//load nodes
    BT_Node *left = leftOnLevel[level];
    RC rc;
    rc = RC_OK;
    if(!root->isLeaf) {
        for (int i = 0; i < root->childrenPages->fill; i++) {
            rc = readNode(&root->children[i], tree, root->childrenPages->elems[i]);
            if (rc != RC_OK) {
                return rc;
            }
            if (left != NULL) {
                left->right = root->children[i];
            }
            root->children[i]->left = left;
            left = root->children[i];
            root->children[i]->parent = root;
            leftOnLevel[level] = left;
            rc = loadBtreeNodes(tree, root->children[i], leftOnLevel, level + 1);
            if (rc) return rc;
        }
    }
    return rc;
}

RC loadBtree(BTreeHandle *tree) //help to load our exist tree to a new tree
{
    RC rc;
    rc = RC_OK;
    tree->root = NULL;
    if (tree->depth) 
    {
        if ((rc = readNode(&tree->root, tree, tree->whereIsRoot))) 
        {
            return rc;
        }
        BT_Node **leftOnLevel =(BT_Node **) malloc(sizeof(BT_Node *) * tree->depth);
        for (int i = 0; i < tree->depth; i++) 
        {
            leftOnLevel[i] = NULL;
        }
        rc = loadBtreeNodes(tree, tree->root, leftOnLevel, 0);
        free(leftOnLevel);
        return rc;
    }
    return rc;
}

RC insertHelper(BTreeHandle *tree, BT_Node *left, BT_Node *right, int key) //use to insert the key
{
    BT_Node *parent = left->parent;
    int index, i;
    if (parent == NULL) //left is the root
    {
        parent = createBTNode(tree->size, 0, tree->nextPage);
        InsertAt(parent->childrenPages, left->pageNum, 0);
        parent->children[0] = left;
        tree->nextPage++;
        tree->whereIsRoot = parent->pageNum;
        tree->numNodes++;
        tree->depth++;
        tree->root = parent;
        writeBtreeHeader(tree);
    }
    right->parent = parent;
    left->parent = parent;
    int pos = -1;
    BinarySearch(parent->vals, key, &pos);
    index = InsertAt(parent->vals, key, pos);
    if (index < 0) 
    {
        // parent is full
        // Overflowed = parent + 1 new item
        BT_Node * overflowed = createBTNode(tree->size + 1, 0, -1);
        overflowed->vals->fill = parent->vals->fill;
        overflowed->childrenPages->fill = parent->childrenPages->fill;

        memcpy(overflowed->vals->elems, parent->vals->elems, sizeof(int) * parent->vals->fill);
        memcpy(overflowed->childrenPages->elems, parent->childrenPages->elems, sizeof(int) * parent->childrenPages->fill);
        memcpy(overflowed->children, parent->children, sizeof(BT_Node *) * parent->childrenPages->fill);
        int pos = -1;
        BinarySearch(overflowed->vals, key, &pos);
        index = InsertAt(overflowed->vals, key, pos);
        InsertAt(overflowed->childrenPages, right->pageNum, index + 1);
        for (i = parent->childrenPages->fill; i > index + 1; i--) 
        {
            overflowed->children[i] = overflowed->children[i - 1];
        }
        overflowed->children[index + 1] = right;
        int leftFill = overflowed->vals->fill / 2;
        int rightFill = overflowed->vals->fill - leftFill;
        BT_Node *rightParent = createBTNode(tree->size, 0, tree->nextPage);
        tree->nextPage++;
        tree->numNodes++;
            // Since overflowed is sorted then it is safe to just copy the content
            // copy left
        parent->vals->fill = leftFill;
        parent->childrenPages->fill = leftFill + 1;
        int lptrsSize = parent->childrenPages->fill;
        memcpy(parent->vals->elems, overflowed->vals->elems, sizeof(int) * leftFill);
        memcpy(parent->childrenPages->elems, overflowed->childrenPages->elems, sizeof(int) * lptrsSize);
        memcpy(parent->children, overflowed->children, sizeof(BT_Node *) * lptrsSize);

            // copy right
        rightParent->vals->fill = rightFill;
        rightParent->childrenPages->fill = overflowed->childrenPages->fill - lptrsSize;
        int rptrsSize = rightParent->childrenPages->fill;
        memcpy(rightParent->vals->elems, overflowed->vals->elems + leftFill, sizeof(int) * rightFill);
        memcpy(rightParent->childrenPages->elems, overflowed->childrenPages->elems + lptrsSize, sizeof(int) * rptrsSize);
        memcpy(rightParent->children, overflowed->children + lptrsSize, sizeof(BT_Node *) * rptrsSize);
        free(overflowed);

            // parent is always less than right and greater than left
        key = rightParent->vals->elems[0];
        rightParent->vals->fill--;
        for(int n = 0; n < rightParent->vals->fill; n++)
        {
            *(rightParent->vals->elems+i) = *(rightParent->vals->elems+i+1);
        }

            // introduce to each other
        rightParent->right = parent->right;
        if (rightParent->right != NULL) 
        {
            rightParent->right->left = rightParent;
        }
        parent->right = rightParent;
        rightParent->left = parent;

        writeNode(parent, tree);
        writeNode(rightParent, tree);
        writeBtreeHeader(tree);
        return insertHelper(tree, parent, rightParent, key);
    }else{
        index += 1; // next position is the pointer
        InsertAt(parent->childrenPages, right->pageNum, index);
        for (int i = parent->vals->fill; i > index; i--) 
        {
            parent->children[i] = parent->children[i - 1];
        }
        parent->children[index] = right;
        return writeNode(parent, tree);
    }
}

/*----------------create, destroy, open, and close an btree index---------------*/
RC createBtree (char *idxId, DataType keyType, int n)
{
    RC rc;
    //create file
    rc = createPageFile(idxId);
    if(rc!=RC_OK)
        return rc;

    SM_FileHandle *fHandle = (SM_FileHandle *) malloc(sizeof(SM_FileHandle));

    rc = openPageFile(idxId, fHandle);//open this file
    if (rc != RC_OK)
    {
        free(fHandle);
        return rc;
    }

    //init header
    char *header = (char*)calloc(PAGE_SIZE,sizeof(char));
    int *pos = (int *)header;
    pos[0] = n;
    pos[1] = keyType;
    pos[6] = 1;//nextpage

    rc = writeBlock(0, fHandle, header);
    if (rc!= RC_OK) 
    {
        free(fHandle);
        free(header);
        return rc;
    }

    free(header);
    rc = closePageFile(fHandle);
    free(fHandle);
    return rc;
}

RC openBtree (BTreeHandle **tree, char *idxId)
{
    RC rc;
    BTreeHandle *tem_tree = (BTreeHandle *)malloc(sizeof(BTreeHandle));
    BM_BufferPool *bm = (BM_BufferPool *)malloc(sizeof(BM_BufferPool));
    rc = initBufferPool(bm, idxId, 10, RS_LRU, NULL);
    if (rc) 
    {
        free(tem_tree);
        free(bm);
        return rc;
    }
    BM_PageHandle *pageHandle = (BM_PageHandle *)malloc(sizeof(BM_PageHandle));
    rc = pinPage(bm, pageHandle, 0);
    if (rc != RC_OK) {
        free(pageHandle);
        free(bm);
        free(tem_tree);
        return rc;
    }
   
    //load the tree
    tem_tree->idxId = idxId;
    tem_tree->mgmtData = bm;

    int *pos = (int *)pageHandle->data; 
    tem_tree->size = *pos;
    tem_tree->keyType = *(pos+1);
    tem_tree->whereIsRoot = *(pos+2);
    tem_tree->numNodes = *(pos+3);
    tem_tree->numEntries = *(pos+4);
    tem_tree->depth = *(pos+5);
    tem_tree->nextPage = *(pos+6);

    rc = unpinPage(bm, pageHandle);
    if (rc != RC_OK) 
    {
        free(pageHandle);
        closeBtree(tem_tree);
        return rc;
    }
    free(pageHandle);
    rc = loadBtree(tem_tree);
    if (rc != RC_OK) 
    {
        closeBtree(tem_tree);
        return rc;
    }

    *tree = tem_tree;//submit the tree
    return RC_OK;
}

RC closeBtree (BTreeHandle *tree)
{
    shutdownBufferPool(tree->mgmtData);
    free(tree);
    return RC_OK;
}

RC deleteBtree (char *idxId) 
{
    idxId = NULL;
    return RC_OK;
}

/*------------------access information about a b-tree-------------------*/
RC getNumNodes (BTreeHandle *tree, int *result)
{   
    *result = tree->numNodes;
    return RC_OK;
}
RC getNumEntries (BTreeHandle *tree, int *result)
{
     *result = tree->numEntries;
    return RC_OK;
}
RC getKeyType (BTreeHandle *tree, DataType *result)
{
    *result = tree->keyType;
    return RC_OK;
}

/*-----------------------------index access------------------------------*/
BT_Node *findNodeByKey(BTreeHandle *tree, int key) 
{
    BT_Node *current = tree->root;
    int index, fitOn;
    while(current != NULL && !current->isLeaf) {
        index = BinarySearch(current->vals, key, &fitOn);
        if (index >= 0) {
        fitOn += 1;
        }
        current = current->children[fitOn];
    }
    return current;
}

RC findKey (BTreeHandle *tree, Value *key, RID *result)
{
    int index, fitOn;
    BT_Node *leaf = findNodeByKey(tree, key->v.intV);
    index = BinarySearch(leaf->vals, key->v.intV, &fitOn);
    if (index < 0) {
        return RC_IM_KEY_NOT_FOUND;
    }
    result->page = leaf->leafRIDPages->elems[index];
    result->slot = leaf->leafRIDSlots->elems[index];
    return RC_OK;
}

RC insertKey (BTreeHandle *tree, Value *key, RID rid)
{
    BT_Node *leaf = findNodeByKey(tree, key->v.intV);

    if (leaf == NULL) //empty tree, add this key to root
    {
        leaf = createBTNode(tree->size, 1, tree->nextPage);
        tree->nextPage++;
        tree->whereIsRoot = leaf->pageNum;
        tree->numNodes++;
        tree->depth++;
        tree->root = leaf;
        writeBtreeHeader(tree);
    }

    int index=-1, fitOn=-1;
    index = BinarySearch(leaf->vals, key->v.intV, &fitOn);
    if (index >= 0) 
    {
        return RC_IM_KEY_ALREADY_EXISTS;
    }
    int pos = -1;
    BinarySearch(leaf->vals, key->v.intV, &pos);
    index = InsertAt(leaf->vals, key->v.intV, pos);
    if (index >= 0) 
    {
        InsertAt(leaf->leafRIDPages, rid.page, index);
        InsertAt(leaf->leafRIDSlots, rid.slot, index);
    }else {
        // leaf is overflow
        BT_Node * overflowed = createBTNode(tree->size + 1, 1, -1);
        overflowed->vals->fill = leaf->vals->fill;
        overflowed->vals->elems = leaf->vals->elems;
        overflowed->leafRIDPages->elems = leaf->leafRIDPages->elems;
        overflowed->leafRIDPages->fill = leaf->leafRIDPages->fill;
        overflowed->leafRIDSlots->elems = leaf->leafRIDSlots->elems;
        overflowed->leafRIDSlots->fill = leaf->leafRIDSlots->fill;
        int pos = -1;
        BinarySearch(overflowed->vals, key->v.intV, &pos);
        index = InsertAt(overflowed->vals, key->v.intV, pos);
        InsertAt(overflowed->leafRIDPages, rid.page, index);
        InsertAt(overflowed->leafRIDSlots, rid.slot, index);

        int leftFill = (overflowed->vals->fill / 2) + 1;
        int rightFill = overflowed->vals->fill - leftFill;
        BT_Node *rightLeaf = createBTNode(tree->size, 1, tree->nextPage);
        tree->nextPage++;
        tree->numNodes++;
        // Since overflowed is sorted then it is safe to just copy the content
        // left
        leaf->vals->fill = leftFill;
        leaf->leafRIDPages->fill = leftFill;
        leaf->leafRIDSlots->fill = leftFill;
        leaf->vals->elems = overflowed->vals->elems;
        leaf->leafRIDPages->elems = overflowed->leafRIDPages->elems;
        leaf->leafRIDSlots->elems = overflowed->leafRIDSlots->elems;

        // right
        rightLeaf->vals->fill = rightFill;
        rightLeaf->leafRIDPages->fill = rightFill;
        rightLeaf->leafRIDSlots->fill = rightFill;
        memcpy(rightLeaf->vals->elems, overflowed->vals->elems + leftFill, sizeof(int) * rightFill);
        memcpy(rightLeaf->leafRIDPages->elems, overflowed->leafRIDPages->elems + leftFill, sizeof(int) * rightFill);
        memcpy(rightLeaf->leafRIDSlots->elems, overflowed->leafRIDSlots->elems + leftFill, sizeof(int) * rightFill);

        free(overflowed);

        // connect to each other
        rightLeaf->right = leaf->right;
        if (rightLeaf->right != NULL)
        {
            rightLeaf->right->left = rightLeaf;
        }
        leaf->right = rightLeaf;
        rightLeaf->left = leaf;
        writeNode(rightLeaf, tree);
        writeNode(leaf, tree);
        insertHelper(tree, leaf, rightLeaf, rightLeaf->vals->elems[0]);
    }
    tree->numEntries++;
    writeBtreeHeader(tree);
    return RC_OK;
}

RC deleteKey (BTreeHandle *tree, Value *key)
{
    BT_Node *leaf = findNodeByKey(tree, key->v.intV);
    if (leaf != NULL) {
    int index, _unused;
    index = BinarySearch(leaf->vals, key->v.intV, &_unused);
    if (index >= 0) {

      leaf->vals->fill--;//delete values,other value iteratelly move upward
      for(int i = index;i<leaf->vals->fill;i++)
      {
          *(leaf->vals->elems+i) = *(leaf->vals->elems+i+1);
      }

      leaf->leafRIDPages->fill--;//delete ridpage,other page iteratelly move upward
      for(int i = index;i<leaf->leafRIDPages->fill;i++)
      {
          *(leaf->leafRIDPages->elems+i) = *(leaf->leafRIDPages->elems+i+1);
      }

      leaf->leafRIDSlots->fill--;//delete ridslot,other slot iteratelly move upward
      for(int i = index;i<leaf->leafRIDSlots->fill;i++)
      {
          *(leaf->leafRIDSlots->elems+i) = *(leaf->leafRIDSlots->elems+i+1);
      }

      tree->numEntries--;
      writeNode(leaf, tree);
      writeBtreeHeader(tree);
    }
  }
  return RC_OK;
}

RC openTreeScan (BTreeHandle *tree, BT_ScanHandle **handle)
{
    BT_ScanHandle *handle1 = (BT_ScanHandle *)malloc(sizeof(BT_ScanHandle));
    ScanMgmtInfo *scanInfo = (ScanMgmtInfo *)malloc(sizeof(ScanMgmtInfo));
    handle1->tree = tree;
    scanInfo->currentNode = tree->root;
    while(!scanInfo->currentNode->isLeaf) { // finding the left most leaf
        scanInfo->currentNode = scanInfo->currentNode->children[0];
    }
    scanInfo->elementIndex = 0;
    handle1->mgmtData = scanInfo;
    *handle = handle1;
    return RC_OK;
}

RC nextEntry (BT_ScanHandle *handle, RID *result)
{
    ScanMgmtInfo *scanInfo = handle->mgmtData;
  if(scanInfo->elementIndex >= scanInfo->currentNode->leafRIDPages->fill) { // finding the left most leaf
    //searchNextNode
    if(scanInfo->elementIndex == scanInfo->currentNode->vals->fill && scanInfo->currentNode->right==NULL){
      return RC_IM_NO_MORE_ENTRIES;
    } else {
      scanInfo->currentNode = scanInfo->currentNode->right;
      scanInfo->elementIndex = 0;
    }
  }
  result->page = scanInfo->currentNode->leafRIDPages->elems[scanInfo->elementIndex];
  result->slot = scanInfo->currentNode->leafRIDSlots->elems[scanInfo->elementIndex];
  scanInfo->elementIndex++;
  return RC_OK;
}

RC closeTreeScan (BT_ScanHandle *handle)
{
    free(handle->mgmtData);
    free(handle);
  return RC_OK;
}

/*--------------------------debug and test functions-------------------------*/
RC printNode(BT_Node *node) //print a single node
{
    if (node == NULL) 
    {
        printf("NULL Node !!\n");
        return RC_IM_ERROR;
    }
    printf("(%d)[", node->pageNum);

    if(node->isLeaf == 0)//internal
    {
       for (int i = 0; i < node->vals->fill; i++) 
        {
            printf("%d,", node->childrenPages->elems[i]);
            printf("%d,", node->vals->elems[i]);
        }
    } else {   //leaf
        for (int i = 0; i < node->vals->fill; i++) 
        {
            printf("%d", node->leafRIDPages->elems[i]);
            printf(".%d,", node->leafRIDSlots->elems[i]);
            printf("%d", node->vals->elems[i]);
            if(i < node->vals->fill-1)
            {
                printf(",");
            }
        }
    }
    printf("]\n");
    return RC_OK;
}

char *printTree (BTreeHandle *tree)
{
    BT_Node *node = tree->root;
    int level=0;
    while(node!=NULL)
    {
        printNode(node);
        if(node->isLeaf)
        {
            node = node->right;
        } else {
            if(NULL == node->right)
            {
                BT_Node *temp = tree->root;
                for(int j=0; j<=level;j++)
                {
                    temp=temp->children[0];
                }
                node = temp;
                level++;
            } else {
                node = node->right;
            }
        }
    }
    return "";
}