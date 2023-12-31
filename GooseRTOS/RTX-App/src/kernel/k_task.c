/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2022 Yiqing Huang
 *                          All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************
 */

/**************************************************************************//**
 * @file        k_task.c
 * @brief       task management C file
 * @version     V1.2021.05
 * @authors     Yiqing Huang
 * @date        2021 MAY
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only three simple tasks and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/


#include "k_inc.h"
//#include "k_task.h"
#include "k_rtx.h"

#define HIGHEST_PRIORITY_INDEX 0
#define MEDIUM_PRIORITY_INDEX 1
#define LOW_PRIORITY_INDEX 2
#define LOWEST_PRIORITY_INDEX 3

#define PRIORITY_LEVEL_TO_INDEX_OFFSET 0x80
/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */

TCB             *gp_current_task = NULL;            // the current RUNNING task
TCB             g_tcbs[MAX_TASKS];                  // an array of TCBs
//TASK_INIT       g_null_task_info;                 // The null task info
U32             g_num_active_tasks = 0;             // number of non-dormant tasks
tsk_ready_queue_t readyQueues[LOWEST - HIGH + 1];   // ready queues for each priority

/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:
                   RAM1_END-->+---------------------------+ High Address
                              |                           |
                              |                           |
                              |       MPID_IRAM1          |
                              |   (for user space heap  ) |
                              |                           |
                 RAM1_START-->|---------------------------|
                              |                           |
                              |  unmanaged free space     |
                              |                           |
&Image$$RW_IRAM1$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      PROC_STACK_SIZE      |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      PROC_STACK_SIZE      |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      PROC_STACK_SIZE      |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      KERN_STACK_SIZE      |     |                
    g_k_stacks[MAX_TASKS-1]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |                              
                              |---------------------------|     |
                              |      KERN_STACK_SIZE      |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      KERN_STACK_SIZE      |     |                      
              g_k_stacks[1]-->|---------------------------|     |
                              |      KERN_STACK_SIZE      |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |          
                              |                           |     |
                              |        Code + RO          |     |
                              |                           |     V
                 IRAM1_BASE-->+---------------------------+ Low Address
    
---------------------------------------------------------------------------*/ 

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */


/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 * @note    you need to change this one to be a priority scheduler
 *
 *****************************************************************************/

TCB *scheduler(void)
{
    uint8_t highestPriorityReady = HIGHEST_PRIORITY_INDEX;
    // Figure out what the highest priority level is that has tasks ready
    while(highestPriorityReady <= LOWEST_PRIORITY_INDEX && readyQueues[highestPriorityReady].head == NULL){
        highestPriorityReady++; 
    }
    if(highestPriorityReady > LOWEST_PRIORITY_INDEX){
        return NULL; // ready queues are empty
    }

    readyQueues[highestPriorityReady].head->state = RUNNING;
    return readyQueues[highestPriorityReady].head;
}

/**
 * @brief initialzie the first task in the system
 */
void k_tsk_init_first(TASK_INIT *p_task)
{
    p_task->prio         = PRIO_NULL;
    p_task->priv         = 0;
    p_task->tid          = TID_NULL;
    p_task->ptask        = &task_null;
    p_task->u_stack_size = PROC_STACK_SIZE;
}

/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 * @see         k_tsk_create_first
 * @see         k_tsk_create_new
 *****************************************************************************/

int k_tsk_init(TASK_INIT *task, int num_tasks)
{
    if (num_tasks > MAX_TASKS - 1) {
        return RTX_ERR;
    }
    
    TASK_INIT taskinfo;
    
    k_tsk_init_first(&taskinfo);
    if ( k_tsk_create_new(&taskinfo, &g_tcbs[TID_NULL], TID_NULL) == RTX_OK ) {
        g_num_active_tasks = 1;
        gp_current_task = &g_tcbs[TID_NULL];
    } else {
        g_num_active_tasks = 0;
        return RTX_ERR;
    }
    
    // create the rest of the tasks
    for ( int i = 0; i < num_tasks; i++ ) {
        TCB *p_tcb = &g_tcbs[i+1];
        if (k_tsk_create_new(&task[i], p_tcb, i+1) == RTX_OK) {
            g_num_active_tasks++;
        }
    }
    
    return RTX_OK;
}
/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task initialization structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR3)
 *              then we stack up the kernel initial context (kLR, kR4-kR12, PSP, CONTROL)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              20 registers in total
 * @note        YOU NEED TO MODIFY THIS FILE!!!
 *****************************************************************************/
