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
#include "tx_thread.h"


/**************************************************************************/
/*                                                                        */
/*  FUNCTION                                               RELEASE        */
/*                                                                        */
/*    _tx_thread_system_preempt_check                     PORTABLE C      */
/*                                                           6.1          */
/*  AUTHOR                                                                */
/*                                                                        */
/*    William E. Lamie, Microsoft Corporation                             */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */
/*    函数作用 ： 检测 _tx_thread_preempt_disable是否为0，是否(为0时允许)允许*/
/*               切换                                                     */
/*    This function checks for preemption that could have occurred as a   */
/*    result scheduling activities occurring while the preempt disable    */
/*    flag was set.                                                       */
/*                                                                        */
/*  INPUT                                                                 */
/*                                                                        */
/*    None                                                                */
/*                                                                        */
/*  OUTPUT                                                                */
/*                                                                        */
/*    None                                                                */
/*                                                                        */
/*  CALLS                                                                 */
/*                                                                        */
/*    _tx_thread_system_return              Return to the system          */
/*                                                                        */
/*  CALLED BY                                                             */
/*                                                                        */
/*    Other ThreadX Components                                            */
/*                                                                        */
/*  RELEASE HISTORY                                                       */
/*                                                                        */
/*    DATE              NAME                      DESCRIPTION             */
/*                                                                        */
/*  05-19-2020     William E. Lamie         Initial Version 6.0           */
/*  09-30-2020     Yuxin Zhou               Modified comment(s),          */
/*                                            resulting in version 6.1    */
/*                                                                        */
/**************************************************************************/
VOID  _tx_thread_system_preempt_check(VOID) 
{

ULONG           combined_flags;
TX_THREAD       *current_thread;
TX_THREAD       *thread_ptr;


    /* Combine the system state and preempt disable flags into one for comparison.  */
    TX_THREAD_SYSTEM_RETURN_CHECK(combined_flags)

    /* Determine if we are in a system state (ISR or Initialization) or internal preemption is disabled.  */
    // Mstep 1 _tx_thread_preempt_disable 为0时才允许抢占，才能切换
    if (combined_flags == ((ULONG) 0)) // 
    {

        /* No, at thread execution level so continue checking for preemption.  */

        /* Pickup thread pointer.  */
        // Mstep2 获取全局current_thread_ptr和下一个thread_ptr比较是不是一个线程
        TX_THREAD_GET_CURRENT(current_thread) // 

        /* Pickup the next execute pointer.  */
        thread_ptr =  _tx_thread_execute_ptr;

        /* Determine if preemption should take place.  */
        if (current_thread != thread_ptr) // 如果不是一个线程
        {

#ifdef TX_ENABLE_STACK_CHECKING

            /* Check this thread's stack.  */
            TX_THREAD_STACK_CHECK(thread_ptr)
#endif


#ifdef TX_THREAD_ENABLE_PERFORMANCE_INFO

            /* Determine if an idle system return is present.  */
            if (thread_ptr == TX_NULL)
            {

                /* Yes, increment the return to idle return count.  */
                _tx_thread_performance_idle_return_count++;
            }
            else
            {

                /* No, there is another thread ready to run and will be scheduled upon return.  */
                _tx_thread_performance_non_idle_return_count++;
            }
#endif

            /* Return to the system so the higher priority thread can be scheduled.  */
            _tx_thread_system_return(); // Mstep 3 调用此函数触发pensv_handle
        }
    }
}

