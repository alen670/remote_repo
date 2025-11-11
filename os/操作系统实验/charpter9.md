# 第九章 运行用户态程序 - 详细解析与代码注释

## 9.1 调试与问题分析

第九章开始就遇到了一个重要的调试案例，展示了操作系统开发中的实际问题解决过程。

### 调试过程分析

```c
// 在Init_Timer()前后添加调试信息
Print("Before Enable_Interrupts\n");
Enable_Interrupts();
Print("After Enable_Interrupts\n");  // 这一行没有执行
```

**问题根源**：在`Enable_Interrupts()`执行后立即发生中断，跳转到`Switch_To_User_Context()`函数，但此时系统还未完全初始化。

**解决方案**：
```c
void Switch_To_User_Context(struct Kernel_Thread* kthread, 
                           struct Interrupt_State* state) {
    if(kthread->userContext == NULL) {
        return;  // 内核线程无需切换用户上下文
    }
    // TODO: 实现完整的用户上下文切换
}
```

## 9.2 用户线程加载流程

### 9.2.1 用户线程与内核线程加载对比





**关键区别**：
- 内核线程：直接使用内核地址空间
- 用户线程：需要独立的地址空间和LDT

## 9.3 项目2核心数据结构

### 9.3.1 User_Context结构体

```c
struct User_Context {
    // LDT相关
    struct Segment_Descriptor ldt[NUM_USER_LDT_ENTRIES];  // 本地描述符表
    struct Segment_Descriptor* ldtDescriptor;           // LDT在GDT中的描述符
    
    // 内存管理
    char* memory;        // 用户内存空间的真实地址
    ulong_t size;        // 用户空间大小
    
    // 段选择子
    ushort_t ldtSelector; // LDT选择子
    ushort_t csSelector;  // 代码段选择子  
    ushort_t dsSelector;  // 数据段选择子
    
    // 执行信息
    ulong_t entryAddr;      // 用户代码入口地址
    ulong_t argBlockAddr;   // 参数块地址
    ulong_t stackPointerAddr; // 栈指针地址
    
    int refCount;        // 引用计数
};
```

### 9.3.2 LDT与GDT关系



## 9.4 关键代码详解

### 9.4.1 用户程序入口（entry.c）

```c
void _Entry(void) {
    struct Argument_Block* argBlock;
    
    // 从esi寄存器获取参数块地址
    _asm__volatile__("movl %%esi, %0" : "=r"(argBlock));
    
    // 调用用户main函数，完成后退出
    Exit(main(argBlock->argc, argBlock->argv));
}
```

**注释说明**：
- `_Entry`是所有用户程序的统一入口点
- 通过内联汇编从esi寄存器获取参数块指针
- 调用用户main函数后通过Exit系统调用结束进程

### 9.4.2 Spawn函数实现

```c
int Spawn(const char* program, const char* command, 
          struct Kernel_Thread** pThread) {
    int rc;
    char* exeFileData = 0;
    ulong_t exeFileLength;
    struct User_Context* userContext = 0;
    struct Kernel_Thread* process = 0;
    struct Exe_Format exeFormat;
    
    // 1. 加载可执行文件
    if((rc = Read_Fully(program, (void**)&exeFileData, &exeFileLength)) != 0 ||
       (rc = Parse_ELF_Executable(exeFileData, exeFileLength, &exeFormat)) != 0 ||
       (rc = Load_User_Program(exeFileData, exeFileLength, &exeFormat, 
                              command, &userContext)) != 0) {
        goto fail;
    }
    
    // 2. 释放文件数据内存
    Free(exeFileData);
    exeFileData = 0;
    
    // 3. 启动用户线程
    process = Start_User_Thread(userContext, false);
    if(process != 0) {
        *pThread = process;
    } else {
        rc = ENOMEM;
    }
    
    return rc;
    
fail:
    // 错误处理
    if(exeFileData != 0) Free(exeFileData);
    if(userContext != 0) Destroy_User_Context(userContext);
    return rc;
}
```

### 9.4.3 Load_User_Program函数

