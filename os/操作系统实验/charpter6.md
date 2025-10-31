# 第六章 进程调度与线程管理详解

## 📖 章节概述
第六章深入探讨了GeekOS的进程调度系统和线程管理机制。进程调度是操作系统的核心功能，负责在多个线程之间分配CPU时间，确保系统资源的高效利用。通过本章学习，您将掌握线程的创建、调度、切换和销毁的全过程，理解调度算法的实现原理，以及如何在内核中管理多线程环境。这些知识是构建现代操作系统的基石，对于理解并发编程和资源管理至关重要。

## 🧠 6.1 线程控制块（Kernel_Thread）结构分析

### 线程控制块定义
线程控制块是操作系统中用于存储线程状态和信息的数据结构，每个线程都有一个对应的Kernel_Thread结构。

```c
/* Kernel_Thread结构定义：存储线程所有状态信息
 * 位置：/include/geekos/kthread.h
 */
struct Kernel_Thread {
    /* 执行上下文 */
    ulong_t esp;                    /* 栈指针：保存线程执行上下文 */
    volatile ulong_t numTicks;      /* 时间片计数器：记录已使用的CPU时间 */
    
    /* 调度信息 */
    int priority;                   /* 线程优先级：决定调度顺序 */
    DEFINE_LINK(Thread_Queue, Kernel_Thread); /* 线程队列指针：prev和next */
    
    /* 资源管理 */
    void* stackPage;                /* 栈页面指针：线程堆栈所在内存页 */
    struct User_Context* userContext; /* 用户上下文：用户线程的地址空间信息 */
    struct Kernel_Thread* owner;   /* 所有者线程：用于线程间关系 */
    int refCount;                   /* 引用计数：管理线程生命周期 */
    
    /* 线程同步 */
    bool alive;                     /* 存活标志：线程是否活跃 */
    struct Thread_Queue joinQueue;  /* 连接队列：等待此线程结束的线程 */
    int exitCode;                   /* 退出代码：线程终止时的返回值 */
    
    /* 标识信息 */
    int pid;                        /* 进程ID：唯一标识符 */
    DEFINE_LINK(All_Thread_List, Kernel_Thread); /* 全局线程列表指针 */
    
    /* 线程本地存储 */
    #define MAX_TLOCAL_KEYS 128     /* 最大线程本地键数 */
    const void* tlocalData[MAX_TLOCAL_KEYS]; /* 线程本地数据数组 */
};
```

### 关键字段详解
1. **esp（栈指针）**：保存线程的执行上下文，当线程被切换时，esp指向该线程的堆栈顶部，用于恢复执行。

2. **numTicks（时间片计数器）**：记录线程已经使用的CPU时间片数，用于时间片轮转调度算法。volatile关键字确保该值在多线程环境下正确访问。

3. **priority（优先级）**：决定线程在就绪队列中的调度顺序，GeekOS定义了多级优先级：
   ```c
   #define PRIORITY_IDLE     0    /* 空闲优先级（最低） */
   #define PRIORITY_USER     1    /* 用户优先级 */
   #define PRIORITY_LOW      2    /* 低优先级 */
   #define PRIORITY_NORMAL   3    /* 正常优先级 */
   #define PRIORITY_HIGH     4    /* 高优先级（最高） */
   ```

4. **DEFINE_LINK宏**：用于实现线程队列的双向链表功能，展开后包含prev和next指针：
   ```c
   /* DEFINE_LINK宏展开示例 */
   struct Kernel_Thread* prevThread_Queue;
   struct Kernel_Thread* nextThread_Queue;
   ```

5. **refCount（引用计数）**：采用引用计数机制管理线程生命周期，当refCount为0时，线程资源被回收。

## ⚙️ 6.2 线程调度器初始化

### Init_Scheduler() 函数详解
**位置**：`/src/geekos/kthread.c`
**功能**：初始化线程调度系统，创建主线程和系统线程。

