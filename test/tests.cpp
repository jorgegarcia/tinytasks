#include "gtest/gtest.h"
#include "../include/tinytasks.h"

using namespace tinytasks;

std::mutex stdOutLock;

void StdOutThreadSafe(const std::string& message)
{
    std::lock_guard<std::mutex> lock(stdOutLock);
    std::cout << message << std::endl;
}

TEST(TinyTasksTest, TestLibVersionNumber)
{
    ASSERT_STREQ(::version(), "1.0.0");
}

TEST(TinyTasksTest, TestCreateTinyTask)
{
    TinyTask task([]{ StdOutThreadSafe("Running tiny task..."); }, UINT32_MAX);
    task.Run();
    
    ASSERT_EQ(task.GetTaskID(), UINT16_MAX);
    ASSERT_TRUE(task.HasCompleted());
}

TEST(TinyTasksTest, TestCreateTinyTaskAndRunInThread)
{
    TinyTask task([]{ uint8_t counter = 0; while(counter < 3) { sleep(1); ++counter; } }, UINT32_MAX);
    std::thread taskThread(&TinyTask::Run, &task);
    
    while(!task.HasCompleted())
    {
        StdOutThreadSafe("Waiting for task to complete...");
        sleep(1);
    }
    
    ASSERT_EQ(task.GetTaskID(), UINT16_MAX);
    ASSERT_TRUE(task.HasCompleted());
    taskThread.join();
}

TEST(TinyTasksTest, TestCreateAndPauseTinyTaskInThread)
{
    TinyTask task([&task]
    {
        uint8_t counter = 5;
        
        while(counter > 0)
        {
            StdOutThreadSafe("Task count down: " + std::to_string(counter));
            sleep(1);
            --counter;
            task.PauseIfNeeded();
        }
    }, UINT32_MAX);
    
    std::thread taskThread(&TinyTask::Run, &task);
    
    sleep(2);
    task.Pause();
    StdOutThreadSafe("Task paused!");
    sleep(2);
    task.Resume();
    
    taskThread.join();
    ASSERT_EQ(task.GetTaskID(), UINT16_MAX);
    ASSERT_TRUE(task.HasCompleted());
}

TEST(TinyTasksTest, TestCreateAndCancelTinyTaskInThread)
{
    TinyTask task([&task]
    {
        uint8_t counter = 5;
        
        while(counter > 0 && !task.HasStopped())
        {
            StdOutThreadSafe("Task count down: " + std::to_string(counter));
            sleep(1);
            --counter;
        }
    }, UINT16_MAX);
    
    std::thread taskThread(&TinyTask::Run, &task);
    
    sleep(3);
    task.Stop();
    StdOutThreadSafe("Task stopped!\n");
    
    taskThread.join();
    ASSERT_EQ(task.GetTaskID(), UINT16_MAX);
    ASSERT_TRUE(task.HasStopped());
}

TEST(TinyTasksTest, TestCreateAndCancelWhileTinyTaskPausedInThread)
{
    TinyTask task([&task]
    {
        uint8_t counter = 5;
        
        while(counter > 0 && !task.HasStopped())
        {
            StdOutThreadSafe("Task count down: " + std::to_string(counter));
            sleep(1);
            --counter;
            task.PauseIfNeeded();
        }
    }, UINT16_MAX);
    
    std::thread taskThread(&TinyTask::Run, &task);
    
    sleep(3);
    task.Pause();
    StdOutThreadSafe("Task paused!");
    task.Stop();
    StdOutThreadSafe("Task stopped!");
    
    taskThread.join();
    ASSERT_EQ(task.GetTaskID(), UINT16_MAX);
    ASSERT_TRUE(task.HasStopped());
}

