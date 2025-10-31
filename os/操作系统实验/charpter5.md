# 第五章 中断系统与中断处理详解

## 📖 章节概述
第五章深入探讨了GeekOS的中断系统，包括中断描述符表（IDT）的初始化、中断处理程序的安装以及中断处理的全流程。中断是操作系统响应硬件事件和软件异常的核心机制，理解中断处理对于操作系统开发至关重要。通过本章学习，您将掌握x86架构的中断处理原理、IDT的结构与配置方法，以及如何编写安全的中断处理程序。

## ⚙️ 5.1 中断系统初始化

### Init_Interrupts() 函数详解
**位置**: `/src/geekos/int.c`
**功能**: 初始化整个中断系统，包括IDT的设置和默认中断处理程序的安装。

```c
/* Init_Interrupts: 初始化中断系统 */
void Init_Interrupts(void)
{
    int i;
    
    /* 1. 初始化中断描述符表（IDT） */
    Init_IDT();
    
    /* 2. 为所有中断向量安装默认处理程序 */
    for (i = 0; i < NUM_IDT_ENTRIES; ++i) {
        Install_Interrupt_Handler(i, Dummy_Interrupt_Handler);
    }
    
    /* 3. 启用中断（设置EFLAGS的IF位） */
    Enable_Interrupts();
}
```

**代码注释**:
- `NUM_IDT_ENTRIES`: IDT中的条目数，通常为256（0-255）
- `Dummy_Interrupt_Handler`: 默认中断处理程序，用于未实现的中断
- `Enable_Interrupts()`: 汇编指令`sti`的封装，开启中断响应

### Init_IDT() 函数详解
**位置**: `/src/geekos/idt.c`
**功能**: 设置中断描述符表，配置每个中断门描述符。

```c
/* Init_IDT: 初始化中断描述符表 */
void Init_IDT(void)
{
    int i;
    ushort_t limitAndBase[3];
    ulong_t idtBaseAddr = (ulong_t) s_IDT;
    
    Print("Initializing IDT...\n");
    
    /* 验证中断处理程序大小一致性 */
    KASSERT(g_handlerSizeErr == g_handlerSizeNoErr);
    KASSERT((&g_entryPointTableEnd - &g_entryPointTableStart) == 
            g_handlerSizeNoErr * NUM_IDT_ENTRIES);
    
    /* 初始化每个IDT条目 */
    for (i = 0, addr = tableBaseAddr; i < NUM_IDT_ENTRIES; ++i) {
        /* 设置特权级：系统调用中断为用户级，其他为内核级 */
        int dpl = (i == SYSCALL_INT) ? USER_PRIVILEGE : KERNEL_PRIVILEGE;
        
        /* 初始化中断门描述符 */
        Init_Interrupt_Gate(&s_IDT[i], addr, dpl);
        
        addr += g_handlerSizeNoErr;  /* 移动到下一个入口点 */
    }
    
    /* 设置IDTR寄存器（界限和基地址） */
    limitAndBase[0] = 8 * NUM_IDT_ENTRIES;  /* 界限：表大小-1 */
    limitAndBase[1] = idtBaseAddr & 0xffff; /* 基地址低16位 */
    limitAndBase[2] = idtBaseAddr >> 16;    /* 基地址高16位 */
    
    Load_IDTR(limitAndBase);  /* 加载IDTR */
}
```

**关键技术点**:
- **IDT结构**: 256个8字节的描述符，每个描述一个中断处理程序
- **特权级设置**: 大多数中断为内核级(0)，系统调用中断为用户级(3)
- **IDTR寄存器**: 48位寄存器，包含IDT的基地址和界限

## 🧩 5.2 中断描述符与中断门

### 中断描述符结构
**位置**: `/include/geekos/idt.h`
**定义**: 中断门描述符的数据结构。

```c
/* 中断门描述符结构 */
struct Interrupt_Gate {
    ushort_t offsetLow;     /* 处理程序地址低16位 */
    ushort_t segmentSelector; /* 代码段选择子（通常为KERNEL_CS） */
    unsigned reserved : 5;   /* 保留位（必须为0） */
    unsigned signature : 8;  /* 类型和属性（0x70 for 32-bit interrupt gate） */
    unsigned dpl : 2;        /* 描述符特权级（0-3） */
    unsigned present : 1;    /* 存在位（1=有效） */
    ushort_t offsetHigh;    /* 处理程序地址高16位 */
};

/* IDT描述符联合体（支持多种门类型） */
union IDT_Descriptor {
    struct Interrupt_Gate ig;  /* 中断门 */
    /* 可能还有其他类型：陷阱门、任务门 */
};
```

