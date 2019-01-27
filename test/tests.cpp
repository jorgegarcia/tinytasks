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
    ASSERT_STREQ(tinytasks_lib_version(), "1.0.0");
}

TEST(TinyTasksTest, TestCreateTinyTask)
{
    TinyTask task([]{ StdOutThreadSafe("Running tiny task..."); }, UINT16_MAX);
    task.Run();
    
    ASSERT_EQ(task.GetID(), UINT16_MAX);
    ASSERT_TRUE(task.HasCompleted());
}

TEST(TinyTasksTest, TestCreateTinyTaskAndRunInThread)
{
    TinyTask task([]{ uint8_t counter = 0; while(counter < 3) { sleep(1); ++counter; } }, UINT16_MAX);
    std::thread taskThread(&TinyTask::Run, &task);
    
    while(!task.HasCompleted())
    {
        StdOutThreadSafe("Waiting for task to complete...");
        sleep(1);
    }
    
    ASSERT_EQ(task.GetID(), UINT16_MAX);
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
            std::this_thread::sleep_for(std::chrono::seconds(1));
            --counter;
            task.PauseIfNeeded();
        }
    }, UINT16_MAX);
    
    std::thread taskThread(&TinyTask::Run, &task);
    
    sleep(2);
    task.Pause();
    StdOutThreadSafe("Task paused!");
    sleep(2);
    task.Resume();
    
    taskThread.join();
    ASSERT_EQ(task.GetID(), UINT16_MAX);
    ASSERT_TRUE(task.HasCompleted());
}

TEST(TinyTasksTest, TestCreateAndCancelTinyTaskInThread)
{
    TinyTask task([&task]
    {
        uint8_t counter = 5;
        
        while(counter > 0 && !task.IsStopping())
        {
            StdOutThreadSafe("Task count down: " + std::to_string(counter));
            std::this_thread::sleep_for(std::chrono::seconds(1));
            --counter;
        }
    }, UINT16_MAX);
    
    std::thread taskThread(&TinyTask::Run, &task);
    
    sleep(3);
    task.Stop();
    while(!task.HasStopped()) {}
    StdOutThreadSafe("Task stopped!");
    
    taskThread.join();
    ASSERT_EQ(task.GetID(), UINT16_MAX);
    ASSERT_TRUE(task.HasStopped());
}

TEST(TinyTasksTest, TestCreateAndCancelWhileTinyTaskPausedInThread)
{
    TinyTask task([&task]
    {
        uint8_t counter = 5;
        
        while(counter > 0 && !task.IsStopping())
        {
            StdOutThreadSafe("Task count down: " + std::to_string(counter));
            std::this_thread::sleep_for(std::chrono::seconds(1));
            --counter;
            task.PauseIfNeeded();
        }
    }, UINT16_MAX);
    
    std::thread taskThread(&TinyTask::Run, &task);
    
    sleep(3);
    task.Pause();
    StdOutThreadSafe("Task paused!");
    task.Stop();
    while(!task.HasStopped());
    StdOutThreadSafe("Task stopped!");
    
    taskThread.join();
    ASSERT_EQ(task.GetID(), UINT16_MAX);
    ASSERT_TRUE(task.HasStopped());
}

