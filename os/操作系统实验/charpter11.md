# 第十一章 进程同步与互斥 - 详细解析与代码注释

## 11.1 信号量机制概述

第十一章深入探讨了操作系统中进程同步与互斥的核心机制——信号量。信号量是解决多线程环境下资源竞争和协调问题的关键工具，由Dijkstra于1965年提出。

### 11.1.1 信号量的基本概念

信号量是一个整型变量，用于控制对共享资源的访问。它支持两个原子操作：
- **P操作**（wait/proberen）：申请资源，信号量值减1
- **V操作**（signal/verhogen）：释放资源，信号量值加1




### 11.1.2 信号量类型

1. **整型信号量**：简单整数，忙等待机制
2. **记录型信号量**：包含值和一个等待队列，避免忙等待
3. **AND型信号量**：同时申请多个资源
4. **信号量集**：一次申请多个同类资源

在GeekOS中，主要实现的是记录型信号量，包含值、等待队列和注册线程信息。

## 11.2 信号量设计原理

### 11.2.1 信号量数据结构

在GeekOS中，信号量通过以下结构体定义：

```c
// syscall.c 中的信号量结构定义
struct Semaphore {
    int semaphoreID;           // 信号量唯一标识
    char* semaphoreName;       // 信号量名称
    int value;                 // 信号量当前值
    int registeredThreadCount; // 注册使用该信号量的线程数
    struct Kernel_Thread* registeredThreads[MAX_REGISTERED_THREADS]; // 注册线程数组
    struct Thread_Queue waitingThreads; // 等待队列
    DEFINE_LINK(Semaphore_List, Semaphore); // 链表指针
};
```

### 11.2.2 信号量操作原理

信号量的P和V操作必须保证原子性，在GeekOS中通过关中断来实现：

```c
// P操作基本逻辑
P(Semaphore S) {
    S.value--;                // 申请资源
    if (S.value < 0) {
        block(S.waitingThreads); // 阻塞当前线程
    }
}

// V操作基本逻辑  
V(Semaphore S) {
    S.value++;                // 释放资源
    if (S.value <= 0) {
        wakeup(S.waitingThreads); // 唤醒等待线程
    }
}
```




## 11.3 信号量实现详解

### 11.3.1 信号量创建函数

```c
// syscall.c 中的信号量创建实现
static int Sys_CreateSemaphore(struct Interrupt_State* state) {
    char* semName = NULL;
    int id;
    
    // 从用户空间复制信号量名称
    Copy_User_String(state->ebx, state->ecx, MAX_LEN, &semName);
    
    // 调用内部创建函数
    id = Semaphore_Create(semName, state->edx);
    return id;
}

// 实际的信号量创建逻辑
int Semaphore_Create(char* semName, int initialValue) {
    struct Kernel_Thread* current = g_currentThread;
    struct Semaphore* s, *sem;
    
    // 检查是否已存在同名信号量
    s = Get_Front_Of_Semaphore_List(&s_SemaphoreList);
    while (s != 0) {
        if (!strcmp(s->semaphoreName, semName)) {
            // 已存在，将当前线程加入注册列表
            s->registeredThreads[s->registeredThreadCount] = current;
            s->registeredThreadCount++;
            return s->semaphoreID;
        }
        s = Get_Next_In_Semaphore_List(s);
    }
    
    // 创建新信号量
    sem = Malloc(sizeof(struct Semaphore));
    if (sem == 0) return -1;
    
    // 初始化信号量字段
    sem->registeredThreadCount = 0;
    sem->registeredThreads[sem->registeredThreadCount] = current;
    sem->registeredThreadCount++;
    sem->semaphoreName = semName;
    sem->value = initialValue;
    sem->semaphoreID = ID++;  // 分配唯一ID
    
    // 初始化等待队列
    Clear_Thread_Queue(&sem->waitingThreads);
    
    // 添加到全局信号量列表
    Add_To_Back_Of_Semaphore_List(&s_SemaphoreList, sem);
    
    return sem->semaphoreID;
}
```

### 11.3.2 P操作实现

```c
// P操作系统调用
static int Sys_P(struct Interrupt_State* state) {
    int r, id = state->ebx;  // 从ebx获取信号量ID
    r = Semaphore_Acquire(id);
    return r;
}

// P操作核心逻辑
int Semaphore_Acquire(int semID) {
    struct Kernel_Thread* current = g_currentThread;
    struct Semaphore* s;
    int i;
    
    // 查找信号量
    s = Get_Front_Of_Semaphore_List(&s_SemaphoreList);
    while (s != 0) {
        if (s->semaphoreID == semID) {
            // 检查当前线程是否已注册
            for (i = 0; i < s->registeredThreadCount; ++i) {
                if (s->registeredThreads[i] == current) {
                    // 执行P操作
                    s->value--;
                    if (s->value < 0) {
                        // 资源不足，加入等待队列
                        Wait(&s->waitingThreads);
                    }
                    return 0;  // 成功获取
                }
            }
            // 线程未注册
            return -1;
        }
        s = Get_Next_In_Semaphore_List(s);
    }
    return -1;  // 信号量不存在
}
```

