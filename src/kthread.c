#include "kthread.h"

kthread_t *curthr;

void kthread_init()
{
    slab_allocator_init(&kthread_allocator, sizeof(kthread_t));
}

kthread_t *kthread_create(proc_t *proc, kthread_func_t func, long arg1,
                          void *arg2)
{
    slab_allocator_init(&kthread_allocator, sizeof(kthread_t));
    kthread_t *new_kthread = slab_obj_alloc(kthread_allocator);

    new_kthread->kt_kstack = page_alloc_n(DEFAULT_STACK_SIZE_PAGES);
    new_kthread->kt_retval = NULL;
    new_kthread->kt_errno = NULL;

    context_setup(&new_kthread->kt_ctx, func, arg1, arg2,
                  new_kthread->kt_kstack, DEFAULT_STACK_SIZE_PAGES*PAGE_SIZE, NULL);

    new_kthread->kt_proc = proc;

    new_kthread->kt_cancelled = 0;
    new_kthread->kt_state = KT_RUNNABLE;

    spinlock_init(&new_kthread->kt_lock);

    // add thread to process thread list
    list_link_init(&new_kthread->kt_plink, new_kthread);
    spinlock_lock(&proc->p_threads_lock);
    list_insert(&proc->p_threads, &new_kthread->kt_plink);
    spinlock_unlock(&proc->p_threads_lock);

    // add thread to thread queue to be processed
    list_link_init(&new_kthread->kt_qlink, new_kthread);
    spinlock_lock(&kt_runq.tq_lock);
    list_insert(&kt_runq.tq_list, &new_kthread->kt_qlink);
    spinlock_unlock(&kt_runq.tq_lock);

    return new_kthread;
}

kthread_t *kthread_clone(kthread_t *thread)
{
    spinlock_lock(&thread->kt_lock);

    slab_allocator_init(&kthread_allocator, sizeof(kthread_t));
    kthread_t *new_kthread = slab_obj_alloc(kthread_allocator);

    new_kthread->kt_kstack = page_alloc_n(DEFAULT_STACK_SIZE_PAGES);
    new_kthread->kt_retval = thread->kt_retval;
    new_kthread->kt_errno = thread->kt_errno;

    context_setup(&new_kthread->kt_ctx, NULL, 0, NULL,
                  new_kthread->kt_kstack, DEFAULT_STACK_SIZE_PAGES*PAGE_SIZE, NULL);

    new_kthread->kt_proc = NULL;

    new_kthread->kt_cancelled = thread->kt_cancelled;
    new_kthread->kt_state = KT_RUNNABLE;

    spinlock_init(&new_kthread->kt_lock);

    // add thread to thread queue to be processed
    list_link_init(&new_kthread->kt_qlink, new_kthread);
    spinlock_lock(&kt_runq.tq_lock);
    list_insert(&kt_runq.tq_list, &new_kthread->kt_qlink);
    spinlock_unlock(&kt_runq.tq_lock);

    spinlock_unlock(&thread->kt_lock);
    return new_kthread;
}

void kthread_destroy(kthread_t *thread)
{
    // free stack
    page_free_n(thread->kt_kstack, DEFAULT_STACK_SIZE_PAGES);

    // remove thread from process' thread list
    spinlock_lock(&thread->kt_proc->p_threads_lock);
    list_remove_link(&thread->kt_proc->p_threads, &thread->kt_plink);
    spinlock_unlock(&thread->kt_proc->p_threads_lock);

    // deallocate
    slab_obj_free(kthread_allocator, thread);
}

void kthread_cancel(kthread_t *thread, void *retval)
{
    if (thread == curthr)
    {
        kthread_exit(retval);
    }
    else
    {
        thread->kt_cancelled = 1;
        thread->kt_retval = retval;
        thread->kt_state = KT_EXITED;
    }
}

void kthread_exit(void *retval)
{
    curthr->kt_retval = retval;
    curthr->kt_state = KT_EXITED;

    proc_thread_exiting(retval);

    sched_switch();
}
