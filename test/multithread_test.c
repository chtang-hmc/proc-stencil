#include "kthread.h"
#include "proc.h"
#include "sched.h"
#include <stdio.h>

const int NUM_INITS = 6;

typedef void (*init_func_t)();
init_func_t init_funcs[] = {
    mem_init,
    slab_init,
    proc_init,
    kthread_init,
    sched_init,
    proc_idleproc_init
};

static context_t bootstrap_ctx;

static void *worker_run(long arg1, void *arg2) {
    printf("worker %ld running in pid %d\n", arg1, curproc->p_pid);
    return NULL;
}

static void *initproc_run(long arg1, void *arg2) {
    printf("hello from init thread, pid %d, thread %ld\n", curproc->p_pid, arg1);

    // create one more process and threads
    proc_t *proc0 = proc_create("proc0");
    kthread_t **worker_threads = malloc(11 * sizeof(kthread_t *));
    for (int i = 100; i <= 110; ++i) {
        worker_threads[i-100] = kthread_create(proc0, worker_run, i, NULL);
    }

    // check the number of threads
    long proc_thread_size = proc0 -> p_threads.size;
    if (proc_thread_size != 11) {
        fprintf(stderr, "Number of threads should be 11, there are %d\n", proc_thread_size);
    }

    sched_switch();
    if (proc_thread_size != 11) {
        fprintf(stderr, "Number of threads should be 11, there are %d\n", proc_thread_size);
    }
    
    for (int i = 0; i < 11; ++i) {
        kthread_cancel(worker_threads[i], NULL);
    }
    
    // all threads should be cleaned up!
    proc_thread_size = proc0 -> p_threads.size;
    if (proc_thread_size != 0) {
        fprintf(stderr, "Number of threads should be 0, there are %d\n", proc_thread_size);
    }

    if (strcmp(curproc->p_name, "init") != 0) {
        fprintf(stderr, "Current process should be init, have %s\n", curproc->p_name);
    }
    proc_destroy(proc0);
    if (strcmp(curproc->p_name, "init") != 0) {
        fprintf(stderr, "Current process should be init, have %s\n", curproc->p_name);
    }

    free(worker_threads);
    return NULL;
}

void *start_initproc(long arg1, void *arg2) {
    proc_initproc = proc_create("init");
    kthread_t *init_thread = kthread_create(proc_initproc, initproc_run, 0, NULL);

    long proc_thread_size = proc_initproc -> p_threads.size;
    if (proc_thread_size != 1) {
        fprintf(stderr, "Number of threads should be 1, there are %d\n", proc_thread_size);
    }

    // don't worry about using the scheduling system...
    curproc = proc_initproc;
    curthr = init_thread;

    // we're bypassing the scheduler, so pull init off the run queue ourselves:
    // sched_switch assumes the running thread is never on kt_runq
    spinlock_lock(&kt_runq.tq_lock);
    list_remove_front(&kt_runq.tq_list);
    spinlock_unlock(&kt_runq.tq_lock);
    init_thread->kt_state = KT_ON_CPU;

    context_make_active(&init_thread->kt_ctx);

    return NULL;
}

int main(int argc, char **argv) {
    // initialize subsystems
    for (int i = 0; i < NUM_INITS; i++) {
        init_funcs[i]();
    }

    void *bootstrap_stack = page_alloc_n(1);
    if (bootstrap_stack == NULL) {
        return -1;
    }

    context_setup(&bootstrap_ctx, start_initproc, 0, NULL, bootstrap_stack, PAGE_SIZE, NULL);
    context_switch(&bios_ctx, &bootstrap_ctx); // saves this as the place where bios ctx will restore

    // check if we have the right process
    if (strcmp(curproc->p_name, "init") != 0) {
        fprintf(stderr, "Current process should be init, have %s\n", curproc->p_name);
    }
    proc_cleanup();

    long num_procs = proc_list.size;
    if (num_procs != 0) {
        fprintf(stderr, "There should be no processes left, there are %d.\n", num_procs);
    }

    return 0;
}
