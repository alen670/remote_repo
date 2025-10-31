# 第七章 系统调用与用户接口详解

## 📖 章节概述
第七章深入探讨了GeekOS的系统调用机制和用户程序接口。系统调用是用户程序与操作系统内核之间的桥梁，允许用户程序安全地访问内核功能。通过本章学习，您将掌握系统调用的实现原理、参数传递机制、用户空间与内核空间的切换过程，以及如何在GeekOS中添加新的系统调用。这些知识是理解操作系统保护机制和用户态程序开发的关键。

## ⚙️ 7.1 系统调用基础概念

### 系统调用定义与作用
系统调用是操作系统提供给用户程序的编程接口，允许用户程序请求内核服务。在GeekOS中，系统调用通过软中断实现，为用户程序提供文件操作、进程管理、内存分配等核心功能。

### 系统调用与普通函数调用的区别
| 特性 | 普通函数调用 | 系统调用 |
|------|-------------|----------|
| 执行环境 | 用户空间 | 内核空间 |
| 特权级别 | 用户特权级(3) | 内核特权级(0) |
| 实现方式 | 直接跳转 | 软中断(INT指令) |
| 性能开销 | 低 | 较高（需要上下文切换） |
| 安全性 | 无特权检查 | 严格的参数和权限验证 |

## 🔧 7.2 系统调用机制实现

### 系统调用中断设置
**位置**: `/src/geekos/syscall.c`
**功能**: 设置系统调用中断门，允许用户程序触发系统调用。

```c
/* Init_Syscalls: 初始化系统调用机制 */
void Init_Syscalls(void)
{
    /* 安装系统调用中断处理程序 */
    Install_Interrupt_Handler(SYSCALL_INT, Syscall_Handler);
    
    Print("System calls initialized (int 0x%x)\n", SYSCALL_INT);
}
```

**关键常量定义**:
```c
#define SYSCALL_INT 0x90  /* 系统调用中断号（144） */
#define MAX_SYSCALLS 256  /* 最大系统调用数量 */
```

### 系统调用门描述符配置
系统调用门需要特殊配置，允许用户态程序调用内核服务：

```c
/* 配置系统调用门描述符（在IDT初始化时调用） */
void Init_Syscall_Gate(union IDT_Descriptor* desc, ulong_t addr)
{
    desc->ig.offsetLow = addr & 0xFFFF;
    desc->ig.segmentSelector = KERNEL_CS;
    desc->ig.reserved = 0;
    desc->ig.signature = 0x7E;    /* 类型：32位中断门，DPL=3 */
    desc->ig.dpl = 3;             /* 描述符特权级=3，允许用户程序调用 */
    desc->ig.present = 1;
    desc->ig.offsetHigh = addr >> 16;
}
```

**DPL（描述符特权级）说明**:
- **DPL=0**: 只允许内核态调用
- **DPL=3**: 允许用户态调用
- 系统调用门必须设置为DPL=3，否则用户程序无法触发系统调用

## 🔄 7.3 系统调用处理流程

### Syscall_Handler() 函数详解
**位置**: `/src/geekos/syscall.c`
**功能**: 系统调用总入口点，负责分发系统调用请求。

```c
/* Syscall_Handler: 系统调用中断处理函数
 * @state: 中断发生时保存的寄存器状态
 */
static void Syscall_Handler(struct Interrupt_State* state)
{
    uint_t syscallNum;
    int result;
    
    /* 1. 从eax寄存器获取系统调用号 */
    syscallNum = state->eax;
    
    /* 2. 验证系统调用号有效性 */
    if (syscallNum >= MAX_SYSCALLS) {
        Print("Invalid system call number: %d\n", syscallNum);
        state->eax = -1;  /* 返回错误码 */
        return;
    }
    
    /* 3. 检查系统调用处理函数是否已安装 */
    if (g_syscallTable[syscallNum] == NULL) {
        Print("Unimplemented system call: %d\n", syscallNum);
        state->eax = -1;  /* 返回错误码 */
        return;
    }
    
    /* 4. 调用对应的系统调用处理函数 */
    result = g_syscallTablestate;
    
    /* 5. 将返回值存入eax寄存器 */
    state->eax = result;
}
```

