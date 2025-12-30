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
    ThreadPool pool;
    MyTask task1;
    pool.start();

    pool.submit(std::make_shared<MyTask>(task1));
    pool.submit(std::make_shared<MyTask>(task1));
    pool.submit(std::make_shared<MyTask>(task1));
    pool.submit(std::make_shared<MyTask>(task1));
    pool.submit(std::make_shared<MyTask>(task1));
    pool.submit(std::make_shared<MyTask>(task1));
    pool.submit(std::make_shared<MyTask>(task1));
    
    getchar();
    return 0;
}