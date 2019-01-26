#ifndef __TINYTASKS_H__
#define __TINYTASKS_H__

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

#define TINYTASKS_VERSION_MAJOR 1
#define TINYTASKS_VERSION_MINOR 0
#define TINYTASKS_VERSION_PATCH 0

#define TINYTASKS_SAFE_DELETE(p)       if(p) { delete p;   p = nullptr; }

namespace tinytasks
{

static const char* version()
{
    std::string version;

    version.append(std::to_string(TINYTASKS_VERSION_MAJOR) + "." +
                   std::to_string(TINYTASKS_VERSION_MINOR) + "." +
                   std::to_string(TINYTASKS_VERSION_PATCH));
    
    return version.c_str();
}
    
namespace constants
{
    static const uint8_t    kMaxNumThreadsInPool    = UINT8_MAX;
    static const uint8_t    kMinNumThreadsInPool    = 2;
    static const uint16_t   kMaxNumTasksInPool      = UINT16_MAX;
}
    
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

class TinyTask : public NonCopyableMovable
{
public:
    TinyTask(std::function<void()> taskLambda, const uint16_t id)
            : m_status(Status::PAUSED), m_lambda(taskLambda), m_ID(id), m_progress(0.0f), m_isStopping(false)
    {
        assert(m_lambda && "Task requires a valid (non-nullptr) lambda");
    }
    
    TinyTask(const uint16_t id)
            : m_status(Status::PAUSED), m_ID(id), m_progress(0.0f), m_isStopping(false)
    {
    }

    ~TinyTask() {}
    
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
    
    void Pause()
    {
        assert(m_status == Status::RUNNING && "Can't pause task as its state has changed");
        m_status = Status::PAUSED;
    }
    
    void Resume()
    {
        assert(m_status == Status::PAUSED && "Can't resume task as its state has changed");
        m_status = Status::RUNNING;
    }

    void Stop()
    {
        assert((m_status == Status::RUNNING || m_status == Status::PAUSED) && "Can't stop task as its state has changed");

        if(IsPaused()) Resume();
        m_isStopping = true;
    }
    
    void PauseIfNeeded() const
    {
        while(IsPaused())
        {
            std::this_thread::sleep_for(std::chrono::seconds{1});
        }
    }
    
    enum Status
    {
        PAUSED,
        STOPPED,
        RUNNING,
        COMPLETED,
    };
    
    uint16_t GetID()    const { return m_ID; }
    Status GetStatus()  const { return m_status; }
    
    bool IsRunning()    const { return m_status == Status::RUNNING; }
    bool IsPaused()     const { return m_status == Status::PAUSED; }
    bool IsStopping()   const { return m_isStopping; }
    
    bool HasStopped()   const { return m_status == Status::STOPPED; }
    bool HasCompleted() const { return m_status == Status::COMPLETED; }
    
    void  SetProgress(const float progress) { m_progress = progress; }
    float GetProgress() const               { return m_progress; }

    void SetLambda(std::function<void()> newLambda) { m_lambda = std::move(newLambda); }

private:
    std::atomic<Status>     m_status;
    std::function<void()>   m_lambda;
    uint16_t                m_ID;
    std::atomic<float>      m_progress;
    std::atomic<bool>       m_isStopping;
};

class TinyTasksPool : public NonCopyableMovable
{
public:
    enum Result
    {
        SUCCEDED,
        TASK_NOT_FOUND,
        ERROR,
    };
    
    explicit TinyTasksPool() : m_numThreads(constants::kMinNumThreadsInPool), m_nextFreeTaskId(0)
    {
        m_threadsTasks.reserve(m_numThreads);
        InitThreads();
        InitThreadsTasks();
    }
    
    TinyTasksPool(uint8_t numThreads) : m_numThreads(numThreads), m_nextFreeTaskId(0)
    {
        assert(m_numThreads > 0);
        m_threadsTasks.reserve(m_numThreads);
        InitThreads();
        InitThreadsTasks();
    }
    
    ~TinyTasksPool()
    {
        assert(m_threads.size() > 0);
        assert(m_pendingTasks.size() >= 0);
        assert(m_tasks.size() >= 0);
        StopAllThreads();
        InitThreadsTasks();
        ClearPendingTasks();
        DeleteTasksAllocations();
    }
    
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
                m_tasks.insert(std::pair<uint16_t, TinyTask*>(m_nextFreeTaskId, newTask));
                currentThreadTask = newTask;
                return m_nextFreeTaskId++;
            }
            
            currentThreadIndex++;
        }

        //If there is no space in the thread vector, create new task and queque it
        TinyTask* newTask = new TinyTask(m_nextFreeTaskId);
        m_tasks.insert(std::pair<uint16_t, TinyTask*>(m_nextFreeTaskId, newTask));
        m_pendingTasks.push(newTask);

        return m_nextFreeTaskId++;
    }
    
    Result SetNewLambdaForTask(const uint16_t taskID, std::function<void()> newLambda)
    {
        std::lock_guard<std::mutex> lock(m_poolDataMutex);
        
        uint8_t currentThreadIndex = 0;
        for(auto& threadTask : m_threadsTasks)
        {
            if(threadTask->GetID() == taskID)
            {
                threadTask->SetLambda(newLambda);
                m_threads[currentThreadIndex].join();
                std::thread newTaskThread(&TinyTask::Run, threadTask);
                m_threads[currentThreadIndex] = std::move(newTaskThread);
                return Result::SUCCEDED;
            }
            
            currentThreadIndex++;
        }
        
        auto task = m_tasks.find(taskID);
        if(task->second)
        {
            task->second->SetLambda(newLambda);
            return Result::SUCCEDED;
        }
    
        return Result::TASK_NOT_FOUND;
    }
    
    Result RunPendingTasks()
    {
        std::lock_guard<std::mutex> lock(m_poolDataMutex);
        
        uint8_t currentThreadIndex = 0;
        
        for(auto& threadTask : m_threadsTasks)
        {
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
        
        return Result::SUCCEDED;
    }
    
    uint8_t          GetNumThreads()                        const { return m_numThreads; }
    uint16_t         GetNumPendingTasks()                   const { return m_pendingTasks.size(); }
    TinyTask*        GetTask(const uint16_t taskID)         const { return m_tasks.find(taskID)->second; }
    TinyTask::Status GetTaskStatus(const uint16_t taskID)   const { return m_tasks.find(taskID)->second->GetStatus(); }
    
private:
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
    
    void InitThreadsTasks()
    {
        m_threadsTasks.clear();
        
        for(uint8_t currentIndex = 0; currentIndex < m_numThreads; ++currentIndex)
        {
            m_threadsTasks.push_back(nullptr);
        }
    }
    
    void ClearPendingTasks()
    {
        std::queue<TinyTask*> empty;
        std::swap(m_pendingTasks, empty);
    }
    
    void DeleteTasksAllocations()
    {
        for(auto& task : m_tasks)
        {
            TINYTASKS_SAFE_DELETE(task.second);
        }
    }
    
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
    
}

#endif