```c
/* Init_Scheduler: 初始化线程调度器 */
void Init_Scheduler(void)
{
    struct Kernel_Thread* mainThread;
    
    /* 1. 初始化主线程（当前执行的内核线程） */
    mainThread = (struct Kernel_Thread*) KERN_THREAD_OBJ;
    Init_Thread(mainThread, (void*) KERN_STACK, PRIORITY_NORMAL, true);
    
    /* 2. 设置当前线程指针 */
    g_currentThread = mainThread;
    
    /* 3. 将主线程加入全局线程列表 */
    Add_To_Back_Of_All_Thread_List(&s_allThreadList, mainThread);
    
    /* 4. 创建空闲线程（Idle Thread） */
    Start_Kernel_Thread(Idle, 0, PRIORITY_IDLE, true);
    
    /* 5. 创建回收线程（Reaper Thread） */
    Start_Kernel_Thread(Reaper, 0, PRIORITY_NORMAL, true);
    
    Print("Scheduler initialized\n");
}
```

### Init_Thread() 函数详解
**位置**：`/src/geekos/kthread.c`
**功能**：初始化线程控制块的基本信息。

```c
/* Init_Thread: 初始化线程结构
 * @kthread: 线程控制块指针
 * @stackPage: 栈页面地址
 * @priority: 线程优先级
 * @detached: 是否分离线程（true=分离，false=可连接）
 */
static void Init_Thread(struct Kernel_Thread* kthread, void* stackPage, 
                       int priority, bool detached)
{
    static int nextFreePid = 1;  /* 静态变量，用于分配唯一PID */
    
    /* 清零线程结构体 */
    memset(kthread, '\0', sizeof(*kthread));
    
    /* 设置基本属性 */
    kthread->stackPage = stackPage;
    kthread->esp = (ulong_t)stackPage + PAGE_SIZE;  /* 栈向低地址增长 */
    kthread->numTicks = 0;
    kthread->priority = priority;
    kthread->userContext = 0;
    
    /* 设置所有者线程 */
    kthread->owner = detached ? 0 : g_currentThread;
    
    /* 设置引用计数：分离线程为1，非分离线程为2 */
    kthread->refCount = detached ? 1 : 2;
    
    /* 初始化线程状态 */
    kthread->alive = true;
    Clear_Thread_Queue(&kthread->joinQueue);
    kthread->pid = nextFreePid++;
}
```

## 🔄 6.3 线程创建与启动

### Start_Kernel_Thread() 函数详解
**位置**：`/src/geekos/kthread.c`
**功能**：创建并启动一个新的内核线程。

```c
/* Start_Kernel_Thread: 创建并启动内核线程
 * @startFunc: 线程入口函数
 * @arg: 传递给线程的参数
 * @priority: 线程优先级
 * @detached: 是否分离线程
 * 返回：成功返回线程指针，失败返回NULL
 */
struct Kernel_Thread* Start_Kernel_Thread(
    Thread_Start_Func startFunc,  /* 线程入口函数类型：void func(ulong_t) */
    ulong_t arg,                   /* 传递给线程的参数 */
    int priority,                  /* 线程优先级 */
    bool detached)                 /* 分离标志 */
{
    struct Kernel_Thread* kthread;
    
    /* 1. 创建线程结构 */
    kthread = Create_Thread(priority, detached);
    if (kthread == 0) {
        return 0;  /* 内存分配失败 */
    }
    
    /* 2. 设置线程执行上下文 */
    Setup_Kernel_Thread(kthread, startFunc, arg);
    
    /* 3. 将线程加入就绪队列 */
    Make_Runnable_Atomic(kthread);
    
    return kthread;
}
```

### Create_Thread() 函数详解
**位置**：`/src/geekos/kthread.c`
**功能**：为新线程分配内存资源。

```c
/* Create_Thread: 创建线程并分配资源
 * @priority: 线程优先级
 * @detached: 是否分离线程
 */
static struct Kernel_Thread* Create_Thread(int priority, bool detached)
{
    struct Kernel_Thread* kthread;
    void* stackPage;
    
    /* 分配线程控制块内存（1页） */
    kthread = Alloc_Page();
    if (kthread == 0) {
        return 0;
    }
    
    /* 分配线程堆栈内存（1页） */
    stackPage = Alloc_Page();
    if (stackPage == 0) {
        Free_Page(kthread);  /* 失败时释放线程控制块 */
        return 0;
    }
    
    /* 初始化线程结构 */
    Init_Thread(kthread, stackPage, priority, detached);
    
    /* 将线程加入全局线程列表 */
    Add_To_Back_Of_All_Thread_List(&s_allThreadList, kthread);
    
    return kthread;
}
```

