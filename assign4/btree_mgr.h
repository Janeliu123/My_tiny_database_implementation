#ifndef BTREE_MGR_H
#define BTREE_MGR_H

#include "dberror.h"
#include "tables.h"

//record the size of value, how many value in this elems and real value
//make our access convient
typedef struct storeArray {
  int size;
  int fill;
  int *elems; 
} storeArray;

typedef struct BT_Node {
  int size; // values size
  int isLeaf;
  int pageNum;
  storeArray *vals;
  storeArray *childrenPages;
  storeArray *leafRIDPages;
  storeArray *leafRIDSlots;
  // tree pointers
  struct BT_Node **children; // in all but in leaf
  struct BT_Node *parent; // in all but in root
  struct BT_Node *left; // in all but in root
  struct BT_Node *right; // in all but in root
} BT_Node;

typedef struct ScanMgmtInfo {
  BT_Node *currentNode;
  int elementIndex;
} ScanMgmtInfo;

typedef struct BTreeHandle {
   DataType keyType;
  char *idxId;
  int size;
  int numEntries;
  int numNodes;
  int depth;
  int whereIsRoot;
  int nextPage;
  BT_Node *root;
  void *mgmtData;
} BTreeHandle;

typedef struct BT_ScanHandle {
  BTreeHandle *tree;
  void *mgmtData;
} BT_ScanHandle;

// init and shutdown index manager
extern RC initIndexManager (void *mgmtData);
extern RC shutdownIndexManager ();

// create, destroy, open, and close an btree index
extern RC createBtree (char *idxId, DataType keyType, int n);
extern RC openBtree (BTreeHandle **tree, char *idxId);
extern RC closeBtree (BTreeHandle *tree);
extern RC deleteBtree (char *idxId);

// access information about a b-tree
extern RC getNumNodes (BTreeHandle *tree, int *result);
extern RC getNumEntries (BTreeHandle *tree, int *result);
extern RC getKeyType (BTreeHandle *tree, DataType *result);

// index access
extern RC findKey (BTreeHandle *tree, Value *key, RID *result);
extern RC insertKey (BTreeHandle *tree, Value *key, RID rid);
extern RC deleteKey (BTreeHandle *tree, Value *key);
extern RC openTreeScan (BTreeHandle *tree, BT_ScanHandle **handle);
extern RC nextEntry (BT_ScanHandle *handle, RID *result);
extern RC closeTreeScan (BT_ScanHandle *handle);

// debug and test functions
extern char *printTree (BTreeHandle *tree);

#endif // BTREE_MGR_H
