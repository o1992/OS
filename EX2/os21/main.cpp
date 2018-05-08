//
// Created by omerrubi on 4/30/18.
//

void f(){
    int x=5;
}

#include "main.h"
#include "uthreads.h"
int main1() {

    int ans = uthread_init(45);
//    std::cout << ans << std::endl ;

    uthread_spawn(&f);
    uthread_spawn(&f);
    uthread_spawn(&f);

    uthread_terminate(2);
    uthread_terminate(1);

    uthread_spawn(&f);
//    uthread_spawn(&f);
//    uthread_spawn(&f);



    uthread_block(3);


//
//    uthread_block(3);
//
    ready_printer();
    blocked_printer();
//
    uthread_resume(3);
//    uthread_resume(1);


//    uthread_terminate(0);


    ready_printer();
    blocked_printer();

    std::cout << "done!";
}



void f1(){
    int x = 1;
    while(x){
        x++;
        if (x%100000000 == 0)
        {
            std::cout<< "running f1" << std::endl;
        }
    }
}


void g1() {
    int x = 1;

    while (x) {
        x++;

        if (x % 100000000 == 0) {
            std::cout << "running g1" << std::endl;
        }
    }
}


int main(){
    int x = 1;
    uthread_init(2000000);
    uthread_spawn(&f1);
    uthread_spawn(&g1);
    while(x){
        x = x+0;
    }
}



//
//f - while true print "running f"
//g - same
//
//
//int main1(){
//    init 2M
//            spawn(f)
//                    uthread_spawn(g)
//                            while(true){
//
//                            }
//}


//
//
//sigset_t set;
//
//sigemptyset(&set);
//sigaddset(&set, SIGVTINT);
//sigaddset(&set, SIGTERM);
//sigprocmask(SIG_SETMASK, &set, NULL);
////blocked signals: SIGINT and SIGTERM
//
//sigemptyset(&set);
//sigaddset(&set, SIGVTINT);
//sigaddset(&set, SIGALRM);
//sigprocmask(SIG_BLOCK, &set, NULL);
////blocked signals: SIGINT, SIGTERM, SIGVTALRM
//
//sigemptyset(&set);
//sigaddset(&set, SIGTERM);
//sigaddset(&set, SIGUSR1);
//sigprocmask(SIG_UNBLOCK, &set, NULL);
////blocked signals: SIGINT and SIGALRM