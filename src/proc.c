#include "proc.h"

proc_t *curproc;

/*
 * TODO: implement me!
 * Hints: we don't have any processes running yet... but what from the process
 * subsystem needs to be initialized?
 */
void proc_init()
{
    // initialize global variables
    list_init(&proc_list);
    spinlock_init(&proc_list_lock);
    next_pid = 0;
    slab_allocator_init(&proc_allocator, sizeof(proc_t));

    // initialize first process
    proc_idleproc_init();
    curproc = proc_initproc; // TODO: find a way to connect idlepro to initproc

    // initialize thread queue
    sched_init(); // TODO: sus
}

/*
 * TODO: implement me!
 * The idle process is a special process that is created by kmain
 * its job is to be the first process on the system, but it does not have any
 * associated threads
 * Hints:
 *   - what would the fields of the process struct be set to for idleproc?
 *   - what is the initial value of curproc? curthr?
 */
void proc_idleproc_init()
{
    idleproc.p_pid = next_pid;
    next_pid++;
    strcpy(idleproc.p_name, "idleproc");

    list_init(&idleproc.p_threads);
    spinlock_init(&idleproc.p_threads_lock);
    list_init(&idleproc.p_children);
    spinlock_init(&idleproc.p_children_lock);
    idleproc.p_pproc = NULL; // TODO: sus

    list_link_init(&idleproc.p_list_link, NULL);  // TODO: sus
    list_link_init(&idleproc.p_child_link, NULL); // TODOL sus

    idleproc.p_status = 0;           // TODO: sus
    idleproc.p_state = PROC_RUNNING; // TODO: sus
}

/*
 * This function is implemented to tell the system to shut down and exit
 */
void initproc_finish()
{
    context_switch(&curthr->kt_ctx, &bios_ctx);
}

/*
 * TODO: implement me!
 * Hints:
 *   - make space for the new process using the process allocator
 *   - we need to update the global structures
 *   - the process becomes a child of the current process
 *   - don't forget to synchronize on shared structures!
 */
proc_t *proc_create(const char *name)
{
    proc_t *new_proc = slab_obj_alloc(proc_allocator);
    new_proc->p_pid = next_pid;
    next_pid++;
    strcpy(new_proc->p_name, name);

    list_init(&new_proc->p_threads);
    // need to clone all of the parents' threads
    spinlock_init(&new_proc->p_threads_lock);
    list_init(&new_proc->p_children);
    spinlock_init(&new_proc->p_children_lock);
    new_proc->p_pproc = curproc;

    list_link_init(&new_proc->p_list_link, new_proc);
    list_link_init(&new_proc->p_child_link, new_proc);
    list_insert(&curproc->p_children, &new_proc->p_child_link);
    list_insert(&proc_list, &new_proc->p_list_link);

    idleproc.p_status = 0; // TODO: sus
    idleproc.p_state = PROC_PENDING;

    return new_proc;
}

/*
 * TODO: implement me!
 * Hints: anything that was allocated needs to be deallocated... deallocated
 * objects should not be accessible by anyone else!
 */
void proc_destroy(proc_t *proc)
{
    slab_obj_free(proc_allocator, proc);
}

/*
 * TODO: implement me!
 * Hints: anything that was allocated needs to be deallocated... deallocated
 * objects should not be accessible by anyone else!
 */
void proc_cleanup()
{
    curproc->p_state = PROC_DEAD;
    if (proc_list.head->next != NULL)
    {
        proc_t *prevproc = curproc;
        curproc = proc_list.head->next->parent;
        list_remove_front(&proc_list);
        proc_destory(prevproc);
    }
    else
    {
        list_remove_front(&proc_list);
        proc_destroy(curproc);
    }
}

/*
 * TODO: implement me!
 * Hints: how should a process behave if all threads exit?
 */
void proc_thread_exiting(void *retval)
{
    spinlock_lock(&curproc->p_threads_lock);

    // TODO: revisit after writing kthreads class
    if (curproc->p_threads.head == NULL)
    {
        curproc->p_state = PROC_DEAD;
        curproc->p_status = (long *)retval;
    }

    spinlock_unlock(&curproc->p_threads_lock);
}

/*
 * TODO: implement me!
 * Hints:
 *   - cancel all threads associated with the provided process
 *   - protect access to the threads list
 */
void proc_kill(proc_t *proc, long status)
{
    // lock the threads list
    // iterate through threads in process
    // call kthreads cancel
    // dont know if this will remove the thread from the list or not, if not then remove it from the list
    // unlock the threads list
}

/*
 * TODO: implement me!
 * Hints:
 *  - protect access to the process list
 *  - kill the current process at the very end... don't kill before function
 * finishes!
 */
void proc_kill_all()
{
}
