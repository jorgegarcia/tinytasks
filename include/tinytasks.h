#ifndef __TINYTASKS_H__
#define __TINYTASKS_H__

//     _____  _               _____             _
//    /__   \(_) _ __   _   _/__   \ __ _  ___ | | __ ___
//      / /\/| || '_ \ | | | | / /\// _` |/ __|| |/ // __|
//     / /   | || | | || |_| |/ /  | (_| |\__ \|   < \__ \
//     \/    |_||_| |_| \__, |\/    \__,_||___/|_|\_\|___/
//                      |___/
//
//    Simple and single-header tasks library in C++11
//
//    The MIT License (MIT)
//
//    Copyright (c) 2019 Jorge Garcia Martin
//
//    Permission is hereby granted, free of charge, to any person obtaining a copy
//    of this software and associated documentation files (the "Software"), to deal
//    in the Software without restriction, including without limitation the rights
//    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//    copies of the Software, and to permit persons to whom the Software is
//    furnished to do so, subject to the following conditions:
//
//    The above copyright notice and this permission notice shall be included in
//    all copies or substantial portions of the Software.
//
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//    THE SOFTWARE.
//

#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <string.h>

#define TINYTASKS_VERSION_MAJOR 1
#define TINYTASKS_VERSION_MINOR 0
#define TINYTASKS_VERSION_PATCH 0

namespace tinytasks
{

//! @brief Local constants (used mainly in the TinyTasksPool class)
namespace constants
{
    static const uint8_t  kMaxNumThreadsInPool  = UINT8_MAX;
    static const uint8_t  kMinNumThreadsInPool  = 2;
    static const uint16_t kMaxNumTasksInPool    = UINT16_MAX;
}

//! @brief Gets the current library version
static std::string tinytasks_lib_version()
{
    std::string version;

    version.append(std::to_string(TINYTASKS_VERSION_MAJOR) + "." +
                   std::to_string(TINYTASKS_VERSION_MINOR) + "." +
                   std::to_string(TINYTASKS_VERSION_PATCH));
    
    return version;
}

//! @brief Base class for disabling copy & move constructors and operators
//!
//! @details
//! In the tinytasks library, the usage of copy and move operations is
//! limited for both the TinyTask and TinyTasksPool classes
//!
class NonCopyableMovable
{
public:
    explicit NonCopyableMovable() {}
    virtual ~NonCopyableMovable() {}

protected:
    NonCopyableMovable(const NonCopyableMovable&)            = delete;
    NonCopyableMovable& operator=(const NonCopyableMovable&) = delete;
    NonCopyableMovable(NonCopyableMovable&&)                 = delete;
    NonCopyableMovable& operator=(NonCopyableMovable&&)      = delete;
};

//! @brief Models a task (minimal unit to be run asynchronously)
//!
//! @details
//! To use this class, provide an ID (that has to be managed externally) and
//! optionally a lambda function to be run asynchronously.
//!
//! @note This class doesn't handle any threads. So potentially any other
//! threading API could be used to manage TinyTask objects
//!
class TinyTask : public NonCopyableMovable
{
public:
    //! Initialize the task
    //! @param lambda function tied to the task
    //! @param ID for the task
    TinyTask(std::function<void()> taskLambda, const uint16_t id)
            : m_status(Status::PAUSED), m_lambda(taskLambda), m_ID(id), m_progress(0.0f), m_isStopping(false)
    {
        assert(m_lambda && "Task requires a valid (non-nullptr) lambda");
    }
    
    //! Initialize the task
    //! @param ID for the task
    TinyTask(const uint16_t id)
            : m_status(Status::PAUSED), m_ID(id), m_progress(0.0f), m_isStopping(false)
    {
    }

    //! Destroys the task
    ~TinyTask() {}

    //! Runs function for the task. This should be called by a thread
    void Run()
    {
        m_status = Status::RUNNING;
        m_lambda();
        
        if(IsStopping())
        {
            m_isStopping = false;
            m_status = Status::STOPPED;
            return;
        }

        m_status = Status::COMPLETED;
    }

    //! Sets the state of the task to paused
    void Pause()
    {
        assert(m_status == Status::RUNNING && "Can't pause task as its state has changed");
        m_status = Status::PAUSED;
    }
    
    //! Sets the state of the task to running
    void Resume()
    {
        assert(m_status == Status::PAUSED && "Can't resume task as its state has changed");
        m_status = Status::RUNNING;
    }

    //! Sets the state of the task to stopping (after resuming it)
    void Stop()
    {
        assert((m_status == Status::RUNNING || m_status == Status::PAUSED) && "Can't stop task as its state has changed");

        if(IsPaused()) Resume();
        m_isStopping = true;
    }

    //! Sleeps the current thread if the task is paused
    //! @note This function should be called often in the task lambda
    //! in order to support pausing the task
    void PauseIfNeeded(const unsigned int milliseconds) const
    {
        while(IsPaused())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }
    }

    //! Possible states of the task
    enum Status
    {
        PAUSED,
        STOPPED,
        RUNNING,
        COMPLETED,
    };
    
