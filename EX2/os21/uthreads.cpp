#include <map>
#include <queue>
#include <cstring>
#include "uthreads.h"




#define SETITIMER_ERR std::ostream(nullptr)
int totalQuantums;
std::map<int, Thread *> all_threads;
std::map<int, Thread *> blocked_threads;
std::deque<Thread *> ready_threads;
std::priority_queue<int, std::vector<int>, std::greater<int> > available_id_pq;
sigset_t signals_set;

//int used_indexes[MAX_THREAD_NUM];

Thread *running;
struct sigaction sa;
struct itimerval timer;



void scheduler();
void terminate_main_thread(int exit_code, bool init_flag);
void block_sync_decision(int tid);
void unsync_all_dependants(Thread * t_thread);
void block_signals();
void resume_signals();


void all_threads_printer();
void ready_printer();
void blocked_printer();



//
//struct{
//    int id;
//    std::stack MAX_THREAD_NUM
//};


void block_signals(){
    if( sigprocmask(SIG_BLOCK, &signals_set, NULL) == -1){
        std::cerr << "system error: block signals failure" << std::endl;
        exit(1);
    }
}

void resume_signals(){
    if( sigprocmask(SIG_UNBLOCK, &signals_set, NULL) == -1){
        std::cerr << "system error: unblock signals failure" << std::endl;
        exit(1);
    }
}

/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
void timer_handler(int sig)
{
    //gotit = 1;
    std::cout << "Time Handler called\n" << std::endl;
    running->save_dataFrame();
    running->set_state(State::Ready);
    scheduler();
}

void scheduler()
{
    State cur_state;
    int return_flag = 0;
    totalQuantums++;
    // if there is only the main thread running
//    if (ready_threads.size() == 0)
    if (ready_threads.empty())
    {
        // add quantum and jump back to main
        running->add_quantum();
        if (setitimer(ITIMER_VIRTUAL, &timer, NULL))     // TODO look at brian informative error
        {
            std::cerr << "system error: timer failure" << std::endl;
            terminate_main_thread(1, false);
//            exit(1);
//            printf("setitimer error.");
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
        // handle states ready (time jump) and blocked
        else {
            cur_state = running->get_state();

            switch (cur_state) {

                case State::Ready:

                    ready_threads.push_back(running);

                    return_flag = sigsetjmp(*(running->get_env()), 1);  // TODO look at brian informative error
                    if (return_flag == 1) {
                        return;
                    }
                    break;

                case State::Blocked:
//                    blocked_threads[running->get_id()] = running;      - this row is already done locally
                    return_flag = sigsetjmp(*(running->get_env()), 1);  // TODO look at brian informative error
                    if (return_flag == 1) {
                        return;
                    }
                    break;

                default:
                    //Throw Error -  not a state
                    break;
            }
        }


        if (return_flag == 0)
        {
            running = next_ready;
            running->set_state(State::Running);
            //add total quantoms and the specific running thread.
            totalQuantums++;
            running->add_quantum();
            // activate timer and jump
            if (setitimer(ITIMER_VIRTUAL, &timer, NULL))     // TODO look at brian informative error
            {
//                printf("setitimer error.");
                std::cerr << "system error: timer failure" << std::endl;
                terminate_main_thread(1, false);

//                exit(1);
            }
            siglongjmp(*(running->get_env()), 1); // look at brian informative error
        }


        }
}




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
        // return error message
    }
//    else
//    {
        try {
            all_threads = *new std::map<int, Thread *>();       // TODO - CATCH EACH ONE SEPARATELY
            blocked_threads = *new std::map<int, Thread *>();
            ready_threads = *new std::deque<Thread *>();
        }
        catch(std::bad_alloc&) {
            std::cerr << "system error: allocation error." << std::endl;
//            terminate_main_thread(1);

            exit(1);
        }

        sa.sa_handler = &timer_handler;



    if (sigemptyset(&sa.sa_mask)== -1)
    {
        std::cerr << "system error: sigemptyseta error." << std::endl;

        terminate_main_thread(1, true);

    }


    if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
            std::cerr << "system error: sigaction error." << std::endl;
            terminate_main_thread(1, true);

//            exit(1);
        }



        // Configure the timer to expire correctly */
        timer.it_value.tv_sec = quantum_usecs/1000000;        // first time interval, seconds part
        timer.it_value.tv_usec = quantum_usecs % 1000000;        // first time interval, microseconds part

        // configure the timer to expire correctly after that.
        timer.it_interval.tv_sec = quantum_usecs/1000000;    // following time intervals, seconds part
        timer.it_interval.tv_usec = quantum_usecs % 1000000;    // following time intervals, microseconds part

        //initialize available ID
        for (int i = 1; i < MAX_THREAD_NUM; i++)
        {
            available_id_pq.push(i);
        }

        // create main thread 0
        mainThread = new Thread();
        all_threads[0] = mainThread;
