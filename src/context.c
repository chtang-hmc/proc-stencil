#include "kthread.h"

#include "context.h"

/*
 * Executes a thread function helper for setting up stack in new thread
 */
static void __context_thread_initial_func(context_func_t func, long arg1,
                                     void *arg2) {
    void *result = func(arg1, arg2);
    kthread_exit(result);
}

/*
 * Sets up a context to begin execution
 */
void context_setup(context_t *c, context_func_t func, long arg1, void *arg2,
                   void *kstack, int kstacksz, page_table_t *pt) {
    c->c_kstack = kstack;
    c->c_kstacksz = kstacksz;
    c->page_table = pt;

    // put initial arguments onto the stack and leave room for the return value
    // from context_thread_initial_func (which calls passed func)
    c->c_rsp = kstack + kstacksz;
    c->c_rdx = arg2;
    c->c_rsi = arg1;
    c->c_rdi = func;

    // set frame pointer to start of stack pointer
    c->c_rbp = c->c_rsp;
    c->c_rip = (void *) __context_thread_initial_func;
}

/*
 * Makes the provided context active (starts executing from current rip)
 */
void context_make_active(context_t *c) {
    // set the current page table to be the page table of the provided context
    // we will come back to this!

    // switch stacks and run thread
    __asm__ volatile(
        "movq %1,%%rbp\n\t" // update rbp
        "movq %2,%%rsp\n\t" // update rsp
        "push %0\n\t"       // save rip onto the stack
        "movq %4,%%rsi\n\t" // update rsi
        "movq %5,%%rdx\n\t" // update rdx
        "movq %3,%%rdi\n\t" // update rdi 
        "ret"               // jump to new rip (__context_thread_initial_func)
        ::"m"(c->c_rip), "m"(c->c_rbp), "m"(c->c_rsp),
          "m"(c->c_rdi), "m"(c->c_rsi), "m"(c->c_rdx)
    );
}

/*
 * Switch context from old to new
 */
void context_switch(context_t *oldc, context_t *newc) {
    // set the current page table to the page table of the new context...
    // we will come back to this!

    // save the stack and frame pointer into the old context, set the
    // instruction pointer to the return address (whoever called us)
    // put all of the other registers onto the kernel stack
    // then, load all of the new registers from the other stack into the
    // active registers
    __asm__ volatile(
        "pushfq          \n\t" // save FLAGS on the stack
        "popq %0         \n\t" // save FLAGS into oldc
        "movq %%rax,%1   \n\t" // save rax into oldc
        "movq %%rbx,%2   \n\t" // save rbx into oldc
        "movq %%rdx,%3   \n\t" // save rdx into oldc
        "movq $1f,%4     \n\t" // save rip into oldc
        "movq %%rsp,%5   \n\t" // save rsp into oldc
        "movq %%rbp,%6   \n\t" // save rbp into oldc
        "movq %%rsi,%7   \n\t" // save rsi into oldc
        "movq %%rdi,%8   \n\t" // save rdi into oldc
        "pushq %9        \n\t" // push FLAGS from newc onto stack
        "popfq           \n\t" // restore FLAGS
        "movq %10,%%rax  \n\t" // restore rax from newc
        "movq %11,%%rbx  \n\t" // restore rbx from newc
        "movq %12,%%rdx  \n\t" // restore rdx from newc
        "movq %14,%%rsp  \n\t" // restore rsp from newc
        "movq %15,%%rbp  \n\t" // restore rbp from newc
        "movq %17,%%rdi  \n\t" // restore rdi from newc
        "pushq %13       \n\t" // push new rip onto stack
        "movq %16,%%rsi  \n\t" // restore rsi from newc
        "ret             \n\t" // jump back to 
        "1:                \t" // this is where oldc starts executing later
        "pushq %0        \n\t" // psuh oldc FLAGS onto stack
        "popfq               " // restore FLAGS
        :"=m"(oldc->c_flags),"=m"(oldc->c_rax), "=m"(oldc->c_rbx),
         "=m"(oldc->c_rdx),  "=m"(oldc->c_rip), "=m"(oldc->c_rsp),
         "=m"(oldc->c_rbp),  "=m"(oldc->c_rsi), "=m"(oldc->c_rdi),
         "=m"(newc->c_flags),"=m"(newc->c_rax), "=m"(newc->c_rbx),
         "=m"(newc->c_rdx),  "=m"(newc->c_rip), "=m"(newc->c_rsp),
         "=m"(newc->c_rbp),  "=m"(newc->c_rsi), "=m"(newc->c_rdi)
    );
}