### Setup_Kernel_Thread() 函数详解
**位置**：`/src/geekos/kthread.c`
**功能**：设置线程的初始执行上下文。

```c
/* Setup_Kernel_Thread: 设置线程执行上下文
 * @kthread: 线程控制块
 * @startFunc: 线程入口函数
 * @arg: 传递给线程的参数
 */
static void Setup_Kernel_Thread(struct Kernel_Thread* kthread,
                               Thread_Start_Func startFunc,
                               ulong_t arg)
{
    /* 1. 将参数压入线程堆栈 */
    Push(kthread, arg);             /* 线程参数 */
    Push(kthread, (ulong_t)&Shutdown_Thread); /* 返回地址（线程退出函数） */
    Push(kthread, (ulong_t)startFunc); /* 入口函数地址 */
    
    /* 2. 设置中断返回上下文（模拟中断返回帧） */
    Push(kthread, 0);              /* EFLAGS（初始为0） */
    Push(kthread, KERNEL_CS);      /* 代码段选择子 */
    Push(kthread, (ulong_t)&Launch_Thread); /* 入口点（Launch_Thread函数） */
    
    /* 3. 压入伪中断信息 */
    Push(kthread, 0);              /* 错误代码 */
    Push(kthread, 0);              /* 中断号 */
    
    /* 4. 保存通用寄存器（初始为0） */
    Push_General_Registers(kthread);
    
    /* 5. 设置段寄存器 */
    Push(kthread, KERNEL_DS);      /* DS */
    Push(kthread, KERNEL_DS);      /* ES */
    Push(kthread, 0);              /* FS */
    Push(kthread, 0);              /* GS */
}
```

## ⏱️ 6.4 线程调度算法

### Schedule() 函数详解
**位置**：`/src/geekos/kthread.c`
**功能**：执行线程调度，选择下一个要运行的线程。

```c
/* Schedule: 线程调度函数 */
void Schedule(void)
{
    struct Kernel_Thread* runnable;
    
    /* 检查调度条件：必须禁用中断且允许抢占 */
    KASSERT(!Interrupts_Enabled());
    KASSERT(!g_preemptionDisabled);
    
    /* 从就绪队列中选择最佳线程 */
    runnable = Get_Next_Runnable();
    KASSERT(runnable != 0);
    
    /* 切换到新线程 */
    Switch_To_Thread(runnable);
}
```

### Get_Next_Runnable() 函数详解
**位置**：`/src/geekos/kthread.c`
**功能**：从就绪队列中选择优先级最高的线程。

```c
/* Get_Next_Runnable: 获取下一个可运行线程 */
static struct Kernel_Thread* Get_Next_Runnable(void)
{
    struct Kernel_Thread* best = 0;
    
    /* 遍历就绪队列，选择优先级最高的线程 */
    best = Find_Best(&s_runQueue);
    KASSERT(best != 0);
    
    /* 从就绪队列中移除选中的线程 */
    Remove_Thread(&s_runQueue, best);
    
    return best;
}
```

### Find_Best() 函数详解
**位置**：`/src/geekos/kthread.c`
**功能**：在线程队列中查找优先级最高的线程。

```c
/* Find_Best: 查找优先级最高的线程
 * @queue: 线程队列指针
 * 返回：优先级最高的线程指针
 */
static struct Kernel_Thread* Find_Best(struct Thread_Queue* queue)
{
    struct Kernel_Thread* kthread = queue->head;
    struct Kernel_Thread* best = 0;
    
    /* 遍历队列，选择优先级最高的线程 */
    while (kthread != 0) {
        if (best == 0 || kthread->priority > best->priority) {
            best = kthread;
        }
        kthread = Get_Next_In_Thread_Queue(kthread);
    }
    
    return best;
}
```

