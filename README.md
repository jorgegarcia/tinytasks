# tinytasks 

Simple and single-header tasks library in C++11

## Build instructions

The tests use Google Test, which is downloaded and compiled as part of the build process. In order to build the tests and the example with cmake, type the following commands from the the repository directory in the terminal (OSX and Linux only):

```shell
$mkdir build
$cd build
$cmake ..
$make
```

This will generate in the folder `build/bin/` the corresponding binaries. 

To run the tests please enter:

```shell
$cd bin
$./tests
```

To see the allowed commands for the example program you can similarly enter:

```shell
$cd bin
$./example --help
```

Alternatively, to build the Visual Studio solution on Windows from the command prompt run:

```shell
md build
cd build
cmake ..
```

Build tested on Visual Studio 2017 (Windows 10), clang 8.0 (OSX El Capitan) and gcc 7.3.0 (Ubuntu 18.04.1).


## How to use the library

The library has two classes:

* `TinyTask` is the minimal unit that represents an asynchronous task. It doesn't know much of threads, as it only holds the state and the `Run()` function for the thread, as well as a lambda that is configurable.
* `TinyTasksPool` is a thread pool that can handle and manage asynchronous tasks. It holds a finite number of worker threads, and it can queue tasks when the number of tasks is above the number of available threads. So for instance, if the pool has 8 threads and all are busy, if a new task is created it will be added to the waiting queue.

The tests and the examples are self-explanatory for how to use the API, but the minimal steps for using it are described below.

First, you need to include the header from the `include/` directory.

```C++
#include "tinytasks.h"
```

Then you can start instantiating objects:

```C++
using namespace tinytasks;

// Create a tiny task with ID 1 and run it synchronously
TinyTask task([]
{
    std::cout << "Running tiny task...\n"; 
}, 1);

task.Run();
```

This is another example. It waits for task completion:

```C++
TinyTask task([]
{
    uint8_t counter = 0;
    while(counter < 3) 
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++counter;
    } 
}, UINT16_MAX);

std::thread taskThread(&TinyTask::Run, &task);

while(!task.HasCompleted())
{
    std::cout << "Waiting for task to complete...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

Tasks can be paused, resumed and stopped. Also, a progress towards completion can be set.

Apart from instantiating tasks manually, a thread pool can be used:

```C++
// Create a pool with 8 worker threads
TinyTasksPool tinyTasksPool(8);

uint16_t taskID = tinyTasksPool.CreateTask();

TinyTasksPool::Result result = tinyTasksPool.SetNewLambdaForTask(taskID, []
{
    std::cout << "Running task from pool..\n"; 
});

// Get the task
TinyTask* task  = tinyTasksPool.GetTask(taskID);

// Wait until the task completes
while(!task->HasCompleted()) {}

// Do something else ...
```

In order to support the functionality of pausing, resuming, stopping and progress reporting, some function calls have to be made in the task lambda. The following example writes random numbers to a txt file, by using `IsStopping()`, `HasStopped()`, `PauseIfNeeded()` and `SetProgress()` task functions.

```C++
TinyTasksPool::Result lambdaResult = tinyTasksPool.SetNewLambdaForTask(taskID, [task]
{
    std::string filename;
    clock_t timeNow = clock();
    filename.append(std::to_string(timeNow) + ".txt");
                    
    std::FILE* fileToWrite = fopen(filename.c_str(), "w");
    
    const unsigned int maxIterations = 300;
    for(unsigned int value = 0; value < maxIterations; ++value)
    {
        if(task->IsStopping() || task->HasStopped()) break;
        const float randomNumber = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        std::string stringToWrite;
        stringToWrite.append(std::to_string(randomNumber) + " ");
        fwrite(stringToWrite.c_str(), sizeof(char), stringToWrite.size(), fileToWrite);
        task->SetProgress(static_cast<float>(value + 1) / static_cast<float>(maxIterations) * 100.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        task->PauseIfNeeded();
    }
    
    fclose(fileToWrite);
});
```

Please consult `test/tests.cpp` and `src/example.cpp` for detailed use cases.

## License

Released under MIT license

