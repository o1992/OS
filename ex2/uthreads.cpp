#include "uthreads.h"
#include "Thread.h"
#include <deque>
#include <stack>
#include <csignal>
#include <sys/time.h>
#include <cstdio>
#include <iostream>
#include <map>
#include <queue>
#include <cstring>


//  ***  GLOBAL STRUCTURES OF U_THREADS LIBRARY  ***
int totalQuantums;
std::map<int, Thread *> all_threads;
std::map<int, Thread *> blocked_threads;
std::deque<Thread *> ready_threads;
std::priority_queue<int, std::vector<int>, std::greater<int> > available_id_pq;
sigset_t signals_set;
Thread *running;
struct sigaction sa;
struct itimerval timer;


//  ***  PRIVATE FUNCTIONS IN U_THREADS LIBRARY  ***
void scheduler();

void terminate_main_thread(int exit_code, bool init_flag);

void block_sync_decision(int tid);

void unsync_all_dependants(Thread *t_thread);

void block_signals();

void resume_signals();


//  ***    PUBLIC LIBRARY FUNCTIONS    ***

/**
 * This function is called when a quantum expires. Our handle is to change state
 * (current thread is not running anymore) and to call the handler.
 * Note: this function IS NOT public, but will only run if writtem before init.
 */
void timer_handler(int sig)
{
    running->save_dataFrame();
    running->set_state(State::Ready);
    scheduler();
}


/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On void block_sync_decision(Thread* thread)success,return 0. On failure,return -1.
*/
int uthread_init(int quantum_usecs)
{
    Thread *mainThread;
    int fail_succeed = 0;
    // negative quantum is an error
    if (quantum_usecs < 0)
    {
        fail_succeed = -1;
        std::cerr << "thread library error: negative quantum input" << std::endl;
        return fail_succeed;
    }

    sa.sa_handler = &timer_handler;
    // create signal set
    if (sigemptyset(&sa.sa_mask) == -1)
    {
        std::cerr << "system error: sigemptyseta error." << std::endl;
        terminate_main_thread(1, true);
    }

    // failing sigaction is a system error
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
    {
        std::cerr << "system error: sigaction error." << std::endl;
        terminate_main_thread(1, true);
    }

    // Configure the timer to expire correctly */
    timer.it_value.tv_sec = quantum_usecs / 1000000;        // first time interval, seconds part
    timer.it_value.tv_usec =
            quantum_usecs % 1000000;        // first time interval, microseconds part
    // configure the timer to expire correctly after that.
    timer.it_interval.tv_sec = quantum_usecs / 1000000;  // following time intervals, seconds part
    timer.it_interval.tv_usec =
            quantum_usecs % 1000000;    // following time intervals, microseconds part

    //initialize available ID
    for (int i = 1; i < MAX_THREAD_NUM; i++)
    {
        available_id_pq.push(i);
    }

    // create main thread 0
    mainThread = new Thread();
    all_threads[0] = mainThread;
    running = mainThread;

    if (sigemptyset(&signals_set) == -1)
    {
        std::cerr << "system error: signal set error." << std::endl;
        terminate_main_thread(1, true);
    }

    if (sigaddset(&signals_set, SIGVTALRM) == -1)
    {
        std::cerr << "system error: signal set error." << std::endl;
        terminate_main_thread(1, true);
    }

    // start timer
    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))
    {
        std::cerr << "system error: setitimer error." << std::endl;
        terminate_main_thread(1, true);
    }

    return fail_succeed;
}


/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)())
{

    block_signals();    // begin of critical code - block signals
    Thread *spawned_thread;
    int thread_id = available_id_pq.top();
    int return_num = thread_id;

    // trying to spawn more than MAX_THREAD_NUM threads is an error
    if (thread_id == MAX_THREAD_NUM)
    {
        std::cerr << "thread library error: too many threads spawned. Reached maximum" << std::endl;
        resume_signals();
        return -1;
    }
        // otherwise, create a new thread
    else
    {
        available_id_pq.pop();
        spawned_thread = new Thread(thread_id, f);
        all_threads[thread_id] = spawned_thread;
        ready_threads.push_back(spawned_thread);
    }
    resume_signals();       // end of critical code, signals resumed
    return return_num;
}


