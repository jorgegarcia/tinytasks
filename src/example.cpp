#include <iostream>
#include <iomanip>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <sstream>
#include "../include/tinytasks.h"

using namespace tinytasks;

void PrintHelp()
{
    std::cout << "\nTinyTasks v" << std::string(tinytasks_lib_version()) << " example | Usage and allowed commands\n\n";

    std::cout << "start <task_type_id>: starts a task of type 1 or 2\n";
    std::cout << "\t1: writes random numbers to disk during 1 minute\n";
    std::cout << "\t2: generates random numbers during 1 minute\n\n";
    std::cout << "start: starts a task of type 2 and prints its ID\n\n";
    std::cout << "pause <task_id>: pauses the task with the given id\n\n";
    std::cout << "resume <task_id>: resumes task with the given id (if paused)\n\n";
    std::cout << "stop <task_id>: stops the task with the given id (if not stopped)\n\n";
    std::cout << "status: prints the id, the status and the progress for each task\n\n";
    std::cout << "status <task_id>: as above, but for a single task\n\n";
    std::cout << "quit: exits the program\n\n";
}

namespace
{

struct Command
{
    enum CommandType
    {
        UNRECOGNISED,
        START_TASK_TYPE_ID,
        START,
        PAUSE_TASK_ID,
        RESUME_TASK_ID,
        STOP_TASK_ID,
        STATUS,
        STATUS_TASK_ID,
    };
    
    explicit Command() : commandType(CommandType::UNRECOGNISED), value(0) {}
    
    CommandType commandType;
    uint16_t value;
};

static void SplitStringElements(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;

    while(getline(ss, item, delim))
    {
        elems.push_back(item);
    }
}

bool IsStringANumber(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(), s.end(), [](char c) { return !std::isdigit(c); }) == s.end();
}

static Command ParseInput(const char* input)
{
    Command command;
    std::string stringToParse(input);
    if(stringToParse.empty()) return command;
    
    std::vector<std::string> elems;
    SplitStringElements(stringToParse, ' ', elems);

    if(strcmp(elems[0].c_str(), "start") == 0)
    {
        if(elems.size() == 1)
            command.commandType = Command::START;
        else if(elems.size() == 2)
            command.commandType = Command::START_TASK_TYPE_ID;
    }
    else if(strcmp(elems[0].c_str(), "pause") == 0)
    {
        if(elems.size() == 2)
            command.commandType = Command::PAUSE_TASK_ID;
    }
    else if(strcmp(elems[0].c_str(), "resume") == 0)
    {
        if(elems.size() == 2)
            command.commandType = Command::RESUME_TASK_ID;
    }
    else if(strcmp(elems[0].c_str(), "stop") == 0)
    {
        if(elems.size() == 2)
            command.commandType = Command::STOP_TASK_ID;
    }
    else if(strcmp(elems[0].c_str(), "status") == 0)
    {
        if(elems.size() == 1)
            command.commandType = Command::STATUS;
        else if(elems.size() == 2)
            command.commandType = Command::STATUS_TASK_ID;
    }
    
    if(elems.size() == 2 && IsStringANumber(elems[1]))
        command.value = static_cast<uint16_t>(std::stoi(elems[1]));
    else if(elems.size() >= 2)
        command.commandType = Command::UNRECOGNISED;
    
    return command;
}
    
static void ConvertTaskStatusToString(const TinyTask::Status taskStatus, std::string& statusString)
{
    if(taskStatus == TinyTask::Status::COMPLETED)
    {
        statusString.append("completed");
    }
    else if(taskStatus == TinyTask::Status::PAUSED)
    {
        statusString.append("paused");
    }
    else if(taskStatus == TinyTask::Status::RUNNING)
    {
        statusString.append("running");
    }
    else if(taskStatus == TinyTask::Status::STOPPED)
    {
        statusString.append("stopped");
    }
}
    
}

