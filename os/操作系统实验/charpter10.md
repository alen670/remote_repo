# 第十章 进程调度优化 - 详细解析与代码注释

## 10.1 项目3设计要求

第十章重点介绍了GeekOS的进程调度优化，主要目标是在原有调度机制基础上增加多级反馈队列调度算法（MLFQ），使系统能够在时间片轮转调度和四级反馈队列调度之间动态切换。

**核心要求**：
1. 实现`Sys_SetSchedulingPolicy`系统调用，允许用户设置调度策略（0为时间片轮转，1为MLFQ）
2. 实现`Sys_GetTimeOfDay`系统调用，获取系统时钟滴答数
3. 修改`kthread.c`中的调度相关函数，支持多级队列调度

## 10.2 多级反馈队列调度原理

### 10.2.1 多级反馈队列基本概念

多级反馈队列调度（MLFQ）是一种自适应调度算法，通过动态调整进程优先级来平衡响应时间和吞吐量。GeekOS中实现了四级反馈队列，优先级从0（最高）到3（最低）。




### 10.2.2 MLFQ算法规则

1. **优先级分配**：新进程进入最高优先级队列（队列0）
2. **时间片分配**：高优先级队列分配较短时间片，低优先级队列分配较长时间片
3. **优先级调整**：
   - 进程用完时间片未结束，降低优先级（移到下一级队列）
   - 进程阻塞后重新就绪，提升优先级（根据阻塞时间）
4. **饥饿预防**：定期提升长时间等待进程的优先级

### 10.2.3 GeekOS中的四级队列实现

在GeekOS中，使用四个就绪队列代替原有的单一队列：

```c
// 在kthread.c中定义四个就绪队列
static struct Thread_Queue s_runQueue[4];  // 优先级0到3的队列

// 队列优先级定义
#define PRIORITY_HIGHEST   0  // 最高优先级队列
#define PRIORITY_HIGH      1
#define PRIORITY_NORMAL    2
#define PRIORITY_LOW       3  // 最低优先级队列
#define PRIORITY_IDLE      3  // Idle线程在此队列
```




## 10.3 关键代码实现详解

### 10.3.1 调度策略设置函数

```c
// syscall.c中的系统调用实现
static int Sys_SetSchedulingPolicy(struct Interrupt_State* state) {
    int policy = state->ebx;      // 策略参数：0=RR, 1=MLFQ
    int quantum = state->ecx;     // 时间片长度
    
    // 参数验证
    if (policy != 0 && policy != 1) {
        return -1;  // 无效策略
    }
    
    // 调用策略切换函数
    Change_Scheduling_Policy(policy, quantum);
    return 0;
}

static int Sys_GetTimeOfDay(struct Interrupt_State* state) {
    return g_numTicks;  // 返回系统时钟滴答数
}
```

### 10.3.2 调度策略切换函数

```c
// kthread.c中的策略切换实现
int Change_Scheduling_Policy(int policy, int quantum) {
    struct Kernel_Thread* thread, *ithread;
    int i;
    
    g_Quantum = quantum;  // 设置全局时间片长度
    
    if (policy == 0) {  // 切换到时间片轮转
        if (g_SchedPolicy == 0) return 0;  // 已经是RR
        
        // 将MLFQ队列中的所有线程合并到RR队列
        for (i = 1; i < MAX_QUEUE_LEVEL; ++i) {
            thread = Get_Front_Of_Thread_Queue(&s_runQueue[i]);
            while (thread != 0) {
                ithread = Get_Next_In_Thread_Queue(thread);
                thread->currentReadyQueue = 0;  // 重置队列级别
                Remove_From_Thread_Queue(&s_runQueue[i], thread);
                Add_To_Back_Of_Thread_Queue(&s_runQueue[0], thread);
                thread = ithread;
            }
        }
        g_SchedPolicy = 0;  // 更新策略标志
    } 
    else if (policy == 1) {  // 切换到MLFQ
        if (g_SchedPolicy == 1) return 0;  // 已经是MLFQ
        
        // 将Idle线程移到最低优先级队列
        thread = Get_Front_Of_Thread_Queue(&s_runQueue[0]);
        while (thread != 0) {
            if (thread->priority == PRIORITY_IDLE) break;
            thread = Get_Next_In_Thread_Queue(thread);
        }
        
        if (thread != 0) {
            thread->currentReadyQueue = 3;  // 移到队列3
            Remove_From_Thread_Queue(&s_runQueue[0], thread);
            Add_To_Front_Of_Thread_Queue(&s_runQueue[3], thread);
        }
        
        g_SchedPolicy = 1;  // 更新策略标志
    }
    
    return 0;
}
```

