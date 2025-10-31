// 静态分配
#include <stdio.h>
#include <stdlib.h>
// 在代码开头添加这行
#include <stdbool.h>
#define MaxSize 10

typedef struct {
    int data[MaxSize];
    int length;
}SqList;

void InitList(SqList* L) {
    for (int i = 0; i < MaxSize; i++)
    {
        L->data[i] = 0;
    }
    L->length = 0;
}

bool ListInsert(SqList* L, int i, int e) {
    if (i<1 || i>L->length + 1)
    {
        return false;
    }
    if (L->length > MaxSize)
    {
        return false;
    }
    for (int j = L->length; j >= i; j--)
    {
        L->data[j] = L->data[j - 1];
    }
    L->data[i - 1] = e;
    L->length++;
    return true;
}  //插入时间复杂度 n/2


bool ListDelete(SqList* L, int i, int *e)
{
    if (i<1 || i>L->length)
    {
        return false;
    }
    for (int j = i; j < L->length; j++)
    {
        L->data[j - 1] = L->data[j];
    }
    return true;
}
// 删除的时间复杂度(n-1)/2

// 位查找
int GetElem(SqList *L,int i)
{
    return L->data[i-1];
}

// 值查找
int LocateElem(SqList *L,int e)
{
    for(int i=0;i<L->length;i++)
    {
        if(L->data[i]==e)
        return i+1;
    }
    return 0;
}

int main() {
    SqList L;
    InitList(&L);
    return 0;
}