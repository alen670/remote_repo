# 第三章 GeekOS启动代码分析详解

## 🚀 章节概述
第三章深入分析了GeekOS操作系统的启动过程，重点讲解了`fd_boot.asm`和`setup.asm`两个关键汇编文件。这些代码构成了操作系统从BIOS加载到进入保护模式的完整启动链条，是理解操作系统启动机制的核心内容。



## 💻 3.1 fd_boot.asm - 主引导记录分析

### 3.1.1 代码结构与初始化
`fd_boot.asm`是系统的**第一段执行代码**，位于软盘的0号扇区，被BIOS加载到内存`0x7C00`处执行。其主要功能包括：

```nasm
%include "defs.asm"        ; 引入常量定义文件，如BOOTSEG=0x7C0, INITSEG=0x9000
[BITS 16]                  ; 指定为16位实模式代码
[ORG 0x0]                  ; 基准偏移量为0，因为代码会被加载到0x7C00，但ORG 0x0方便地址计算

BeginText:
    mov ax, BOOTSEG        ; BOOTSEG=0x7C0，设置数据段寄存器DS指向原始引导位置
    mov ds, ax             ; DS = 0x7C0，这样DS:SI指向0x7C00:0x0000 = 0x7C00
    xor si, si             ; SI = 0，源偏移地址为0
    mov ax, INITSEG        ; INITSEG=0x9000，设置附加段ES指向目标位置0x90000
    mov es, ax             ; ES = 0x9000
    xor di, di             ; DI = 0，目标偏移地址为0
    cld                    ; 清除方向标志，确保字符串操作从低地址向高地址进行
    mov cx, 256            ; CX = 256，因为要复制512字节（256字，1字=2字节）
    rep movsw              ; 重复移动字指令：从DS:SI复制到ES:DI，每次CX减1，直到CX=0
    jmp INITSEG:after_move ; 远跳转到0x9000:after_move，继续执行复制后的代码
```
注释说明​：

- 这段代码将引导扇区从0x7C00复制到0x90000，以避免被后续加载的模块覆盖。

- rep movsw指令高效地复制内存：每次复制2字节，重复256次。

- 跳转后，代码在0x90000处继续执行，CS被设置为INITSEG。
### 3.1.2 内存布局管理
引导程序执行后内存变化如下表所示：

| 内存地址范围 | 内容说明 |
|------------|---------|
| 0x7C00-0x7DFF | BIOS加载的原始引导代码 |
| 0x90000-0x901FF | 复制后的引导代码 |
| 0x90200-0x9xxxx | setup模块加载区域 |
| 0x10000-0xxxxxx | kernel模块加载区域 |

### 3.1.3 磁盘读取机制
`Load_Setup`函数负责从软盘加载setup模块：

```nasm
load_setup:
    mov ax, word[setupStart] ; setupStart是常量，表示setup模块的起始扇区号（例如1）
    mov word[sec_count], ax  ; 保存当前要读取的扇区号
    add ax, [setupSize]      ; 计算最大扇区号（setupStart + setupSize）
    mov word[max_sector], ax ; 保存最大扇区号
.again:
    mov ax, [sec_count]      ; 获取当前扇区号
    push ax                  ; 参数1：扇区号入栈
    push word SETUPSEG       ; 参数2：目标段地址（SETUPSEG=0x9020，对应物理地址0x90200）
    sub ax, [setupStart]     ; 计算当前扇区在setup模块内的偏移（扇区索引）
    shl ax, 9                ; 左移9位（乘以512），得到字节偏移量
    push ax                  ; 参数3：目标偏移量入栈
    call ReadSector          ; 调用ReadSector函数读取一个扇区
    add sp, 6                ; 清理栈（3个参数，每个2字节，共6字节）
    inc word[sec_count]      ; 增加扇区号，准备读取下一个扇区
    mov bx, word[max_sector] 
    cmp word[sec_count], bx  ; 比较是否达到最大扇区号
    jl .again                ; 如果未达到，继续循环
    jmp SETUPSEG:0           ; 跳转到setup模块执行（SETUPSEG:0 = 0x9020:0 = 0x90200）
```
注释说明​：

- 此循环读取setup模块的所有扇区到内存0x90200处。

- ReadSector函数使用BIOS中断0x13读取磁盘扇区。

- 最后跳转到setup模块的入口点。
### 3.1.4 ReadSector子程序详解
`ReadSector`是核心的磁盘读取函数，使用BIOS 13h中断：