### 10.3.3 多级队列调度函数

```c
// kthread.c中的调度核心函数
struct Kernel_Thread* Get_Next_Runnable(void) {
    struct Kernel_Thread* best = 0;
    int i;
    
    // 从最高优先级队列开始查找
    for (i = 0; i < MAX_QUEUE_LEVEL; ++i) {
        if (!Is_Thread_Queue_Empty(&s_runQueue[i])) {
            // 在当前队列中查找最佳线程
            best = Find_Best(&s_runQueue[i]);
            if (best != 0) {
                Remove_Thread(&s_runQueue[i], best);
                break;
            }
        }
    }
    
    KASSERT(best != 0);  // 确保找到可运行线程
    return best;
}

static struct Kernel_Thread* Find_Best(struct Thread_Queue* queue) {
    struct Kernel_Thread* kthread = queue->head;
    struct Kernel_Thread* best = 0;
    
    // 遍历队列寻找优先级最高的线程
    while (kthread != 0) {
        if (best == 0 || kthread->priority > best->priority) {
            best = kthread;
        }
        kthread = Get_Next_In_Thread_Queue(kthread);
    }
    
    return best;
}
```

### 10.3.4 时间片处理与优先级调整

```c
// timer.c中的时钟中断处理
static void Timer_Interrupt_Handler(struct Interrupt_State* state) {
    struct Kernel_Thread* current = g_currentThread;
    
    Begin_IRQ(state);
    
    ++g_numTicks;           // 全局时钟计数
    ++current->numTicks;    // 线程时间片计数
    
    // 检查时间片是否用完
    if (current->numTicks >= g_Quantum) {
        current->numTicks = 0;  // 重置时间片计数
        
        if (g_SchedPolicy == 1) {  // MLFQ调度
            // 降低优先级（除非已在最低队列）
            if (current->currentReadyQueue < MAX_QUEUE_LEVEL - 1) {
                current->currentReadyQueue++;
            }
        }
        
        g_needReschedule = true;  // 设置重调度标志
    }
    
    End_IRQ(state);
}
```

### 10.3.5 阻塞唤醒时的优先级提升

```c
// kthread.c中的唤醒函数
void Wake_Up(struct Thread_Queue* waitQueue) {
    struct Kernel_Thread* kthread = waitQueue->head;
    struct Kernel_Thread* next;
    
    KASSERT(!Interrupts_Enabled());
    
    while (kthread != 0) {
        next = Get_Next_In_Thread_Queue(kthread);
        
        if (g_SchedPolicy == 1) {  // MLFQ调度
            // 根据阻塞时间提升优先级
            if (kthread->blockedTicks > BLOCK_THRESHOLD) {
                kthread->currentReadyQueue = 0;  // 提升到最高队列
            } else if (kthread->currentReadyQueue > 0) {
                kthread->currentReadyQueue--;  // 提升一级
            }
        }
        
        Make_Runnable(kthread);  // 放入就绪队列
        kthread = next;
    }
    
    Clear_Thread_Queue(waitQueue);
}
```

## 10.4 调度算法测试

### 10.4.1 测试程序介绍

GeekOS提供了三个测试程序来验证调度算法：
- `long`：模拟长作业，消耗大量CPU时间
- `short`：模拟短作业，快速执行完成
- `workload`：主测试程序，创建并管理测试作业

