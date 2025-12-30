#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <memory>

const int MAX_TASK_SIZE = 10;
const int MAX_THREAD_SIZE = 10;

class Semaphore
{
public:
    Semaphore() : nLimits(0){}
    ~Semaphore() = default;

    void post()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        nLimits++;
        std::cout<<"post nLimits = "<<nLimits<<std::endl;
        cond_.notify_all();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock,[this]()->bool{return nLimits > 0;});
        std::cout<<"wait nLimits = "<<nLimits<<std::endl;
        nLimits--;
    }
private:
    int nLimits;
    std::mutex mtx_;
    std::condition_variable cond_;
};

class Any
{
public:
    Any() = default;
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;

    template<typename T>
    Any(T data):base_(std::make_unique<Derive<T>>(data))
    {}
    ~Any() = default;

    template<typename T>
    T cast_()
    {
        Derive<T>* ptr = dynamic_cast<Derive<T>*>(base_.get());
        if(nullptr == ptr)
        {
           throw "type is imcompatible";
        }

        return ptr->data_;
    }
private:
    class Base
    {
    public:
        virtual ~Base() = default;
    };

    template<typename T>
    class Derive : public Base
    {
    public:
        Derive(T data):data_(data)
        {}
        virtual ~Derive() = default;
    public:
        T data_;
    };
private:
    std::unique_ptr<Base> base_;
};

class Result;

class Task
{
public:
    Task();
    ~Task() = default;
    virtual Any run() = 0;

    void SetResult(Result* result) {result_ = result;}
    void exec();
private:
    Result* result_; // 主要！！！！！！！ 千万不要用裸指针，能正常运行都是运气
    //std::shared_ptr<Result> result_;
};

class Result 
{
public:
    Result(std::shared_ptr<Task> task,bool isValid = true);
    ~Result()
    {
        std::cout<<"~Result"<<std::endl;
    }
    void setAny(Any any);
    Any get();

public:
    std::shared_ptr<Task> task_;
private:
    Any any_;
    Semaphore sem_;
    std::atomic_bool bIsValid_;
};



class Thread
{
    using threadhandler = std::function<void(int)>;
public:

    Thread();

    int GetId(){return threadid_;}
    Thread(threadhandler handler);
    ~Thread()
    {
        std::cout<<"~Thread"<<std::endl;
    }

    void start();
private:
    threadhandler handler_;
    int threadid_;
    static int generalid;
};


enum POOLMODE
{
    FIXED,
    CACHED,
};


class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    void start(int initThreadSize = 4);
    Result submit(std::shared_ptr<Task> task);

    void threadHandler(int threadid);
    
private:
    int initThreadSize_;
    // std::vector<std::unique_ptr<Thread>> threads_; 
    std::unordered_map<int,std::unique_ptr<Thread>> threads_;
    std::queue<std::shared_ptr<Task>> taskQue_;//问题 为什么要用shared_ptr???
    POOLMODE poolMode_; 
    std::condition_variable NotEmpty_;
    std::condition_variable NotFull_;
    std::condition_variable ExitCond_;
    std::mutex mtx_;
    std::atomic_int taskSize_;
    std::atomic_int threadSize_;
    std::atomic_bool bIsPoolRunning_;
};



#endif