**字段详解**:
- `offsetLow/offsetHigh`: 中断处理程序的32位地址
- `segmentSelector`: 目标代码段选择子（通常指向内核代码段）
- `signature`: 0x70表示32位中断门（0b01110000）
- `dpl`: 描述符特权级，控制哪些特权级可以触发此中断
- `present`: 1表示描述符有效，0表示无效

### Init_Interrupt_Gate() 函数
**位置**: `/src/geekos/idt.c`
**功能**: 初始化一个中断门描述符。

```c
/* Init_Interrupt_Gate: 设置中断门描述符
 * @desc: 目标描述符指针
 * @addr: 中断处理程序地址
 * @dpl: 描述符特权级
 */
void Init_Interrupt_Gate(union IDT_Descriptor* desc, ulong_t addr, int dpl)
{
    desc->ig.offsetLow = addr & 0xffff;           /* 地址低16位 */
    desc->ig.segmentSelector = KERNEL_CS;         /* 内核代码段选择子 */
    desc->ig.reserved = 0;                        /* 保留位清零 */
    desc->ig.signature = 0x70;                    /* 32位中断门类型 */
    desc->ig.dpl = dpl;                           /* 设置特权级 */
    desc->ig.present = 1;                         /* 标记为有效 */
    desc->ig.offsetHigh = addr >> 16;             /* 地址高16位 */
}
```

## 🔄 5.3 中断处理流程

### 中断入口表（g_entryPointTable）
**位置**: `/src/geekos/lowlevel.asm`
**功能**: 为每个中断号生成统一的入口点，处理错误码差异。

```nasm
; 中断入口表定义
align 8
g_entryPointTableStart:
; 前18个中断有特定处理（异常和NMI）
Int_No_Err 0        ; 除零错误（无错误码）
align 8
Int_With_Err 8      ; 双故障错误（有错误码）
; ... 其他异常中断

; 剩余中断（18-255）为可编程中断
%assign intNum 18
%rep (256 - 18)     ; 循环生成238个中断入口
Int_No_Err intNum   ; 生成无错误码的中断入口
%assign intNum intNum+1
%endrep

align 8
g_entryPointTableEnd:
```

**宏定义说明**:
```nasm
; Int_No_Err宏：处理无错误码的中断
%macro Int_No_Err 1
align 8
push dword 0         ; 压入伪错误码（0）
push dword %1        ; 压入中断号
jmp Handle_Interrupt ; 跳转到通用处理程序
%endmacro

; Int_With_Err宏：处理有错误码的中断  
%macro Int_With_Err 1
align 8
push dword %1        ; 直接压入中断号（错误码已由CPU压入）
jmp Handle_Interrupt ; 跳转到通用处理程序
%endmacro
```

### 通用中断处理程序（Handle_Interrupt）
**位置**: `/src/geekos/lowlevel.asm`
**功能**: 处理所有中断的通用流程，包括上下文保存和恢复。

