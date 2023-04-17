<!-- TOC -->
  * [Introduction](#introduction)
    * [ThreadPool](#threadpool)
    * [BlockingQueue](#blockingqueue)
  * [Installation](#installation)
<!-- TOC -->
## Introduction
My collection of Concurrency related header-only classes while going through `C++ Concurrency in Action` book.
Following is the brief description of classes

### ThreadPool

A thread pool to manage a group of worker threads to execute tasks - It expects a template parameter for the underlying container to store tasks:
* By default, it uses `BlockingQueue` to store and extract tasks
* Otherwise, `PriorityQueue` is another candidate for underlying queue

`Threadpool` permits concurrent invocation of `add_task` member methods and other helper methods 
* except `stop` and `stop_early`
* and methods which permit concurrent invocation have been annotated with `thread_safe`


### BlockingQueue
A `std::condition_variable` based `BlockingQueue` to hold tasks in a queue and supports following methods:
*  `push` to push an item (task) on the queue
*  `pop` waits for `std::chrono::milliseconds::max()` for an element to be on queue to pop, otherwise throws `std::runtime_error`
*  `try_pop_for` waits for a user specified time interval for an element to be on queue and returns a pair:
    - `pair.first` is a `std::unique_ptr` containing the item, otherwise nullptr if there was nothing in the queue
    - `pair.second` returns true if queue has been closed, otherwise false
*  `try_pop` like `try_pop_for`, only difference is that it tries for `_wait_time` milliseconds (set in constructor)

Following is the class template details
* `tparam T`: type of tasks, can be either executables or executables wrapped inside `PriorityWrapper` to add priority
* `tparam Container`: `std::queue<T>` or `std::multiset<T>`, not any other container. This is enforced in constructor 
                      `std::multiset<T>` is supported in case item has a priority (e.g. wrapped inside `PriorityWrapper`)

## Installation
As the classes are header-only, user can simply copy the [include](https://github.com/aniliitb10/ConcurrencyInAction/tree/master/include) directory and use in their projects.