```c
int Load_User_Program(char* exeFileData, ulong_t exeFileLength,
                     struct Exe_Format* exeFormat, const char* command,
                     struct User_Context** pUserContext) {
    ulong_t maxva = 0;
    unsigned numArgs;
    ulong_t argBlockSize;
    ulong_t size, argBlockAddr;
    struct User_Context* userContext = 0;
    int i;
    
    // 1. 计算最大虚拟地址
    for(i = 0; i < exeFormat->numSegments; ++i) {
        struct Exe_Segment* segment = &exeFormat->segmentList[i];
        ulong_t topva = segment->startAddress + segment->sizeInMemory;
        if(topva > maxva) maxva = topva;
    }
    
    // 2. 计算参数块大小
    Get_Argument_Block_Size(command, &numArgs, &argBlockSize);
    
    // 3. 计算总内存需求（代码+栈+参数块）
    size = Round_Up_To_Page(maxva) + DEFAULT_USER_STACK_SIZE;
    argBlockAddr = size;
    size += argBlockSize;
    
    // 4. 创建用户上下文
    userContext = Create_User_Context(size);
    if(userContext == 0) return -1;
    
    // 5. 加载段数据到内存
    for(i = 0; i < exeFormat->numSegments; ++i) {
        struct Exe_Segment* segment = &exeFormat->segmentList[i];
        memcpy(userContext->memory + segment->startAddress,
               exeFileData + segment->offsetInFile,
               segment->lengthInFile);
    }
    
    // 6. 格式化参数块
    Format_Argument_Block(userContext->memory + argBlockAddr, 
                         numArgs, argBlockAddr, command);
    
    // 7. 设置入口点和地址信息
    userContext->entryAddr = exeFormat->entryAddr;
    userContext->argBlockAddr = argBlockAddr;
    userContext->stackPointerAddr = argBlockAddr;  // 栈和参数块地址相同
    
    *pUserContext = userContext;
    return 0;
}
```

### 9.4.4 Create_User_Context函数

```c
static struct User_Context* Create_User_Context(ulong_t size) {
    struct User_Context* context;
    int index;
    
    // 1. 内存页对齐
    size = Round_Up_To_Page(size);
    
    // 2. 分配用户上下文结构
    Disable_Interrupts();
    context = (struct User_Context*) Malloc(sizeof(*context));
    if(context != 0) 
        context->memory = Malloc(size);
    Enable_Interrupts();
    
    if(context == 0 || context->memory == 0) goto fail;
    
    // 3. 初始化用户内存
    memset(context->memory, '\0', size);
    context->size = size;
    
    // 4. 分配LDT描述符
    context->ldtDescriptor = Allocate_Segment_Descriptor();
    if(context->ldtDescriptor == 0) goto fail;
    
    // 5. 初始化LDT描述符
    Init_LDT_Descriptor(context->ldtDescriptor, context->ldt, 
                       NUM_USER_LDT_ENTRIES);
    
    // 6. 设置选择子
    index = Get_Descriptor_Index(context->ldtDescriptor);
    context->ldtSelector = Selector(KERNEL_PRIVILEGE, true, index);
    
    // 7. 初始化LDT中的段描述符
    Init_Code_Segment_Descriptor(&context->ldt[0], 
                                 (ulong_t)context->memory,
                                 size / PAGE_SIZE, 
                                 USER_PRIVILEGE);
                                 
    Init_Data_Segment_Descriptor(&context->ldt[1],
                                 (ulong_t)context->memory, 
                                 size / PAGE_SIZE,
                                 USER_PRIVILEGE);
    
    // 8. 设置段选择子
    context->csSelector = Selector(USER_PRIVILEGE, false, 0);
    context->dsSelector = Selector(USER_PRIVILEGE, false, 1);
    
    context->refCount = 0;
    return context;
    
fail:
    // 错误处理
    Disable_Interrupts();
    if(context != 0) {
        if(context->memory != 0) Free(context->memory);
        Free(context);
    }
    Enable_Interrupts();
    return 0;
}
```

## 9.5 线程调度与上下文切换

### 9.5.1 用户线程调度过程



### 9.5.2 Setup_User_Thread函数

