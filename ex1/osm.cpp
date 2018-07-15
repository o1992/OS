#include <sys/time.h>
#include <iostream>
#include <cmath>
#include "osm.h"

#define SUCCESS 0
#define FAILURE (-1)
#define CONST_ITER_NUM 5
#define DEFAULT_ITERS 1000
#define CONVERT_USEC 1000
#define CONVERT_SEC 1000000000


/* Initialization function that the user must call
 * before running any other library function.
 * The function may, for example, allocate memory or
 * create/open files.
 * Pay attention: this function may be empty for some desings. It's fine.
 * Returns 0 uppon success and -1 on failure
 */
int osm_init(){
    return SUCCESS;
}


/* finalizer function that the user must call
 * after running any other library function.
 * The function may, for example, free memory or
 * close/delete files.
 * Returns 0 uppon success and -1 on failure
 */
int osm_finalizer(){
    return SUCCESS;
}


/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_operation_time(unsigned int iterations){
    int remainder, calc=0;
    int is_time_ok;
    double final_time;

    if (iterations == 0){
        iterations = DEFAULT_ITERS;
    }

    struct timeval begin, end;

    is_time_ok = gettimeofday(&begin, NULL);
    if (is_time_ok == -1){
        return FAILURE;
    }

    remainder = iterations % CONST_ITER_NUM;
    if (remainder != 0) {
        iterations += CONST_ITER_NUM - (iterations % CONST_ITER_NUM);
    }
    for (unsigned int i = 0; i<iterations; i+=CONST_ITER_NUM){
        calc += 1;
        calc += 1;
        calc += 1;
        calc += 1;
        calc += 1;
    }

    is_time_ok = gettimeofday(&end, NULL);

    if (is_time_ok == -1){
        return FAILURE;
    }

    final_time = 1000 * ((end.tv_sec-begin.tv_sec)*1000000 + end.tv_usec-begin.tv_usec);
    final_time = final_time / iterations;

    return final_time;

}

void empty_func(){

}


/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_function_time(unsigned int iterations){
    int remainder;
    int is_time_ok;
    double final_time;

    if (iterations == 0){
        iterations = 1000;
    }

    struct timeval begin, end;

    is_time_ok = gettimeofday(&begin, NULL);
    if (is_time_ok == -1){
        return FAILURE;
    }

    remainder = iterations % CONST_ITER_NUM;
    if (remainder != 0) {
        iterations += CONST_ITER_NUM - (iterations % CONST_ITER_NUM);
    }
    for (unsigned int i = 0; i<iterations; i+=CONST_ITER_NUM){
        empty_func();
        empty_func();
        empty_func();
        empty_func();
        empty_func();
    }

    is_time_ok = gettimeofday(&end, nullptr);
    if (is_time_ok == -1){
        return FAILURE;
    }

    final_time = 1000 * ((end.tv_sec-begin.tv_sec)*1000000 + end.tv_usec-begin.tv_usec);
    final_time = final_time / iterations;

    return final_time;
}




/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_syscall_time(unsigned int iterations){
    int remainder;
    int is_time_ok;
    double final_time;

    if (iterations == 0){
        iterations = 1000;
    }

    struct timeval begin, end;

    is_time_ok = gettimeofday(&begin, nullptr);
    if (is_time_ok == -1){
        return FAILURE;
    }

    remainder = iterations % CONST_ITER_NUM;
    if (remainder != 0) {
        iterations += CONST_ITER_NUM - (iterations % CONST_ITER_NUM);
    }
    for (unsigned int i = 0; i<iterations; i+=CONST_ITER_NUM){
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
    }

    is_time_ok = gettimeofday(&end, nullptr);
    if (is_time_ok == -1){
        return FAILURE;
    }

    final_time = 1000 * ((end.tv_sec-begin.tv_sec)*1000000 + end.tv_usec-begin.tv_usec);
    final_time = final_time / iterations;


    return final_time;
}


