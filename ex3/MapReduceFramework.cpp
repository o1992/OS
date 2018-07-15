
#include <pthread.h>
#include "MapReduceClient.h"
#include "Barrier.h"
#include <cstdio>
#include <atomic>
#include <stdexcept>
#include <algorithm>
#include <queue>
#include <semaphore.h>
#include <iostream>

/**
 * This structure contains all the necessary data for the Data Framework
 */
struct ThreadContext
{
    int threadID;
    Barrier *barrier;
    std::atomic<int> *atomic_counter;
    const InputVec *inputVec;
    std::vector<IntermediateVec> *inter_vec_list;       // pointer to a vector of intermediate vectors
    IntermediateVec *interVec;
    const MapReduceClient *client;
    int multiThreadLevel;
    std::vector<IntermediateVec> *to_reduce_queue;
    pthread_mutex_t *lock_queue;
    pthread_mutex_t *lock_outputvec;
    sem_t *sem_jobs;
    bool *finished_shuffle;
    OutputVec *outputVec;
};


//      ###      PRIVATE FUNCTIONS      ###

void do_map(ThreadContext *tc);

void do_reduce(ThreadContext *tc);

void *thread_operation(void *arg);

void do_sort(ThreadContext *tc);

void shuffle(ThreadContext *);

void clean_and_exit(ThreadContext *tc);



//      ###      FUNCTIONS IMPLEMENTATION      ###


/**
 * This function receives a K2 key, a V2 value and a context. It creates a pair and adds
 * it to the context.
 */
void emit2(K2 *key, V2 *value, void *context)
{
    auto *tc = (ThreadContext *) context;
    IntermediatePair pair;
    pair.first = key;
    pair.second = value;
    tc->interVec->push_back(pair);
}


/**
 *  * This function receives a K3 key, a V3 value and a context. It creates a pair and adds
 *  it to the context.
 */
void emit3(K3 *key, V3 *value, void *context)
{
    auto *tc = (ThreadContext *) context;
    OutputPair pair;
    pair.first = key;
    pair.second = value;

    // lock mutex before adding to the output vector
    if (pthread_mutex_lock(tc->lock_outputvec) != 0)
    {
        std::cerr << "unable to lock mutex for output vector";
        clean_and_exit(tc);
        exit(-1);
    };
    tc->outputVec->push_back(pair);
    // unlock mutex
    if (pthread_mutex_unlock(tc->lock_outputvec) != 0)
    {
        std::cerr << "unable to unlock mutex for output vector";
        clean_and_exit(tc);
        exit(-1);
    }
}


/**
 * This function is called when a fail along the program occures.
 * It "cleans" the mutexes and semaphore, and then exits.
 */
void clean_and_exit(ThreadContext *tc)
{

    if (pthread_mutex_destroy(tc->lock_queue) != 0)
    {
        std::cerr << "cannot destroy mutex";
        exit(-1);

    }
    if (pthread_mutex_destroy(tc->lock_outputvec) != 0)
    {
        std::cerr << "cannot destroy mutex";
        exit(-1);
    }
    if (sem_destroy(tc->sem_jobs) != 0)
    {
        std::cerr << "cannot destroy semaphore";
        exit(-1);
    }
}


/**
 * This function takes the input vector and handles a single input from the input ec each time.
 * The atomic counter mechanism prevents race conditions and "double handle".
 */
void do_map(ThreadContext *tc)
{
    unsigned int current_idx;
    const InputVec *inputVec;
    inputVec = (tc->inputVec);
    const MapReduceClient *client = (tc->client);

    // as long as there are unhandled inputs - keep handling
    while ((unsigned int) (*(tc->atomic_counter)) < (tc->inputVec->size())) // operator '!=' changed to '<'
    {
        current_idx = (unsigned int)(*(tc->atomic_counter))++;
        if (current_idx <  (tc->inputVec->size()))
        {
            client->map(inputVec->at(current_idx).first, inputVec->at(current_idx).second, tc);
        }
    }
}


/**
 * This function sorts a given vector (each thread has an intermediate vector, this function sorts
 * a single thread's vector.
 */