    //! Gets this task ID
    uint16_t GetID()    const { return m_ID; }
    //! Gets this task status
    Status GetStatus()  const { return m_status; }
    
    //! Gets if this task is currently running
    bool IsRunning()    const { return m_status == Status::RUNNING; }
    //! Gets if this task is currently paused
    bool IsPaused()     const { return m_status == Status::PAUSED; }
    //! Gets if this task is currently stopping
    bool IsStopping()   const { return m_isStopping; }
        
    //! Gets if this task has stopped
    bool HasStopped()   const { return m_status == Status::STOPPED; }
    //! Gets if this task has completed
    bool HasCompleted() const { return m_status == Status::COMPLETED; }
    
    //! Sets the current progress of the task (from e.g. the task lambda)
    //! @param the current progress of the task (in any format)
    void  SetProgress(const float progress) { m_progress = progress; }
    //! Gets the current progress of the task
    float GetProgress() const               { return m_progress; }

    //! Sets the lambda that is tied to the task (e.g. run from a thread)
    void SetLambda(std::function<void()> newLambda) { m_lambda = std::move(newLambda); }

private:
    std::atomic<Status>     m_status;
    std::function<void()>   m_lambda;
    uint16_t                m_ID;
    std::atomic<float>      m_progress;
    std::atomic<bool>       m_isStopping;
};

//! @brief Implements a thread pool for handling tasks
//!
//! @details
//! To use this class, just instantiate an object of it and be sure to
//! specify the number of desired worker threads in the constructor if
//! you want to limit them in your application.
//! Once you create tasks, the IDs are stored in the pool object and
//! aren't recycled for new tasks. Have this into account in order to
//! drive the logic of your application.
//! Beware that the call to GetTask(taskID) returns a pointer to the task
//! that is owned by the pool. If you delete the pointer it will lead
//! to trouble. This probably needs a workaround in order to prevent it.
//! If the number of tasks created exceeds the number of threads, the
//! tasks are queued (with a "paused" state"), so if you want to run
//! them you have to call the member function RunPendingTasks().
//!
//! @note This class handles std::thread objects. Any other threading API
//! could potentially be used, but it will require some rework
//!
class TinyTasksPool : public NonCopyableMovable
{
public:
    //! Possible results for the pool operations
    enum Result
    {
        SUCCEEDED,
        SUCCEEDED_AT_QUEUE,
        TASK_NOT_FOUND,
    };
    
    //! Initialize the pool with default values
    explicit TinyTasksPool() : m_numThreads(constants::kMinNumThreadsInPool), m_nextFreeTaskId(0)
    {
        m_threadsTasks.reserve(m_numThreads);
        InitThreads();
        InitThreadsTasks();
    }

    //! Initialize the pool
    //! @param number of worker threads in the pool
    TinyTasksPool(uint8_t numThreads) : m_numThreads(numThreads), m_nextFreeTaskId(0)
    {
        assert(m_numThreads > 0);
        m_threadsTasks.reserve(m_numThreads);
        InitThreads();
        InitThreadsTasks();
    }
    
    //! Destroys the pool, and deallocates resources
    ~TinyTasksPool()
    {
        assert(GetNumRunningTasks() == 0 && "There are still tasks running. Tasks must be stopped before pool destruction");
        StopAllThreads();
        InitThreadsTasks();
        ClearPendingTasks();
        DeleteTasksAllocations();
    }
    
    //! Creates a new task in the pool and assigns a thread to it
    //! @note It doesn't start the task until you assign a lambda to it
    uint16_t CreateTask()
    {
        assert(m_nextFreeTaskId < constants::kMaxNumTasksInPool && "Can't create more tasks. Ran out of IDs");

        std::lock_guard<std::mutex> lock(m_poolDataMutex);

        uint8_t currentThreadIndex = 0;
        for(auto& currentThreadTask : m_threadsTasks)
        {
            if(m_threadsTasks[currentThreadIndex] == nullptr)
            {
                TinyTask* newTask = new TinyTask(m_nextFreeTaskId);
                assert(newTask && "New task wasn't allocated");
                m_tasks.insert(std::pair<uint16_t, TinyTask*>(m_nextFreeTaskId, newTask));
                currentThreadTask = newTask;
                return m_nextFreeTaskId++;
            }
            
            currentThreadIndex++;
        }

        //If there is no space in the thread vector, create new task and queque it
        TinyTask* newTask = new TinyTask(m_nextFreeTaskId);
        assert(newTask && "New task wasn't allocated");
        m_tasks.insert(std::pair<uint16_t, TinyTask*>(m_nextFreeTaskId, newTask));
        m_pendingTasks.push(newTask);

        return m_nextFreeTaskId++;
    }
    
