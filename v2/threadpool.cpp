#include <iostream>
#include "threadpool.h"

//int Thread::generalid = 0;

Result::Result(std::shared_ptr<Task> task,bool bIsValid)
  :task_(std::move(task))
  ,bIsValid_(bIsValid)
{
  //  ???????why 
  //  会导致未定义行为
    //    task_->SetResult(this);
}

void Result::setAny(Any any)
{
  any_ = std::move(any);
  sem_.post();
}

Any Result::get()
{
  if(!bIsValid_)
  {
    return "";
  }

  sem_.wait();
  return std::move(any_);
}

Task::Task()
{

}
void Task::exec()
{
  // if(nullptr != result_)
  // {
  //   result_->setAny(this->run());
  // }
}

ThreadPool::ThreadPool()
    :taskSize_(0)
    ,bIsPoolRunning_(false)
    ,poolMode_(POOLMODE::FIXED)
{

}

ThreadPool::~ThreadPool()
{
  bIsPoolRunning_ = false;
  std::unique_lock<std::mutex> lock(mtx_);
  //  为什么要notify_all? 唤醒阻塞线程
  NotEmpty_.notify_all();
  ExitCond_.wait(lock,[this]()->bool{return threads_.size() == 0;});
  std::cout<<"~ThreadPool"<<std::endl;
}

void ThreadPool::start(int initThreadSize)
{
    initThreadSize_ = initThreadSize;
    threadSize_ = initThreadSize_;

    bIsPoolRunning_ = true;
    //启动线程
    for(int i = 0; i < initThreadSize_; i++)
    {
        //问题 这里用bind是为什么？
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler,this,std::placeholders::_1));
        int id = ptr->GetId();
        threads_.emplace(id,std::move(ptr));
    }

    // 开始执行任务
    for(int i = 0; i < initThreadSize_; i++)
    {
       threads_[i]->start(); 
    }

}


Result ThreadPool::submit(std::shared_ptr<Task> task)
{
    std::unique_lock<std::mutex> lock(mtx_);

    while(taskQue_.size() >= MAX_TASK_SIZE)
    {
       NotFull_.wait_for(lock,std::chrono::seconds(1)); 
       std::cerr<<"taskQue full...."<<std::endl;
       return Result(nullptr,false);
      //continue;
    }

    //taskQue_.emplace(task);
    std::shared_ptr<Result> result = std::make_shared<Result>(task);
    taskQue_.emplace([result](){
        Any any = result->task_->run();
        result->setAny(std::move(any));
    });
    taskSize_++;
    // std::cout<<"taskSize = "<<taskSize_<<std::endl;

    NotEmpty_.notify_all();

    if(poolMode_ == POOLMODE::CACHED && threadSize_ < MAX_THREAD_SIZE)
    {
      auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler,this,std::placeholders::_1));
      int id = ptr->GetId();
      threads_.emplace(id,std::move(ptr));
      threadSize_++;
    }

    return Result(task,true);
}

void ThreadPool::threadHandler(int threadid)
{
  auto last = std::chrono::high_resolution_clock::now();
    for(;;)
    {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(mtx_);  
          while(taskQue_.empty())
          {
            if(poolMode_ == POOLMODE::CACHED)
            {
              if(std::cv_status::timeout == NotEmpty_.wait_for(lock,std::chrono::seconds(1)))
              {
                auto now = std::chrono::high_resolution_clock::now();
                auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - last);
                if(dur.count() > 60)
                {
                  std::cout<<"thread exit"<<std::endl;
                  return;
                }
              }

            }
            else
            {
              std::cout<<"taskQue empty..."<<std::endl;
              NotEmpty_.wait(lock);
            }

            if(!bIsPoolRunning_)
            {
              //  在这里删除线程
              threads_.erase(threadid);
              ExitCond_.notify_all();
              return;
            }
            //NotEmpty_.wait_for(lock,std::chrono::seconds(1));
            // std::cerr<<"taskQue empty..."<<std::endl;
            // continue;
          }
          if(!bIsPoolRunning_ && taskQue_.empty())
          {
            break;
          }

          std::cout<<"tid : "<<std::this_thread::get_id()<<" get a task"<<std::endl;

          if(taskQue_.empty())
          {
            break;
          }

          task = std::move(taskQue_.front());
          taskQue_.pop();
          taskSize_--;

          //问题 为什么不能写在这里
        //   if(taskQue_.empty())
        //   {
        //     break;
        //   }

          if(taskQue_.size() > 0)
          {
            NotEmpty_.notify_all();
          }

          NotFull_.notify_all();
        } // 问题 为什么要把task->run写在此作用域

        // 执行任务
        if(task)
        {
            task();
        }
        // if(nullptr != task)
        // {
        //     //task->run();
        //     task->exec();
        //     std::cout<<"tid : "<<std::this_thread::get_id()<<" task over"<<std::endl;
        // }

        last = std::chrono::high_resolution_clock::now();
    }

    threads_.erase(threadid);
    ExitCond_.notify_all();
}

int Thread::generalid = 0;

Thread::Thread(threadhandler handler)
    : handler_(handler)
    , threadid_(generalid++)
{

}

void Thread::start()
{
    std::thread t(handler_,threadid_);
    t.detach();
}