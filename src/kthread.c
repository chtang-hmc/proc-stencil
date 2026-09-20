#include "kthread.h"

kthread_t *curthr; // TODO: ASK what does this mean?, are we assuming only one thread per process? and if so, why do we have a list for threads for a process, can many be running at once?

/*
 * TODO: implement me!
 * Hints: we don't have any threads running yet... but what from the thread
 * subsystem needs to be initialized?
 */
void kthread_init()
{
    slab_allocator_init(&kthread_allocator, sizeof(kthread_t));
}

/*
 * TODO: implement me!
 * Hints:
 *   - make space for the new thread using the kthread allocator
 *   - set default values for thread fields
 *   - you will need to allocate a kernel stack
 *   - you will need to set up the thread's context
 *     --> for now, the page table for the process is NULL
 *   - remember to add the thread to the proc's p_thread list
 *   - initialize the kt_recent_core to ~0UL (unsigned -1)
 *   - return NULL if allocation not possible
 */
kthread_t *kthread_create(proc_t *proc, kthread_func_t func, long arg1,
                          void *arg2)
{
    slab_allocator_init(&kthread_allocator, sizeof(kthread_t));
    kthread_t *new_kthread = slab_obj_alloc(kthread_allocator);

    new_kthread->kt_kstack = page_alloc_n(DEFAULT_STACK_SIZE_PAGES);
    new_kthread->kt_retval = NULL;
    new_kthread->kt_errno = NULL;

    context_setup(&new_kthread->kt_ctx, func, arg1, arg2,
                  new_kthread->kt_kstack, DEFAULT_STACK_SIZE_PAGES, NULL); // TODO: check page table stuff

    new_kthread->kt_proc = proc;

    new_kthread->kt_cancelled = 0;
    new_kthread->kt_state = KT_RUNNABLE; // ?

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
}

/*
 * TODO: implement me!
 * Hints:
 *   - the only parts of the context that must be initialized are c_kstack and
 *     c_kstacksz
 *   - the thread's process should be set outside of this function
 *   - copy over the retval, errno, and cancelled... other fields should be
 *     freshly initialized
 *   - remember to protect access to the thread via its spinlock
 *   - see kthread_create for more hints!
 */
kthread_t *kthread_clone(kthread_t *thread)
{
    spinlock_lock(&thread->kt_lock);

    slab_allocator_init(&kthread_allocator, sizeof(kthread_t));
    kthread_t *new_kthread = slab_obj_alloc(kthread_allocator);

    new_kthread->kt_kstack = page_alloc_n(DEFAULT_STACK_SIZE_PAGES);
    new_kthread->kt_retval = thread->kt_retval;
    new_kthread->kt_errno = thread->kt_errno;

    context_setup(&new_kthread->kt_ctx, NULL, 0, NULL,
                  new_kthread->kt_kstack, DEFAULT_STACK_SIZE_PAGES, NULL);

    new_kthread->kt_proc = NULL;

    new_kthread->kt_cancelled = thread->kt_cancelled;
    new_kthread->kt_state = KT_RUNNABLE;

    spinlock_init(&new_kthread->kt_lock);

    // add thread to process thread list (need to do outside this function)
    // list_link_init(&new_kthread->kt_plink, new_kthread);
    // spinlock_lock(&proc->p_threads_lock);
    // list_insert(&proc->p_threads, &new_kthread->kt_plink);
    // spinlock_unlock(&proc->p_threads_lock);

    // add thread to thread queue to be processed
    list_link_init(&new_kthread->kt_qlink, new_kthread);
    spinlock_lock(&kt_runq.tq_lock);
    list_insert(&kt_runq.tq_list, &new_kthread->kt_qlink);
    spinlock_unlock(&kt_runq.tq_lock);

    spinlock_unlock(&thread->kt_lock);
    return new_kthread;
}

/*
 * TODO: implement me!
 * Hints:
 *   - deallocate thread memory
 *   - remove thread from process' thread list
 *   - protect all accesses to shared data
 *   - don't forget to free thread's stack!
 */
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

/*
 * TODO: implement me! TODO: ASK
 * Hints:
 *   - cannot "cancel" the current thread, so call exit
 *   - mark the thread as cancelled and stop executing
 *   - remember to the protect access to the thread
 */
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

/*
 * TODO: implement me!
 * Hints: there's (some but) not much to do here... remember, it's up to the
 * parent process to manage its threads!
 */
void kthread_exit(void *retval)
{
    curthr->kt_retval = retval;
    curthr->kt_state = KT_EXITED;
}