## 🔀 6.5 线程上下文切换

### Switch_To_Thread() 函数详解
**位置**：`/src/geekos/lowlevel.asm`
**功能**：执行线程上下文切换的汇编代码。

```nasm
; Switch_To_Thread: 线程上下文切换
; 参数：线程控制块指针（通过栈传递）
align 16
Switch_To_Thread:
    ; 1. 调整栈结构，模拟中断返回帧
    push eax
    mov eax, [esp + 4]        ; 获取返回地址
    mov [esp - 4], eax        ; 移动返回地址到正确位置
    add esp, 8
    pushfd                    ; 压入EFLAGS
    push dword KERNEL_CS      ; 压入代码段选择子
    push dword .here          ; 压入返回地址
    push dword 0              ; 错误代码
    push dword 0              ; 中断号
    
    ; 2. 保存当前寄存器状态
    Save_Registers
    
    ; 3. 保存当前线程ESP
    mov eax, [g_currentThread]
    mov [eax + 0], esp        ; 保存ESP到线程控制块
    
    ; 4. 清空当前线程时间片
    mov [eax + 4], dword 0
    
    ; 5. 切换到新线程
    mov eax, [esp + INTERRUPT_STATE_SIZE] ; 获取新线程指针
    mov [g_currentThread], eax            ; 更新当前线程
    mov esp, [eax + 0]                    ; 加载新线程ESP
    
    ; 6. 恢复新线程寄存器状态
    Restore_Registers
    
    ; 7. 中断返回（切换到新线程）
    iret
    
.here:
    ret
```

### Save_Registers 和 Restore_Registers 宏
**位置**：`/src/geekos/lowlevel.asm`
**功能**：保存和恢复所有通用寄存器。

```nasm
; Save_Registers宏：保存所有通用寄存器到栈中
%macro Save_Registers 0
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp
    push ds
    push es
    push fs
    push gs
%endmacro

; Restore_Registers宏：从栈中恢复所有通用寄存器  
%macro Restore_Registers 0
    pop gs
    pop fs
    pop es
    pop ds
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    add esp, 8                ; 跳过错误代码和中断号
%endmacro
```

## 🧹 6.6 线程终止与清理

### Exit() 函数详解
**位置**：`/src/geekos/kthread.c`
**功能**：终止当前线程的执行。

```c
/* Exit: 终止当前线程
 * @exitCode: 线程退出代码
 */
void Exit(int exitCode)
{
    struct Kernel_Thread* current = g_currentThread;
    
    /* 确保中断已禁用 */
    if (Interrupts_Enabled()) {
        Disable_Interrupts();
    }
    
    /* 设置线程终止状态 */
    current->exitCode = exitCode;
    current->alive = false;
    
    /* 释放线程本地存储 */
    Tlocal_Exit(current);
    
    /* 唤醒所有等待此线程的线程 */
    Wake_Up(&current->joinQueue);
    
    /* 减少引用计数，可能触发线程回收 */
    Detach_Thread(current);
    
    /* 调度新线程运行 */
    Schedule();
    
    /* 不会执行到这里 */
    KASSERT(false);
}
```

### Shutdown_Thread() 函数详解
**位置**：`/src/geekos/kthread.c`
**功能**：线程退出时执行的清理函数。

```c
/* Shutdown_Thread: 线程关闭处理函数
 * 这是线程执行的最后一段代码，负责最终清理工作
 */
static void Shutdown_Thread(ulong_t arg)
{
    /* 线程退出，减少引用计数 */
    Detach_Thread(g_currentThread);
    
    /* 永久挂起，等待回收线程处理 */
    for (;;) {
        Disable_Interrupts();
        Wait(&s_reaperWaitQueue);
    }
}
```

### Reaper线程详解
**位置**：`/src/geekos/kthread.c`
**功能**：回收已终止线程的资源。