int k_tsk_create_new(TASK_INIT *p_taskinfo, TCB *p_tcb, task_t tid)
{
    extern U32 SVC_RTE;

    U32 *usp;
    U32 *ksp;

    if (p_taskinfo == NULL || p_tcb == NULL)
    {
        return RTX_ERR;
    }

    p_tcb->tid   = tid;
    p_tcb->state = READY;
    p_tcb->prio  = p_taskinfo->prio;
    p_tcb->priv  = p_taskinfo->priv;
    
    /*---------------------------------------------------------------
     *  Step1: allocate user stack for the task
     *         stacks grows down, stack base is at the high address
     * ATTENTION: you need to modify the following three lines of code
     *            so that you use your own dynamic memory allocator
     *            to allocate variable size user stack.
     * -------------------------------------------------------------*/
    
    usp = k_alloc_p_stack(tid);             // ***you need to change this line***
    if (usp == NULL) {
        return RTX_ERR;
    }

    /*-------------------------------------------------------------------
     *  Step2: create task's thread mode initial context on the user stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the exception stack frame saved on the user stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         8 registers listed in push order
     *         <xPSR, PC, uLR, uR12, uR3, uR2, uR1, uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    
    *(--usp) = INITIAL_xPSR;             // xPSR: Initial Processor State
    *(--usp) = (U32) (p_taskinfo->ptask);// PC: task entry point
        
    // uR14(LR), uR12, uR3, uR3, uR1, uR0, 6 registers
    for ( int j = 0; j < 6; j++ ) {
        
#ifdef DEBUG_0
        *(--usp) = 0xDEADAAA0 + j;
#else
        *(--usp) = 0x0;
#endif
    }
    
    // allocate kernel stack for the task
    ksp = k_alloc_k_stack(tid);
    if ( ksp == NULL ) {
        return RTX_ERR;
    }

    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         12 registers listed in push order
     *         <kLR, kR4-kR12, PSP, CONTROL>
     * -------------------------------------------------------------*/
    // a task never run before directly exit
    *(--ksp) = (U32) (&SVC_RTE);
    // kernel stack R4 - R12, 9 registers
#define NUM_REGS 9    // number of registers to push
      for ( int j = 0; j < NUM_REGS; j++) {        
#ifdef DEBUG_0
        *(--ksp) = 0xDEADCCC0 + j;
#else
        *(--ksp) = 0x0;
#endif
    }
        
    // put user sp on to the kernel stack
    *(--ksp) = (U32) usp;
    
    // save control register so that we return with correct access level
    if (p_taskinfo->priv == 1) {  // privileged 
        *(--ksp) = __get_CONTROL() & ~BIT(0); 
    } else {                      // unprivileged
        *(--ksp) = __get_CONTROL() | BIT(0);
    }

    p_tcb->msp = ksp;

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param       p_tcb_old, the old tcb that was in RUNNING
 * @return      RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre         gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note        caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * @note        The control register setting will be done by the caller
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PRESERVE8
        EXPORT  K_RESTORE
        
        PUSH    {R4-R12, LR}                // save general pupose registers and return address
        MRS     R4, CONTROL                 
        MRS     R5, PSP
        PUSH    {R4-R5}                     // save CONTROL, PSP
        STR     SP, [R0, #TCB_MSP_OFFSET]   // save SP to p_old_tcb->msp
K_RESTORE
        LDR     R1, =__cpp(&gp_current_task)
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_MSP_OFFSET]   // restore msp of the gp_current_task
        POP     {R4-R5}
        MSR     PSP, R5                     // restore PSP
        MSR     CONTROL, R4                 // restore CONTROL
        ISB                                 // flush pipeline, not needed for CM3 (architectural recommendation)
        POP     {R4-R12, PC}                // restore general purpose registers and return address
}


__asm void k_tsk_start(void)
{
        PRESERVE8
        B K_RESTORE
}

/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(void)
{
    TCB *p_tcb_old = NULL;
    
    if (gp_current_task == NULL) {
        return RTX_ERR;
    }

    p_tcb_old = gp_current_task;
    gp_current_task = scheduler();
    
    if ( gp_current_task == NULL  ) {
        gp_current_task = p_tcb_old;        // revert back to the old task
        return RTX_ERR;
    }

    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
        gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
        p_tcb_old->state = READY;           // change state of the to-be-switched-out tcb
        k_tsk_switch(p_tcb_old);            // switch kernel stacks       
    }

    return RTX_OK;
}

 
/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state = RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{
    readyQueues[gp_current_task->prio - PRIORITY_LEVEL_TO_INDEX_OFFSET].head = gp_current_task->next;

    if(readyQueues[gp_current_task->prio - PRIORITY_LEVEL_TO_INDEX_OFFSET].head != NULL){
        gp_current_task->next->prev = NULL;
    }

    k_push_back_ready_queue(readyQueues[gp_current_task->prio - PRIORITY_LEVEL_TO_INDEX_OFFSET], gp_current_task);
    
    return k_tsk_run_new();
}

