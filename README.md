# ThreadPool

A thread pool is a design pattern where a number of threads are created to perform a number of tasks, which are usually organized in a queue. <br/>
Typically, there are many more tasks than threads. As soon as a thread completes its task,
it will request the next task from the queue until all tasks have been completed. <br/>
The thread will then sleep until there are new tasks available <br/>


My thread pool created with N threads - so that it can handle at most N tasks temporarily.
The function tpInsertTask() is responsible for inserting a new task into a task queue within the thread pool.
An enqueue task will remain there until one of the threads dequeues it in order to treat it.
If there is no free thread to perform it, the task will be performed only after one of the threads becomes free.
If there is a free thread, he will of course execute it.
If a thread from the thread pool finishes executing its task, and there are no tasks placed in the queue, it is will wait (no busy waiting!) until a new task is put in the queue.

## Running
Use Makefile to compile the files.
./a.out -> will run the project.


![a](https://github.com/sapirhender123/ThreadPool/blob/master/Threadpool.png)