```c
void Setup_User_Thread(struct Kernel_Thread* kthread, 
                      struct User_Context* userContext) {
    // 用户模式下必须开启中断
    ulong_t eflags = EFLAGS_IF;  // 只设置中断标志位
    
    Attach_User_Context(kthread, userContext);
    
    // 模拟用户线程被中断时的栈状态
    
    // 用户模式栈段和指针
    Push(kthread, userContext->dsSelector);        // 用户ss
    Push(kthread, userContext->stackPointerAddr);  // 用户esp
    
    // eflags, cs, eip
    Push(kthread, eflags);
    Push(kthread, userContext->csSelector);
    Push(kthread, userContext->entryAddr);
    
    // 伪错误代码和中断号
    Push(kthread, 0);
    Push(kthread, 0);
    
    // 通用寄存器初始值
    Push(kthread, 0);                    // eax
    Push(kthread, 0);                    // ebx  
    Push(kthread, 0);                    // ecx
    Push(kthread, 0);                    // edx
    Push(kthread, userContext->argBlockAddr); // esi - 参数块地址
    Push(kthread, 0);                    // edi
    Push(kthread, 0);                    // ebp
    
    // 数据段寄存器
    Push(kthread, userContext->dsSelector);  // ds
    Push(kthread, userContext->dsSelector);  // es  
    Push(kthread, userContext->dsSelector);  // fs
    Push(kthread, userContext->dsSelector);  // gs
}
```

## 9.6 系统调用实现

### 9.6.1 系统调用机制

用户程序通过`int 0x90`指令触发系统调用，CPU自动切换到内核态执行系统调用处理程序。

### 9.6.2 系统调用处理流程

```c
static void Syscall_Handler(struct Interrupt_State* state) {
    // 从eax寄存器获取系统调用号
    uint_t syscallNum = state->eax;
    
    // 验证系统调用号合法性
    if(syscallNum < 0 || syscallNum >= g_numSyscalls) {
        Print("Illegal system call %d by process %d\n", 
              syscallNum, g_currentThread->pid);
        Exit(-1);
    }
    
    // 调用对应的系统调用函数，返回值存入eax
    state->eax = g_syscallTablestate;
}
```

### 9.6.3 关键系统调用实现

```c
// 进程退出系统调用
static int Sys_Exit(struct Interrupt_State* state) {
    Exit(state->ebx);  // 退出码从ebx寄存器获取
}

// 字符串输出系统调用  
static int Sys_PrintString(struct Interrupt_State* state) {
    int rc = 0;
    uint_t length = state->ecx;  // 字符串长度
    char* buf = 0;
    
    if(length > 0) {
        // 从用户空间复制字符串到内核
        if((rc = Copy_User_String(state->ebx, length, 1020, &buf)) != 0)
            goto done;
        
        // 输出到控制台
        Put_Buf(buf, length);
    }
    
done:
    if(buf != 0) Free(buf);
    return rc;
}

// 创建进程系统调用
static int Sys_Spawn(struct Interrupt_State* state) {
    int rc;
    char* program = 0;
    char* command = 0;
    struct Kernel_Thread* process;
    
    // 从用户空间复制程序名和命令
    if((rc = Copy_User_String(state->ebx, state->ecx, 
                             VFS_MAX_PATH_LEN, &program)) != 0 ||
       (rc = Copy_User_String(state->edx, state->esi, 
                             1023, &command)) != 0)
        goto done;
    
    Enable_Interrupts();
    // 创建新进程
    rc = Spawn(program, command, &process);
    if(rc == 0) {
        rc = process->pid;  // 返回新进程的PID
    }
    Disable_Interrupts();
    
done:
    if(program != 0) Free(program);
    if(command != 0) Free(command);
    return rc;
}
```

## 9.7 用户线程回收

### 9.7.1 线程回收流程



## 9.8 知识点总结

### 核心概念
1. **用户态与内核态分离**：用户程序运行在受限的环境中
2. **LDT机制**：每个用户进程有自己的段描述符表
3. **系统调用门**：通过`int 0x90`实现用户态到内核态的切换
4. **参数传递**：通过esi寄存器传递参数块指针

### 关键技术点
1. **地址空间隔离**：用户程序使用独立的线性地址空间
2. **特权级保护**：用户程序运行在特权级3，内核运行在特权级0
3. **上下文切换**：通过精心构造的栈帧实现用户线程调度
4. **内存管理**：用户内存空间的分配和释放

### 重要数据结构
- `User_Context`：用户进程的完整上下文信息
- `Argument_Block`：main函数参数传递结构
- `Kernel_Thread`：增加userContext指针支持用户线程

这一章完整展示了从内核线程到用户线程的演进过程，是现代操作系统实现多任务环境的基础。