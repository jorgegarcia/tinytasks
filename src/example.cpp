#include <iostream>
#include "../include/tinytasks.h"

using namespace tinytasks;

void PrintHelp()
{
    std::cout << "\nTinyTasks v" << std::string(version()) << " example | Usage and allowed commands\n\n";

    std::cout << "start: starts a task and prints its ID\n\n";
    std::cout << "start <task_type_id>: starts a task of type 1 or 2\n";
    std::cout << "\t1: writes random numbers to disk during 1 minute\n";
    std::cout << "\t2: generates random numbers during 1 minute\n\n";
    std::cout << "pause <task_id>: pauses the task with the given id\n\n";
    std::cout << "resume <task_id>: resumes task with the given id (if paused)\n\n";
    std::cout << "stop <task_id>: stops the task with the given id (if not stopped)\n\n";
    std::cout << "status: prints the id, the status and the progress for each task\n\n";
    std::cout << "status <task_id>: as above, but for a single task\n\n";
    std::cout << "quit: exits the program\n\n";
}

int main(int argc, char* argv[])
{
    if(argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        PrintHelp();
        return 0;
    }

    return 0;
}
