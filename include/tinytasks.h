#ifndef __TINYTASKS_H__
#define __TINYTASKS_H__

#include <cassert>
#include <string>
#include <atomic>

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
protected:
    NonCopyableMovable(const NonCopyableMovable&) = delete;
    NonCopyableMovable& operator=(const NonCopyableMovable&) = delete;
    NonCopyableMovable(NonCopyableMovable&&) = delete;
    NonCopyableMovable& operator=(NonCopyableMovable&&) = delete;
};

class TinyTasksPool : public NonCopyableMovable
{
public:
    explicit TinyTasksPool() : m_numThreads(constants::kMinNumThreadsInPool) {}
    TinyTasksPool(uint8_t numThreads) : m_numThreads(numThreads) { assert(m_numThreads > 0); }
    ~TinyTasksPool() {}
    
    uint8_t GetNumThreads() const { return m_numThreads; }

private:
    uint8_t m_numThreads;
};
    
}

#endif