//        ready_threads.push_back(mainThread);
        running = mainThread;
//        scheduler();

        // add signal interrupt block bullshit, and resume interrupt bullshit.
        std::cout << "init created. return val of init: " << fail_succeed << std::endl;

        if ( sigemptyset(&signals_set) == -1){
            std::cerr << "system error: signal set error." << std::endl;
            terminate_main_thread(1,true);

        }
    if ( sigaddset(&signals_set,SIGVTALRM) == -1){
        std::cerr << "system error: signal set error." << std::endl;
        terminate_main_thread(1,true);

    }



    if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
            std::cerr << "system error: setitimer error." << std::endl;
            terminate_main_thread(1,true);

//            exit(1);
        }

        return fail_succeed;

//    }

    return fail_succeed;
    // activate timer - itimer line 31

    // create thread 0 from main - without pc line
    // point to a function when the signal occures - line 26

}
//void add_all_quantoms(Thread* thread){
//    totalQuantums++;
//    thread->add_quantum();
//}


int uthread_spawn(void (*f)())
{
    block_signals();
    Thread *spawned_thread;
    int thread_id = available_id_pq.top();
    int return_num = thread_id;

    if (thread_id == MAX_THREAD_NUM)
    {
        return_num = -1;
        std::cerr << "thread library error: too many threads spawned. Reached maximum" << std::endl;
        resume_signals();
        return return_num;
    }
    else
    {
        available_id_pq.pop();
        spawned_thread = new Thread(thread_id, f);
        all_threads[thread_id] = spawned_thread;
        ready_threads.push_back(spawned_thread);
//        return thread_id;
        //resume timer ----!!!

    }
    std::cout << "created new thread.   id: " << thread_id << std::endl;
    resume_signals();
    return return_num;


    // call a function that finds next available space - sorted by id
    // create new thread, and add it to the end of ready list


    // sigset - maybe move from Thread
}


