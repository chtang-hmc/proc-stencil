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

static void *childproc_run(long arg1, void *arg2) {
    // simulate waiting
    long j = 0;
    for (long i = 0; i < arg1 * 10; ++i){
        ++j;
    }
    return NULL;
}

static void *initproc_run(long arg1, void *arg2) {

    // create 200 processes, each with 20 threads
    int num_proc = 200;
    int num_threads = 20;

    // create each process
    proc_t **childproc_list = malloc(num_proc * sizeof(proc_t *));
    for (int i = 0; i < num_proc; ++i) {
        // create the name of the process as process_id_{num}
        char proc_name[100] = "process_id_";
        snprintf(proc_name, sizeof(proc_name), "%s%d", proc_name, i);

        // create process and add to process list
        proc_t *childproc = proc_create(proc_name);
        childproc_list[i] = childproc;
        
        // create 20 threads linked to the process we just created
        kthread_t **worker_threads = malloc(num_threads * sizeof(kthread_t *));
        for (int i = 100; i <= 99 + num_threads; ++i) {
            // have each thread run the child_proc run function
            worker_threads[i-100] = kthread_create(childproc, childproc_run, i, NULL);
        }
        
        // check that we created 20 threads
        long proc_thread_size = childproc -> p_threads.size;
        if (proc_thread_size != num_threads) {
            fprintf(stderr, "Number of threads should be %d, there are %d\n", num_threads, proc_thread_size);
        }

        // exectute and context switch the threads
        sched_switch();
        if (proc_thread_size != num_threads) {
            fprintf(stderr, "Number of threads should be %d, there are %d\n", num_threads, proc_thread_size);
        }
        
        // cancel all the created threads
        for (int i = 0; i < num_threads; ++i) {
            kthread_cancel(worker_threads[i], NULL);
        }

        // free the thread storage 
        free(worker_threads);
    }

    // we need to check that we have all 200 processes and 1 init process
    if (proc_list.size != num_proc + 1) {
        fprintf(stderr, "(initial) There should be %d processes running, there are %ld.\n", num_proc, proc_list.size);
    }

    // make sure all threads are cancelled/deleted
    for (long i = 0; i < num_proc; ++i) {
        if (childproc_list[i] -> p_threads.size != 0) {
            fprintf(stderr, "Number of threads should be 0, there are %d\n", childproc_list[i] -> p_threads.size);
        }
        // destroy each proc and remove it from the global proc list
        proc_destroy(childproc_list[i]);
        if (proc_list.size != num_proc - i) {
            fprintf(stderr, "There should be %ld processes running, there are %ld.\n", num_proc - i, proc_list.size);
        }
    }

    // free the memory storing all the procecess
    free(childproc_list);
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

    return 0;
}