### 10.4.2 测试用例示例

```c
// workload.c中的测试逻辑
int main(int argc, char** argv) {
    int policy = (strcmp(argv[1], "rr") == 0) ? 0 : 1;
    int quantum = atoi(argv[2]);
    
    // 设置调度策略
    Set_Scheduling_Policy(policy, quantum);
    
    // 创建测试作业
    for (int i = 0; i < NUM_SHORT_JOBS; i++) {
        Spawn("/c/short.exe", "short", 0);
    }
    
    // 创建长作业
    Spawn("/c/long.exe", "long", 0);
    
    // 等待所有作业完成
    for (int i = 0; i <= NUM_SHORT_JOBS; i++) {
        int pid = Wait(0);
        Print("Process %d finished\n", pid);
    }
    
    return 0;
}
```

### 10.4.3 测试结果分析

**时间片轮转调度（RR）**：
- 所有进程平等分享CPU时间
- 响应时间均匀但可能较长
- 适合交互式应用

**多级反馈队列调度（MLFQ）**：
- 短作业优先获得CPU时间
- 长作业逐渐降低优先级
- 平衡响应时间和吞吐量




## 10.5 系统调用机制深入解析

### 10.5.1 系统调用执行流程

系统调用通过软中断实现，用户程序调用`int 0x90`触发系统调用：

```c
// 用户空间系统调用封装
DEF_SYSCALL(Set_Scheduling_Policy, SYS_SETSCHEDULINGPOLICY, int, 
            (int policy, int quantum), 
            int arg0 = policy; int arg1 = quantum;, SYSCALL_REGS_2)
```

展开后的汇编代码：
```asm
mov eax, SYS_SETSCHEDULINGPOLICY  ; 系统调用号
mov ebx, policy                   ; 第一个参数
mov ecx, quantum                  ; 第二个参数
int 0x90                          ; 触发系统调用
```

### 10.5.2 系统调用处理程序

```c
// trap.c中的系统调用处理
static void Syscall_Handler(struct Interrupt_State* state) {
    uint_t syscallNum = state->eax;  // 从eax获取系统调用号
    
    // 验证系统调用号合法性
    if (syscallNum < 0 || syscallNum >= g_numSyscalls) {
        Print("Illegal system call %d by process %d\n", 
              syscallNum, g_currentThread->pid);
        Exit(-1);
    }
    
    // 调用对应的系统调用处理函数
    state->eax = g_syscallTablestate;
}
```




## 10.6 知识点总结与关键问题

### 10.6.1 核心概念

1. **调度算法目标**：在响应时间和吞吐量之间取得平衡
2. **多级反馈队列优势**：自适应调整优先级，兼顾短作业和长作业
3. **时间片设计**：不同优先级队列分配不同长度时间片
4. **优先级提升机制**：防止低优先级进程饥饿

### 10.6.2 关键技术点

1. **队列管理**：维护四个就绪队列，实现优先级调度
2. **策略切换**：运行时动态切换调度算法
3. **时间片处理**：时钟中断驱动的时间片计数和优先级调整
4. **系统调用接口**：为用户程序提供调度策略控制能力

### 10.6.3 常见问题解答

**Q: 为什么需要多级反馈队列？**
A: 单纯优先级调度会导致低优先级进程饥饿，MLFQ通过动态调整优先级平衡各种类型作业的需求。

**Q: 时间片长度如何影响性能？**
A: 较短时间片提高响应速度但增加上下文切换开销，较长时间片减少开销但降低响应性。MLFQ在不同队列使用不同时间片长度优化整体性能。

**Q: 系统调用如何保证安全性？**
A: 通过中断门机制实现用户态到内核态的切换，内核验证参数合法性后再执行相应操作。

通过本章学习，可以深入理解操作系统中进程调度的核心原理和实现技术，为后续学习更高级的调度算法打下坚实基础。