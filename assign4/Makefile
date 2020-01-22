CC = gcc
CFLAGS  = -g -Wall 
MYPATH_1 = ../assign1
MYPATH_2 = ../assign2
MYPATH_3 = ../assign3
 
all: b_tree

b_tree: test_assign4_1.o btree_mgr.o rm_serializer.o record_mgr.o dberror.o storage_mgr.o buffer_mgr.o expr.o buffer_mgr_stat.o
	$(CC) $(CFLAGS) -o b_tree test_assign4_1.o dberror.o expr.o record_mgr.o rm_serializer.o storage_mgr.o buffer_mgr.o buffer_mgr_stat.o btree_mgr.o -lm

test_expr: test_expr.o dberror.o expr.o record_mgr.o rm_serializer.o storage_mgr.o buffer_mgr.o buffer_mgr_stat.o btree_mgr.o
	$(CC) $(CFLAGS) -o test_expr test_expr.o dberror.o expr.o record_mgr.o rm_serializer.o storage_mgr.o buffer_mgr.o buffer_mgr_stat.o btree_mgr.o -lm

test_assign4_1.o: test_assign4_1.c btree_mgr.h dberror.h storage_mgr.h test_helper.h buffer_mgr.h buffer_mgr_stat.h
	$(CC) $(CFLAGS) -c test_assign4_1.c 

test_expr.o: test_expr.c dberror.h expr.h record_mgr.h tables.h test_helper.h
	$(CC) $(CFLAGS) -c test_expr.c

btree_mgr.o: btree_mgr.c btree_mgr.h buffer_mgr.h storage_mgr.h dberror.h dt.h
	$(CC) $(CFLAGS) -c btree_mgr.c 

record_mgr.o: $(MYPATH_3)/record_mgr.c record_mgr.h buffer_mgr.h storage_mgr.h
	$(CC) $(CFLAGS) -c  $(MYPATH_3)/record_mgr.c

expr.o: expr.c dberror.h record_mgr.h expr.h tables.h
	$(CC) $(CFLAGS) -c expr.c

rm_serializer.o: rm_serializer.c dberror.h tables.h record_mgr.h
	$(CC) $(CFLAGS) -c rm_serializer.c

buffer_mgr_stat.o: buffer_mgr_stat.c buffer_mgr_stat.h buffer_mgr.h
	$(CC) $(CFLAGS) -c buffer_mgr_stat.c

buffer_mgr.o: $(MYPATH_2)/buffer_mgr.c buffer_mgr.h dt.h storage_mgr.h
	$(CC) $(CFLAGS) -c $(MYPATH_2)/buffer_mgr.c

storage_mgr.o: $(MYPATH_1)/storage_mgr.c storage_mgr.h 
	$(CC) $(CFLAGS) -c $(MYPATH_1)/storage_mgr.c

dberror.o: dberror.c dberror.h 
	$(CC) $(CFLAGS) -c dberror.c

clean: 
	$(RM) b_tree test_expr *.o *~
