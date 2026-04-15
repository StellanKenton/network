#define SYS_ARCH_GLOBALS

#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/lwip_sys.h"
#include "lwip/mem.h"
#include "includes.h"
#include "delay.h"
#include "arch/sys_arch.h"
#include "malloc.h"

static const void * const pvNullPointer = (mem_ptr_t *)0xffffffff;

static OS_TICK sys_arch_ms_to_ticks(u32_t timeout_ms)
{
	CPU_INT64U ticks;

	if (timeout_ms == 0u) {
		return 0u;
	}

	ticks = ((CPU_INT64U)timeout_ms * (CPU_INT64U)OSCfg_TickRate_Hz + 999u) / 1000u;
	if (ticks == 0u) {
		ticks = 1u;
	}

	return (OS_TICK)ticks;
}

static u32_t sys_arch_ticks_to_ms(OS_TICK start, OS_TICK end)
{
	OS_TICK delta;

	if (end >= start) {
		delta = end - start;
	} else {
		delta = (OS_TICK)(0xffffffffu - start + end + 1u);
	}

	return (u32_t)(((CPU_INT64U)delta * 1000u) / (CPU_INT64U)OSCfg_TickRate_Hz);
}

static OS_TICK sys_arch_get_time(void)
{
	OS_TICK tick;

	tick = OSTimeGet();
	return tick;
}

static void sys_arch_delay_retry(void)
{
	OSTimeDly(1u);
}

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
	INT16U queue_size;
	void **queue_buffer;

	queue_size = (size <= 0) ? 1u : (INT16U)size;
	queue_buffer = (void **)mymalloc(SRAMIN, (u32)(queue_size * sizeof(void *)));
	if (queue_buffer == NULL) {
		mbox->valid = DEF_FALSE;
		return ERR_MEM;
	}

	mbox->queue_buffer = queue_buffer;
	mbox->queue_size = queue_size;
	mbox->queue = OSQCreate(queue_buffer, queue_size);
	if (mbox->queue == NULL) {
		mbox->valid = DEF_FALSE;
		myfree(SRAMIN, queue_buffer);
		return ERR_MEM;
	}

	mbox->valid = DEF_TRUE;
	return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
	OS_ERR err;

	if (mbox->valid == DEF_FALSE) {
		return;
	}

	(void)OSQDel(mbox->queue, OS_DEL_ALWAYS, &err);
	if (mbox->queue_buffer != NULL) {
		myfree(SRAMIN, mbox->queue_buffer);
		mbox->queue_buffer = NULL;
	}
	mbox->valid = DEF_FALSE;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
	OS_ERR err;

	if (msg == NULL) {
		msg = (void *)&pvNullPointer;
	}

	do {
		err = OSQPost(mbox->queue, msg);
		if (err == OS_ERR_Q_MAX) {
			sys_arch_delay_retry();
		}
	} while (err == OS_ERR_Q_MAX);
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
	OS_ERR err;

	if (msg == NULL) {
		msg = (void *)&pvNullPointer;
	}

	err = OSQPost(mbox->queue, msg);
	return (err == OS_ERR_NONE) ? ERR_OK : ERR_MEM;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
	OS_ERR err;
	OS_TICK start;
	OS_TICK end;
	OS_TICK ticks;
	void *entry;

	ticks = sys_arch_ms_to_ticks(timeout);
	start = sys_arch_get_time();
	entry = OSQPend(mbox->queue, ticks, &err);
	if (err == OS_ERR_TIMEOUT) {
		if (msg != NULL) {
			*msg = NULL;
		}
		return SYS_ARCH_TIMEOUT;
	}

	LWIP_ASSERT("OSQPend", err == OS_ERR_NONE);
	if (msg != NULL) {
		*msg = (entry == (void *)&pvNullPointer) ? NULL : entry;
	}

	end = sys_arch_get_time();
	return sys_arch_ticks_to_ms(start, end);
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	return sys_arch_mbox_fetch(mbox, msg, 1u);
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
	return (mbox->valid == DEF_TRUE) ? 1 : 0;
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
	mbox->valid = DEF_FALSE;
}

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
	sem->sem = OSSemCreate((INT16U)count);
	if (sem->sem == NULL) {
		sem->valid = DEF_FALSE;
		return ERR_MEM;
	}

	sem->valid = DEF_TRUE;
	return ERR_OK;
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
	OS_ERR err;
	OS_TICK start;
	OS_TICK end;
	OS_TICK ticks;

	ticks = sys_arch_ms_to_ticks(timeout);
	start = sys_arch_get_time();
	OSSemPend(sem->sem, ticks, &err);
	if (err == OS_ERR_TIMEOUT) {
		return SYS_ARCH_TIMEOUT;
	}

	LWIP_ASSERT("OSSemPend", err == OS_ERR_NONE);
	end = sys_arch_get_time();
	return sys_arch_ticks_to_ms(start, end);
}

void sys_sem_signal(sys_sem_t *sem)
{
	OS_ERR err;

	err = OSSemPost(sem->sem);
	LWIP_ASSERT("OSSemPost", err == OS_ERR_NONE);
}

void sys_sem_free(sys_sem_t *sem)
{
	OS_ERR err;

	if (sem->valid == DEF_FALSE) {
		return;
	}

	(void)OSSemDel(sem->sem, OS_DEL_ALWAYS, &err);
	sem->valid = DEF_FALSE;
}

int sys_sem_valid(sys_sem_t *sem)
{
	return (sem->valid == DEF_TRUE) ? 1 : 0;
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
	sem->valid = DEF_FALSE;
}

void sys_init(void)
{
}

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
	OS_ERR err;
	CPU_STK *stack;
	OS_TCB *tcb;
	CPU_STK *stack_top;

	stack = (CPU_STK *)mymalloc(SRAMIN, (u32)(stacksize * sizeof(CPU_STK)));
	tcb = (OS_TCB *)mymalloc(SRAMIN, sizeof(OS_TCB));
	if ((stack == NULL) || (tcb == NULL)) {
		if (stack != NULL) {
			myfree(SRAMIN, stack);
		}
		if (tcb != NULL) {
			myfree(SRAMIN, tcb);
		}
		return NULL;
	}

	stack_top = &stack[stacksize - 1];
	err = OSTaskCreateExt(thread,
					 arg,
					 stack_top,
					 (OS_PRIO)prio,
					 (INT16U)prio,
					 stack,
					 (INT32U)stacksize,
					 NULL,
					 APP_TASK_OPT);
	#if OS_TASK_NAME_EN > 0u
	if (err == OS_ERR_NONE) {
		OSTaskNameSet((OS_PRIO)prio, (INT8U *)name, &err);
	}
	#endif
	if (err != OS_ERR_NONE) {
		myfree(SRAMIN, stack);
		myfree(SRAMIN, tcb);
		return NULL;
	}

	return tcb;
}

void sys_msleep(u32_t ms)
{
	delay_ms((u16)ms);
}

u32_t sys_now(void)
{
	return (u32_t)(((CPU_INT64U)sys_arch_get_time() * 1000u) / (CPU_INT64U)OSCfg_TickRate_Hz);
}













































