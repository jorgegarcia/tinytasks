#include "gtest/gtest.h"
#include "../include/tinytasks.h"

using namespace tinytasks;

TEST(TinyTasksTest, TestLibVersionNumber)
{
    ASSERT_STREQ(tinytasks::version(), "1.0.0");
}

TEST(TinyTasksTest, TestCreateTinyTasksPoolDefault)
{
    TinyTasksPool tinyTasksPool;
    
    ASSERT_EQ(tinyTasksPool.GetNumThreads(), tinytasks::constants::kMinNumThreadsInPool);
}

TEST(TinyTasksTest, TestCreateTinyTasksPoolNonDefault)
{
    const uint8_t numThreads = 20;
    TinyTasksPool tinyTasksPool(numThreads);
    
    ASSERT_EQ(tinyTasksPool.GetNumThreads(), numThreads);
}

TEST(TinyTasksTest, TestCreateTinyTasksPoolNonDefaultMax)
{
    const uint8_t numThreads = tinytasks::constants::kMaxNumThreadsInPool;
    TinyTasksPool tinyTasksPool(numThreads);
    
    ASSERT_EQ(tinyTasksPool.GetNumThreads(), numThreads);
}

class TinyTasksPoolTest : public testing::Test
{
public:
    TinyTasksPoolTest() : m_tinyTasksPool(8) {}

protected:
    void SetUp() override {}
    
    void TearDown() override {}
    
    TinyTasksPool m_tinyTasksPool;
};

TEST_F(TinyTasksPoolTest, TestCreateNewTaskInTinyTasksPool)
{
    ASSERT_EQ(m_tinyTasksPool.GetNumThreads(), 8);
}

TEST(TinyTasksTest, TestCreateTinyTask)
{
    TinyTask task([]{ std::cout << "Running tiny task..." << std::endl; }, UINT32_MAX);
    task.Run();
    
    ASSERT_EQ(task.GetTaskID(), UINT32_MAX);
    ASSERT_TRUE(task.HasCompleted());
}

TEST(TinyTasksTest, TestCreateTinyTaskAndRunInThread)
{
    TinyTask task([]{ uint8_t counter = 0; while(counter < 5) { sleep(1); ++counter; } }, UINT32_MAX);
    std::thread taskThread(&TinyTask::Run, &task);

    while(!task.HasCompleted())
    {
        std::cout << "Waiting for task to complete...\n";
        sleep(1);
    }
    
    ASSERT_EQ(task.GetTaskID(), UINT32_MAX);
    ASSERT_TRUE(task.HasCompleted());
    taskThread.join();
}

TEST(TinyTasksTest, TestCreateAndPauseTinyTaskInThread)
{
    bool bPause = false;

    TinyTask task([&bPause]
    {
        uint8_t counter = 5;
        while(counter > 0)
        {
            std::cout << "Task count down: " << std::to_string(counter) << std::endl; sleep(1);
            while(bPause) std::this_thread::sleep_for(std::chrono::seconds{1});
            --counter;
        }
    }, UINT32_MAX);

    std::thread taskThread(&TinyTask::Run, &task);
    
    sleep(2);
    bPause = true;
    std::cout << "Task paused!\n";
    sleep(3);
    bPause = false;
    std::cout << "0: end of countdown\n";
    
    taskThread.join();
    ASSERT_EQ(task.GetTaskID(), UINT32_MAX);
    ASSERT_TRUE(task.HasCompleted());
}

