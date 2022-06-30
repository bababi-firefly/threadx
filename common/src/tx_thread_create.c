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

#define TX_SOURCE_CODE


/* Include necessary system files.  */

#include "tx_api.h"
#include "tx_trace.h"
#include "tx_thread.h"
#include "tx_initialize.h"


/**************************************************************************/
/*                                                                        */
/*  FUNCTION                                               RELEASE        */
/*                                                                        */
/*    _tx_thread_create                                   PORTABLE C      */
/*                                                           6.1.8        */
/*  AUTHOR                                                                */
/*                                                                        */
/*    William E. Lamie, Microsoft Corporation                             */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */
/*    This function creates a thread and places it on the list of created */
/*    threads.                                                            */
/*                                                                        */
/*  INPUT                                                                 */
/*                                                                        */
/*    thread_ptr                            Thread control block pointer  */
/*    name                                  Pointer to thread name string */
/*    entry_function                        Entry function of the thread  */
/*    entry_input                           32-bit input value to thread  */
/*    stack_start                           Pointer to start of stack     */
/*    stack_size                            Stack size in bytes           */
/*    priority                              Priority of thread            */
/*                                            (default 0-31)              */
/*    preempt_threshold                     Preemption threshold          */
/*    time_slice                            Thread time-slice value       */
/*    auto_start                            Automatic start selection     */
/*                                                                        */
/*  OUTPUT                                                                */
/*                                                                        */
/*    return status                         Thread create return status   */
/*                                                                        */
/*  CALLS                                                                 */
/*                                                                        */
/*    _tx_thread_stack_build                Build initial thread stack    */
/*    _tx_thread_system_resume              Resume automatic start thread */
/*    _tx_thread_system_ni_resume           Noninterruptable resume thread*/
/*                                                                        */
/*  CALLED BY                                                             */
/*                                                                        */
/*    Application Code                                                    */
/*    _tx_timer_initialize                  Create system timer thread    */
/*                                                                        */
/*  RELEASE HISTORY                                                       */
/*                                                                        */
/*    DATE              NAME                      DESCRIPTION             */
/*                                                                        */
/*  05-19-2020      William E. Lamie        Initial Version 6.0           */
/*  09-30-2020      William E. Lamie        Modified comment(s), and      */
/*                                            changed stack calculations  */
/*                                            to use ALIGN_TYPE integers, */
/*                                            resulting in version 6.1    */
/*  06-02-2021      William E. Lamie        Modified comment(s), and      */
/*                                            supported TX_MISRA_ENABLE,  */
/*  08-02-2021      Scott Larson            Removed unneeded cast,        */
/*                                            resulting in version 6.1.8  */
/*                                                                        */
/**************************************************************************/
UINT  _tx_thread_create(TX_THREAD *thread_ptr, CHAR *name_ptr, VOID (*entry_function)(ULONG id), ULONG entry_input,
                            VOID *stack_start, ULONG stack_size, UINT priority, UINT preempt_threshold, // 栈起始地址，大小
                            ULONG time_slice, UINT auto_start)  // 相同优先级的线程按时间片运行
{

TX_INTERRUPT_SAVE_AREA // 定义用来保存中断信息的变量，用来存储disable_irq时的DAIF(aarch64时是这样)的值的

TX_THREAD               *next_thread;
TX_THREAD               *previous_thread;
TX_THREAD               *saved_thread_ptr;
UINT                    saved_threshold =  ((UINT) 0);
UCHAR                   *temp_ptr;

// Mstep 2 初始化栈
#ifdef TX_ENABLE_STACK_CHECKING
ALIGN_TYPE              new_stack_start;
ALIGN_TYPE              updated_stack_start;
#endif

#ifndef TX_DISABLE_STACK_FILLING

    /* Set the thread stack to a pattern prior to creating the initial
       stack frame.  This pattern is used by the stack checking routines
       to see how much has been used.  */
    TX_MEMSET(stack_start, ((UCHAR) TX_STACK_FILL), stack_size);  // 把栈用0XEFEFEFEF 填满
#endif
// 栈检查
#ifdef TX_ENABLE_STACK_CHECKING

    /* Ensure that there are two ULONG of 0xEF patterns at the top and
       bottom of the thread's stack. This will be used to check for stack
       overflow conditions during run-time.  */
       // 真实可用大小，要按4字节对齐
    stack_size =  ((stack_size/(sizeof(ULONG))) * (sizeof(ULONG))) - (sizeof(ULONG)); // 栈顶和第留两个0xEF的样式，用来在运行过程中判断是否溢出

    /* Ensure the starting stack address is evenly aligned.  */
#ifdef TX_MISRA_ENABLE // 定义后，ThreadX将使用符合MISRA C的约定
    new_stack_start = TX_POINTER_TO_ULONG_CONVERT(stack_start);
#else
    new_stack_start =  TX_POINTER_TO_ALIGN_TYPE_CONVERT(stack_start);
#endif /* TX_MISRA_ENABLE */
    // 判断栈首地址是不是4字节对齐的，地址低2位如果有值的话就会得到不同的结果
    // (111111000 + 011) & 100
    updated_stack_start =  (((new_stack_start) + ((sizeof(ULONG)) - ((ULONG) 1)) ) & (~((sizeof(ULONG)) - ((ULONG) 1))));

    /* Determine if the starting stack address is different.  */
    if (new_stack_start != updated_stack_start) // 起始地址不对齐
    {

        /* Yes, subtract another ULONG from the size to avoid going past the stack area.  */
        stack_size =  stack_size - (sizeof(ULONG)); // 不是4字节对齐的,大小需要变化
    }

    /* Update the starting stack pointer.  */
#ifdef TX_MISRA_ENABLE
    stack_start = TX_ULONG_TO_POINTER_CONVERT(updated_stack_start); // 更新栈起始地址
#else
    stack_start =  TX_ALIGN_TYPE_TO_POINTER_CONVERT(updated_stack_start);
#endif /* TX_MISRA_ENABLE */
#endif

    /* Prepare the thread control block prior to placing it on the created
       list.  */

    // Mstep 2 初始化Thread 结构
    /* Initialize thread control block to all zeros.  */
    TX_MEMSET(thread_ptr, 0, (sizeof(TX_THREAD)));

    /* Place the supplied parameters into the thread's control block.  */
    thread_ptr -> tx_thread_name =              name_ptr;
    thread_ptr -> tx_thread_entry =             entry_function;
    thread_ptr -> tx_thread_entry_parameter =   entry_input;
    thread_ptr -> tx_thread_stack_start =       stack_start;
    thread_ptr -> tx_thread_stack_size =        stack_size;
    thread_ptr -> tx_thread_priority =          priority;
    thread_ptr -> tx_thread_user_priority =     priority;
    thread_ptr -> tx_thread_time_slice =        time_slice;
    thread_ptr -> tx_thread_new_time_slice =    time_slice;
    thread_ptr -> tx_thread_inherit_priority =  ((UINT) TX_MAX_PRIORITIES); // 设置无效继承优先级

    /* Calculate the end of the thread's stack area.  */
    temp_ptr =  TX_VOID_TO_UCHAR_POINTER_CONVERT(stack_start);
    temp_ptr =  (TX_UCHAR_POINTER_ADD(temp_ptr, (stack_size - ((ULONG) 1)))); // stack_size是字节数量
    thread_ptr -> tx_thread_stack_end =         TX_UCHAR_TO_VOID_POINTER_CONVERT(temp_ptr); // 栈尾地址

#ifndef TX_DISABLE_PREEMPTION_THRESHOLD

    /* Preemption-threshold is enabled, setup accordingly.  */
    thread_ptr -> tx_thread_preempt_threshold =       preempt_threshold;
    thread_ptr -> tx_thread_user_preempt_threshold =  preempt_threshold;
#else

    /* Preemption-threshold is disabled, determine if preemption-threshold was required.  */
    if (priority != preempt_threshold)
    {

        /* Preemption-threshold specified. Since specific preemption-threshold is not supported,
           disable all preemption.  */
        thread_ptr -> tx_thread_preempt_threshold =       ((UINT) 0);
        thread_ptr -> tx_thread_user_preempt_threshold =  ((UINT) 0);
    }
    else
    {

        /* Preemption-threshold is not specified, just setup with the priority.  */
        thread_ptr -> tx_thread_preempt_threshold =       priority;
        thread_ptr -> tx_thread_user_preempt_threshold =  priority;
    }
#endif

    /* Now fill in the values that are required for thread initialization.  */
    thread_ptr -> tx_thread_state =  TX_SUSPENDED;  // 创建时都是挂起状态，resume之后才会是ready状态

    /* Setup the necessary fields in the thread timer block.  */
    TX_THREAD_CREATE_TIMEOUT_SETUP(thread_ptr) // 设置定时器，定时器用于在thread suspend时将thread唤醒

    /* Perform any additional thread setup activities for tool or user purpose.  */
    TX_THREAD_CREATE_INTERNAL_EXTENSION(thread_ptr) // 给用户自定义的

    /* Call the target specific stack frame building routine to build the
       thread's initial stack and to setup the actual stack pointer in the
       control block.  */
    _tx_thread_stack_build(thread_ptr, _tx_thread_shell_entry);  // 这块看汇编实现，初始化栈信息

#ifdef TX_ENABLE_STACK_CHECKING

    /* Setup the highest usage stack pointer.  */
    thread_ptr -> tx_thread_stack_highest_ptr =  thread_ptr -> tx_thread_stack_ptr;
#endif

    /* Prepare to make this thread a member of the created thread list.  */
    TX_DISABLE  // 需要搞明白什么时候可以关中断

    /* Load the thread ID field in the thread control block.  */
    thread_ptr -> tx_thread_id =  TX_THREAD_ID;

    /* Place the thread on the list of created threads.  First,
       check for an empty list.  */
    // 加入created list中
    if (_tx_thread_created_count == TX_EMPTY)
    {

        /* The created thread list is empty.  Add thread to empty list.  */
        _tx_thread_created_ptr =                    thread_ptr;
        thread_ptr -> tx_thread_created_next =      thread_ptr;
        thread_ptr -> tx_thread_created_previous =  thread_ptr;
    }
    else  // 每一个新的thread会放到链表的末端
    {
        // 挂到双向链表上
        /* This list is not NULL, add to the end of the list.  */
        next_thread =  _tx_thread_created_ptr;
        previous_thread =  next_thread -> tx_thread_created_previous;

        /* Place the new thread in the list.  */
        next_thread -> tx_thread_created_previous =  thread_ptr;
        previous_thread -> tx_thread_created_next =  thread_ptr;

        /* Setup this thread's created links.  */
        thread_ptr -> tx_thread_created_previous =  previous_thread;
        thread_ptr -> tx_thread_created_next =      next_thread;
    }

    /* Increment the thread created count.  */
    _tx_thread_created_count++;

    // 底下4行都是debug使用
    /* If trace is enabled, register this object.  */
    TX_TRACE_OBJECT_REGISTER(TX_TRACE_OBJECT_TYPE_THREAD, thread_ptr, name_ptr, TX_POINTER_TO_ULONG_CONVERT(stack_start), stack_size)

    /* If trace is enabled, insert this event into the trace buffer.  */
    TX_TRACE_IN_LINE_INSERT(TX_TRACE_THREAD_CREATE, thread_ptr, priority, TX_POINTER_TO_ULONG_CONVERT(stack_start), stack_size, TX_TRACE_THREAD_EVENTS)

    /* Register thread in the thread array structure.  */
    TX_EL_THREAD_REGISTER(thread_ptr)

    /* Log this kernel call.  */
    TX_EL_THREAD_CREATE_INSERT

#ifndef TX_NOT_INTERRUPTABLE

    /* Temporarily disable preemption.  */
    _tx_thread_preempt_disable++;
#endif

    /* Determine if an automatic start was requested.  If so, call the resume
       thread function and then check for a preemption condition.  */
    if (auto_start == TX_AUTO_START)
    {
 
        /* Determine if the create call is being called from initialization.  */
        // M1 thread system在初始化阶段tx_thread_preempt_threshold不应该起作用，所以把它的值设置为了tx_thread_priority
        if (TX_THREAD_GET_SYSTEM_STATE() >= TX_INITIALIZE_IN_PROGRESS)
        {

            /* Yes, this create call was made from initialization.  */

            /* Pickup the current thread execute pointer, which corresponds to the
               highest priority thread ready to execute.  Interrupt lockout is
               not required, since interrupts are assumed to be disabled during
               initialization.  */
            saved_thread_ptr =  _tx_thread_execute_ptr;

            /* Determine if there is thread ready for execution.  */
            if (saved_thread_ptr != TX_NULL)
            {

                /* Yes, a thread is ready for execution when initialization completes.  */

                /* Save the current preemption-threshold.  */
                saved_threshold =  saved_thread_ptr -> tx_thread_preempt_threshold;

                /* For initialization, temporarily set the preemption-threshold to the
                   priority level to make sure the highest-priority thread runs once
                   initialization is complete.  */
                saved_thread_ptr -> tx_thread_preempt_threshold =  saved_thread_ptr -> tx_thread_priority;
            }
        }
        else
        {

            /* Simply set the saved thread pointer to NULL.  */
            saved_thread_ptr =  TX_NULL;
        }

#ifdef TX_NOT_INTERRUPTABLE

        /* Perform any additional activities for tool or user purpose.  */
        TX_THREAD_CREATE_EXTENSION(thread_ptr)

        /* Resume the thread!  */
        _tx_thread_system_ni_resume(thread_ptr);

        /* Restore previous interrupt posture.  */
        TX_RESTORE
#else

        /* Restore previous interrupt posture.  */
        TX_RESTORE

        /* Perform any additional activities for tool or user purpose.  */
        TX_THREAD_CREATE_EXTENSION(thread_ptr)

        /* Call the resume thread function to make this thread ready.  */
        // M2 autoStart时需要resume线程到ready状态
        _tx_thread_system_resume(thread_ptr);
#endif

        /* Determine if the thread's preemption-threshold needs to be restored.  */
        if (saved_thread_ptr != TX_NULL)
        {

            /* Yes, restore the previous highest-priority thread's preemption-threshold. This
               can only happen if this routine is called from initialization.  */
            saved_thread_ptr -> tx_thread_preempt_threshold =  saved_threshold;
        }
    }
    else
    {

#ifdef TX_NOT_INTERRUPTABLE

        /* Perform any additional activities for tool or user purpose.  */
        TX_THREAD_CREATE_EXTENSION(thread_ptr)

        /* Restore interrupts.  */
        TX_RESTORE
#else

        /* Restore interrupts.  */
        TX_RESTORE

        /* Perform any additional activities for tool or user purpose.  */
        TX_THREAD_CREATE_EXTENSION(thread_ptr) // 留给用户无用

        /* Disable interrupts.  */
        TX_DISABLE

        /* Re-enable preemption.  */
        _tx_thread_preempt_disable--;

        /* Restore interrupts.  */
        TX_RESTORE

        /* Check for preemption.  */
        _tx_thread_system_preempt_check();
#endif
    }

    /* Always return a success.  */
    return(TX_SUCCESS);
}

