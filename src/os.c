
#include "cpu.h"
#include "timer.h"
#include "sched.h"
#include "loader.h"
#include "mm.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int time_slot;
static int num_cpus;
static int done = 0;
static struct krnl_t os;

#ifdef MM_PAGING
static int memramsz;
static int memswpsz[PAGING_MAX_MMSWP];

struct mmpaging_ld_args {
	/* A dispatched argument struct to compact many-fields passing to loader */
	int vmemsz;
	struct memphy_struct *mram;
	struct memphy_struct **mswp;
	struct memphy_struct *active_mswp;
	int active_mswp_id;
	struct timer_id_t  *timer_id;
};
#endif

static struct ld_args{
	char ** path;
	unsigned long * start_time;
#ifdef MLQ_SCHED
	unsigned long * prio;
#endif
} ld_processes;
int num_processes;

struct cpu_args {
	struct timer_id_t * timer_id;
	int id;
};


static void * cpu_routine(void * args) {
	struct timer_id_t * timer_id = ((struct cpu_args*)args)->timer_id;
	int id = ((struct cpu_args*)args)->id;
	/* Check for new process in ready queue */
	int time_left = 0;
	struct pcb_t * proc = NULL;
	while (1) {
		/* Check the status of current process */
		if (proc == NULL) {
			/* No process is running, the we load new process from
		 	* ready queue */
			proc = get_proc();
			if (proc == NULL) {
                           next_slot(timer_id);
                           continue; /* First load failed. skip dummy load */
                        }
		}else if (proc->pc == proc->code->size) {
			/* The porcess has finish it job */
			printf("\tCPU %d: Processed %2d has finished\n",
				id ,proc->pid);
			free(proc);
			proc = get_proc();
			time_left = 0;
		}else if (time_left == 0) {
			/* The process has done its job in current time slot */
			printf("\tCPU %d: Put process %2d to run queue\n",
				id, proc->pid);
			put_proc(proc);
			proc = get_proc();
		}
		
		/* Recheck process status after loading new process */
		if (proc == NULL && done) {
			/* No process to run, exit */
			printf("\tCPU %d stopped\n", id);
			break;
		}else if (proc == NULL) {
			/* There may be new processes to run in
			 * next time slots, just skip current slot */
			next_slot(timer_id);
			continue;
		}else if (time_left == 0) {
			printf("\tCPU %d: Dispatched process %2d\n",
				id, proc->pid);
			time_left = time_slot;
		}
		
		/* Run current process */
		run(proc);
		time_left--;
		next_slot(timer_id);
	}
	detach_event(timer_id);
	pthread_exit(NULL);
}

static void * ld_routine(void * args) {
#ifdef MM_PAGING
	struct memphy_struct* mram = ((struct mmpaging_ld_args *)args)->mram;
	struct memphy_struct** mswp = ((struct mmpaging_ld_args *)args)->mswp;
	struct memphy_struct* active_mswp = ((struct mmpaging_ld_args *)args)->active_mswp;
	struct timer_id_t * timer_id = ((struct mmpaging_ld_args *)args)->timer_id;
#else
	struct timer_id_t * timer_id = (struct timer_id_t*)args;
#endif
	int i = 0;
	printf("ld_routine\n");
	while (i < num_processes) {
		struct pcb_t * proc = load(ld_processes.path[i]);
		struct krnl_t * krnl = proc->krnl = &os;	

#ifdef MLQ_SCHED
		proc->prio = ld_processes.prio[i];
#endif
		while (current_time() < ld_processes.start_time[i]) {
			next_slot(timer_id);
		}
#ifdef MM_PAGING
		proc->mm = calloc(1, sizeof(struct mm_struct));
		init_mm(proc->mm, proc);
		krnl->mram = mram;
		krnl->mswp = mswp;
		krnl->active_mswp = active_mswp;
#endif
		printf("\tLoaded a process at %s, PID: %d PRIO: %ld\n",
			ld_processes.path[i], proc->pid, ld_processes.prio[i]);
		add_proc(proc);
		free(ld_processes.path[i]);
		i++;
		next_slot(timer_id);
	}
	free(ld_processes.path);
	free(ld_processes.start_time);
	done = 1;
	detach_event(timer_id);
	pthread_exit(NULL);
}

static void read_config(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("Cannot find configure file at %s\n", path);
        exit(1);
    }

    /* Dòng 1: time slice, số CPU, số process */
    if (fscanf(f, "%d %d %d", &time_slot, &num_cpus, &num_processes) != 3) {
        printf("Invalid first line in config file %s\n", path);
        fclose(f);
        exit(1);
    }

    /* Cấp phát mảng cho loader */
    ld_processes.path = (char **) malloc(sizeof(char *) * num_processes);
    ld_processes.start_time =
        (unsigned long *) malloc(sizeof(unsigned long) * num_processes);

#ifdef MM_PAGING
    /* Cấu hình phần RAM / SWAP */
    int i;