TEST(TinyTasksTest, TestCreateAndQueryTinyTaskProgressInThread)
{
    TinyTask task([&task]
    {
        uint8_t counter = 0;
        uint8_t maxCount = 5;

        while(counter < maxCount)
        {
            task.SetProgress(static_cast<float>(counter) / static_cast<float>(maxCount) * 100.0f);
            std::this_thread::sleep_for(std::chrono::seconds(1));
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
    ASSERT_EQ(task.GetID(), UINT16_MAX);
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
    
    TinyTasksPool::Result result1 = m_tinyTasksPool.SetNewLambdaForTask(taskID, []{ StdOutThreadSafe("Running task from pool.."); });
    ASSERT_EQ(result1, TinyTasksPool::Result::SUCCEEDED);
    
    TinyTask* task  = m_tinyTasksPool.GetTask(taskID);
    ASSERT_TRUE(task);
    while(!task->HasCompleted()) {}
    ASSERT_TRUE(task->HasCompleted());
    
    uint32_t taskID2 = m_tinyTasksPool.CreateTask();
    ASSERT_EQ(taskID2, 1);
    
    TinyTasksPool::Result result2 = m_tinyTasksPool.SetNewLambdaForTask(taskID, []{ StdOutThreadSafe("Running task from pool.."); });
    ASSERT_EQ(result2, TinyTasksPool::Result::SUCCEEDED);
    
    TinyTask* task2  = m_tinyTasksPool.GetTask(taskID);
    ASSERT_TRUE(task2);
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
        
        TinyTasksPool::Result result = m_tinyTasksPool.SetNewLambdaForTask(taskID, [currentTaskID]
        {
            StdOutThreadSafe("Running Task ID: " + std::to_string(currentTaskID));
        });
        
        if(currentTaskID >= m_tinyTasksPool.GetNumThreads())
            ASSERT_EQ(result, TinyTasksPool::Result::SUCCEEDED_AT_QUEUE);
        else
            ASSERT_EQ(result, TinyTasksPool::Result::SUCCEEDED);
    }
}

TEST_F(TinyTasksPoolTest, TestCreateNewStopTaskInTinyTasksPool)
{
    ASSERT_EQ(m_tinyTasksPool.GetNumThreads(), 8);
    
    uint32_t taskID = m_tinyTasksPool.CreateTask();
    TinyTask* task  = m_tinyTasksPool.GetTask(taskID);
    ASSERT_TRUE(task);
                                                 
    TinyTasksPool::Result result = m_tinyTasksPool.SetNewLambdaForTask(taskID, [task]
    {
        while(!task->IsStopping())
        {
            StdOutThreadSafe("Running task from pool..");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    
    ASSERT_EQ(result, TinyTasksPool::Result::SUCCEEDED);
    
    sleep(3);
    task->Stop();
    while(!task->HasStopped()) {}
    StdOutThreadSafe("Task stopped!");

    ASSERT_TRUE(task->HasStopped());
}

TEST_F(TinyTasksPoolTest, TestCreateNewPauseResumeTaskInTinyTasksPool)
{
    ASSERT_EQ(m_tinyTasksPool.GetNumThreads(), 8);
    
    uint32_t taskID = m_tinyTasksPool.CreateTask();
    TinyTask* task  = m_tinyTasksPool.GetTask(taskID);
    ASSERT_TRUE(task);
    
    TinyTasksPool::Result result = m_tinyTasksPool.SetNewLambdaForTask(taskID, [task]
    {
        while(!task->IsStopping())
        {
            StdOutThreadSafe("Running task from pool..");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            task->PauseIfNeeded();
        }
    });
    
    ASSERT_EQ(result, TinyTasksPool::Result::SUCCEEDED);
    
    sleep(3);
    task->Pause();
    StdOutThreadSafe("Task paused...");
    sleep(2);
    task->Resume();
    StdOutThreadSafe("Task resumed...");
    sleep(3);
    task->Stop();
    while(!task->HasStopped()) {}
    
    ASSERT_TRUE(task->HasStopped());
    
    ASSERT_EQ(m_tinyTasksPool.GetNumRunningTasks(), 0);
}

TEST_F(TinyTasksPoolTest, TestRunPendingTasksInTinyTasksPool)
{
    ASSERT_EQ(m_tinyTasksPool.GetNumThreads(), 8);
    
    const uint16_t numQueuedTasks = 16;
    const uint16_t numTasksInPool = numQueuedTasks + m_tinyTasksPool.GetNumThreads();
    for(uint16_t currentTaskID = 0; currentTaskID < numTasksInPool; ++currentTaskID)
    {
        uint16_t taskID = m_tinyTasksPool.CreateTask();
        ASSERT_EQ(taskID, currentTaskID);
        
        TinyTasksPool::Result result = m_tinyTasksPool.SetNewLambdaForTask(taskID, [currentTaskID]
        {
            StdOutThreadSafe("Running Task ID: " + std::to_string(currentTaskID));
        });

        if(currentTaskID >= m_tinyTasksPool.GetNumThreads())
            ASSERT_EQ(result, TinyTasksPool::Result::SUCCEEDED_AT_QUEUE);
        else
            ASSERT_EQ(result, TinyTasksPool::Result::SUCCEEDED);
    }
    
    ASSERT_EQ(m_tinyTasksPool.GetNumPendingTasks(), numQueuedTasks);
    
    while(m_tinyTasksPool.GetNumPendingTasks() > 0)
    {
        m_tinyTasksPool.RunPendingTasks();
    }
    
    ASSERT_EQ(m_tinyTasksPool.GetNumPendingTasks(), 0);
    
    for(uint16_t currentTaskID = 0; currentTaskID < numTasksInPool; ++currentTaskID)
    {
        TinyTask* task = m_tinyTasksPool.GetTask(currentTaskID);
        ASSERT_TRUE(task);
        
        while(!task->HasCompleted()) {}
        ASSERT_TRUE(task->HasCompleted());
    }
}

TEST_F(TinyTasksPoolTest, TestGetNumRunningTasksInTinyTasksPool)
{
    ASSERT_EQ(m_tinyTasksPool.GetNumThreads(), 8);

    const uint16_t numTasksInPool = m_tinyTasksPool.GetNumThreads();
    for(uint16_t currentTaskID = 0; currentTaskID < numTasksInPool; ++currentTaskID)
    {
        uint16_t taskID = m_tinyTasksPool.CreateTask();
        ASSERT_EQ(taskID, currentTaskID);
        
        TinyTask* task = m_tinyTasksPool.GetTask(taskID);
        ASSERT_TRUE(task);
        
        TinyTasksPool::Result result = m_tinyTasksPool.SetNewLambdaForTask(taskID, [currentTaskID, task]
        {
            while(!task->IsStopping())
            {
                StdOutThreadSafe("Running Task ID: " + std::to_string(currentTaskID));
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
        ASSERT_EQ(result, TinyTasksPool::Result::SUCCEEDED);
    }

    //Wait for tasks to actually start running
    for(uint16_t currentTaskID = 0; currentTaskID < numTasksInPool; ++currentTaskID)
    {
        TinyTask* task = m_tinyTasksPool.GetTask(currentTaskID);
        ASSERT_TRUE(task);
        
        while(!task->IsRunning()) {}
    }
    
    ASSERT_EQ(m_tinyTasksPool.GetNumRunningTasks(), numTasksInPool);

    for(uint16_t currentTaskID = 0; currentTaskID < numTasksInPool; ++currentTaskID)
    {
        TinyTask* task = m_tinyTasksPool.GetTask(currentTaskID);
        ASSERT_TRUE(task);
        task->Stop();
        while(!task->HasStopped()) {}
    }
    
    ASSERT_EQ(m_tinyTasksPool.GetNumRunningTasks(), 0);
}
