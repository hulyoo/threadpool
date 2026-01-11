#include <iostream>
#include "threadpool.h"


class MyTask : public Task
{
public:
    MyTask() = default;
    ~MyTask() = default;

    Any run() override
    {
        int sum = 0;
        for(int i = 0; i < 100000; i++)
        {
            sum += i;
        }
        std::cout<<"mytask"<<std::endl;

        return sum;
    }
private:
};


int main()
{
    MyTask task1;
    ThreadPool pool;
    pool.start();
    Result res = pool.submit(std::make_shared<MyTask>(task1));
    // pool.submit(std::make_shared<MyTask>(task1));
    // pool.submit(std::make_shared<MyTask>(task1));
    // pool.submit(std::make_shared<MyTask>(task1));
    // pool.submit(std::make_shared<MyTask>(task1));
    // pool.submit(std::make_shared<MyTask>(task1));
    // pool.submit(std::make_shared<MyTask>(task1));
    // pool.submit(std::make_shared<MyTask>(task1));
    // pool.submit(std::make_shared<MyTask>(task1));
    //Result res1 = pool.submit(std::make_shared<MyTask>(task1));
    //Result res = pool.submit(std::make_shared<MyTask>(task1));
    

    int num = res.get().cast_<int>();

    std::cout<<"num = " <<num<<std::endl;
    getchar();

    return 0;
}