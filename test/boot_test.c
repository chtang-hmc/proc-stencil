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

static void *initproc_run(long arg1, void *arg2) {
    return NULL;
}

void *start_initproc(long arg1, void *arg2) {
    proc_initproc = proc_create("init");
    kthread_t *init_thread = kthread_create(proc_initproc, initproc_run, 0, NULL);

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

    // check if process is running
    if (strcmp(curproc->p_name, "init") != 0) {
        fprintf(stderr, "Current process should be init, have %s", curproc->p_name);
    }

    // check if there is only one process
    if (proc_list.size != 1) {
        fprintf(stderr, "There should be one process running, there are %ld.\n", proc_list.size);
    }

    // finish initproc and clean up
    initproc_finish();
    proc_cleanup();

    // all processes should be gone!
    if (proc_list.size != 0) {
        fprintf(stderr, "There should be no process running, there are %ld.\n", proc_list.size);
    }

    return 0;
}