```nasm
ReadSector:
    push bp
    mov bp, sp               ; 设置栈帧，便于访问参数
    pusha                    ; 保存所有通用寄存器
    mov ax, [bp+8]           ; 获取参数1：逻辑扇区号
    xor dx, dx
    mov bx, SECTORS_PER_TRACK ; SECTORS_PER_TRACK=18（每磁道扇区数）
    div bx                   ; AX = 商（磁道号），DX = 余数（扇区号）
    mov [sec], dx            ; 保存扇区号
    and ax, 1                ; 取磁道号的低1位，得到磁头号（0或1）
    mov [head], ax           ; 保存磁头号
    ; 计算磁道号（柱面号）
    mov ax, [bp+8]           ; 重新加载逻辑扇区号
    xor dx, dx
    mov bx, SECTORS_PER_TRACK*2 ; 因为有两个磁头，所以总扇区数 per cylinder = 18 * 2
    div bx                   ; AX = 商（磁道号）
    mov [track], ax          ; 保存磁道号
    ; 设置BIOS调用参数
    mov ax, [bp+6]           ; 参数2：目标段地址
    mov es, ax               ; ES = 目标段
    mov ax, 0x0201           ; AH=0x02（读取扇区），AL=1（读取1个扇区）
    mov ch, [track]          ; CH = 磁道号低8位
    mov cl, [sec]            ; CL = 扇区号（1-based，所以后面inc cl）
    inc cl                   ; 扇区号从1开始，而不是0
    mov dh, [head]           ; DH = 磁头号
    xor dl, dl               ; DL = 驱动器号（0表示第一个软驱）
    mov bx, [bp+4]           ; 参数3：目标偏移量
    int 0x13                 ; 调用BIOS磁盘中断
    jc .error                ; 如果出错（进位标志置位），跳转到错误处理
    popa
    pop bp
    ret                      ; 返回
.error:
    ; 错误处理代码（省略）
    jmp $                    ; 无限循环
```
注释说明​：

- ReadSector将逻辑扇区号转换为CHS（柱面-磁头-扇区）格式，因为BIOS中断0x13需要CHS参数。

- 中断0x13的AH=0x02用于读取扇区，参数设置如下：

  - CH: 磁道号（柱面号）低8位

  - CL: 扇区号（1-based）

  - DH: 磁头号

  - DL: 驱动器号

  - ES:BX: 目标缓冲区地址

- 如果出错，进位标志CF置位，程序进入错误处理。

## ⚙️ 3.2 setup.asm - 系统设置程序分析

### 3.2.1 初始化工作
setup.asm负责获取系统信息、初始化描述符表，并切换到保护模式:

```nasm
start_setup:
    mov ax, SETUPSEG         ; SETUPSEG=0x9020，设置数据段
    mov ds, ax
    mov ah, 0x88            ; BIOS功能号0x88：获取扩展内存大小（单位KB）
    int 0x15                 ; 调用BIOS中断
    add ax, 1024             ; 加上1MB（1024KB）的基本内存，因为BIOS返回的是1MB以上的内存
    mov [mem_size_kbytes], ax ; 保存总内存大小
    call Kill_Motor          ; 关闭软驱马达，避免不必要的干扰
    cli                      ; 关中断，防止在设置过程中被中断打断
```
注释说明​：

- 中断0x15, AH=0x88返回扩展内存大小（从1MB开始），结果在AX中（单位KB）。

- 加上1024KB（1MB）是因为基本内存（0-1MB）不包括在返回值中。

- Kill_Motor是一个子程序，用于关闭软驱马达。

- cli关中断，确保后续操作原子性。
### 3.2.2 设置描述符表
setup程序初始化中断描述符表(IDT)和全局描述符表(GDT)：

```nasm
  ; 加载IDT和GDT
    lidt [IDT_Pointer]       ; 加载中断描述符表寄存器（IDTR）
    lgdt [GDT_Pointer]       ; 加载全局描述符表寄存器（GDTR）
    call Init_PIC            ; 初始化可编程中断控制器（8259A）
    call Enable_A20          ; 开启A20地址线，允许访问1MB以上内存
    ; 进入保护模式
    mov ax, 0x01             ; 设置CR0的PE位（保护模式启用位）
    lmsw ax                  ; 加载机器状态字（CR0的低16位）
    ; 跳转到32位代码
    jmp dword KERNEL_CS:(SETUPSEG<<4)+setup_32 ; 远跳转，刷新指令流水线
```
注释说明​：

- lidt和lgdt加载描述符表指针：IDT_Pointer和GDT_Pointer是内存中的结构，包含界限和基址。

- Init_PIC：初始化8259A芯片，设置中断向量偏移。

- Enable_A20：通过键盘控制器或端口操作开启A20线，解决地址回绕问题。

- lmsw ax设置CR0的PE位，CPU进入保护模式。

- 远跳转jmp dword切换到32位代码段：KERNEL_CS是代码段选择子（例如0x08），目标地址是setup_32的线性地址。
### 3.2.3 GDT结构详解
GDT包含三个重要的描述符：

