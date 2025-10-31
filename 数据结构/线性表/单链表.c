// 动态分配
#include <stdio.h>
#include <stdlib.h>

#define InitSize 10

typedef struct{
    int *data;
    int length;
    int MaxSize
}SeqList;

void InitList(SeqList *L){
    L->data=(int *)malloc(InitSize*sizeof(int));
    L->length=0;
    L->MaxSize=InitSize;
}

void IncreaseSize(SeqList *L,int len){
    int *p=L->data;
    L->data=(int *)malloc((L->MaxSize+len)*sizeof(int));
    for(int i=0;i<len;i++){
        L->data[i]=p[i];          
    }
    L->MaxSize=L->MaxSize+len;
    free(p);
}


int main(){
    SeqList L;
    InitList(&L);

    IncreaseSize(&L,5);
}