/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered as an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid)
{
    block_signals();    // begin of critical code - block signals
    if (tid == 0)
    {
        resume_signals();   // end of critical code, signals resumed
        terminate_main_thread(0, false);
    }

    // if the given tid does not exist - it is an error
    if (all_threads.count(tid) == 0)
    {
        std::cerr << "thread library error: input id for termination not found" << std::endl;
        resume_signals();   // end of critical code, signals resumed
        return -1;
    }
        // otherwise, id found, lets kill it.
    else
    {
        Thread *t_thread;
        t_thread = all_threads[tid];
        all_threads.erase(tid);
        // release all synced threads that waited for this thread to be terminated:
        unsync_all_dependants(t_thread);

        // handle case of a thread terminating itself - terminate and jump to scheduler:
        if (t_thread == running)
        {

            delete t_thread;
            running = nullptr;
            available_id_pq.push(tid);
            resume_signals();   // end of critical code, signals resumed
            scheduler();
            return 0;
        }
            // otherwise, only terminate:
        else
        {

            available_id_pq.push(tid);

            // if in ready threads - erase from there
            if (blocked_threads.count(tid) == 0)
            {
                uint16_t ready_thread_idx;
                for (ready_thread_idx = 0;
                     (static_cast<unsigned int>(ready_thread_idx) <
                      ready_threads.size()); ready_thread_idx++)
                {
                    if (ready_threads[ready_thread_idx] == t_thread)
                    {
                        break;
                    }
                }
                ready_threads.erase(ready_threads.begin() + ready_thread_idx);

            }
                // if blocked, remove from blocked threads
            else
            {
                blocked_threads.erase(tid);
            }
            // de-alloc memory
            delete t_thread;
        }

    }
    resume_signals();   // end of critical code, signals resumed
    return 0;
}


/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered as an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
    int ready_thread_idx;
    block_signals();        // begin of critical code - block signals

    // if no id tid exists - error
    if (all_threads.count(tid) == 0)
    {
        std::cerr << "thread library error: input id for BLOCK not found." << std::endl;
        resume_signals();   // end of critical code, signals resumed
        return -1;
    }

    // trying to block the main thread - error
    if (tid == 0)
    {
        std::cerr << "thread library error: tried to block main thread." << std::endl;
        resume_signals();   // end of critical code, signals resumed
        return -1;
    }

    // set block flag on, even if in blocked (may have only been synced)
    Thread *t_thread;
    t_thread = all_threads[tid];
    t_thread->set_block(true);

    // if already in blocked - nothing more to do
    if (blocked_threads.count(tid) != 0)
    {
        resume_signals();   // end of critical code, signals resumed
        return 0;
    }

    // otherwise, move to blocked
    t_thread->set_state(State::Blocked);
    blocked_threads[tid] = t_thread;

    // if a running thread is blocking itself - jump to scheduler
    if (t_thread == running)
    {
        resume_signals();
        scheduler();
        return 0;
    }

    // if not, it is in the ready dequeue. eraese it from there
    for (ready_thread_idx = 0;
         (static_cast<unsigned int>(ready_thread_idx) < ready_threads.size()); ready_thread_idx++)
    {
        if (ready_threads[ready_thread_idx] == t_thread)
        {
            break;
        }
    }
    ready_threads.erase(ready_threads.begin() + ready_thread_idx);

    resume_signals();       // end of critical code, signals resumed
    return 0;

}


