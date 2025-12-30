// threadpool version 3.0
#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <map>
#include <queue>
#include <thread>
#include <mutex>
#include <functional>
#include <memory>
#include <future>
#include <chrono>

const int MAX_TASK_SIZE = 10;
const int MAX_THREAD_SIZE = 20;
const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 300;
const int THREAD_MAX_IDLE_TIME = 60; // seconds

class Thread
{
    using threadhandler = std::function<void(int)>;

public:
    Thread(threadhandler handler)
	: handler_(handler)
	, threadid_(generalid++)
{

}

    int GetId() { return threadid_; }
    ~Thread()
    {
        std::cout << "~Thread" << std::endl;
    }

    void start()
    {
        std::thread t(handler_, threadid_);
        t.detach(); //设置分离线程 
    }

private:
    threadhandler handler_;
    int threadid_;
    static int generalid;
};

int Thread::generalid = 0;

enum POOLMODE
{
    FIXED,
    CACHED,
};

class ThreadPool
{
public:
    ThreadPool()
        : initThreadSize_(4),
          taskSize_(0),
          taskSizeThreadhold_(TASK_MAX_THRESHHOLD),
          poolMode_(POOLMODE::FIXED),
          bIsPoolRunning_(false),
          idleThreadSize_(0),
          threadSizeThreadhold_(THREAD_MAX_THRESHHOLD),
          curThreadSize_(0)
    {
    }
    ~ThreadPool()
    {
        bIsPoolRunning_ = false;
        // wait for all threads exit
        //	NotEmpty_.notify_all();
        std::unique_lock<std::mutex> lock(mtx_);
        NotEmpty_.notify_all();
        ExitCond_.wait(lock, [&]() -> bool{ 
            return threads_.size() == 0; 
        });
    }

    void start(int initThreadSize = 4)
    {
        initThreadSize_ = initThreadSize;
        curThreadSize_ = initThreadSize_;

        bIsPoolRunning_ = true;
        // 启动线程
        for (int i = 0; i < initThreadSize_; i++)
        {
            // 问题 这里用bind是为什么？
            auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1));
            int id = ptr->GetId();
            threads_.emplace(id, std::move(ptr));
        }

        // 开始执行任务
        for (int i = 0; i < initThreadSize_; i++)
        {
            threads_[i]->start();
            idleThreadSize_++;
        }
    }
    template <typename Func, typename... Args>
    auto submit(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
    {
        using ResType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<ResType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

        std::future<ResType> result = task->get_future();
        {
            std::unique_lock<std::mutex> lock(mtx_);
            if (!NotFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool
                                   { return taskQue_.size() < 10; }))
            {
                throw std::runtime_error("taskque is full submit task failed");
            }

            taskQue_.emplace([task]()
                             { (*task)(); });

            taskSize_++;
            NotEmpty_.notify_all();
            if (poolMode_ == POOLMODE::CACHED &&
                taskSize_ > idleThreadSize_ &&
                curThreadSize_ < threadSizeThreadhold_)
            {
                auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1));
                int id = ptr->GetId();
                threads_.emplace(id, std::move(ptr));
                threads_[id]->start();
                curThreadSize_++;
                std::cout << "create new Thread" << std::endl;
            }
        }
        return result;
    }

    void threadHandler(int threadid)
    {
        auto last = std::chrono::high_resolution_clock::now();
        for (;;)
        {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mtx_);
                while (taskQue_.empty())
                {
                    if (poolMode_ == POOLMODE::CACHED)
                    {
                        if (std::cv_status::timeout == NotEmpty_.wait_for(lock, std::chrono::seconds(1)))
                        {
                            auto now = std::chrono::high_resolution_clock::now();
                            auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - last);
                            if (dur.count() > THREAD_MAX_IDLE_TIME
                                && curThreadSize_ > initThreadSize_)
                            {
                                std::cout << "thread exit" << std::endl;
                                threads_.erase(threadid);
                                curThreadSize_--;
                                idleThreadSize_--;
                                return;
                            }
                        }
                    }
                    else
                    {
                        std::cout << "taskQue empty..." << std::endl;
                        NotEmpty_.wait(lock);
                    }

                    if (!bIsPoolRunning_)
                    {
                        //  在这里删除线程
                        threads_.erase(threadid);
                        ExitCond_.notify_all();
                        return;
                    }
                    // NotEmpty_.wait_for(lock,std::chrono::seconds(1));
                    //  std::cerr<<"taskQue empty..."<<std::endl;
                    //  continue;
                }
                if (!bIsPoolRunning_ && taskQue_.empty())
                {
                    break;
                }
                
                idleThreadSize_--;

                std::cout << "tid : " << std::this_thread::get_id() << " get a task" << std::endl;

                if (taskQue_.empty())
                {
                    break;
                }

                task = taskQue_.front();
                taskQue_.pop();
                taskSize_--;

                // 问题 为什么不能写在这里
                //   if(taskQue_.empty())
                //   {
                //     break;
                //   }

                if (taskQue_.size() > 0)
                {
                    NotEmpty_.notify_all();
                }

                NotFull_.notify_all();
            } // 问题 为什么要把task->run写在此作用域
            // 执行任务

            if (nullptr != task)
            {
                task();
                // std::cout << "tid : " << std::this_thread::get_id() << " task over" << std::endl;
            }
            idleThreadSize_++;

            last = std::chrono::high_resolution_clock::now();
        }

        threads_.erase(threadid);
        ExitCond_.notify_all();
    }

private:
    int initThreadSize_;
    // std::vector<std::unique_ptr<Thread>> threads_;
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;
    using Task = std::function<void()>;
    std::queue<Task> taskQue_; // 问题 为什么要用shared_ptr???
    POOLMODE poolMode_;
    std::condition_variable NotEmpty_;
    std::condition_variable NotFull_;
    std::condition_variable ExitCond_;
    std::mutex mtx_;
    std::atomic_int taskSize_;
    std::atomic_bool bIsPoolRunning_;

    int taskSizeThreadhold_;         // 任务数量阈值
    int threadSizeThreadhold_;       // 线程数量阈值
    std::atomic_int idleThreadSize_; // 空闲线程数量
    std::atomic_int curThreadSize_;  // 线程数量
};
#endif