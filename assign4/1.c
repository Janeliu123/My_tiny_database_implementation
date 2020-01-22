#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>

int BinarySearch(int *arr, int elem, int *fitOn) //find the element in array
{
    int first = 0;
    int last = sizeof(arr);
    if (last < 0) // empty array
    { 
            (*fitOn) = first;
            return -1;
    }
    int position;
    while(1) 
    {
            position = (first + last) / 2;
            if (elem == arr[position]) 
            {
                while(position && elem == arr[position - 1]) 
                { // always get the first occurance
                    position--;
                }
                (*fitOn) = position;
                return position;
            }
            if (first >= last) 
            {
                if (elem > arr[first]) 
                    first++;
                (*fitOn) = first; // must fit here
                return -1; // not found
            }
            if (elem < arr[position]) 
            {
                last = position - 1;
            }else {
                first = position + 1;
            }
    }
}

int Search(int *arr, int elem, int *fitOn) //find the element in array
{
    int num_elems = sizeof(arr);
    if(num_elems < 0)//empty
    {
        (*fitOn) = 0;
        return -1;
    }
    for(int i = 0; i <= num_elems; i++)
    {
        if(elem == arr[i])
        {
            (*fitOn) = i;
            return i;
        }
    }
    return -1;
}
void main()
{
    int *a=[13691013151819,22,25,27,29,37,49,58,67”;
    int b,c;
    BinarySearch(a,18,&b);
    Search(a,18,&c);
    printf("%d,%d",b,c);
}
