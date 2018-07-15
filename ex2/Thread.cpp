#include "Thread.h"

#define JB_SP 6
#define JB_PC 7

/**
 * Default Constructor for thread 0 ("main")
 */
Thread::Thread()
{
    this->_state = State::Running;
    this->id = 0;

    sync_flag = false;
    block_flag = false;

    sigsetjmp(env, 1);
    quantums = 1;
    sigemptyset(&env->__saved_mask);
}

/**
 * Constructor for a "regular" newly spawned thread.
 */
Thread::Thread(int id, void (*f)())
{
    address_t sp, pc;
    this->_state = State::Ready;
    this->id = id;

    sync_flag = false;
    quantums = 0;
    // create new pc and sp, and load them into the env struct
    sp = (address_t) _stack + STACK_SIZE - sizeof(address_t);
    pc = (address_t) f;
    sigsetjmp(env, 1);
    (env->__jmpbuf)[JB_SP] = translate_address(sp);
    (env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env->__saved_mask);
}

/**
 * This function cleans the thread and frees all the mallocs made. It is called upon termination.
 */



/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t Thread::translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
            "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


/**  getter for env field of Thread.  */
sigjmp_buf *Thread::get_env()
{
    return &env;
}

/*  getter for dependancy field of Thread.  */
std::vector<int> Thread::get_depend_vec()
{
    return dependancy;
}

/* returns true iff the thread was synced. */
bool Thread::is_sync()
{
    return sync_flag;
}

/* changes sync flag according to input */
void Thread::set_sync(bool is_sync)
{
    sync_flag = is_sync;
}

/* returns true iff the thread was blocked. */
bool Thread::is_block()
{
    return block_flag;
}

/* changes block flag according to input */
void Thread::set_block(bool is_block)
{
    block_flag = is_block;
}

/* returns Thread id */
int Thread::get_id()
{
    return this->id;
}

/*  getter for state field of Thread.  */
State Thread::get_state()
{
    return this->_state;
}

/*  setter for state field of Thread.  */
void Thread::set_state(State new_state)
{
    this->_state = new_state;
}

/* function adds a new Thread to this thread's dependancy list.
 * This means that when this current thread is terminated,
 * all threads in the dependancy list will be un-synced. */
void Thread::add_dependancy(int dependant_id)
{
    // when this thread is terminated, all of these threads will be ready again
    dependancy.push_back(dependant_id);
}

/*  getter for quantum field of Thread.  */
int Thread::get_quantums()
{
    return this->quantums;
}

/* add a quantum to the count. */
void Thread::add_quantum()
{
    this->quantums++;
}

/* saves the data frame of the thread (stack, pc, sp etc')  */
void Thread::save_dataFrame()
{
    sigsetjmp(env, 1);
}