int uthread_terminate(int tid)
{
    block_signals();
    if (tid == 0)
    {
        resume_signals();
        terminate_main_thread(0, false);
        //terminate_main_thread
    }


    if (all_threads.count(tid) == 0) // id not found
    {
        // fprintf(stderr, "");     // TODO CHANGE ALL PRINTS TO THIS
        std::cerr << "thread library error: input id for termination not found" << std::endl;
        resume_signals();
        return -1;
    }
    else
    {
        // otherwise, id found, lets kill it.
        Thread *t_thread;
        t_thread = all_threads[tid];
        // release all synced threads that waited for this thread to be terminated:
        unsync_all_dependants(t_thread);

        if (t_thread == running)
        {
            all_threads.erase(tid);
            t_thread->clean_thread();
            running = nullptr;
            available_id_pq.push(tid);
            std::cout << "terminating running thread" << std::endl;
            resume_signals();
            scheduler();
            return 0;
        }
        else
        {
            all_threads.erase(tid);
            available_id_pq.push(tid);
            // if in ready threads - erase from there
            if (blocked_threads.count(tid) == 0)
            {
                int ready_thread_idx;
                for (ready_thread_idx = 0;
                     ready_thread_idx < ready_threads.size(); ready_thread_idx++)
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
            // remove dependencies
            t_thread->clean_thread();


        }

    }
    std::cout << "thread " << tid << " teminated" << std::endl;
    resume_signals();
    return 0;
    // delete stack and dependancy
    // remove from all_threads etc
    // resume all synced - by order
    // term main - exit(0)
    // if terminate is the running - go to scheduler

    // in scheduler -    reset timer, ++ quantums and take first


}

// if t_thread is terminated, we will take all the threads in the dependancy list and mark them "unsynced".
void unsync_all_dependants(Thread * t_thread){
    std::vector<int> dependant_lst = t_thread->get_depend_vec();
    Thread *tmp;
    for(std::vector<int>::size_type i = 0; i != dependant_lst.size(); i++)
    {
        tmp = all_threads[dependant_lst[i]];
        tmp->set_sync(false);
        block_sync_decision(dependant_lst[i]);
    }

}

void block_sync_decision(int tid){
    Thread* thread = all_threads[tid];
    if(!(thread->is_block()|thread->is_sync())) {    // not sync & not blocked
        thread->set_state(State::Ready);
        ready_threads.push_back(thread);
        blocked_threads.erase(thread->get_id());
    }


}


int uthread_block(int tid) {
    int ready_thread_idx;
    // block running
    block_signals();

    // if no id tid exists - error
    if (all_threads.count(tid) == 0)
    {
        std::cerr << "thread library error: input id for BLOCK not found." << std::endl;
        resume_signals();
        return -1;
    }

    // trying to block the main thread - error
    if (tid == 0){
        std::cerr << "thread library error: tried to block main thread." << std::endl;
        resume_signals();
        return -1;
    }

    // set block flag on, even if in blocked (may have only been synced)
    Thread *t_thread;
    t_thread = all_threads[tid];
    t_thread->set_block(true);

    // if already in blocked - nothing more to do
    if (blocked_threads.count(tid) != 0) {
        resume_signals();
        return 0;
    }

    // otherwise, move to blocked
    t_thread->set_state(State::Blocked);
    blocked_threads[tid] = t_thread;

    // if a running thread is blocking itself - jump to scheduler
    if (t_thread == running){
        resume_signals();
        scheduler();
        return 0;
    }

    // if not, it is in the ready dequeue. eraese it from there
    for (ready_thread_idx = 0;
         ready_thread_idx < ready_threads.size(); ready_thread_idx++)
    {
        if (ready_threads[ready_thread_idx] == t_thread)
        {
            break;
        }
    }
    ready_threads.erase(ready_threads.begin() + ready_thread_idx);




    std::cout << "blocking thread id: " << tid << std::endl;
    resume_signals();
    return 0;

}



int uthread_resume(int tid){
    block_signals();
    Thread *t_thread;


    // if no id tid exists - error
    if (all_threads.count(tid) == 0)
    {
        std::cerr << "thread library error: input id for RESUME not found" << std::endl;
        resume_signals();
        return -1;
    }

    t_thread = all_threads[tid];
    if (t_thread->get_state() == State ::Blocked){
        // handle:
        t_thread->set_block(false);
        block_sync_decision(tid);
    }
    else{
        // not an error, do nothing
    }
    std::cout << "resuming thread " << tid << "  (maybe)"<< std::endl;
    resume_signals();
    return 0;


}


int uthread_sync(int tid){
    block_signals();
    Thread *t_thread;
    int ready_thread_idx;

    // if no id tid exists - error
    if (all_threads.count(tid) == 0)
    {
        std::cerr << "thread library error: input id for SYNC not found" << std::endl;
        resume_signals();
        return -1;
    }
    // sync 0 is illegal
    if (tid == 0){
        std::cerr << "thread library error: main thread cannot be synced" << std::endl;
        resume_signals();
        return -1;
    }
    // if trying to sync itself:
    if(tid == running->get_id()){
        std::cerr << "thread library error: a thread is trying to sync itself" << std::endl;
        resume_signals();
        return -1;
    }
    // handle:
    t_thread = all_threads[tid];
    running->set_state(State::Blocked);
    running->set_sync(true);
    t_thread->add_dependancy(running->get_id());

//    // if not in blocked - erase from ready deque
//    if (blocked_threads.count(tid) == 0) {
//
//        for (ready_thread_idx = 0;
//             ready_thread_idx < ready_threads.size(); ready_thread_idx++) {
//            if (ready_threads[ready_thread_idx] == running) {
//                break;
//            }
//        }
//        ready_threads.erase(ready_threads.begin() + ready_thread_idx);
//
//    }
    blocked_threads[running->get_id()] = running;
    resume_signals();
    scheduler();
    return 0;

}


int uthread_get_tid(){
    return running->get_id();
}


int uthread_get_total_quantums(){
    return totalQuantums;
}


int uthread_get_quantums(int tid){
    if (all_threads.count(tid) == 0)
    {
        std::cerr << "thread library error: get_quantums of non existing thread" << std::endl;
        return -1;
    }
    return all_threads[tid]->get_quantums();
}




void terminate_main_thread(int exit_code, bool init_flag)
{

    for (auto &all_thread : all_threads)
    {
        all_thread.second->clean_thread();
    }
    all_threads.clear();
    blocked_threads.clear();
    ready_threads.clear();
    blocked_threads;

//    delete &sa;
//    delete &timer;
    std::cout << "terminating main";
    if (!init_flag){
        resume_signals();
    }
    exit(exit_code);

}

void all_threads_printer(){
    std::cout << "all threads: " << std::endl;
    for (auto &all_thread : all_threads) {
        int id = all_thread.second->get_id();
        std::cout << id << " ";
    }
    std::cout << std::endl;
}

void ready_printer(){
    std::cout << "ready threads: " << std::endl;
    for (auto &ready_thread : ready_threads){
        int id = ready_thread->get_id();
        std::cout << id << " ";
    }
    std::cout << std::endl;
}

void blocked_printer(){
    std::cout << "blocked threads: " << std::endl;
    for (auto &blocked_thread : blocked_threads){
        int id = blocked_thread.second->get_id();
        std::cout << id << " ";
    }
    std::cout << std::endl;
}