### 系统调用表定义
**位置**: `/src/geekos/syscall.c`
**功能**: 系统调用处理函数的跳转表。

```c
/* 系统调用处理函数类型定义 */
typedef int (*Syscall_Handler)(struct Interrupt_State* state);

/* 系统调用表：256个处理函数指针 */
static Syscall_Handler g_syscallTable[MAX_SYSCALLS] = {
    [0 ... MAX_SYSCALLS-1] = NULL  /* 初始化为空 */
};

/* 系统调用号常量定义 */
#define SYS_EXIT        1    /* 进程退出 */
#define SYS_PRINTSTRING 2    /* 打印字符串 */
#define SYS_GETKEY      3    /* 获取键盘输入 */
#define SYS_SPAWN       4    /* 创建新进程 */
#define SYS_WAIT        5    /* 等待子进程 */
/* ... 其他系统调用号 */
```

## 🛠️ 7.4 具体系统调用实现

### Sys_Exit() 系统调用
**功能**: 终止当前用户进程。

```c
/* Sys_Exit: 进程退出系统调用
 * @state: 中断状态，包含退出码在ebx寄存器
 */
static int Sys_Exit(struct Interrupt_State* state)
{
    int exitCode = state->ebx;  /* 退出码从ebx寄存器获取 */
    
    /* 调用内核线程退出函数 */
    Exit(exitCode);
    
    /* 不会返回到这里 */
    return 0;
}
```

### Sys_PrintString() 系统调用
**功能**: 在控制台输出字符串。

```c
/* Sys_PrintString: 打印字符串系统调用
 * @state: 中断状态，ebx=字符串地址，ecx=字符串长度
 */
static int Sys_PrintString(struct Interrupt_State* state)
{
    ulong_t userBufAddr = state->ebx;  /* 用户空间字符串地址 */
    ulong_t length = state->ecx;        /* 字符串长度 */
    char* kernelBuffer;
    int result = 0;
    
    /* 1. 验证参数有效性 */
    if (length == 0) {
        return 0;  /* 空字符串，直接返回成功 */
    }
    
    if (length > MAX_STRING_LENGTH) {
        return -1;  /* 字符串过长 */
    }
    
    /* 2. 分配内核缓冲区 */
    kernelBuffer = (char*) Malloc(length + 1);
    if (kernelBuffer == NULL) {
        return -1;  /* 内存分配失败 */
    }
    
    /* 3. 从用户空间复制字符串到内核空间 */
    if (!Copy_From_User(kernelBuffer, userBufAddr, length)) {
        Free(kernelBuffer);
        return -1;  /* 复制失败 */
    }
    
    kernelBuffer[length] = '\0';  /* 添加字符串结束符 */
    
    /* 4. 调用内核打印函数 */
    Print("%s", kernelBuffer);
    
    /* 5. 释放缓冲区并返回 */
    Free(kernelBuffer);
    return result;
}
```

### Sys_Spawn() 系统调用
**功能**: 创建新的用户进程。

