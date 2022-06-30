/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/*       This software is licensed under the Microsoft Software License   */
/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
/*       and in the root directory of this software.                      */
/*                                                                        */
/**************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */
/** ThreadX Component                                                     */
/**                                                                       */
/**   Thread                                                              */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/


    .text  // .text段保存代码，是只读和可执行的，后面那些指令都属于.text段。
    .align 3
/**************************************************************************/
/*                                                                        */
/*  FUNCTION                                               RELEASE        */
/*                                                                        */
/*    _tx_thread_stack_build                               ARMv8-A        */
/*                                                           6.1.10       */
/*  AUTHOR                                                                */
/*                                                                        */
/*    William E. Lamie, Microsoft Corporation                             */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */
/*    This function builds a stack frame on the supplied thread's stack.  */
/*    The stack frame results in a fake interrupt return to the supplied  */
/*    function pointer.                                                   */
/*                                                                        */
/*  INPUT                                                                 */
/*                                                                        */
/*    thread_ptr                            Pointer to thread             */
/*    function_ptr                          Pointer to entry function     */
/*                                                                        */
/*  OUTPUT                                                                */
/*                                                                        */
/*    None                                                                */
/*                                                                        */
/*  CALLS                                                                 */
/*                                                                        */
/*    None                                                                */
/*                                                                        */
/*  CALLED BY                                                             */
/*                                                                        */
/*    _tx_thread_create                     Create thread service         */
/*                                                                        */
/*  RELEASE HISTORY                                                       */
/*                                                                        */
/*    DATE              NAME                      DESCRIPTION             */
/*                                                                        */
/*  09-30-2020     William E. Lamie         Initial Version 6.1           */
/*  01-31-2022     Andres Mlinar            Updated comments,             */
/*                                            resulting in version 6.1.10 */
/*                                                                        */
/**************************************************************************/
// VOID   _tx_thread_stack_build(TX_THREAD *thread_ptr, VOID (*function_ptr)(VOID))
// {
    .global _tx_thread_stack_build
    .type   _tx_thread_stack_build, @function
_tx_thread_stack_build:

// 切换thread时，上下文就保存在stack中，希望build后就可以按正常调度流程拉起之执行
// x0-x30 64bit 通用寄存器x0-x30 64bit 通用寄存器，如果有需要可以当做32bit使用：W0-W30。
// 其中x0 - x7：这 8 个寄存器主要用来存储传递参数 。如果参数超过 8 个，则会通过栈来传递 ；
// x0 也用来存放上文方法的返回值。
    /* Build an interrupt frame.  On Cortex-A35 it should look like this:
                                                                低地址
       Stack Top:      SSPR        Initial SSPR
                       ELR         Point of interrupt
                       x28         Initial value for x28
                       not used    Not used
                       x26         Initial value for x26
                       x27         Initial value for x27
                       x24         Initial value for x24
                       x25         Initial value for x25
                       x22         Initial value for x22
                       x23         Initial value for x23
                       x20         Initial value for x20
                       x21         Initial value for x21
                       x18         Initial value for x18
                       x19         Initial value for x19
                       x16         Initial value for x16
                       x17         Initial value for x17
                       x14         Initial value for x14
                       x15         Initial value for x15
                       x12         Initial value for x12
                       x13         Initial value for x13
                       x10         Initial value for x10
                       x11         Initial value for x11
                       x8          Initial value for x8
                       x9          Initial value for x9
                       x6          Initial value for x6
                       x7          Initial value for x7
                       x4          Initial value for x4
                       x5          Initial value for x5
                       x2          Initial value for x2
                       x3          Initial value for x3
                       x0          Initial value for x0 // 也用来保存上下文方法的返回值
                       x1          Initial value for x1
                       x29         Initial value for x29 (frame pointer)  FP寄存器 // 保存栈帧地址(栈底指针)，指向当前方法栈的底部
                       x30         Initial value for x30 (link register)  LR寄存器 // 保存调用结束时的返回执行地址
                       
                       0           For stack backtracing  
                                                                高地址
、、

    Stack Bottom: (higher memory address)  */

    LDR     x4, [x0, #24]                       // Pickup end of stack area //从第一个参数移动24位获取到 *tx_thread_stack_end，保存到x4中
    BIC     x4, x4, #0xF                        // Ensure 16-byte alignment //按16字节对齐 ARM64中对栈的操作时16字节对齐的

    /* Actually build the stack frame.  */

    MOV     x2, #0                              // Build clear value
    MOV     x3, #0                              //

    STP     x2, x3, [x4, #-16]!                  // Set backtrace to 0 // 从栈底(需要再确认)往回走16个字节，刷为0值 // 写地址是往高地址写
    STP     x2, x3, [x4, #-16]!                 // Set initial x29, x30
    STP     x2, x3, [x4, #-16]!                 // Set initial x0, x1
    STP     x2, x3, [x4, #-16]!                 // Set initial x2, x3
    STP     x2, x3, [x4, #-16]!                 // Set initial x4, x5
    STP     x2, x3, [x4, #-16]!                 // Set initial x6, x7
    STP     x2, x3, [x4, #-16]!                 // Set initial x8, x9
    STP     x2, x3, [x4, #-16]!                 // Set initial x10, x11
    STP     x2, x3, [x4, #-16]!                 // Set initial x12, x13
    STP     x2, x3, [x4, #-16]!                 // Set initial x14, x15
    STP     x2, x3, [x4, #-16]!                 // Set initial x16, x17
    STP     x2, x3, [x4, #-16]!                 // Set initial x18, x19
    STP     x2, x3, [x4, #-16]!                 // Set initial x20, x21
    STP     x2, x3, [x4, #-16]!                 // Set initial x22, x23
    STP     x2, x3, [x4, #-16]!                 // Set initial x24, x25
    STP     x2, x3, [x4, #-16]!                 // Set initial x26, x27
    STP     x2, x3, [x4, #-16]!                 // Set initial x28
#ifdef EL1
    MOV     x2, #0x4                            // Build initial SPSR  (EL1) // 操作系统或者一些特权函数
#else
#ifdef EL2
    MOV     x2, #0x8                            // Build initial SPSR  (EL2) // Hypervisor
#else
    MOV     x2, #0xC                            // Build initial SPSR  (EL3) // Secure Monitor
#endif
#endif
    MOV     x3, x1                              // Build initial ELR   // x1中保存的是 *function_ptr的指针
    STP     x2, x3, [x4, #-16]!                 // Set initial SPSR & ELR // 初始化保存程序状态寄存器和LR,设置模式

    /* Setup stack pointer.  */
    // thread_ptr -> tx_thread_stack_ptr =  x2;

    STR     x4, [x0, #8]                        // Save stack pointer in thread's  // 栈顶指针存到 tx_thread_stack_ptr 中 // 此时stack X4指针已经移动到栈顶了应该
    RET                                         // Return to caller

// }