```nasm
; Handle_Interrupt: 通用中断处理程序
align 8
Handle_Interrupt:
    ; 1. 保存所有寄存器到栈中
    Save_Registers      ; 宏：push eax, ebx, ecx, edx, esi, edi, ebp, ds, es, fs, gs
    
    ; 2. 设置内核数据段
    mov ax, KERNEL_DS
    mov ds, ax
    mov es, ax
    
    ; 3. 获取中断处理函数地址
    mov eax, g_interruptTable  ; 中断处理函数数组
    mov esi, [esp + REG_SKIP]  ; 从栈中获取中断号（跳过已保存的寄存器）
    mov ebx, [eax + esi * 4]   ; 根据中断号获取处理函数地址
    
    ; 4. 调用中断处理函数（传递栈指针作为参数）
    push esp                 ; 传递Interrupt_State结构指针
    call ebx                 ; 调用C语言处理函数
    add esp, 4               ; 清理参数
    
    ; 5. 检查是否需要重新调度
    cmp [g_preemptionDisabled], dword 0 ; 检查抢占是否禁用
    jne .restore             ; 如果禁用，直接恢复上下文
    
    cmp [g_needReschedule], dword 0 ; 检查是否需要调度
    je .restore              ; 不需要调度，直接恢复
    
    ; 6. 需要调度：当前线程放入就绪队列
    push dword [g_currentThread]
    call Make_Runnable
    add esp, 4
    
    ; 7. 选择新线程运行
    mov eax, [g_currentThread]
    mov [eax + 0], esp       ; 保存当前线程ESP
    mov [eax + 4], dword 0   ; 清空时间片
    
    call Get_Next_Runnable   ; 获取下一个可运行线程
    mov [g_currentThread], eax ; 更新当前线程指针
    mov esp, [eax + 0]       ; 切换到新线程栈
    
    mov [g_needReschedule], dword 0 ; 清除调度标志
    
.restore:
    ; 8. 恢复寄存器并返回
    Restore_Registers     ; 宏：pop所有寄存器并调整栈指针
    iret                  ; 中断返回
```

**关键寄存器说明**:
- `REG_SKIP`: 常量，表示跳过已保存寄存器的偏移量
- `g_interruptTable`: 中断处理函数指针数组，在C代码中定义
- `g_preemptionDisabled`: 全局标志，表示是否允许抢占
- `g_needReschedule`: 全局标志，表示是否需要调度新线程

## 🛠️ 5.4 中断处理程序安装

### Install_Interrupt_Handler() 函数
**位置**: `/src/geekos/int.c`
**功能**: 为特定中断向量安装处理程序。

```c
/* Install_Interrupt_Handler: 安装中断处理程序
 * @interrupt: 中断向量号（0-255）
 * @handler: 处理函数指针
 */
void Install_Interrupt_Handler(int interrupt, Interrupt_Handler handler)
{
    /* 验证中断号有效性 */
    KASSERT(interrupt >= 0 && interrupt < NUM_IDT_ENTRIES);
    
    /* 将处理函数存入全局数组 */
    g_interruptTable[interrupt] = handler;
}
```

**Interrupt_Handler类型定义**:
```c
/* 中断处理函数类型定义
 * @state: 指向中断发生时寄存器状态的指针
 */
typedef void (*Interrupt_Handler)(struct Interrupt_State* state);
```

### Interrupt_State 结构体
**位置**: `/include/geekos/int.h`
**功能**: 保存中断发生时的处理器状态。

```c
/* 中断状态结构体：保存中断时的寄存器状态 */
struct Interrupt_State {
    /* 通用寄存器 */
    uint_t gs;
    uint_t fs;
    uint_t es;
    uint_t ds;
    uint_t ebp;
    uint_t edi;
    uint_t esi;
    uint_t edx;
    uint_t ecx;
    uint_t ebx;
    uint_t eax;
    
    /* 中断相关信息 */
    uint_t intNum;     /* 中断号 */
    uint_t errorCode;  /* 错误码（如果有） */
    
    /* 由CPU自动压入的信息 */
    uint_t eip;        /* 指令指针 */
    uint_t cs;         /* 代码段选择子 */
    uint_t eflags;     /* 标志寄存器 */
};
```

## 🔧 5.5 默认中断处理程序

### Dummy_Interrupt_Handler() 函数
**位置**: `/src/geekos/int.c`
**功能**: 默认中断处理程序，处理未实现的中断。

```c
/* Dummy_Interrupt_Handler: 默认中断处理程序
 * 处理未预期或未实现的中断，打印错误信息并终止当前线程
 */
static void Dummy_Interrupt_Handler(struct Interrupt_State* state)
{
    /* 打印错误信息 */
    Print("*** Unexpected interrupt! ***\n");
    Print("Interrupt number: %d\n", state->intNum);
    
    /* 显示中断状态 */
    Dump_Interrupt_State(state);
    
    /* 终止当前线程 */
    Exit(-1);
}
```

### Dump_Interrupt_State() 函数
**位置**: `/src/geekos/int.c`
**功能**: 显示中断状态信息，用于调试。

