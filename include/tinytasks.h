#ifndef __TINYTASKS_H__
#define __TINYTASKS_H__

#include <cassert>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

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
    NonCopyableMovable(const NonCopyableMovable&) = delete;
    NonCopyableMovable& operator=(const NonCopyableMovable&) = delete;
    NonCopyableMovable(NonCopyableMovable&&) = delete;
    NonCopyableMovable& operator=(NonCopyableMovable&&) = delete;
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
        
        for(auto& aThread : m_threads)
        {
            TT_SAFE_DELETE(aThread);
        }
    }
    
    uint8_t GetNumThreads() const { return m_numThreads; }

private:
    void CreateThreads()
    {
        for(uint8_t threadIndex = 0; threadIndex < m_numThreads; ++threadIndex)
        {
            std::thread* newThread = new std::thread([]{});
            m_threads.push_back(newThread);
        }
    }
    
    void StopAllThreads()
    {
        for(auto* aThread : m_threads)
        {
            aThread->join();
        }
    }
    
    uint8_t m_numThreads;
    std::vector<std::thread*> m_threads;
};
    
}

#endif