```c
/* Sys_Spawn: 创建进程系统调用
 * @state: 中断状态，ebx=程序路径，ecx=路径长度，edx=命令行参数，esi=参数长度
 */
static int Sys_Spawn(struct Interrupt_State* state)
{
    char* programPath = NULL;
    char* commandLine = NULL;
    struct Kernel_Thread* newThread = NULL;
    int result = 0;
    
    /* 1. 从用户空间复制程序路径 */
    result = Copy_User_String(state->ebx, state->ecx, 
                             VFS_MAX_PATH_LENGTH, &programPath);
    if (result != 0) {
        goto done;
    }
    
    /* 2. 从用户空间复制命令行参数 */
    result = Copy_User_String(state->edx, state->esi, 
                             MAX_COMMAND_LENGTH, &commandLine);
    if (result != 0) {
        goto done;
    }
    
    /* 3. 启用中断（Spawn函数可能需要阻塞） */
    Enable_Interrupts();
    
    /* 4. 创建新进程 */
    result = Spawn(programPath, commandLine, &newThread);
    
    /* 5. 禁用中断（返回内核态需要关中断） */
    Disable_Interrupts();
    
    if (result == 0) {
        KASSERT(newThread != NULL);
        result = newThread->pid;  /* 返回新进程的PID */
    }
    
done:
    /* 6. 清理资源 */
    if (programPath != NULL) {
        Free(programPath);
    }
    if (commandLine != NULL) {
        Free(commandLine);
    }
    
    return result;
}
```

## 🔒 7.5 用户空间与内核空间数据传递

### Copy_From_User() 函数详解
**位置**: `/src/geekos/uservm.c`
**功能**: 安全地从用户空间复制数据到内核空间。

```c
/* Copy_From_User: 从用户空间复制数据
 * @destInKernel: 内核目标缓冲区
 * @srcInUser: 用户空间源地址
 * @numBytes: 要复制的字节数
 * 返回: true=成功, false=失败
 */
bool Copy_From_User(void* destInKernel, ulong_t srcInUser, ulong_t numBytes)
{
    struct User_Context* userContext = g_currentThread->userContext;
    ulong_t userVirtAddr;
    ulong_t kernelVirtAddr;
    ulong_t bytesCopied = 0;
    
    /* 1. 验证用户地址有效性 */
    if (!Validate_User_Address(userContext, srcInUser, numBytes)) {
        return false;
    }
    
    /* 2. 将用户虚拟地址转换为内核可访问地址 */
    userVirtAddr = srcInUser + USER_VM_START;
    
    /* 3. 逐页复制数据（处理跨页边界情况） */
    while (bytesCopied < numBytes) {
        ulong_t pageOffset = userVirtAddr & (PAGE_SIZE - 1);
        ulong_t bytesThisPage = PAGE_SIZE - pageOffset;
        ulong_t bytesToCopy = numBytes - bytesCopied;
        
        if (bytesToCopy > bytesThisPage) {
            bytesToCopy = bytesThisPage;
        }
        
        /* 4. 验证当前页可访问 */
        if (!Validate_User_Page(userContext, userVirtAddr, bytesToCopy)) {
            return false;
        }
        
        /* 5. 计算内核虚拟地址 */
        kernelVirtAddr = User_To_Kernel(userContext, userVirtAddr);
        
        /* 6. 执行内存复制 */
        memcpy((char*)destInKernel + bytesCopied, 
               (char*)kernelVirtAddr, bytesToCopy);
        
        bytesCopied += bytesToCopy;
        userVirtAddr += bytesToCopy;
    }
    
    return true;
}
```

### Copy_User_String() 函数详解
**位置**: `/src/geekos/syscall.c`
**功能**: 专门用于复制用户空间字符串的辅助函数。

```c
/* Copy_User_String: 复制用户空间字符串
 * @uaddr: 用户空间字符串地址
 * @len: 字符串长度
 * @maxLen: 最大允许长度（防止缓冲区溢出）
 * @pStr: 输出参数，指向分配的内核缓冲区
 * 返回: 0=成功, 错误码=失败
 */
static int Copy_User_String(ulong_t uaddr, ulong_t len, ulong_t maxLen, char** pStr)
{
    char* str = NULL;
    int result = 0;
    
    /* 1. 验证字符串长度 */
    if (len > maxLen) {
        result = EINVALID;  /* 无效参数 */
        goto done;
    }
    
    /* 2. 分配内核缓冲区 */
    str = (char*) Malloc(len + 1);
    if (str == NULL) {
        result = ENOMEM;  /* 内存不足 */
        goto done;
    }
    
    /* 3. 从用户空间复制数据 */
    if (!Copy_From_User(str,