```c
/* Dump_Interrupt_State: 显示中断状态
 * @state: 中断状态结构指针
 */
void Dump_Interrupt_State(struct Interrupt_State* state)
{
    Print("EAX=%08x EBX=%08x ECX=%08x EDX=%08x\n",
          state->eax, state->ebx, state->ecx, state->edx);
    Print("ESI=%08x EDI=%08x EBP=%08x ESP=%08x\n",
          state->esi, state->edi, state->ebp, state->esp);
    Print("DS=%04x ES=%04x FS=%04x GS=%04x\n",
          state->ds, state->es, state->fs, state->gs);
    Print("EIP=%08x CS=%04x EFLAGS=%08x\n",
          state->eip, state->cs, state->eflags);
    
    if (state->errorCode != 0) {
        Print("Error code: %08x\n", state->errorCode);
    }
}
```

## 💡 5.6 中断处理实践指南

### 添加自定义中断处理程序
**步骤1: 定义处理函数**
```c
/* 自定义中断处理函数示例 */
static void My_Interrupt_Handler(struct Interrupt_State* state)
{
    /* 处理中断逻辑 */
    Print("Handling interrupt %d\n", state->intNum);
    
    /* 必要时发送EOI信号给PIC */
    if (state->intNum >= 32 && state->intNum < 48) {
        Send_EOI(state->intNum - 32);
    }
}
```

**步骤2: 安装处理程序**
```c
/* 在系统初始化时安装 */
void Init_My_Device(void)
{
    /* 安装IRQ对应的中断处理程序 */
    Install_Interrupt_Handler(32 + MY_IRQ, My_Interrupt_Handler);
    
    /* 启用该中断 */
    Enable_IRQ(MY_IRQ);
}
```

### 中断处理最佳实践
1. **保持简短**: 中断处理程序应尽可能短小，避免长时间关中断
2. **禁用中断**: 在关键操作中使用`Begin_Int_Atomic()`和`End_Int_Atomic()`
3. **发送EOI**: 硬件中断处理后必须发送EOI（End of Interrupt）信号
4. **避免阻塞**: 中断处理程序中不能调用可能阻塞的函数
5. **线程化处理**: 将耗时操作推迟到内核线程中执行

## 📊 中断类型总结

### x86架构中断分类
1. **硬件中断**（外部中断）:
   - 可屏蔽中断（INTR）：通过PIC管理，可被EFLAGS.IF屏蔽
   - 不可屏蔽中断（NMI）：用于严重硬件错误

2. **软件中断**（内部中断）:
   - 异常：处理器执行指令时产生的错误（如除零、页故障）
   - 软件生成中断：INT指令产生（如系统调用）

3. **中断优先级**:
   - NMI > 异常 > 可屏蔽中断 > 软件中断
   - 同级中断按中断向量号排序

### GeekOS中断向量分配
| 向量号范围 | 类型 | 描述 |
|----------|------|------|
| 0-31 | 异常 | 处理器异常（除零、页故障等） |
| 32-47 | 硬件中断 | PIC控制的硬件中断（IRQ0-15） |
| 48-255 | 软件中断 | 用户定义中断和系统调用 |

## 🎯 学习重点总结

### 核心知识点
1. **IDT结构**: 理解中断描述符表的组成和初始化过程
2. **中断门描述符**: 掌握中断门的字段含义和设置方法
3. **中断处理流程**: 从中断发生到处理程序执行的完整路径
4. **上下文保存**: 理解Interrupt_State结构的作用和内容
5. **中断调度**: 中断处理如何触发线程调度

### 实践技能
1. **编写中断处理程序**: 能够为硬件设备编写安全的中断处理代码
2. **安装中断处理程序**: 掌握在系统中注册中断处理程序的方法
3. **调试中断问题**: 使用Dump_Interrupt_State调试中断相关错误
4. **中断安全编程**: 理解原子操作和临界区保护的重要性

### 进一步学习方向
1. **高级中断处理**: 研究中断线程化、中断亲和性等高级主题
2. **多处理器中断**: 学习APIC和SMP环境下的中断处理
3. **实时系统中断**: 探索硬实时系统的中断响应优化
4. **虚拟化中断**: 了解虚拟化环境中的中断虚拟化技术

通过本章的深入学习，您已经掌握了GeekOS中断系统的核心机制。这些知识是理解操作系统响应硬件事件、处理异常和实现系统调用的基础。建议通过实际实验巩固理解，例如为虚拟设备添加中断处理程序或修改中断调度策略。