#ifdef MM_FIXED_MEMSZ
    /* Trường hợp giữ nguyên kích thước cố định,
       giống logic ban đầu nhưng viết khác */
    memramsz = 0x100000;      /* 1MB RAM */
    for (i = 0; i < PAGING_MAX_MMSWP; ++i) {
        memswpsz[i] = 0;
    }
    memswpsz[0] = 0x1000000;  /* 16MB swap 0 */

#else
    /* Trường hợp đọc cấu hình bộ nhớ từ file:
       MEM_RAM_SZ SWP0 SWP1 SWP2 SWP3 */
    if (fscanf(f, "%d", &memramsz) != 1) {
        printf("Invalid memory config line in %s\n", path);
        fclose(f);
        exit(1);
    }

    for (i = 0; i < PAGING_MAX_MMSWP; ++i) {
        if (fscanf(f, "%d", &memswpsz[i]) != 1) {
            /* Nếu thiếu số thì coi như 0 */
            memswpsz[i] = 0;
        }
    }

    /* Ăn nốt ký tự newline nếu còn */
    int ch;
    while ((ch = fgetc(f)) != '\n' && ch != EOF) {
        /* bỏ qua */
    }
#endif /* MM_FIXED_MEMSZ */
#endif /* MM_PAGING */

#ifdef MLQ_SCHED
    ld_processes.prio =
        (unsigned long *) malloc(sizeof(unsigned long) * num_processes);
#endif

    /* Đọc danh sách process */
    for (int idx = 0; idx < num_processes; ++idx) {
        char proc_name[128];

        /* Cấp phát chỗ cho đường dẫn đầy đủ */
        ld_processes.path[idx] = (char *) malloc(128);
        ld_processes.path[idx][0] = '\0';

        /* Đọc theo từng mode */
#ifdef MLQ_SCHED
        if (fscanf(f, "%lu %127s %lu",
                   &ld_processes.start_time[idx],
                   proc_name,
                   &ld_processes.prio[idx]) != 3) {
            printf("Invalid process line %d in config file %s\n", idx, path);
            fclose(f);
            exit(1);
        }
#else
        if (fscanf(f, "%lu %127s",
                   &ld_processes.start_time[idx],
                   proc_name) != 2) {
            printf("Invalid process line %d in config file %s\n", idx, path);
            fclose(f);
            exit(1);
        }
#endif
        /* Ghép thành đường dẫn: "input/proc/<tên>" */
        snprintf(ld_processes.path[idx], 128, "input/proc/%s", proc_name);
    }
    fclose(f);
}

int main(int argc, char * argv[]) {
	/* Read config */
	if (argc != 2) {
		printf("Usage: os [path to configure file]\n");
		return 1;
	}
	char path[100];
	path[0] = '\0';
	strcat(path, "input/");
	strcat(path, argv[1]);
	read_config(path);

	pthread_t * cpu = (pthread_t*)malloc(num_cpus * sizeof(pthread_t));
	struct cpu_args * args =
		(struct cpu_args*)malloc(sizeof(struct cpu_args) * num_cpus);
	pthread_t ld;
	
	/* Init timer */
	int i;
	for (i = 0; i < num_cpus; i++) {
		args[i].timer_id = attach_event();
		args[i].id = i;
	}
	struct timer_id_t * ld_event = attach_event();
	start_timer();

#ifdef MM_PAGING
	/* Init all MEMPHY include 1 MEMRAM and n of MEMSWP */
	int rdmflag = 1; /* By default memphy is RANDOM ACCESS MEMORY */

	struct memphy_struct mram;
	struct memphy_struct mswp[PAGING_MAX_MMSWP];

	/* Create MEM RAM */
	init_memphy(&mram, memramsz, rdmflag);

        /* Create all MEM SWAP */ 
	int sit;
	for(sit = 0; sit < PAGING_MAX_MMSWP; sit++)
	       init_memphy(&mswp[sit], memswpsz[sit], rdmflag);

	/* In Paging mode, it needs passing the system mem to each PCB through loader*/
	struct mmpaging_ld_args *mm_ld_args = malloc(sizeof(struct mmpaging_ld_args));

	mm_ld_args->timer_id = ld_event;
	mm_ld_args->mram = (struct memphy_struct *) &mram;
	mm_ld_args->mswp = (struct memphy_struct**) &mswp;
	mm_ld_args->active_mswp = (struct memphy_struct *) &mswp[0];
        mm_ld_args->active_mswp_id = 0;
#endif

	/* Init scheduler */
	init_scheduler();

	/* Run CPU and loader */
#ifdef MM_PAGING
	pthread_create(&ld, NULL, ld_routine, (void*)mm_ld_args);
#else
	pthread_create(&ld, NULL, ld_routine, (void*)ld_event);
#endif
	for (i = 0; i < num_cpus; i++) {
		pthread_create(&cpu[i], NULL,
			cpu_routine, (void*)&args[i]);
	}

	/* Wait for CPU and loader finishing */
	for (i = 0; i < num_cpus; i++) {
		pthread_join(cpu[i], NULL);
	}
	pthread_join(ld, NULL);

	/* Stop timer */
	stop_timer();

	return 0;

}







