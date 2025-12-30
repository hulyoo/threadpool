#include <iostream>
#include "threadpool.h"


int main()
{
    ThreadPool pool;
    pool.start();

    auto res = pool.submit([]()->int{
        return 100 + 200;
    });

    auto res2 = pool.submit([]()->int{
        return 100000 + 20000000;
    });

    std::cout<<"res = "<<res.get()<<std::endl;
    std::cout<<"res2 = "<<res2.get()<<std::endl;

    return 0;
}