void do_sort(ThreadContext *tc)
{
    std::sort(tc->interVec->begin(), tc->interVec->end(),
              [](IntermediatePair a, IntermediatePair b)
              {
                  return (*(a.first) < *(b.first));
              });
}


/**
 * This function is the thread's "main function". It determines it's action/
 * Each thread maps vectors from the input, and then sorts them into intermediate vectors.
 * Then, thread 0 shuffles these vectors, and the other vectors reduce (when needed).
 */
void *thread_operation(void *arg)
{
    auto *tc = (ThreadContext *) arg;
    do_map(tc);
    do_sort(tc);
    tc->barrier->barrier();

    if (tc->threadID == 0)
    {
        shuffle(tc);
    }

    do_reduce(tc);

    return nullptr;
}


/**
 * This function takes the "work vectors" of the threads, that contain sorted intermediate vectors.
 * This function creates a new vector of all occurrences of the largest key, and puts them into the
 * "to_reduce" queue. After every such add, it calls "sem_post" for some other vector to reduce.
 */
void shuffle(ThreadContext *tc)
{
    std::vector<IntermediateVec> *inter_vec_list = (tc->inter_vec_list);
    IntermediateVec single_type;

    // as long as there are vectors in the intermediate_vector_list - keep creating new single_key vectors
    while (!inter_vec_list->empty())
    {
        single_type.clear();
        bool first_run = true;

        // find largest key of vectors:
        K2 *current_max_key;
        for (unsigned int i = 0; i < inter_vec_list->size(); i++)
        {
            if (inter_vec_list->at(i).empty())
            {
                continue;
            }
            if (first_run)
            {
                current_max_key = inter_vec_list->at(i).back().first;
                first_run = false;
            }
            if (*current_max_key < *inter_vec_list->at(i).back().first)
            {
                current_max_key = inter_vec_list->at(i).back().first;
            }
        }
        // we are holding the current max.

        // build a new vector of all keys of type "current max":
        for (unsigned int i = 0; i < inter_vec_list->size(); i++)
        {
            if (!inter_vec_list->at(i).empty())
            {
                //
                while ((!(*inter_vec_list->at(i).back().first < *current_max_key))
                       &&
                       (!(*current_max_key < *inter_vec_list->at(i).back().first)))
                {
                    single_type.push_back(std::move(inter_vec_list->at(i).back()));
                    inter_vec_list->at(i).pop_back();
                    if (inter_vec_list->at(i).empty())
                    {
                        break;
                    }
                }
            }

        }
        // if an intermediate vector is empty - delete it
        int offset = 0;
        int cur_place;
        for (unsigned int i = 0; i < inter_vec_list->size(); i++)
        {
            cur_place = i - offset;
            // if an intermediate vector is empty - delete it
            if (inter_vec_list->at(cur_place).empty())
            {
                inter_vec_list->at(cur_place).clear();
                inter_vec_list->erase(inter_vec_list->begin() + cur_place);
                offset++;
            }
        }
        if (single_type.empty())
        {
            continue;
        }

        // we now created the new "single-key" vector.
        // lock mutex, push new vector into to_reduce queue and unlock:
        if (pthread_mutex_lock(tc->lock_queue) != 0)
        {
            std::cerr << "unable to lock mutex for to_reduce queue";
            clean_and_exit(tc);
            exit(-1);
        }
        tc->to_reduce_queue->push_back(
                single_type); // - add this new vector of the same key to the queue
        if (pthread_mutex_unlock(tc->lock_queue) != 0)
        {
            std::cerr << "unable to unlock mutex for to_reduce queue";
            clean_and_exit(tc);
            exit(-1);
        }

        // there is now a mission waiting for handle, therefore wake a waiting thread
        if (sem_post(tc->sem_jobs) != 0)
        {
            std::cerr << "unable to post for semaphore";
            clean_and_exit(tc);
            exit(-1);
        };
    }
    *(tc->finished_shuffle) = true;

}


/**
 * This function reduces the intermediate vectors via the client's "reduce" function, and pushes the result into
 * the output vector. This function uses the semaphore for thread wait (while the queue is empty) and a mutex
 * for the output vector.
 */