```nasm
GDT:
    ; 描述符0：未使用
    dw 0, 0, 0, 0
    
    ; 描述符1：内核代码段
    dw 0xFFFF                 ; 段界限低16位
    dw 0x0000                 ; 段基址低16位
    db 0x00                   ; 段基址高8位
    db 0x9A                   ; 属性：存在、特权级0、代码段、可读
    db 0xCF                   ; 粒度4K、32位、界限高4位
    db 0x00                   ; 段基址最高8位
    
    ; 描述符2：内核数据段
    dw 0xFFFF                 ; 段界限低16位
    dw 0x0000                 ; 段基址低16位
    db 0x00                   ; 段基址高8位
    db 0x92                   ; 属性：存在、特权级0、数据段、可写
    db 0xCF                   ; 粒度4K、32位、界限高4位
    db 0x00                   ; 段基址最高8位
```



### 3.2.4 32位保护模式初始化
进入保护模式后，setup完成最后的初始化工作：

```nasm
[BITS 32]                   ; 切换到32位代码
setup_32:
    mov ax, KERNEL_DS       ; KERNEL_DS是数据段选择子（例如0x10）
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax              ; 设置所有段寄存器为内核数据段
    mov esp, KERN_STACK+4096 ; 设置栈指针（KERN_STACK是内核栈地址，例如0x10000）
    ; 准备启动信息传递给内核
    xor eax, eax
    mov ax, [(SETUPSEG<<4)+mem_size_kbytes] ; 从内存中加载内存大小值
    push eax                 ; 将内存大小压栈（作为bootInfo参数）
    push dword 8             ; 压入bootInfo结构大小（8字节）
    push esp                 ; 压入bootInfo指针（当前ESP）
    push dword (SETUPSEG<<4)+.returnAddr ; 压入返回地址（伪返回地址）
    jmp KERNEL_CS:ENTRY_POINT ; 跳转到内核入口点（例如main函数）
.returnAddr:
    ; 这里不会执行，因为内核不会返回
```
注释说明​：

- 在32位模式下，段寄存器现在存储选择子，而不是段基址。

- 栈指针ESP设置为内核栈顶部。

- 通过栈传递参数给内核：内存大小、bootInfo结构等。

- jmp到内核入口点，控制权交给GeekOS内核。
## 🔧 技术细节深度解析

### 1. 实模式与保护模式地址转换
- **实模式**：物理地址 = 段寄存器×16 + 偏移地址
- **保护模式**：通过GDT/LDT进行地址转换，提供内存保护和虚拟内存支持

- **实模式 vs. 保护模式**​：实模式使用段基址:偏移地址，直接访问物理内存；保护模式使用描述符表，提供内存保护和虚拟内存。
- **描述符表（GDT/IDT）​**​：GDT定义内存段属性，IDT定义中断处理程序。描述符包含基址、界限和访问权限。
### 2. A20地址线的作用
A20地址线控制决定了CPU是否可以访问1MB以上的内存空间：
- A20=0：地址回绕，模仿8086的1MB地址空间
- A20=1：可以访问完整的4GB地址空间
- A20地址线​：早期CPU为兼容性限制地址线20位以上，开启A20后可以访问完整4GB空间
### 3. 保护模式特权级
x86架构定义了4个特权级（0-3）：
- 特权级0：操作系统内核，最高权限
- 特权级3：用户程序，最低权限
- 特权级1-2：通常未使用，可为设备驱动程序保留

## 📊 启动流程总结
GeekOS的完整启动流程如下：

1. **BIOS阶段**：加电自检，加载引导扇区到0x7C00
2. **fd_boot阶段**：复制自身到0x90000，加载setup和kernel模块
3. **setup阶段**：获取系统信息，初始化GDT/IDT，开启保护模式
4. **kernel阶段**：​跳转到内核，传递参数，GeekOS内核开始运行

## 💡 学习建议

### 实践练习
1. 修改`fd_boot.asm`中的显示信息，观察启动变化
2. 尝试在setup中添加内存检测代码，显示更多系统信息
3. 研究不同磁盘参数（SECTORS_PER_TRACK等）对引导过程的影响

### 调试技巧
使用Bochs调试功能单步跟踪启动过程：
```bash
$ bochs -f bochsrc -q
# 在调试器中设置断点
b 0x7C00
b 0x90000
c
```

### 重点理解
- 理解实模式到保护模式的转换机制
- 掌握段描述符的格式和含义
- 熟悉BIOS磁盘中断的使用方法
- 了解x86架构的内存寻址方式

## ❓ 思考题
1. 为什么需要将引导程序从0x7C00复制到0x90000？
2. 保护模式下段寄存器的作用发生了什么变化？
3. A20地址线控制为什么对32位系统至关重要？
4. GDT和IDT在系统保护中各起什么作用？

通过本章的学习，你应该能够深入理解x86架构计算机的启动过程，为后续操作系统的开发打下坚实基础。