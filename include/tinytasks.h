#ifndef __TINYTASKS_H__
#define __TINYTASKS_H__

#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

#define TT_SAFE_DELETE(p) if(p) delete p; p = nullptr;
#define TT_SAFE_DELETE_ARRAY(p) if(p) delete[] p; p = nullptr;

#define TINYTASKS_VERSION_MAJOR 1
#define TINYTASKS_VERSION_MINOR 0
#define TINYTASKS_VERSION_PATCH 0

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
    static const uint8_t kMaxNumThreadsInPool = UINT8_MAX;
    static const uint8_t kMinNumThreadsInPool = 2;
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

class TinyTasksPool : public NonCopyableMovable
{
public:
    explicit TinyTasksPool() : m_numThreads(constants::kMinNumThreadsInPool)
    {
        m_threads.reserve(m_numThreads);
        CreateThreads();
    }
    
    TinyTasksPool(uint8_t numThreads) : m_numThreads(numThreads)
    {
        assert(m_numThreads > 0);
        m_threads.reserve(m_numThreads);
        CreateThreads();
    }
    
    ~TinyTasksPool()
    {
        assert(m_threads.size() > 0);
        StopAllThreads();
    }
    
    uint8_t GetNumThreads() const { return m_numThreads; }

private:
    void CreateThreads()
    {
        assert(m_threads.size() == 0);
        m_threads.reserve(m_numThreads);
        
        for(uint8_t threadIndex = 0; threadIndex < m_numThreads; ++threadIndex)
        {
            std::thread newThread([]{});
            m_threads.push_back(std::move(newThread));
        }
    }
    
    void StopAllThreads()
    {
        for(auto& aThread : m_threads)
        {
            aThread.join();
        }
    }
    
    uint8_t                     m_numThreads;
    std::vector<std::thread>    m_threads;
};

class TinyTask : public NonCopyableMovable
{
public:
    explicit TinyTask(std::function<void()> taskLambda, const uint32_t id)
                    : m_taskStatus(TinyTaskStatus::PAUSED), m_taskLambda(taskLambda) , m_taskID(id), m_taskProgress(0.0f) {}
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
    
    uint32_t GetTaskID() const { return m_taskID; }
    
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
    uint32_t                    m_taskID;
    std::atomic<float>          m_taskProgress;
};
    
}

#endif