int main(int argc, char* argv[])
{
    if(argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        PrintHelp();
        return 0;
    }
    
    std::cout << "\nTinyTasks v" << std::string(tinytasks_lib_version()) << " example\n\n";
    
    //Setup
    TinyTasksPool tasksPool(8);
    std::vector<uint16_t> taskIDs;
    taskIDs.reserve(UINT16_MAX);
    
    const unsigned int inputLength = 64;
    char input[inputLength];
    
    //Loop to get keyboard input
    while(true)
    {
        std::cin.getline(input, sizeof(input));
        if(strcmp(input, "quit") == 0) break;
        
        Command command = ParseInput(input);
        
        if(command.commandType == Command::UNRECOGNISED)
        {
            std::cout << "Command not recognised\n\n";
            continue;
        }
        
        TinyTask* task = nullptr;
        
        if(command.commandType == Command::PAUSE_TASK_ID   ||
            command.commandType == Command::RESUME_TASK_ID ||
            command.commandType == Command::STOP_TASK_ID   ||
            command.commandType == Command::STATUS_TASK_ID)
        {
            if(std::find(taskIDs.begin(), taskIDs.end(), command.value) == taskIDs.end())
            {
                std::cout << "Command task ID not found\n\n";
                continue;
            }
            
            task = tasksPool.GetTask(command.value);
            assert(task && "Task not found");
        }
        
        switch(command.commandType)
        {
            case Command::START:
            {
                assert(!task);
                
                if(tasksPool.GetNumThreads() + tasksPool.GetNumPendingTasks() >= tinytasks::constants::kMaxNumTasksInPool)
                {
                    std::cout << "Can't create more tasks, as the pool ran out of IDs\n\n";
                    break;
                }

                uint16_t taskID = tasksPool.CreateTask();
                taskIDs.push_back(taskID);

                TinyTask* currentTask = tasksPool.GetTask(taskID);
                
                tasksPool.SetNewLambdaForTask(taskID, [currentTask]
                {
                    const unsigned int maxIterations = 300;
                    for(unsigned int value = 0; value < maxIterations; ++value)
                    {
                        if(currentTask->IsStopping() || currentTask->HasStopped()) break;
                        const float randomNumber = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                        (void)randomNumber;
                        currentTask->SetProgress(static_cast<float>(value + 1) / static_cast<float>(maxIterations) * 100.0f);
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        currentTask->PauseIfNeeded();
                    }
                });
                
                std::cout << "Created task with ID: " << std::to_string(taskID) << "\n" << std::endl;
                break;
            }
            case Command::START_TASK_TYPE_ID:
            {
                assert(!task);
                
                if(tasksPool.GetNumThreads() + tasksPool.GetNumPendingTasks() >= tinytasks::constants::kMaxNumTasksInPool)
                {
                    std::cout << "Can't create more tasks, as the pool ran out of IDs\n\n";
                    break;
                }
                
                uint16_t taskID = tasksPool.CreateTask();
                taskIDs.push_back(taskID);
                TinyTask* currentTask = tasksPool.GetTask(taskID);
                TinyTasksPool::Result lambdaResult = TinyTasksPool::Result::TASK_NOT_FOUND;
                
                if(command.value == 1)
                {
                    lambdaResult =tasksPool.SetNewLambdaForTask(taskID, [currentTask]
                    {
                        std::string filename;
                        clock_t timeNow = clock();
                        filename.append(std::to_string(timeNow) + ".txt\n");
                                        
                        std::FILE* fileToWrite = fopen(filename.c_str(), "w");
                        
                        const unsigned int maxIterations = 300;
                        for(unsigned int value = 0; value < maxIterations; ++value)
                        {
                            if(currentTask->IsStopping() || currentTask->HasStopped()) break;
                            const float randomNumber = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                            std::string stringToWrite;
                            stringToWrite.append(std::to_string(randomNumber) + " ");
                            fwrite(stringToWrite.c_str(), sizeof(char), stringToWrite.size(), fileToWrite);
                            currentTask->SetProgress(static_cast<float>(value + 1) / static_cast<float>(maxIterations) * 100.0f);
                            std::this_thread::sleep_for(std::chrono::milliseconds(200));
                            currentTask->PauseIfNeeded();
                        }
                        
                        fclose(fileToWrite);
                    });
                }
                else if(command.value == 2)
                {
                    lambdaResult = tasksPool.SetNewLambdaForTask(taskID, [currentTask]
                    {
                        const unsigned int maxIterations = 300;
                        for(unsigned int value = 0; value < maxIterations; ++value)
                        {
                            if(currentTask->IsStopping() || currentTask->HasStopped()) break;
                            const float randomNumber = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                            (void)randomNumber;
                            currentTask->SetProgress(static_cast<float>(value + 1) / static_cast<float>(maxIterations) * 100.0f);
                            std::this_thread::sleep_for(std::chrono::milliseconds(200));
                            currentTask->PauseIfNeeded();
                        }
                    });
                }
                else
                {
                    std::cout << "Task type not recognised. Only values 1 and 2 are allowed\n\n";
                }
                
                assert(lambdaResult == TinyTasksPool::Result::SUCCEDED || lambdaResult == TinyTasksPool::Result::SUCCEEDED_AT_QUEUE);
                
                //Wait until task starts running (if it's not queued)
                while(!currentTask->IsRunning() && lambdaResult != TinyTasksPool::Result::SUCCEEDED_AT_QUEUE) {}
                
                std::cout << "Created task of type " << std::to_string(command.value) << " and ID " << std::to_string(taskID) << "\n\n";
                break;
            }
            case Command::PAUSE_TASK_ID:
            {
                assert(task);
                if(task->IsStopping() || task->HasStopped() || task->IsPaused())
                {
                    std::cout << "Can't pause task, because it's stopped or already paused\n\n";
                    break;
                }
                
                task->Pause();
                while(!task->IsPaused()) {}
                std::cout << "Task ID " << std::to_string(task->GetID()) << " has paused\n\n";
                break;
            }
            case Command::RESUME_TASK_ID:
            {
                assert(task);
                if(task->IsStopping() || task->HasStopped() || task->IsRunning())
                {
                    std::cout << "Can't resume task, because it's stopped or already running\n\n";
                    break;
                }
                
                task->Resume();
                while(!task->IsRunning()) {}
                std::cout << "Task ID " << std::to_string(task->GetID()) << " has resumed\n\n";
                break;
            }
            case Command::STOP_TASK_ID:
            {
                assert(task);
                if(task->IsStopping() || task->HasStopped())
                {
                    std::cout << "Task is already stopped\n\n";
                    break;
                }
                
                if(task->IsPaused()) task->Resume();
                while(!task->IsRunning()) {}
                task->Stop();
                while(task->IsStopping() || !task->HasStopped()) {}
                std::cout << "Task ID " << std::to_string(task->GetID()) << " has stopped\n\n";
                break;
            }
            case Command::STATUS:
            {
                if(taskIDs.size() == 0)
                {
                    std::cout << "There are no tasks to show status for\n\n";
                }
                else
                {
                    std::cout << std::left << std::setw(10) << "[Task ID]" << std::setw(11) << "[Status]"
                              << std::setw(8) << "[Progress]\n";
                    
                    for(unsigned int taskIndex = 0; taskIndex < taskIDs.size(); ++taskIndex)
                    {
                        TinyTask* currentTask = tasksPool.GetTask(taskIDs[taskIndex]);
                        assert(currentTask);
                        
                        const TinyTask::Status status = currentTask->GetStatus();
                        std::string statusString;
                        ConvertTaskStatusToString(status, statusString);
                        
                        std::string progress(std::to_string(currentTask->GetProgress()));
                        progress.resize(5);
                        
                        std::cout << std::left << std::setw(10) << currentTask->GetID() << std::setw(11)
                                  << statusString << std::setw(8) << progress + " %\n";
                    }
                    std::cout << "\n";
                }
                break;
            }
            case Command::STATUS_TASK_ID:
            {
                assert(task);
                TinyTask::Status taskStatus = task->GetStatus();
                
                std::string statusString;
                ConvertTaskStatusToString(taskStatus, statusString);
                
                std::string progress(std::to_string(task->GetProgress()));
                progress.resize(5);
                
                std::cout << "Task ID " << std::to_string(task->GetID())
                          << " is " + statusString + " at progress " + progress + " %" << "\n\n";
                break;
            }
            case Command::UNRECOGNISED:
            {
                std::cout << "Unrecognised command\n\n";                
                break;
            }
            default:
                break;
        }
    }
    
    //Stop tasks that are running before closing
    for(unsigned int taskIndex = 0; taskIndex < taskIDs.size(); ++taskIndex)
    {
        TinyTask* currentTask = tasksPool.GetTask(taskIDs[taskIndex]);
        assert(currentTask);
        
        if(currentTask->IsRunning())
        {
            currentTask->Stop();
            while(currentTask->IsStopping() && !currentTask->HasStopped()) {}
        }
        
        if(currentTask->IsPaused() && currentTask->GetProgress() > 0.0f)
        {
            currentTask->Resume();
            while(!currentTask->IsRunning()) {}
            currentTask->Stop();
            while(currentTask->IsStopping() && !currentTask->HasStopped()) {}
        }
    }
    
    //Cleanup
    taskIDs.clear();

    return 0;
}