/**
 * @brief   get task identification
 * @return  the task ID (TID) of the calling task
 */
task_t k_tsk_gettid(void)
{
    return gp_current_task->tid;
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */

int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U32 stack_size)
{
#ifdef DEBUG_0
    printf("k_tsk_create: entering...\n\r");
    printf("task = 0x%x, task_entry = 0x%x, prio=%d, stack_size = %d\n\r", task, task_entry, prio, stack_size);
#endif /* DEBUG_0 */
    if(prio > LOWEST || prio < HIGH){
        errno = EINVAL;
        return RTX_ERR;
    }
    if(g_num_active_tasks >= MAX_TASKS){
        errno = EAGAIN;
    }
    // if the requested stack size is less than the minimum, set it to the minimum
    if(stack_size < PROC_STACK_SIZE){
        g_tcbs[g_num_active_tasks].stackSize = PROC_STACK_SIZE
    } else {
        g_tcbs[g_num_active_tasks].stackSize = stack_size;
    }

    g_tcbs[g_num_active_tasks].pspBase = (U32) k_mpool_alloc(MPID_IRAM2, g_tcbs[g_num_active_tasks].stackSize);
    if(g_tcbs[g_num_active_tasks].pspBase == NULL){
        errno = ENOMEM;
        return RTX_ERR;
    }

    g_tcbs[g_num_active_tasks].prio = prio;
    g_tcbs[g_num_active_tasks].state = READY;
    g_tcbs[g_num_active_tasks].tid = g_num_active_tasks;
    g_tcbs[g_num_active_tasks].priv = UNPRIVILEGED;
    g_tcbs[g_num_active_tasks].msp = &g_k_stacks[g_num_active_tasks][0];
    g_tcbs[g_num_active_tasks].ptask = task_entry;

    *(--g_tcbs[g_num_active_tasks].msp) = g_tcbs[g_num_active_tasks].ptask; // push PC onto stack
    *(--g_tcbs[g_num_active_tasks].msp) = 0; // push LR with an arbitrary value
    g_tcbs[g_num_active_tasks].msp -= 8; // Skip some of the registers
    *(--g_tcbs[g_num_active_tasks].msp) = g_tcbs[g_num_active_tasks].pspBase; // save the PSP to R5
    *(--g_tcbs[g_num_active_tasks].msp) = 1 << 1; // set bit[1] of the CONTROL to 1 since this is unprivileged

    // add the task to the ready queue
    k_push_back_ready_queue(&readyQueues[prio], &g_tcbs[g_num_active_tasks]);

    *task = g_num_active_tasks;
    
    g_num_active_tasks++; // increment the total number of active tasks

    k_tsk_run_new();

    return RTX_OK;
}

void k_tsk_exit(void) 
{
#ifdef DEBUG_0
    printf("k_tsk_exit: entering...\n\r");
#endif /* DEBUG_0 */

    gp_current_task->state = DORMANT;

    k_mpool_dealloc(MPID_IRAM2, gp_current_task->pspBase);

    g_num_active_tasks--;
    
    k_tsk_run_new();
}