/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered as an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{
    block_signals();        // begin of critical code - block signals
    Thread *t_thread;

    // if no id tid exists - error
    if (all_threads.count(tid) == 0)
    {
        std::cerr << "thread library error: input id for RESUME not found" << std::endl;
        resume_signals();
        return -1;
    }


    t_thread = all_threads[tid];

    if (t_thread->get_state() == State::Blocked)
    {
        // handle:
        t_thread->set_block(false);
        block_sync_decision(tid);
    }
    // if not blocked-the state is READY or RUNNING.This is not an error and therefore not handled.

    resume_signals();
    return 0;
}


/*
 * Description: This function blocks the RUNNING thread until thread with
 * ID tid will move to RUNNING state (i.e.right after the next time that
 * thread tid will stop running, the calling thread will be resumed
 * automatically). If thread with ID tid will be terminated before RUNNING
 * again, the calling thread should move to READY state right after thread
 * tid is terminated (i.e. it won’t be blocked forever). It is considered
 * as an error if no thread with ID tid exists or if the main thread (tid==0)
 * calls this function. Immediately after the RUNNING thread transitions to
 * the BLOCKED state a scheduling decision should be made.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_sync(int tid)
{
    block_signals();        // begin of critical code - block signals
    Thread *t_thread;

    // if no id tid exists - error
    if (all_threads.count(tid) == 0)
    {
        std::cerr << "thread library error: input id for SYNC not found" << std::endl;
        resume_signals();
        return -1;
    }
    // sync 0 is illegal
    if (tid == 0)
    {
        std::cerr << "thread library error: main thread cannot be synced" << std::endl;
        resume_signals();
        return -1;
    }
    // if trying to sync itself:
    if (tid == running->get_id())
    {
        std::cerr << "thread library error: a thread is trying to sync itself" << std::endl;
        resume_signals();
        return -1;
    }
    // handle:
    t_thread = all_threads[tid];
    running->set_state(State::Blocked);
    running->set_sync(true);
    t_thread->add_dependancy(running->get_id());

    blocked_threads[running->get_id()] = running;
    resume_signals();
    scheduler();
    return 0;

}


/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid()
{
    return running->get_id();
}


/*
 * Description: This function returns the total number of quantums that were
 * started since the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums()
{
    return totalQuantums;
}


/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered as an error.
 * Return value: On success,return the number of quantums of the thread with ID tid.
 * On failure, return -1.
 *
*/
int uthread_get_quantums(int tid)
{
    if (all_threads.count(tid) == 0)
    {
        std::cerr << "thread library error: get_quantums of non existing thread" << std::endl;
        return -1;
    }
    return all_threads[tid]->get_quantums();
}



//          ***    PRIVATE LIBRARY FUNCTIONS    ***

/**
 * This function is the "heart" of the library.
 * It acts according to the state of the running Thread:
 *      - if nullptr, the running thread has terminated itself.
 *      Therefore, only a new thread should begin to run.
 *      - if READY, current thread will be pushed to the back of the ready threads,
 *      and the first of ready threads will begin running.
 *      - if BLOCKED, current thread was already put in blocked list.
 *      The 1st on ready list will begin running.
 */
void scheduler()
{


    State cur_state;
    int return_flag = 0;
    totalQuantums++;
    // if there is only the main thread running
    if (ready_threads.empty())
    {
        // add quantum and jump back to main
        running->add_quantum();
        if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))
        {
            std::cerr << "system error: timer failure" << std::endl;
            terminate_main_thread(1, false);
        }
        siglongjmp(*(running->get_env()), 1);
    }
        // otherwise - there is some switching to do!
    else
    {
        Thread *next_ready = ready_threads.front();
        ready_threads.pop_front();

        //handle terminated - running is nullptr
        if (running == nullptr)
        {
            return_flag = 0;
        }
            // handle states ready (= time jump) and blocked
        else
        {
            cur_state = running->get_state();

            switch (cur_state)
            {

                case State::Ready:
                    ready_threads.push_back(running);
                    return_flag = sigsetjmp(*(running->get_env()), 1);
                    if (return_flag == 1)
                    {
                        return;
                    }
                    break;

                case State::Blocked:
                    return_flag = sigsetjmp(*(running->get_env()), 1);
                    if (return_flag == 1)
                    {
                        return;
                    }
                    break;

                default: // should not reach here
                    std::cerr << "thread library error: problem with scheduler." << std::endl;
                    terminate_main_thread(1, false);
                    break;
            }
        }

        // after saving current state (using sigsetjmp),
        // the return flag is 0 and we want to jump to the next Thread
        if (return_flag == 0)
        {
            running = next_ready;
            running->set_state(State::Running);
            //add a quantum to the upcoming running thread.
            running->add_quantum();
            // activate timer and jump
            if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))
            {
                std::cerr << "system error: timer failure" << std::endl;
                terminate_main_thread(1, false);
            }
            siglongjmp(*(running->get_env()), 1);
        }
    }
}


