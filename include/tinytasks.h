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

#define TT_SAFE_DELETE(p)       if(p) { delete p;   p = nullptr; }
#define TT_SAFE_DELETE_ARRAY(p) if(p) { delete[] p; p = nullptr; }

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
    explicit TinyTask(std::function<void()> taskLambda, const uint16_t id)
                    : m_taskStatus(TinyTaskStatus::PAUSED), m_taskLambda(taskLambda) , m_taskID(id), m_taskProgress(0.0f)
    {
        assert(m_taskLambda && "Task requires a valid (non-nullptr) lambda");
    }

    ~TinyTask() {}
    
    void Run()
    {
        assert(m_taskStatus == TinyTaskStatus::PAUSED);

        m_taskStatus = TinyTaskStatus::RUNNING;
        m_taskLambda();
        
        if(!HasStopped())
            m_taskStatus = TinyTaskStatus::COMPLETED;
    }
    
    void Pause()
    {
        assert(m_taskStatus == TinyTaskStatus::RUNNING);
        m_taskStatus = TinyTaskStatus::PAUSED;
    }
    
    void Resume()
    {
        assert(m_taskStatus == TinyTaskStatus::PAUSED);
        m_taskStatus = TinyTaskStatus::RUNNING;
    }

    void Stop()
    {
        assert(m_taskStatus == TinyTaskStatus::RUNNING || m_taskStatus == TinyTaskStatus::PAUSED);
        m_taskStatus = TinyTaskStatus::STOPPED;
    }
    
    void PauseIfNeeded() const
    {
        while(IsPaused())
        {
            std::this_thread::sleep_for(std::chrono::seconds{1});
        }
    }
    
    bool IsPaused()      const { return m_taskStatus == TinyTaskStatus::PAUSED; }
    
    bool HasStopped()    const { return m_taskStatus == TinyTaskStatus::STOPPED; }
    bool HasCompleted()  const { return m_taskStatus == TinyTaskStatus::COMPLETED; }
    
    uint16_t GetTaskID() const { return m_taskID; }
    
    void SetProgress(const float progress) { m_taskProgress = progress; }
    float GetProgress()  const { return m_taskProgress; }

private:
    enum TinyTaskStatus
    {
        PAUSED,
        STOPPED,
        RUNNING,
        COMPLETED,
    };
    
    std::atomic<TinyTaskStatus> m_taskStatus;
    std::function<void()>       m_taskLambda;
    uint16_t                    m_taskID;
    std::atomic<float>          m_taskProgress;
};

class TinyTasksPool : public NonCopyableMovable
{
public:
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
    
    uint16_t CreateTask(std::function<void()> taskLambda)
    {
        assert(m_nextFreeTaskId < constants::kMaxNumTasksInPool && "Can't create more tasks. Ran out of IDs");

        std::lock_guard<std::mutex> lock(m_createTaskMutex);

        uint8_t currentThreadIndex = 0;
        for(auto& currentThread : m_threads)
        {
            if(m_threadsTasks[currentThreadIndex] == nullptr)
            {
                TinyTask* newTask = new TinyTask(taskLambda, m_nextFreeTaskId);
                m_tasks.insert(std::pair<uint16_t, TinyTask*>(m_nextFreeTaskId, newTask));
                m_threadsTasks[currentThreadIndex] = newTask;
                currentThread.join();
                std::thread newTaskThread(&TinyTask::Run, newTask);
                currentThread = std::move(newTaskThread);
                return m_nextFreeTaskId++;
            }
            
            currentThreadIndex++;
        }

        //If there is no space in the thread vector, create new task and queque it
        TinyTask* newTask = new TinyTask(taskLambda, m_nextFreeTaskId);
        m_tasks.insert(std::pair<uint16_t, TinyTask*>(m_nextFreeTaskId, newTask));
        m_pendingTasks.push(newTask);

        return m_nextFreeTaskId++;
    }
    
    uint8_t GetNumThreads() const { return m_numThreads; }
    
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
            TT_SAFE_DELETE(task.second);
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
    std::mutex                      m_createTaskMutex;
};
    
}

#endif