TEST(TinyTasksTest, TestCreateAndQueryTinyTaskProgressInThread)
{
    TinyTask task([&task]
    {
        uint8_t counter = 0;
        uint8_t maxCount = 10;

        while(counter < maxCount)
        {
            task.SetProgress(static_cast<float>(counter) / static_cast<float>(maxCount) * 100.0f);
            sleep(1);
            ++counter;
        }
    }, UINT16_MAX);
    
    std::thread taskThread(&TinyTask::Run, &task);
    
    while(!task.HasCompleted())
    {
        std::string progress(std::to_string(task.GetProgress()));
        progress.resize(4);
        StdOutThreadSafe("Task progress: " + progress + " %");
        sleep(1);
    }
    
    taskThread.join();
    ASSERT_EQ(task.GetTaskID(), UINT16_MAX);
    ASSERT_TRUE(task.HasCompleted());
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
    
    uint32_t taskID = m_tinyTasksPool.CreateTask();
    ASSERT_EQ(taskID, 0);
    
    TinyTasksPool::TTPResult result1 = m_tinyTasksPool.SetNewLambdaForTask(taskID, []{ StdOutThreadSafe("Running task from pool.."); });
    ASSERT_EQ(result1, TinyTasksPool::TTPResult::SUCCEDED);
    
    TinyTask* task  = m_tinyTasksPool.GetTask(taskID);
    while(!task->HasCompleted()) {}
    ASSERT_TRUE(task->HasCompleted());
    
    uint32_t taskID2 = m_tinyTasksPool.CreateTask();
    ASSERT_EQ(taskID2, 1);
    
    TinyTasksPool::TTPResult result2 = m_tinyTasksPool.SetNewLambdaForTask(taskID, []{ StdOutThreadSafe("Running task from pool.."); });
    ASSERT_EQ(result2, TinyTasksPool::TTPResult::SUCCEDED);
    
    TinyTask* task2  = m_tinyTasksPool.GetTask(taskID);
    while(!task->HasCompleted()) {}
    ASSERT_TRUE(task2->HasCompleted());
}

TEST_F(TinyTasksPoolTest, TestCreateManyTasksInTinyTasksPool)
{
    ASSERT_EQ(m_tinyTasksPool.GetNumThreads(), 8);
    
    for(uint16_t currentTaskID = 0; currentTaskID < constants::kMaxNumTasksInPool; ++currentTaskID)
    {
        uint16_t taskID = m_tinyTasksPool.CreateTask();
        ASSERT_EQ(taskID, currentTaskID);
        
        TinyTasksPool::TTPResult result = m_tinyTasksPool.SetNewLambdaForTask(taskID, [currentTaskID]
        {
            StdOutThreadSafe("Running Task ID: " + std::to_string(currentTaskID));
        });
        ASSERT_EQ(result, TinyTasksPool::TTPResult::SUCCEDED);
    }
}

TEST_F(TinyTasksPoolTest, TestCreateNewStopTaskInTinyTasksPool)
{
    ASSERT_EQ(m_tinyTasksPool.GetNumThreads(), 8);
    
    uint32_t taskID = m_tinyTasksPool.CreateTask();
    TinyTask* task  = m_tinyTasksPool.GetTask(taskID);
                                                 
    TinyTasksPool::TTPResult result = m_tinyTasksPool.SetNewLambdaForTask(taskID, [&task]
    {
        while(!task->HasStopped())
        {
            StdOutThreadSafe("Running task from pool..");
            sleep(1);
        }
    });
    
    ASSERT_EQ(result, TinyTasksPool::TTPResult::SUCCEDED);
    
    sleep(3);
    task->Stop();

    ASSERT_TRUE(task->HasStopped());
}

TEST_F(TinyTasksPoolTest, TestCreateNewPauseResumeTaskInTinyTasksPool)
{
    ASSERT_EQ(m_tinyTasksPool.GetNumThreads(), 8);
    
    uint32_t taskID = m_tinyTasksPool.CreateTask();
    TinyTask* task  = m_tinyTasksPool.GetTask(taskID);
    
    TinyTasksPool::TTPResult result = m_tinyTasksPool.SetNewLambdaForTask(taskID, [&task]
    {
        while(!task->HasStopped())
        {
            StdOutThreadSafe("Running task from pool..");
            sleep(1);
            task->PauseIfNeeded();
        }
    });
    
    ASSERT_EQ(result, TinyTasksPool::TTPResult::SUCCEDED);
    
    sleep(3);
    task->Pause();
    StdOutThreadSafe("Task paused...");
    sleep(2);
    task->Resume();
    StdOutThreadSafe("Task resumed...");
    sleep(3);
    task->Stop();
    
    ASSERT_TRUE(task->HasStopped());
}