### 11.3.3 V操作实现

```c
// V操作系统调用
static int Sys_V(struct Interrupt_State* state) {
    int r, id = state->ebx;  // 从ebx获取信号量ID
    r = Semaphore_Release(id);
    return r;
}

// V操作核心逻辑
int Semaphore_Release(int semID) {
    struct Kernel_Thread* current = g_currentThread;
    struct Semaphore* s;
    int i;
    
    // 查找信号量
    s = Get_Front_Of_Semaphore_List(&s_SemaphoreList);
    while (s != 0) {
        if (s->semaphoreID == semID) {
            // 检查当前线程是否已注册
            for (i = 0; i < s->registeredThreadCount; ++i) {
                if (s->registeredThreads[i] == current) {
                    // 执行V操作
                    s->value++;
                    if (s->value <= 0) {
                        // 有线程在等待，唤醒一个
                        Wake_Up_One(&s->waitingThreads);
                    }
                    return 0;  // 成功释放
                }
            }
            // 线程未注册
            return -1;
        }
        s = Get_Next_In_Semaphore_List(s);
    }
    return -1;  // 信号量不存在
}
```

### 11.3.4 信号量销毁

```c
// 信号量销毁系统调用
static int Sys_DestroySemaphore(struct Interrupt_State* state) {
    int r, id = state->ebx;  // 从ebx获取信号量ID
    r = Semaphore_Destroy(id);
    return r;
}

// 信号量销毁逻辑
int Semaphore_Destroy(int semID) {
    struct Kernel_Thread* current = g_currentThread;
    struct Semaphore* s;
    int i, j;
    
    // 查找信号量
    s = Get_Front_Of_Semaphore_List(&s_SemaphoreList);
    while (s != 0) {
        if (s->semaphoreID == semID) {
            // 检查当前线程是否已注册
            for (i = 0; i < s->registeredThreadCount; ++i) {
                if (s->registeredThreads[i] == current) {
                    // 从注册列表中移除线程
                    for (j = i; j < s->registeredThreadCount - 1; ++j) {
                        s->registeredThreads[j] = s->registeredThreads[j + 1];
                    }
                    s->registeredThreadCount--;
                    
                    // 如果没有线程注册，彻底销毁信号量
                    if (s->registeredThreadCount == 0) {
                        Remove_From_Semaphore_List(&s_SemaphoreList, s);
                        Free(s->semaphoreName);
                        Free(s);
                    }
                    return 0;  // 成功销毁
                }
            }
            // 线程未注册
            return -1;
        }
        s = Get_Next_In_Semaphore_List(s);
    }
    return -1;  // 信号量不存在
}
```




## 11.4 信号量的使用

### 11.4.1 用户程序接口

GeekOS为用户程序提供了一套完整的信号量操作接口：

```c
// sema.h 中的用户接口定义
#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <geekos/ktypes.h>

// 信号量创建
int Create_Semaphore(const char* name, int ival);

// P操作
int P(int s);

// V操作  
int V(int s);

// 信号量销毁
int Destroy_Semaphore(int s);

#endif
```

### 11.4.2 实际使用示例

以下是一个使用信号量实现生产者-消费者问题的示例：

```c
// producer_consumer.c
#include <conio.h>
#include <process.h>
#include <sema.h>
#include <string.h>

#define BUFFER_SIZE 10

int main(int argc, char** argv) {
    int empty, full, mutex;
    int buffer[BUFFER_SIZE];
    int in = 0, out = 0;
    
    // 创建信号量
    empty = Create_Semaphore("empty", BUFFER_SIZE); // 空闲缓冲区数
    full = Create_Semaphore("full", 0);             // 已填充缓冲区数  
    mutex = Create_Semaphore("mutex", 1);           // 互斥锁
    
    // 生产者进程
    if (Spawn("/c/producer.exe", "producer", 0) == 0) {
        // 生产者逻辑
        int item;
        while (true) {
            item = produce_item();  // 生产项目
            
            P(empty);  // 等待空闲缓冲区
            P(mutex);  // 进入临界区
            
            buffer[in] = item;      // 放入缓冲区
            in = (in + 1) % BUFFER_SIZE;
            
            V(mutex);  // 离开临界区
            V(full);   // 增加已填充计数
        }
    }
    
    // 消费者进程
    if (Spawn("/c/consumer.exe", "consumer", 0) == 0) {
        // 消费者逻辑
        int item;
        while (true) {
            P(full);   // 等待有内容的缓冲区
            P(mutex);  // 进入临界区
            
            item = buffer[out];     // 取出项目
            out = (out + 1) % BUFFER_SIZE;
            
            V(mutex);  // 离开临界区
            V(empty);  // 增加空闲计数
            
            consume_item(item);  // 消费项目
        }
    }
    
    // 清理信号量
    Destroy_Semaphore(mutex);
    Destroy_Semaphore(full);
    Destroy_Semaphore(empty);
    
    return 0;
}
```