int k_tsk_set_prio(task_t task_id, U8 prio) 
{
#ifdef DEBUG_0
    printf("k_tsk_set_prio: entering...\n\r");
    printf("task_id = %d, prio = %d.\n\r", task_id, prio);
#endif /* DEBUG_0 */
    if(prio > HIGH || prio < LOWEST){
        errno = EINVAL;
        return RTX_ERR;
    }
    if(task_id <= 0 || task_id >= MAX_TASKS){
        errno = EINVAL;
        return RTX_ERR;
    }
    if(g_tcbs[task_id].state == DORMANT){
        return RTX_OK;
    }
    if(g_tcbs[task_id].prio == prio){
        return RTX_OK;
    }
    // Check if the calling task is unprivileged and trying to change the priority
    // of a privileged task
    if(gp_current_task->priv < g_tcbs[task_id].priv){
        errno = EPERM;
        return RTX_ERR;
    }
    // Remove the task from its old ready queue
    if(g_tcbs[task_id].prev != NULL){
        g_tcbs[task_id].prev->next = g_tcbs[task_id].next;
    } else {
        // this task is the head of the linked list
        readyQueues[g_tcbs[task_id].prio - PRIORITY_LEVEL_TO_INDEX_OFFSET].head = g_tcbs[task_id].next;
    }
    if(g_tcbs[task_id].next != NULL){
        g_tcbs[task_id].next->prev = g_tcbs[task_id].prev;
    } else {
        // this task is the tail of the list
        readyQueues[g_tcbs[task_id].prio - PRIORITY_LEVEL_TO_INDEX_OFFSET].tail = g_tcbs[task_id].prev;
    }
    // add task to the back of its new priority level ready queue
    k_push_back_ready_queue(readyQueues[prio - PRIORITY_LEVEL_TO_INDEX_OFFSET], g_tcbs[task_id]);

    if(prio > gp_current_task->prio){
        // schedule the adjusted task to run
        return k_tsk_run_new();
    }

    return RTX_OK;    
}

/**
 * @brief   Retrieve task internal information 
 * @note    this is a dummy implementation, you need to change the code
 */
int k_tsk_get(task_t tid, RTX_TASK_INFO *buffer)
{
#ifdef DEBUG_0
    printf("k_tsk_get: entering...\n\r");
    printf("tid = %d, buffer = 0x%x.\n\r", tid, buffer);
#endif /* DEBUG_0 */    
    if (buffer == NULL) {
        errno = EFAULT;
        return RTX_ERR;
    }
    if(tid >= MAX_TASKS){
        errno = EINVAL;
        return RTX_ERR;
    }
    
    buffer->tid           = tid;
    buffer->prio          = g_tcbs[tid].prio;
    buffer->u_stack_size  = g_tcbs[tid].stackSize;
    buffer->priv          = g_tcbs[tid].priv;
    buffer->ptask         = g_tcbs[tid].ptask;
    buffer->k_sp          = __get_MSP();
    buffer->k_sp_base     = &g_k_stacks[tid][0];
    buffer->k_stack_size  = KERN_STACK_SIZE;
    buffer->state         = g_tcbs[tid].state;
    buffer->u_sp          = __get_PSP();
    buffer->u_sp_base     = g_tcbs[tid].pspBase;
    return RTX_OK;     
}

int k_tsk_ls(task_t *buf, size_t count){
#ifdef DEBUG_0
    printf("k_tsk_ls: buf=0x%x, count=%u\r\n", buf, count);
#endif /* DEBUG_0 */
    if(buf == NULL){
        errno = EFAULT;
        return RTX_ERR;
    }
    if(count == 0){
        errno = EFAULT;
        return RTX_ERR;
    }
    size_t numActiveTasks = 0;
    for(size_t i = 0; (i < MAX_TASKS) && (numActiveTasks < count); ++i){
        if(g_tcbs[i].state != DORMANT){
            buf[numActiveTasks] = g_tcbs[i].tid;
            numActiveTasks++;
        }
    }
    return numActiveTasks;
}

int k_rt_tsk_set(TIMEVAL *p_tv)
{
#ifdef DEBUG_0
    printf("k_rt_tsk_set: p_tv = 0x%x\r\n", p_tv);
#endif /* DEBUG_0 */
    return RTX_OK;   
}

int k_rt_tsk_susp(void)
{
#ifdef DEBUG_0
    printf("k_rt_tsk_susp: entering\r\n");
#endif /* DEBUG_0 */
    return RTX_OK;
}

int k_rt_tsk_get(task_t tid, TIMEVAL *buffer)
{
#ifdef DEBUG_0
    printf("k_rt_tsk_get: entering...\n\r");
    printf("tid = %d, buffer = 0x%x.\n\r", tid, buffer);
#endif /* DEBUG_0 */    
    if (buffer == NULL) {
        return RTX_ERR;
    }   
    
    /* The code fills the buffer with some fake rt task information. 
       You should fill the buffer with correct information    */
    buffer->sec  = 0xABCD;
    buffer->usec = 0xEEFF;
    
    return RTX_OK;
}


static void k_push_back_ready_queue(tsk_ready_queue_t* queue, TCB *task) {
    task->prev = queue->tail;
    task->next = NULL;

    // Update the tail pointer
    if (queue->tail != NULL) {
        queue->tail->next = task;
    }
    queue->tail = task;

    // Update the head pointer if the list is empty
    if (queue->head == NULL) {
        queue->head = task;
    }
}
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */

