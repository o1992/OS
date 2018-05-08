
#include <signal.h>
#include "Thread.h"

#define JB_SP 6
#define JB_PC 7


Thread::Thread()
{
    // my code
    this->_state = State::Ready;
    this->id = 0;
//    dependancy = new(std::nothrow) std::vector<int>();       // TODO - delete this when terminating

    if (!dependancy){
        std::cerr << "system error: allocation problem in Thread" << std::endl;
    }
    sync_flag = false;
    block_flag = false;
    this->_stack = new(std::nothrow) char[STACK_SIZE];        // TODO - delete this when terminating
    if (!_stack){
        std::cerr << "system error: allocation problem in Thread" << std::endl;
    }
    sigsetjmp(env, 1);
    quantums = 1;
    sigemptyset(&env->__saved_mask);

}


Thread::Thread(int id, void (*f)())
{
    address_t sp, pc;
    // my code
    this->_state = State::Ready;
    this->id = id;
    dependancy = new(std::nothrow) std::vector<int>();       // TODO - delete this when terminating
    if (!dependancy){
        std::cerr << "system error: allocation problem in Thread" << std::endl;
    }
    sync_flag = false;
    quantums = 0;
    this->_stack = new(std::nothrow) char[STACK_SIZE];        // TODO - delete this when terminating
    if (!_stack){
        std::cerr << "system error: allocation problem in Thread" << std::endl;
    }


    // modifying the given code in demo_jmp
    sp = (address_t) _stack + STACK_SIZE - sizeof(address_t);
    pc = (address_t) f;
    sigsetjmp(env, 1);
    (env->__jmpbuf)[JB_SP] = translate_address(sp);
    (env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env->__saved_mask);


//    return this*;
}
void Thread::clean_thread(){

//    for (auto &all_thread : dependancy) {
//    dependancy->clear();
//    dependancy->shrink_to_fit();
//    std::vector<int>().swap(*dependancy);
    delete [] _stack;
}



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

sigjmp_buf *Thread::get_env()
{
    return &env;
}
std::vector<int> Thread::get_depend_vec()
{
    return *dependancy;
}


bool Thread::is_sync()
{
    return sync_flag;
}

void Thread::set_sync(bool is_sync)
{
    sync_flag = is_sync;
}
bool Thread::is_block()
{
    return block_flag;
}

void Thread::set_block(bool is_block)
{
    block_flag = is_block;
}


int Thread::get_id()
{
    return this->id;
}

State Thread::get_state()
{
    return this->_state;
}

void Thread::set_state(State new_state)
{
    this->_state = new_state;
}

void Thread::add_dependancy(int dependant_id)
{     // when this thread is terminated, all of these threads will be ready again
    dependancy->push_back(dependant_id);
//    sync_flag = true;
}

int Thread::get_quantums()
{
    return this->quantums;
}

void Thread::add_quantum()
{
    this->quantums++;
}

void Thread::save_dataFrame()
{
    sigsetjmp(env, 1);
}

void Thread::jump_to()
{
    siglongjmp(env, 2);
}

//
// Created by chen_by on 4/26/18.
//

//#include "Thread.h"