```c
/* Reaper: 线程回收器
 * @arg: 未使用参数
 * 功能：无限循环回收已终止的线程资源
 */
static void Reaper(ulong_t arg)
{
    struct Kernel_Thread* kthread;
    
    Disable_Interrupts();
    
    for (;;) {
        /* 检查是否有线程需要回收 */
        if ((kthread = s_graveyardQueue.head) == 0) {
            /* 无线程可回收，等待新任务 */
            Wait(&s_reaperWaitQueue);
            continue;
        }
        
        /* 清空回收队列 */
        Clear_Thread_Queue(&s_graveyardQueue);
        
        /* 重新启用中断，允许其他线程运行 */
        Enable_Interrupts();
        
        /* 回收所有死亡线程的资源 */
        while (kthread != 0) {
            struct Kernel_Thread* next = Get_Next_In_Thread_Queue(kthread);
            Destroy_Thread(kthread);  /* 销毁线程 */
            kthread = next;
        }
        
        /* 准备下一次循环 */
        Disable_Interrupts();
    }
}
```

## 💡 6.7 实践指导与调试技巧

### 线程调试技巧
1. **线程状态检查**：
   ```c
   /* 调试函数：打印线程状态 */
   void Debug_Thread_State(struct Kernel_Thread* thread)
   {
       Print("Thread %d: ESP=%x, Priority=%d, Ticks=%d, Alive=%d\n",
             thread->pid, thread->esp, thread->priority, 
             thread->numTicks, thread->alive);
   }
   ```

2. **就绪队列监控**：
   ```c
   /* 检查就绪队列状态 */
   void Check_Run_Queue(void)
   {
       int count = 0;
       struct Kernel_Thread* thread = s_runQueue.head;
       
       while (thread != 0) {
           count++;
           thread = Get_Next_In_Thread_Queue(thread);
       }
       
       Print("Run queue has %d threads\n", count);
   }
   ```

### 常见问题解决
1. **线程调度失败**：
   - 症状：系统挂起或无响应
   - 解决：检查就绪队列是否为空，确保空闲线程正确配置

2. **栈溢出问题**：
   - 症状：随机崩溃或数据损坏
   - 解决：增加线程栈大小或检查递归深度

3. **优先级反转**：
   - 症状：高优先级线程无法及时运行
   - 解决：实现优先级继承或优先级天花板协议

## 📊 调度算法总结

### GeekOS调度策略
1. **优先级调度**：线程按优先级排序，高优先级线程优先运行
2. **时间片轮转**：同优先级线程按时间片轮流执行
3. **抢占式调度**：高优先级线程可抢占低优先级线程的CPU时间

### 调度相关全局变量
```c
/* 调度器全局状态变量 */
struct Thread_Queue s_runQueue;      /* 就绪队列 */
struct Thread_Queue s_graveyardQueue; /* 回收队列 */
struct Thread_Queue s_reaperWaitQueue; /* 回收器等待队列 */
int g_preemptionDisabled = 0;        /* 抢占禁用标志 */
int g_needReschedule = 0;            /* 重新调度标志 */
```

## 🎯 学习重点总结

### 核心知识点
1. **线程生命周期**：创建、就绪、运行、阻塞、终止的全过程管理
2. **上下文切换**：保存和恢复线程执行上下文的技术细节
3. **调度算法**：优先级调度和时间片轮转的实现原理
4. **同步机制**：线程间的等待和唤醒操作
5. **资源管理**：线程资源的分配和回收策略

### 实践技能
1. **线程创建**：能够创建和启动新的内核线程
2. **调度调试**：诊断和解决线程调度相关问题
3. **性能优化**：优化线程调度策略以提高系统效率
4. **并发编程**：编写线程安全的内核代码

### 进一步学习方向
1. **多处理器调度**：学习SMP环境下的负载均衡和亲和性调度
2. **实时调度**：研究硬实时系统的调度算法（如RM、EDF）
3. **虚拟化调度**：了解虚拟化环境中的CPU时间分配策略
4. **能源感知调度**：探索移动设备的节能调度算法

通过本章的深入学习，您已经掌握了GeekOS线程管理和调度的核心机制。这些知识是理解现代操作系统并发管理的基础，为进一步学习多线程编程、同步原语和性能优化奠定了坚实基础。建议通过实际代码实验来巩固理解，例如修改调度算法或添加新的线程同步机制。