/**
 * This function is called when a thread is terminated.
 * the function takes all the threads in the dependancy list and mark them "unsynced".
 */
void unsync_all_dependants(Thread *t_thread)
{
    std::vector<int> dependant_lst = t_thread->get_depend_vec();
    Thread *tmp;
    for (std::vector<int>::size_type i = 0; i != dependant_lst.size(); i++)
    {
        tmp = all_threads[dependant_lst[i]];
        tmp->set_sync(false);
        block_sync_decision(dependant_lst[i]);
    }
}


/**
 * This function decides if to resume a thread.
 * In the mail it was clarified that a thread that was both blocked
 * and synced - must meet up with both requirements before becoming ready again
 * (i.e. sync thread must be terminated AND current thread has to be resumed).
 * This function transfers the thread back to ready list iff both sync-flag and block-flag are down.
 */
void block_sync_decision(int tid)
{
    Thread *thread = all_threads[tid];
    if (!(thread->is_block() | thread->is_sync()))
    {    // not sync & not blocked
        thread->set_state(State::Ready);
        ready_threads.push_back(thread);
        blocked_threads.erase(thread->get_id());
    }
}


/**
 * This function "cleans" all memory allocation.
 * It is called any time we are about to exit the library.
 * "init flag" determines wheather we arrived from the init function or not.
 */
void terminate_main_thread(int exit_code, bool init_flag)
{

    // clean all existing threads
    Thread *tmp;
    if (running != nullptr)
    {
        running->set_state(State::Running);
    }
    std::map<int, Thread *>::iterator threadIt;
    while (!all_threads.empty())
    {
        threadIt = all_threads.begin();

        switch (threadIt->second->get_state())
        {

            case (State::Ready) :
                ready_threads.pop_back();

                tmp = all_threads[threadIt->second->get_id()];
                all_threads.erase(threadIt->second->get_id());
                delete tmp;
                break;
            case (State::Running) :

                all_threads.erase(threadIt);

                break;
            case (State::Blocked) :
                blocked_threads.erase(threadIt->second->get_id());
                tmp = all_threads[threadIt->second->get_id()];
                all_threads.erase(threadIt->second->get_id());
                delete tmp;
                break;


        }
    }
    all_threads.clear();
    blocked_threads.clear();
    ready_threads.clear();
    delete running;
    running = nullptr;

    if (!init_flag)
    {
        resume_signals();
    }
    exit(exit_code);
}


/**
 * This function blocks signals from OS. It is called upon entry to critical code sections,
 * where the library's data structures are not stable.
 */
void block_signals()
{
    if (sigprocmask(SIG_BLOCK, &signals_set, nullptr) == -1)
    {
        std::cerr << "system error: block signals failure" << std::endl;
        exit(1);
    }
}

/**
 * This function resumes signals (and timer) to handle of OS.
 * It is called upon finishing any critical code section.
 */
void resume_signals()
{
    if (sigprocmask(SIG_UNBLOCK, &signals_set, nullptr) == -1)
    {
        std::cerr << "system error: unblock signals failure" << std::endl;
        exit(1);
    }
}
