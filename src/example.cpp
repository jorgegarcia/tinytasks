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
    std::cout << "\nTinyTasks v" << std::string(version()) << " example | Usage and allowed commands\n\n";

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

    while (getline(ss, item, delim))
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
    
    if(elems.size() > 1 && elems.size() < 3 && IsStringANumber(elems[1]))
        command.value = static_cast<uint16_t>(std::stoi(elems[1]));
    
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
            command.commandType = Command::PAUSE_TASK_ID;
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
        if(elems.size() == 2)
            command.commandType = Command::STATUS_TASK_ID;
    }
    
    return command;
}
    
}

int main(int argc, char* argv[])
{
    if(argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        PrintHelp();
        return 0;
    }
    
    std::cout << "\nTinyTasks v" << std::string(version()) << " example\n\n";
    
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
        
        if((command.commandType == Command::PAUSE_TASK_ID  ||
            command.commandType == Command::RESUME_TASK_ID ||
            command.commandType == Command::STOP_TASK_ID   ||
            command.commandType == Command::STATUS_TASK_ID)
           && std::find(taskIDs.begin(), taskIDs.end(), command.value) == taskIDs.end())
        {
            std::cout << "Command task ID not found\n\n";
            continue;
        }
        
        TinyTask* task = nullptr;
        
        if(command.commandType  == Command::PAUSE_TASK_ID   ||
            command.commandType == Command::RESUME_TASK_ID  ||
            command.commandType == Command::STOP_TASK_ID    ||
            command.commandType == Command::STATUS_TASK_ID)
        {
            task = tasksPool.GetTask(command.value);
            assert(task && "Task not found");
        }
        
        switch(command.commandType)
        {
            case Command::START:
            {
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
                if(tasksPool.GetNumThreads() + tasksPool.GetNumPendingTasks() >= tinytasks::constants::kMaxNumTasksInPool)
                {
                    std::cout << "Can't create more tasks, as the pool ran out of IDs\n\n";
                    break;
                }
                
                uint16_t taskID = tasksPool.CreateTask();
                taskIDs.push_back(taskID);
                TinyTask* currentTask = tasksPool.GetTask(taskID);
                
                if(command.value == 1)
                {
                    tasksPool.SetNewLambdaForTask(taskID, [currentTask]
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
                }
                else
                {
                    std::cout << "Task type not recognised. Only values 1 and 2 are allowed\n\n";
                }
                
                //Wait until task starts running
                while(!currentTask->IsRunning()) {}
                
                std::cout << "Created task of type " << std::to_string(command.value) << " and ID " << std::to_string(taskID) << "\n\n";
                break;
            }
            case Command::PAUSE_TASK_ID:
            {
                task->Pause();
                while(!task->IsPaused()) {}
                break;
            }
            case Command::RESUME_TASK_ID:
            {
                task->Resume();
                while(!task->IsRunning()) {}
                break;
            }
            case Command::STOP_TASK_ID:
            {
                task->Stop();
                while(task->IsStopping() || !task->HasStopped()) {}
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
                    std::cout << std::left << std::setw(10) << "[Task ID]" << std::setw(11) << "[Status]" << std::setw(8) << "[Progress]\n";
                    
                    for(unsigned int taskIndex = 0; taskIndex < taskIDs.size(); ++taskIndex)
                    {
                        TinyTask* currentTask = tasksPool.GetTask(taskIDs[taskIndex]);
                        assert(currentTask);
                        
                        const TinyTask::Status status = currentTask->GetStatus();
                        std::string statusString;

                        if(status == TinyTask::Status::COMPLETED)
                        {
                            statusString.append("completed");
                        }
                        else if(status == TinyTask::Status::PAUSED)
                        {
                            statusString.append("paused");
                        }
                        else if(status == TinyTask::Status::RUNNING)
                        {
                            statusString.append("running");
                        }
                        else if(status == TinyTask::Status::STOPPED)
                        {
                            statusString.append("stopped");
                        }
                        
                        std::string progress(std::to_string(currentTask->GetProgress()));
                        progress.resize(5);
                        
                        std::cout << std::left << std::setw(10) << currentTask->GetID() << std::setw(11) << statusString
                        << std::setw(8) << progress + " %\n";
                    }
                    std::cout << "\n";
                }
                break;
            }
            case Command::STATUS_TASK_ID:
            {
                TinyTask::Status taskStatus = task->GetStatus();
                
                if(taskStatus == TinyTask::Status::COMPLETED)
                {
                    std::cout << "Task COMPLETED\n\n";
                }
                else if(taskStatus == TinyTask::Status::PAUSED)
                {
                    std::cout << "Task PAUSED\n\n";
                }
                else if(taskStatus == TinyTask::Status::RUNNING)
                {
                    std::cout << "Task RUNNING\n\n";
                }
                else if(taskStatus == TinyTask::Status::STOPPED)
                {
                    std::cout << "Task STOPPED\n\n";
                }
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
    
    //Stop runnig tasks before closing
    for(unsigned int taskIndex = 0; taskIndex < taskIDs.size(); ++taskIndex)
    {
        TinyTask* currentTask = tasksPool.GetTask(taskIDs[taskIndex]);
        assert(currentTask);
        
        if(currentTask->IsRunning())
        {
            currentTask->Stop();
            while(currentTask->IsStopping() && !currentTask->HasStopped()) {}
        }
        
        if(currentTask->IsPaused())
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
