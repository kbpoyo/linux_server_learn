#include "thread_pool.hpp"

using namespace thread_pool;


int main() {

    thread_pool::thread_pool test_case(10, 10, 10);

    test_case.fun();

    test_case.const_fun();

    test_case.static_fun();




    return 0;

}