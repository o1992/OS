//
// Created by chen_by on 4/26/18.
//

#ifndef OS21_THREADS_H
#define OS21_THREADS_H


#include <vector>
#include <setjmp.h>
#include "uthreads.h"

typedef unsigned long address_t;


enum class State { Running, Blocked, Ready };

//class{

//    int sync_thread = -1;
//}; struct Thread

class Thread {

private:
    int id;
    std::vector<int>* dependancy;    // = sync list
    bool sync_flag;
    bool block_flag;
    char* _stack;
    State _state;
    int quantums;
    sigjmp_buf env;



public:
//    Thread(int id, State new_state, void (*f)(void));
    Thread(int id,  void (*f)());
    Thread();
    int get_id();
    State get_state();
    void set_state(State new_state);
    void add_dependancy(int dependant_id);
    void set_block(bool is_block);
    void set_sync(bool is_synced);

    int get_quantums();
    void add_quantum();
    address_t translate_address(address_t);
    bool is_sync();
    bool is_block();
    sigjmp_buf* get_env();
    void clean_thread();
    std::vector<int> get_depend_vec();



        void save_dataFrame();
    void jump_to();

//        push & pop from stack

};


#endif //OS21_THREADS_H