## 11.5 信号量测试

### 11.5.1 测试程序介绍

GeekOS提供了多个测试程序来验证信号量的正确性：

1. **semtest**：基本信号量功能测试
2. **semtest1**：多线程竞争测试
3. **semtest2**：复杂同步场景测试
4. **pingpong**：使用信号量实现进程间同步

### 11.5.2 pingpong程序分析

```c
// ping.c
#include <conio.h>
#include <process.h>
#include <sched.h>
#include <sema.h>
#include <string.h>

int main(int argc, char** argv) {
    int i, j;
    int scr_sem;    // 屏幕输出互斥信号量
    int time;
    int ping, pong; // 同步信号量
    
    time = Get_Time_Of_Day();
    scr_sem = Create_Semaphore("screen", 1);  // 屏幕互斥
    ping = Create_Semaphore("ping", 1);       // ping信号量
    pong = Create_Semaphore("pong", 0);       // pong信号量
    
    for (i = 0; i < 5; i++) {
        P(pong);        // 等待pong信号
        P(scr_sem);     // 获取屏幕锁
        Print("Ping\n");
        V(scr_sem);     // 释放屏幕锁
        
        for (j = 0; j < 35; j++); // 短暂延迟
        
        V(ping);        // 发送ping信号
    }
    
    time = Get_Time_Of_Day() - time;
    P(scr_sem);
    Print("Process done at time: %d\n", time);
    V(scr_sem);
    
    Destroy_Semaphore(pong);
    Destroy_Semaphore(ping);
    Destroy_Semaphore(scr_sem);
    
    return 0;
}
```

对应的pong程序：

```c
// pong.c
#include <conio.h>
#include <process.h>
#include <sched.h>
#include <sema.h>
#include <string.h>

int main(int argc, char** argv) {
    int i, j;
    int scr_sem;
    int time;
    int ping, pong;
    
    time = Get_Time_Of_Day();
    scr_sem = Create_Semaphore("screen", 1);
    ping = Create_Semaphore("ping", 0);  // 初始状态不同
    pong = Create_Semaphore("pong", 1);  // 初始状态不同
    
    for (i = 0; i < 5; i++) {
        P(ping);        // 等待ping信号
        P(scr_sem);
        Print("Pong\n");
        V(scr_sem);
        
        for (j = 0; j < 35; j++); // 短暂延迟
        
        V(pong);        // 发送pong信号
    }
    
    time = Get_Time_Of_Day() - time;
    P(scr_sem);
    Print("Process done at time: %d\n", time);
    V(scr_sem);
    
    Destroy_Semaphore(pong);
    Destroy_Semaphore(ping);
    Destroy_Semaphore(scr_sem);
    
    return 0;
}
```




## 11.6 关键知识点总结

### 11.6.1 信号量与互斥锁的区别

| 特性 | 信号量 | 互斥锁 |
|------|--------|--------|
| 主要用途 | 同步和互斥 | 主要用于互斥 |
| 值范围 | 非负整数 | 0或1 |
| 操作 | P/V操作 | Lock/Unlock |
| 所有权 | 无所有权概念 | 有所有权概念 |
| 唤醒策略 | 可唤醒任意等待线程 | 通常唤醒最早等待线程 |

### 11.6.2 信号量的应用场景

1. **互斥访问**：保护临界区，同一时间只允许一个线程访问
2. **同步协调**：协调线程执行顺序，确保特定操作顺序
3. **资源计数**：管理有限数量的资源分配
4. **条件同步**：等待特定条件成立

### 11.6.3 常见问题与解决方案

**问题1：死锁**
- 原因：循环等待资源
- 解决方案：按固定顺序申请资源，使用超时机制

**问题2：优先级反转**
- 原因：低优先级线程持有高优先级线程需要的资源
- 解决方案：优先级继承或优先级天花板协议

**问题3：资源饥饿**
- 原因：某些线程始终无法获得资源
- 解决方案：公平调度，老化机制

### 11.6.4 最佳实践建议

1. **最小化临界区**：减少在临界区内的操作时间
2. **避免嵌套锁**：尽量减少锁的嵌套层次
3. **使用超时机制**：避免无限期等待
4. **合理选择同步原语**：根据需求选择信号量、互斥锁或条件变量
5. **彻底测试**：特别是在多线程环境下测试同步逻辑

通过本章学习，可以深入理解信号量机制的原理和实现，掌握在多线程编程中正确使用同步原语的方法，避免常见的并发问题。信号量是操作系统提供的最重要的同步工具之一，正确使用它们可以构建可靠、高效的多线程应用程序。