//
// thread_pool.hpp
//
// exercise solution - chapter 7
// modern cpp tutorial
//
// created by changkun at changkun.de
// https://github.com/changkun/modern-cpp-tutorial/
//

#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>

#include <functional>
#include <stdexcept>
#include <utility>

class ThreadPool {
public:
    // initialize the number of concurrency threads
    ThreadPool(size_t threadNum);

    // enqueue new thread task
    template<typename TASK, typename... Args>
    auto enqueue(TASK&& t, Args&&... args);

    // destory thread pool and all created threads
    ~ThreadPool();


private:
    // thread list, stores all threads
    std::vector<std::thread> mWorkers;

    // queue task, the type of queue elements are funcitons with void reture type
    std::queue< std::function<void()> > mTasks;

    // for synchonization
    std::mutex mMutex;
    std::condition_variable mCondition;

    bool mStop;
};

inline ThreadPool::ThreadPool(size_t threadNum) {
    mStop = false;


    auto thread_callback =  [this](){
        do {// avoid fake awake

            // define function task container, return type is void
            std::function< void() > task;

            // critical section
            {
                std::unique_lock<std::mutex> lock(this->mMutex);

                auto predicate = [this]{
                    return this->mStop || !this->mTasks.empty();
                };
                
                // block the current thread if stop or task empty
                this->mCondition.wait(lock,predicate);

                // return if queue empty and task finished
                if(this->mStop && this->mTasks.empty()) {
                    return;
                }

                // otherwise excute the fist element of queue
                task = std::move(this->mTasks.front());
                this->mTasks.pop();
            }

            // execution task
            task();

        }while(true);
    };

    for( auto i = 0; i < threadNum; i++) {
        mWorkers.emplace_back(thread_callback);
    }
} 

// enqueue new thread

template<typename TASK, typename... Args>
auto ThreadPool::enqueue(TASK&& t, Args&&... args) {
    // deduce reture type
    using return_type = typename std::result_of<TASK(Args...)>::type;

    // fetch task
    using PackagedTask = std::packaged_task<return_type()>;

    auto task = std::make_shared<PackagedTask>( 
                std::bind(std::forward<TASK>(t),std::forward<Args>(args)... )
            );

    auto res = task->get_future();

    // critical section
    {
        std::unique_lock<std::mutex> lock(mMutex);

        // avoid add new thread if threadpool is destroyed
        if (mStop)
        {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        // add task to queue
        mTasks.emplace([task]{(*task)();});
    }
    
    // notify a wait thread
    mCondition.notify_one();

    return res;
}

// destroy everything
inline ThreadPool::~ThreadPool() {
    // critical section
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mStop = true;
    }
    
    // wake up all threads
    mCondition.notify_all();

    // let all processes into synchonous execution, use c++11 new for-loop: for(value:values)
    for(std::thread& work: mWorkers)
    {
        work.join();
    }
}