void do_reduce(ThreadContext *tc)
{
    // enter loop as long as the shuffle is not finished, OR as long as the reduce queue is not empty.
    // if both are false - we are sure there will be no more missions, and the thread can die in piece.
    while (!(*tc->finished_shuffle) || !tc->to_reduce_queue->empty())
    {
        // wait for jobs
        if (sem_wait(tc->sem_jobs) != 0)
        {
            std::cerr << "semaphore wait failed";
            clean_and_exit(tc);
            exit(-1);
        }

        // if reached here, it is because the OS gave us an input to handle.
        // Lock mutex, pull a vector out of the queue (+ send to client's "reduce" function), and unlock.
        if (pthread_mutex_lock(tc->lock_queue) != 0)
        {
            std::cerr << "unable to lock mutex for to_reduce queue";
            clean_and_exit(tc);
            exit(-1);
        }
        if (!tc->to_reduce_queue->empty())
        {
            IntermediateVec iter_vec = std::move(tc->to_reduce_queue->back());
            (tc->to_reduce_queue)->pop_back();
            pthread_mutex_unlock(tc->lock_queue);
            tc->client->reduce(&iter_vec, tc);
        }
        else
        {
            if (pthread_mutex_unlock(tc->lock_queue) != 0)
            {
                std::cerr << "unable to unlock mutex for to_reduce queue";
                clean_and_exit(tc);
                exit(-1);
            }
        }
    }

    if (sem_post(tc->sem_jobs) != 0)
    {
        std::cerr << "unable to post for semaphore";
        clean_and_exit(tc);
        exit(-1);
    }
}


/**
 * This function runs this exercise. it receives a client, an input vector and an output vector.
 * It creates Threads, preforms map and reduce and puts the output into the output vector.
 */
void
runMapReduceFramework(const MapReduceClient &client, const InputVec &inputVec, OutputVec &outputVec,
                      int multiThreadLevel)
{

    pthread_mutex_t lock_queue;
    pthread_mutex_t lock_outputvec;
    bool finished_shuffle = false;
    //multiThreadLevel--; //main thread is deducted from the number of thread the user requested
    // initialize the queue mutex, the output vector mutex, and the semaphore:
    if (pthread_mutex_init(&lock_queue, nullptr) != 0)
    {
        std::cerr << "\n queue mutex init fail \n";
        exit(-1);
    }
    if (pthread_mutex_init(&lock_outputvec, nullptr) != 0)
    {
        std::cerr << "\n output vector mutex init fail \n";
        exit(-1);
    }
    sem_t sem_jobs;

    if (sem_init(&sem_jobs, 0, 0) != 0)
    {
        std::cerr << "\n semaphore init fail \n";
    }


    // initialize others:
    std::atomic<int> atomic_counter(0);
    std::vector<IntermediateVec> to_reduce_queue;

    pthread_t threads[multiThreadLevel];
    ThreadContext contexts[multiThreadLevel];
    Barrier barrier(multiThreadLevel);

    //create intermediary vectors
    std::vector<IntermediateVec> inter_vec_list;

    for (int i = 0; i < multiThreadLevel; ++i)
    {
        IntermediateVec thread_inter_vec;
        inter_vec_list.push_back(thread_inter_vec);

    }

    // create context for each thread:
    for (int i = 0; i < multiThreadLevel; ++i)
    {
        contexts[i] = {i, &barrier, &atomic_counter, &inputVec, &inter_vec_list, &inter_vec_list[i],
                       &client, multiThreadLevel, &to_reduce_queue, &lock_queue, &lock_outputvec,
                       &sem_jobs, &finished_shuffle, &outputVec};
    }

    // create threads:
    for (int i = 0; i < multiThreadLevel; ++i)
    {
        pthread_create(threads + i, nullptr, thread_operation /*Function of the thread */,
                       contexts + i);
    }

    // the main thread waits for all threads to finish running, and only then terminates
    for (int i = 0; i < multiThreadLevel; ++i)
    {
        if (pthread_join(threads[i], nullptr) != 0)
        {
            std::cerr << "unable to join thread";
            exit(-1);
        }
    }
}