    //! Sets a lambda function to a task, and starts running it (if possible)
    //! @param ID of the task to set the new lambda to
    //! @param lambda function to set (has to be valid)
    Result SetNewLambdaForTask(const uint16_t taskID, std::function<void()> newLambda)
    {
        assert(newLambda);
        
        std::lock_guard<std::mutex> lock(m_poolDataMutex);
        
        uint8_t currentThreadIndex = 0;
        for(auto& threadTask : m_threadsTasks)
        {
            assert(threadTask);
            if(threadTask->GetID() == taskID)
            {
                assert(threadTask->HasCompleted() || !threadTask->IsRunning() || !threadTask->IsPaused());
                threadTask->SetLambda(newLambda);
                m_threads[currentThreadIndex].join();
                std::thread newTaskThread(&TinyTask::Run, threadTask);
                m_threads[currentThreadIndex] = std::move(newTaskThread);
                return Result::SUCCEEDED;
            }
            
            currentThreadIndex++;
        }
        
        auto task = m_tasks.find(taskID);
        if(task->second)
        {
            task->second->SetLambda(newLambda);
            return Result::SUCCEEDED_AT_QUEUE;
        }
    
        return Result::TASK_NOT_FOUND;
    }
    
    //! Runs the pending tasks that are in the queue (if possible)
    Result RunPendingTasks()
    {
        std::lock_guard<std::mutex> lock(m_poolDataMutex);
        
        uint8_t currentThreadIndex = 0;
        
        for(auto& threadTask : m_threadsTasks)
        {
            assert(threadTask);
            if(threadTask->HasStopped() || threadTask->HasCompleted())
            {
                if(m_pendingTasks.empty()) break;
                m_threads[currentThreadIndex].join();
                threadTask = m_pendingTasks.front();
                m_pendingTasks.pop();
                std::thread newTaskThread(&TinyTask::Run, threadTask);
                m_threads[currentThreadIndex] = std::move(newTaskThread);
            }
            
            currentThreadIndex++;
        }
        
        return Result::SUCCEEDED;
    }
    
    //! Gets the number of tasks that are currently running
    uint8_t GetNumRunningTasks()
    {
        std::lock_guard<std::mutex> lock(m_poolDataMutex);
        
        uint8_t numRunningTasks = 0;
        
        for(auto& threadTask : m_threadsTasks)
        {
            if(threadTask && threadTask->IsRunning())
            {
                numRunningTasks++;
            }
        }
        
        return numRunningTasks;
    }

    //! Gets a task that is in the pool, given its ID
    //! @param ID of the task
    //! @note This is a thread safe operation
    TinyTask* GetTask(const uint16_t taskID)
    {
        std::lock_guard<std::mutex> lock(m_poolDataMutex);
        
        auto foundPair = m_tasks.find(taskID);
        
        if(foundPair != m_tasks.end())
            return m_tasks.find(taskID)->second;
        
        return nullptr;
    }
    
    //! Gets the status of a task in the pool
    //! @param ID of the task
    //! @note This is a thread safe operation
    TinyTask::Status GetTaskStatus(const uint16_t taskID)
    {
        TinyTask* task = GetTask(taskID);
        assert(task && "Task not found!");
        
        return task->GetStatus();
    }
    
    //! Gets the number of threads in the pool
    uint8_t     GetNumThreads()         const { return m_numThreads; }
    //! Gets the number of pending tasks that are queued in the pool
    uint16_t    GetNumPendingTasks()    const { return static_cast<uint16_t>(m_pendingTasks.size()); }
    
private:
    //! Initialises the threads for the pool
    void InitThreads()
    {
        assert(m_threads.size() == 0);
        m_threads.reserve(m_numThreads);
        
        for(uint8_t threadIndex = 0; threadIndex < m_numThreads; ++threadIndex)
        {
            std::thread newThread([]{});
            m_threads.push_back(std::move(newThread));
        }
    }
    
    //! Initialises the tasks that are tied to the threads
    void InitThreadsTasks()
    {
        m_threadsTasks.clear();
        
        for(uint8_t currentIndex = 0; currentIndex < m_numThreads; ++currentIndex)
        {
            m_threadsTasks.push_back(nullptr);
        }
    }
    
    //! Clears the pending tasks in the queue
    void ClearPendingTasks()
    {
        std::queue<TinyTask*> empty;
        std::swap(m_pendingTasks, empty);
    }
    
    //! Deletes the allocated TinyTask objects in the pool
    void DeleteTasksAllocations()
    {
        for(auto& task : m_tasks)
        {
            assert(task.second && "The task object has been deleted outside of the pool");
            
            if(task.second)
            {
                delete task.second;
                task.second = nullptr;
            }
        }
    }
    
    //! Stops the threads in the pool)
    void StopAllThreads()
    {
        for(auto& aThread : m_threads)
        {
            aThread.join();
        }
    }
    
    uint8_t                         m_numThreads;
    std::vector<std::thread>        m_threads;
    std::vector<TinyTask*>          m_threadsTasks;
    std::queue<TinyTask*>           m_pendingTasks;
    std::map<uint16_t, TinyTask*>   m_tasks;
    uint16_t                        m_nextFreeTaskId;
    std::mutex                      m_poolDataMutex;
};
    
} // namespace tinytasks

#endif
