/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;
#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i ;
	for (i = 0; i < MAX_PRIO; i ++) {
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i; 
	}
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	running_list.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t *get_mlq_proc(void) {
    struct pcb_t *proc = NULL;

    /* 
     * Ghi nhớ mức ưu tiên đang phục vụ và số lần đã cấp CPU
     * cho mức đó trong "chu kỳ" hiện tại.
     */
    static int cur_prio = 0;
    static int used_slot = 0;

    int scanned = 0;  /* tránh vòng lặp vô hạn */

    pthread_mutex_lock(&queue_lock);

    while (scanned < MAX_PRIO) {
        /* Nếu hàng đợi ở mức cur_prio đang trống, nhảy sang mức kế tiếp */
        if (empty(&mlq_ready_queue[cur_prio])) {
            used_slot = 0;  // không phục vụ gì ở mức này
            cur_prio = (cur_prio + 1) % MAX_PRIO;
            scanned++;
            continue;
        }

        /* Hàng đợi có process */
        if (used_slot < slot[cur_prio]) {
            /* Vẫn còn quota cho mức ưu tiên này trong chu kỳ hiện tại */
            proc = dequeue(&mlq_ready_queue[cur_prio]);
            used_slot++;
            break;
        } else {
            /* Hết slot cho mức ưu tiên này -> chuyển sang mức khác */
            used_slot = 0;
            cur_prio = (cur_prio + 1) % MAX_PRIO;
            scanned++;
        }
    }

    /* Nếu không lấy được proc nào -> reset lại dùng cho lần gọi sau */
    if (proc == NULL) {
        used_slot = 0;
    }

    pthread_mutex_unlock(&queue_lock);
    return proc;
}


void put_mlq_proc(struct pcb_t * proc) {
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->mlq_ready_queue = mlq_ready_queue;
	proc->krnl->running_list = &running_list;

	/* TODO: put running proc to running_list 
	 *       It worth to protect by a mechanism.
	 * 
	 */
	
	pthread_mutex_lock(&queue_lock);
	enqueue(&running_list, proc);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->mlq_ready_queue = mlq_ready_queue;
	proc->krnl->running_list = &running_list;

	/* TODO: put running proc to running_list
	 *       It worth to protect by a mechanism.
	 * 
	*/
      
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	return add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;

	pthread_mutex_lock(&queue_lock);
	/*TODO: get a process from [ready_queue].
	 *       It worth to protect by a mechanism.
	 * 
	 */
	if (!empty(&run_queue)) {
		proc = dequeue(&run_queue);
	} else if (!empty(&ready_queue)) {
		proc = dequeue(&ready_queue);
	}
	if (proc != NULL)
		enqueue(&running_list, proc);				
	pthread_mutex_unlock(&queue_lock);

	return proc;
}

void put_proc(struct pcb_t * proc) {
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->running_list = &running_list;

	/* TODO: put running proc to running_list 
	 *       It worth to protect by a mechanism.
	 * 
	*/
	pthread_mutex_lock(&queue_lock);
	enqueue(&running_list, proc);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->running_list = &running_list;

	/* TODO: put running proc to running_list 
	 *       It worth to protect by a mechanism.
	 * 
	 */
	pthread_mutex_lock(&queue_lock);
	enqueue(&running_list, proc);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif


