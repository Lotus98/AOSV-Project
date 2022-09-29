# AOSV Final Project Report
_A.Y. 2020/2021_

Author: Nalin Dhingra (1967105)

# Introduction
This report will describe the main components that have been developed to ultimate the project.
In particular, there will be a description of the features developed and the reason why certain implementations were adopted.

On a final note, this report aims to describe the functioning of the developed software and it is not intended as guide on how to use its functionalities.
For additional informations regarding the _"how to"_, refer to the attached documentation.

## System specifications
### Virtualization
This project has been developed in a virtual machine with 2 cores and 6 GB of dedicated RAM, using the virtualization software `qemu`.

### Software
The loadable kernel module (LKM) has been developed and tested in **Ubuntu 20.04.4 LTS** utilizing the specific _Linux Kernel version: 5.10.122._

There is no guarantee on the correct functioning of the project using a different environment.


# Bugs
- The functionalities of this project do not support floating point operations within the workers context.


# Components
The project, as requested, is divided in two main components:
- The kernel module _umsdev.ko_:
  - Manages context switches between workers and schedulers, performed in the kernel with the usage of the IOCTL interface.
  - Exposes, in the _/proc_ filesystem, data relative to each process registered in **UMS** mode.
- The user library _libums.so_:
  - Provides high level API calls to user applications to interact with the LKM in a transparent way.

## Kernel Module (umsdev.ko)
The kernel module has been subject to an ulterior division, to improve the modularization of the project:
- The ums object composed by: **ums.c**
- The ioctl object composed by: **ioctl.h** and **ioctl.c**
- The utils object composed by: **utils.h** and **utils.c**
- The shared object composed by: **shared.h** and **shared.c**
- The procfs object composed by: **procfs.h** and **procfs.c**

### UMS
The **ums** file is used to perform the module initialization and destruction with the following functions:
```c
static int __init init_umsmodule(void);
static void __exit exit_umsmodule(void);
```

These functions are registered in the kernel through the API:
```c
module_init(init_umsmodule);
module_exit(exit_umsmodule);
```
#### Initialization
The initialization process is used to:
- Register a misc device.
- Initialize the **ProcFS** interface for the **UMS** module.
- Collect informations about the system, in particular the number of online CPUs.

#### Destruction
The destruction process is used to:
- Cleanup the entries created in **ProcFS**.
- Cleanup the data structures related to the processes that made use of the **UMS** module.
- Deregister the misc device.

### IOCTL
The **ioctl** files expose all the kernel API, accessible through the `ioctl();` system call, and used to perform all the needed operations in the kernel.
In particular the exposed commands are the following, registered in the file *src/dev/ioctl.h*:

```c
#define INIT_WORKER _IO(0x1337, 'a')
#define REGISTER_PROC _IO(0x1337, 'b')
#define TERMINATE_PROC _IO(0x1337, 'c')
#define REGISTER_SCHED _IOW(0x1337, 'd', unsigned int)
#define REGISTER_WORKER _IOW(0x1337, 'e', struct ums_usr_worker)
#define EXECUTE_THREAD _IOW(0x1337, 'f', pid_t)
#define THREAD_YIELD _IO(0x1337, 'g')
#define DEQUEUE_LIST _IOWR(0x1337, 'h', int)
#define TERMINATE_WORKER _IO(0x1337, 'i')
```
- **INIT_WORKER:** The command is used to initialize and register a worker thread in the context of its main process.
- **REGISTER_PROC:** The command is used to register a process to use the **UMS** module.
- **TERMINATE_PROC:** The command is used to deregister a process from **UMS** mode.
- **REGISTER_SCHED:** The command is used to register a scheduler thread for the calling process, if there are still CPU cores available.
- **EXECUTE_THREAD:** The command is to be used by a scheduler thread in userspace to execute a worker thread of its completion list.
- **THREAD_YIELD:** The command is to be used by a worker thread to be yielded. This will cause the scheduler to continue its execution.
- **DEQUEUE_LIST:** The command is to be used by a scheduler thread to get a list of the workers which are ready to be executed.
- **TERMINATE_WORKER:** The command is used to terminate the execution of a worker thread and restore the scheduler's context of execution.

### Shared
These files contain mainly the data structures used to define the objects needed for the implementation of the project functionalities.
In particular we can highlight the following, as the main structures:
- `struct ums_proc`: Represents and holds the informations of a process in **UMS** mode.
- `struct ums_sched`: Represents and holds the informations of a scheduler thread.
- `struct ums_worker`: Represents and holds the informations of a worker thread.

#### `struct ums_proc`
Following is the representation of the struct related to processes in **UMS** mode.
```c
struct ums_proc {
        struct ums_sched **schedulers; ///< An array of pointers to all the active scheduler threads.
        struct hlist_node node; ///< Node element for placing the process in the global hashtable.
        pid_t pid; ///< The process PID.
        DECLARE_HASHTABLE(workers, HBITS); ///< Hashtable of all workers initialized by a process.
        rwlock_t hash_lock; ///< Lock used to access the workers hashtable.
        wait_queue_head_t wq; ///< A wait queue used to implement the blocking call DequeueUmsCompletionListItems.
        struct procfs_proc_umsdata *proc_data; ///< The procfs data relative to the process.
};
```
The most important members are:
- `workers`: This is a hashtable declared as `hlist_head workers[1<<HBITS]`, it contains all the workers initialized by a process.
This is used to have a global access to all the workers, which simplifies access for the ProcFS interface and for cleaning up workers not used in any completion list.
- `wait_queue_head_t wq`: This member is used with the concept of wait queues in the Linux Kernel to implement the blocking user function `DequeueUmsCompletionListItems()`.
In particular a wait queue allows to suspend the execution of a thread until a certain condition is met.
More details will be given in the next section when explaining the implementation of this functionality.

#### `struct ums_sched`
Following is the representation of the struct related to scheduler threads in **UMS** mode.
```c
struct ums_sched {
        struct task_struct *sched_task; ///< The task_struct of the scheduler thread.
        struct ums_worker *current_worker; ///< The worker currently running on the scheduler context, NULL if none.
        struct pt_regs sched_regs; ///< Backup of the scheduler's state, used to implement the context switch.
        DECLARE_HASHTABLE(worker_list, HBITS); ///< The completion list (implemented as an hashtable).
        rwlock_t lock; ///< Lock used to access the hashtable.
        struct procfs_sched_umsdata *sched_data; ///< The procfs data related to the scheduler.
};
```
The most important members are:
- `struct pt_regs sched_regs`: This member will contain all the information related to the execution context of the scheduler thread in userspace.
In particular it is used to save such execution state and restore it during context switches.
This feature is further explained in the next section.
- `worker_list`: This member is a hashtable used to represent the completion list of a certain scheduler thread.
The data structure of the hashtable is chosen to provide greater efficiency with respect to a simple linked list.
In fact this implementation improves the speed of access to a needed worker by a great margin compared to the other mentioned data structure.

#### `struct ums_worker`
Following is the representation of the struct related to worker threads in **UMS** mode.
```c
struct ums_worker {
        struct task_struct *task; ///< The task_struct of the worker thread.
        enum state state; ///< The current state of the worker. (IDLE, TERMINATED, SCHEDULED, RUNNING)
        struct kref refcnt; ///< Reference counter for the worker.
        rwlock_t rwlock; ///< Lock used to keep coherent the state of a worker.
        struct procfs_worker_umsdata *worker_data; ///< The procfs data of the worker.
};
```
The most important members are:
- `enum state state`: This member is used to determine the operations that are and that can be performed on a certain worker thread. In particular it is differentiated in the states defined as:
  ```c
  enum state {WORKER_RUNNING, WORKER_IDLE, WORKER_TERMINATED, WORKER_SCHEDULED};
  ```
  - `WORKER_RUNNING`: Is used to specify a worker thread is executing on top of a scheduler thread.
  - `WORKER_IDLE`: Is used to specify a worker thread which is available to be executed or dequeued in a scheduler's list.
  - `WORKER_TERMINATED`: Is used to specify a worker thread which has terminated its routine in **UMS** mode.
  - `WORKER_SCHEDULED`: Is used to specify a worker thread which has been dequeued in a list to be used by a scheduler thread in userspace.
- `struct kref refcnt`: This member is very important as it guarantees the consistency of a worker thread.
It is using the kernel data structure `struct kref`, which is the _Linux Kernel_ implementation for reference counters.
Given the fact that worker threads can be shared among different schedulers it is necessary to maintain their pointers consistent, for this reason this object as been used.

### Utils
The **utils** files define all the major functions to perform the commands exposed by the **ioctl** object.
Here we will discuss the most important commands provided and what decisions have been made regarding their implementation.

#### INIT_WORKER
The main objective of the project is to have threads that can be scheduled in userspace.
To do so, we need to create threads that will not be scheduled by the _Linux Kernel_, in order to be able to control their scheduling policy in user mode.
Since in userspace, a **UMS** thread is created on top of a classic **pthread**, we have to rely on the kernel module to perform the action mentioned above.
This command in fact, in addition to the initialization of the necessary data structures, needed to manage a worker thread, it performs one more important role. The role of setting the worker thread state to `TASK_IDLE`. This operation is performed in the following snippet of code placed in the file _ioctl.c_.
```c
__set_current_state(TASK_IDLE);
schedule();
```
As it can be seen we are changing the `state` member of the `struct task_struct` corresponding to the calling thread.
Analyzing in greater detail the `TASK_IDLE` state, it is defined in the following macro:
```c
#define TASK_IDLE			(TASK_UNINTERRUPTIBLE | TASK_NOLOAD)
```
I have chosen this state as it avoids unexpected behaviour in case of an interrupt, and does not affect the scheduling algorithm since it is set to have no working load on the CPU.

#### Context switch
This is a very key functionality of the **UMS** module, as it is needed to perform the commands:
- `EXECUTE_THREAD`
- `THREAD_YIELD`
- `TERMINATE_WORKER`
In fact in all these commands we need to switch the execution context of a scheduler to the one of a worker or viceversa.
After a deep research on the kernel function `schedule()`, I outlined the main component that performs the context switch operation for the Linux kernel, and that is `__switch_to_asm()` which definition can be found [here](https://elixir.bootlin.com/linux/v5.10.122/source/arch/x86/entry/entry_64.S#L226) for reference.
In brief this piece of code is architecture-dependent and performs a context switch in kernel space by switching the stack, registers, TLS and other architectural facilities.
Moreover it also perform a switch of the `task_struct` of the two processes involved, and their FPU context (which is the context used to execute floating point operations).
This initially looked ideal, but unfortunately `__switch_to_asm()` is not an exported symbol.
##### Hack to retrieve non-exported symbols
To overcome this problem, I found a solution applied in modern kernel versions which make use of the exported function `register_kprobe()`.
In fact this function allows us to collect debugging informations on any kernel component, but, more of interest to our situation is the fact that we can use it to retrieve the address of non-exported symbols at run-time.
Therefore I made use of this hack to get the needed symbol and use it as per my initial idea.
Unfortunately, after a lot of debugging and many tries to get the wanted component to work correctly, I realized that it wasn't feasible to integrate this useful symbol in the **UMS** module, since its usage was inevitably tainting the core data kept by the Linux kernel's **scheduling component**, thus causing a kernel panic.
##### The final solution
Ultimately with a little more research I opted for a less common but more efficient solution than the ones found on the internet.
Which, in the case of the command `EXECUTE_THREAD`, is to save the scheduler thread's `struct pt_regs` to preserve its execution context, and swap it in memory with the one of the worker that is to be executed.
And once a worker thread yields or is terminated, the **UMS** module will restore the scheduler's context by rewriting in memory the original `struct pt_regs` that was previously saved.
The general concept can be grasped from the following snippet of code taken from the function `execute_thread(pid_t tid)` defined in the file _src/dev/utils.c_.
```c
// Save scheduler's context.
memcpy(&scheduler->sched_regs, task_pt_regs(scheduler->sched_task), sizeof(struct pt_regs));
// Perform context switch.
memcpy(task_pt_regs(scheduler->sched_task), task_pt_regs(worker_node->worker->task), sizeof(struct pt_regs));
```

#### DEQUEUE_LIST
Another functionality of interest is the one corresponding to the user function `DequeueUmsCompletionListItems()`.
In particular the request for this function was that it should be blocking, for the scheduler thread, until a worker thread would become available.
To implement this functionality I made use of the wait queues provided by the kernel API, the use case can be outlined in the following snippet of code taken from the function `void dequeue_list (size_t size, unsigned int *tid_list)` in the file _src/dev/utils.c_
```c
do {
        wait_event(process->wq, wait_queue_check());
        terminated = true;

        hash_for_each(scheduler->worker_list, bkt, worker_node, node) {
                if (size <= tid_list[0])
                        break;
                write_lock(&worker_node->worker->rwlock);
                if ( (worker_node->worker->state == WORKER_RUNNING || worker_node->worker->state == WORKER_SCHEDULED) && terminated)
                        terminated = false;
                else if (worker_node->worker->state == WORKER_IDLE) {
                        PRINTDBG("Found worker to put in queue: [TID]: %d", worker_node->tid);
                        worker_node->worker->state = WORKER_SCHEDULED;
                        tid_list[++tid_list[0]] = worker_node->tid;
                }
                write_unlock(&worker_node->worker->rwlock);
        }
} while (!terminated && (tid_list[0] == 0) );
```
The core of the dequeue function is represented by this while loop, where we immediately check the wait condition represented by the function `wait_queue_check()`:
```c
static bool wait_queue_check (void)
{
        ...
        terminated = true;
        hash_for_each(sched->worker_list, bkt, worker_node, node) {
                read_lock(&worker_node->worker->rwlock);
                if (worker_node->worker->state == WORKER_IDLE) {
                        read_unlock(&worker_node->worker->rwlock);
                        return true;
                }
                if ( (worker_node->worker->state == WORKER_RUNNING || worker_node->worker->state == WORKER_SCHEDULED) && terminated)
                        terminated = false;
                read_unlock(&worker_node->worker->rwlock);
        }

        return terminated;
}
```
As it is noticeable, both the wait condition and the do-while loop that fills the dequeue list have a similar structure.
The general principle of both, is that the scheduler thread needs to wait until either one of the two following conditions are met:
- There is a worker in `WORKER_IDLE` state, thus ready to be scheduled.
- All the workers in a scheduler's completion list are terminated.
When one these two conditions is met the **UMS** module, will continue its execution to populate the requested dequeue list.
Finally it is important to notice that there are two possible cases when returning to userspace from this routine.
The first one is when there is at least one available worker, thus the scheduler has to still continue its functioning.
The second is when the number of returned workers is 0, meaning that all workers from the scheduler's completion list are terminated, thus the scheduler thread can terminate its execution as well.

### ProcFS
The ProcFS interface has been implemented as per request of the process, thus the files and directory structures are exposed just as expected.


## User Library (libums.so)
Also the user library has been divided in sub-modules:
- _bitmap.h_
- _list.h_
- _shared.h_
- _ums.c_ and _ums.h_
- _utils.c_ and _utils.h_

### Bitmap management
The file _bitmap.h_ has been created to abstract the management of bitmaps, in particular that used to manage the system CPUs, through the usage of macros.
To be more detailed this sub-module abstract the type `bitmap_t` which is defined as:
```c
typedef unsigned long *bitmap_t;
```

### List management
The file _list.h_ is an adaptation of the kernel linked list implementation, to work in userspace.

### Shared data
The shared data comprehends all the macros, data structures, and global variables needed for the correct functioning of the library.
For our interest and for the sake of simplicity, we will just cover the most crucial and important ones.

#### `struct ums_thread`
Our first structure of interest is the structure `struct ums_thread`:
```c
struct ums_thread {
        pthread_t pthread; ///< The related pthread.
        pid_t tid; ///< The TID of the created thread.
};
```
It is not very interesting by itself, but it is a structure at the core of every thread in **UMS** mode, both workers and schedulers. We will discuss in the next sections its importance.

#### `struct ums_worker`
```c
struct ums_worker {
        struct ums_thread thread; ///< The corresponding UMS thread.
        unsigned long refcnt; ///< Reference counter, used to keep track of how many lists contain this worker.
        enum state state; ///< The state of the worker.
        pthread_mutex_t mutex; ///< Mutex to keep the worker's state coherent as well as the refcnt.
};
```

#### `struct ums_sched`
```c
struct ums_sched {
        struct ums_thread *ums_thread; ///< The UMS thread related to the scheduler.
        struct ums_worker *current_worker; ///< The worker currently executing in the scheduler's context.
        unsigned int nworkers; ///< The total number of workers in the worker_list.
        struct list_head *worker_list; ///< The completion list assigned to the scheduler.
};
```

All these structures have nothing unusual but it is useful to know their composition to understand the explanations in the following sections.
On the contrary it is needed to discuss the following data structures, that will be crucial the creation of **UMS** threads.

#### `struct ums_worker_arg`
```c
struct ums_worker_arg {
        struct ums_thread *ums_thread; ///< The corresponding UMS specific thread.
        void (*ums_routine) (void *); ///< The routine passed to ums_worker_create.
        void *arg; ///< The argument passed to ums_worker_create.
        sem_t *tid_sem; ///< Semaphore used to coordinate the assignment of the TID in the struct ums_thread.
};
```
This object is used in the creation of worker threads in **UMS** mode, and it is particularly crucial in the synchronization between the newly created thread and the main thread.

#### `struct ums_sched_arg`
```c
struct ums_sched_arg {
        struct ums_thread *ums_thread; ///< The UMS thread corresponding to the scheduler thread.
        void (*sched_routine) (void); ///< The scheduler function.
        struct list_head *list; /// The head of the completion list.
        unsigned int cpuid; ///< The CPU to which the thread will be bound.
        sem_t *sem; ///< Semaphore used to coordinate the main thread with the scheduler thread.
};
```
The same thing applies to this object, which is used in the creation of scheduler threads in **UMS** mode.

In particular for both the last seen structures it is easy to notice a member of the type `sem_t`, in fact this semaphore is used to guarantee the coherency of data in the `struct ums_thread` between the main thread and the children threads.

### UMS core
The library core is composed of the API exposed to the user applications, which are:
- `ums_init()` and `ums_destroy()`
- `ums_worker_create()`
- `EnterUmsSchedulingMode()`
- `ExecuteUmsThread()`
- `UmsThreadYield()`
- `DequeueUmsCompletionListItems()`

#### Init and Destroy
The init and destroy routines are used respectively to initialize the data structures needed to manage the process in **UMS** mode, and to clean them up.

#### UMS Worker creation
The creation of a **UMS** worker thread is an interesting process, which is mainly composed of executing a function call to `pthread_create()` which will run a specific routine developed to put the newly created thread in an **IDLE** state.
The following snippets of code will show a general workflow for the **UMS** thread creation process:
```c
int ums_worker_create (struct ums_worker *worker,
                       void (*start_routine) (void *),
                       void *arg)
{
        struct ums_worker_arg *wrapper_arg;
        struct ums_thread *thread;

        // 1. Initialization of the struct ums_worker.
        ...

        // 2. Initialization of the struct ums_worker_arg to be passed as an argument to the pthread routine.
        wrapper_arg->ums_thread = thread;
        wrapper_arg->ums_routine = start_routine;
        wrapper_arg->arg = arg;
        if (sem_init(wrapper_arg->tid_sem, 0, 0) != 0) {
                ...
        }
        ...

        // 3. Perform the call to pthread_create()
        ret_pthread = pthread_create(&thread->pthread, NULL, worker_wrap_routine, wrapper_arg);
        ...

        // 4. Wait for the ums_thread->tid to be populated
        if (sem_wait(wrapper_arg->tid_sem) != 0) {
                ...
        }
        ...

        return SUCCESS;
}
```
Within the outlined steps it is particularly important the creation of the object `wrapper_arg` as it is used as an argument to the routine executed with `pthread_create()`.
This routine, `worker_wrap_routine` will be further explained in the next section, for the moment it suffices to know that among its various jobs, it has to populate the `tid` member of the `struct ums_thread` related to the worker created.
And since this member is used to uniquely identify each worker it is important to keep its value coherent among all threads that will access it.
For this reason the main thread will perform a wait operation on the semaphore mentioned previously, which will block the calling thread until the member in question is correctly populated.

#### UMS scheduling mode
The creation of a scheduler thread is very much the same as the previously explained creation of a worker thread, with the major difference being the fact that after registering the scheduler thread within the **UMS** module, the thread needs also to register its completion list. This will be seen in more details in the [following section](#markdown-header-helper-functions).

#### Dequeue completion list
This function is quite particular, as in the way it communicates to the Kernel module.
In fact we need to obtain from the kernel function a list of available workers, and to do so the kernel will return an array filled with the TIDs of said workers.
An interesting note, is to be made on the way the dynamic size of this array is managed.
```c
struct list_head *DequeueUmsCompletionListItems (size_t nworkers)
{
        ...
        tid_list = calloc(nworkers + 1, sizeof(*tid_list));
        // In the first position copy the nmemb size of the array. (Little hack for LKM)
        *tid_list = (unsigned int)nworkers;

        // IOCTL call
        ioctl(dev_fd, DEQUEUE_LIST, tid_list);
        if (*tid_list == 0) // All the workers are terminated
                return NULL;

        // Create list
        head = malloc(sizeof(*head));
        INIT_LIST_HEAD(head);
        for (size_t i = 1; i <= tid_list[0]; i++) {
                if (tid_list[i] == 0)
                        break;
                worker = find_worker_tid(ums_schedulers[cpuid]->worker_list, (pid_t)tid_list[i]);
                ...
                worker_node = malloc(sizeof(*worker_node));
                worker_node->worker = worker;
                list_add(&worker_node->list, head);
        }

        return head;
}
```
We can see how the function accepts a parameter, whose value determines the size of the array.
But more interestingly we can see that the actual allocation size is `+ 1`, with respect to the given size.
This is made so that the first element of the array can actually be used to communicate to the kernel the number of workers to retrieve, and implicitly the size of the array itself.
In the same first element the kernel will write the number of workers retrieved, which is equal to `0` when all the workers in the completion list are terminated.
In which case the library function will return a NULL pointer, else it will generate the actual list to be returned to the user application.

### Helper functions
In this section we will discuss the most important helper functions, which are used, respectively, in the creation of worker threads and the creation of scheduler threads:
- `void *worker_wrap_routine (void *arg)`
- `void *sched_wrap_routine (void *arg)`
We already saw this functions used as parameters to the call `pthread_create()`, in the functions `ums_worker_create()` and `EnterUmsSchedulingMode()`.
They are in fact the routines that should be run by the newly created threads.

#### `worker_wrap_routine`
We will now see through the code the internals of this function.
```c
void *worker_wrap_routine (void *arg)
{
        struct ums_worker_arg *wrap_arg = (struct ums_worker_arg *)arg;
        struct ums_thread *thread = wrap_arg->ums_thread;

        // 1. Populate the tid member of the struct ums_thread.
        thread->tid = gettid();
        // 2. Signal the semaphore so that the main thread can continue its execution.
        if (sem_post(wrap_arg->tid_sem) != 0) {
                ...
        }

        // 3. Initialize the calling worker within the UMS module.
        // Note: This ioctl call, as already explained, will stop the calling thread execution.
        if (ioctl(dev_fd, INIT_WORKER) != SUCCESS) {
                ...
        }

        // 4. Execute worker function.
        // Note: This call will be reached only when the thread will be scheduled
        wrap_arg->ums_routine(wrap_arg->arg);

        ...

}
```
This snippet of code is related to the "first part" of the routine job, namely that of initialization of the **UMS** worker and execution of the target function.
Once the routine assigned to the worker will be completed, the wrapper routine will perform the cleanup of data structures used and restoration of the scheduler thread's execution context.
```c
void *worker_wrap_routine (void *arg)
{

        ...

        wrap_arg->ums_routine(wrap_arg->arg);

        struct ums_worker *worker;
        unsigned int cpuid;

        // 1. Retrieve the cpuid, to get a pointer to the scheduler who is hosting the worker's execution.
        getcpu(&cpuid, NULL);
        worker = ums_schedulers[cpuid]->current_worker;
        ums_schedulers[cpuid]->current_worker = NULL;

        // 2. Set the state of the worker as terminated
        pthread_mutex_lock(&worker->mutex);
        worker->state = WORKER_TERMINATED;
        pthread_mutex_unlock(&worker->mutex);

        // 3. Execute the IOCTL call to restore the scheduler and terminate the worker.
        ioctl(dev_fd, TERMINATE_WORKER);

        // 4. Cleanup
        free(wrap_arg);

        return NULL;
}
```

#### `sched_wrap_routine`
Let's now see the difference with the wrapper routine for the creation of a scheduler thread.
```c
void *sched_wrap_routine (void *arg)
{
        long retval;
        struct ums_sched_arg *wrap_arg = (struct ums_sched_arg *)arg;
        ums_worker_node_t *worker_node;

        // 1. Same as in the worker_wrap_routine.
        wrap_arg->ums_thread->tid = gettid();

        // 2. Bind thread to a precise CPU core.
        // Note: This is done through the function bind_to_cpu, which ultimately
        //       calls the function sched_setaffinity(), that will communicate to
        //       the kernel that the thread can only execute on the indicated CPU core.
        bind_to_cpu(wrap_arg->cpuid);

        // 3. Register the scheduler thread within the UMS module.
        retval = ioctl(dev_fd, REGISTER_SCHED, &wrap_arg->cpuid);
        ...

        // 4. Register the completion list within the UMS module, and assign it to this specific scheduler.
        list_for_each_entry(worker_node, wrap_arg->list, list) {
                struct ums_usr_worker *usr_worker;

                usr_worker = malloc(sizeof(*usr_worker));
                ...
                usr_worker->tid = worker_node->worker->thread.tid;
                usr_worker->cpuid = (unsigned int)wrap_arg->cpuid;
                ioctl(dev_fd, REGISTER_WORKER, usr_worker);
                free(usr_worker);
        }

        // 5. Signal on the semaphore so the main thread can continue its execution.
        if (sem_post(wrap_arg->sem) != 0) {
                ...
        }

        // 6. Execute the scheduler routine.
        wrap_arg->sched_routine();

        ...

}
```
Once again we have shown the first part of the function which performs the actions described, the next snippet will show the cleanup phase.
```c
void *sched_wrap_routine (void *arg)
{

        ...

        wrap_arg->sched_routine();

        struct ums_sched *scheduler = ums_schedulers[wrap_arg->cpuid];
        ums_worker_node_t *tmp;
        struct ums_worker *worker;

        // 1. Cleanup and free the memory used to manage the completion list.
        list_for_each_entry_safe(worker_node, tmp, scheduler->worker_list, list) {
                worker = worker_node->worker;
                pthread_mutex_lock(&worker->mutex);
                worker->refcnt--;
                pthread_mutex_unlock(&worker->mutex);
                list_del(&worker_node->list);
                free(worker_node);
        }

        // 2. Cleanup
        free(wrap_arg);

        return NULL;
}
```


# Results
The results of this project have shown that it is feasible to create a **UMS** subsystem in the Linux Kernel, through the creation of a LKM to manage context switches.
I wanted to have some statistical data and provide the average time needed to perform a switch, but it wasn't feasible due to the fact that FPU operations are not available in kernelspace.
For this reason I will simply show the last switch time, collected by running the example user applications, with 128, 256, 512 and 1024 workers.
To have more precise results, I collected the value from 5 runs of the same example, of course this approach is not the best, but it is enough to showcase the capabilities of the developed project.
These values where collected from the exposed data in ProcFS.

## Single scheduler
### 128 workers
```
Time last switch:      	2467 ns
Time last switch:      	2369 ns
Time last switch:      	2989 ns
Time last switch:      	2645 ns
Time last switch:      	6387 ns
```
### 256 workers
```
Time last switch:      	2000 ns
Time last switch:      	7773 ns
Time last switch:      	2315 ns
Time last switch:      	2857 ns
Time last switch:      	2374 ns
```
### 512 workers
```
Time last switch:      	5813 ns
Time last switch:      	1425 ns
Time last switch:      24895 ns
Time last switch:      	2465 ns
Time last switch:      	2634 ns
```
### 1024 workers
```
Time last switch:      	3622 ns
Time last switch:      	2462 ns
Time last switch:      	2154 ns
Time last switch:      	1878 ns
Time last switch:      	1137 ns
```

## Multiple schedulers non-shared completion list
### 128 workers
#### Scheduler 0
```
Time last switch:      	1084 ns
Time last switch:      	1425 ns
Time last switch:      	1954 ns
Time last switch:      	2636 ns
Time last switch:      	2961 ns
```
#### Scheduler 1
```
Time last switch:      	2738 ns
Time last switch:      	2799 ns
Time last switch:      	2148 ns
Time last switch:      	1284 ns
Time last switch:      	1933 ns
```
### 256 workers
#### Scheduler 0
```
Time last switch:      	2296 ns
Time last switch:      	2197 ns
Time last switch:      	8305 ns
Time last switch:      	2463 ns
Time last switch:      	4026 ns
```
#### Scheduler 1
```
Time last switch:      	2853 ns
Time last switch:      	2333 ns
Time last switch:      	2617 ns
Time last switch:      	2346 ns
Time last switch:      	1627 ns
```
### 512 workers
#### Scheduler 0
```
Time last switch:      	1274 ns
Time last switch:      	1674 ns
Time last switch:      	2875 ns
Time last switch:      	2724 ns
Time last switch:      	2607 ns
```
#### Scheduler 1
```
Time last switch:      	2123 ns
Time last switch:      	1715 ns
Time last switch:      	2093 ns
Time last switch:      	2363 ns
Time last switch:      	1837 ns
```
### 1024 workers
#### Scheduler 0
```
Time last switch:      	2146 ns
Time last switch:      	1615 ns
Time last switch:      	2453 ns
Time last switch:      	2159 ns
Time last switch:      	1824 ns
```
#### Scheduler 1
```
Time last switch:      	2784 ns
Time last switch:      	3108 ns
Time last switch:      	1561 ns
Time last switch:      	3868 ns
Time last switch:      	2768 ns
```

## Multiple schedulers shared completion list
### 128 workers
#### Scheduler 0
```
Time last switch:      	2042 ns
Time last switch:      	2752 ns
Time last switch:      	1882 ns
Time last switch:      	6561 ns
Time last switch:      	1540 ns
```
#### Scheduler 1
```
Time last switch:      	2017 ns
Time last switch:      	1992 ns
Time last switch:      	2744 ns
Time last switch:      	2765 ns
Time last switch:      	2487 ns
```
### 256 workers
#### Scheduler 0
```
Time last switch:      	2939 ns
Time last switch:      	4378 ns
Time last switch:      	2472 ns
Time last switch:      	2619 ns
Time last switch:      	2763 ns
```
#### Scheduler 1
```
Time last switch:      	2252 ns
Time last switch:      	2440 ns
Time last switch:      	1797 ns
Time last switch:      	3496 ns
Time last switch:      	2407 ns
```
### 512 workers
#### Scheduler 0
```
Time last switch:      	8424 ns
Time last switch:      	2866 ns
Time last switch:      	2580 ns
Time last switch:      	3304 ns
Time last switch:      	 942 ns
```
#### Scheduler 1
```
Time last switch:      	2296 ns
Time last switch:      	2992 ns
Time last switch:      10692 ns
Time last switch:      	2045 ns
Time last switch:      	1483 ns
```
### 1024 workers
#### Scheduler 0
```
Time last switch:      	2115 ns
Time last switch:      	3147 ns
Time last switch:      	2449 ns
Time last switch:      	1241 ns
Time last switch:      	2651 ns
```
#### Scheduler 1
```
Time last switch:      	2094 ns
Time last switch:      	2437 ns
Time last switch:      	2407 ns
Time last switch:      	3447 ns
Time last switch:      	6581 ns
```


# Conclusions
To finalize this project, I would like to take this opportunity to express the fact that this has been a great learning experience.
In fact to know the theory about a certain subsystem, and to be able to actually make use of it in the best and more efficient way are two different worlds.
For this reason I really enjoyed the development process, especially being able to learn many kernel hacks and use cases.
By no means I think of myself as being capable of writing efficient kernel code, but it was an exiting first step to a deeper knowledge of how to develop kernel-level subsystems.

## Further improvements
As a final note, I want to briefly showcase the ideas I had regarding potential improvements to be made to the developed module.
To set a realistic goal, some features where not developed and some could have done better, in particular, I would like to make use of this section to list some of the improvements that came to my mind while developing and reviewing the project:
- Use the **RCU** lockless subsystem instead of **rwlocks** in the Kernel module.
- Implement shared dequeue lists.



# References
- [Misc device drivers](https://embetronicx.com/tutorials/linux/device-drivers/misc-device-driver/)
- [IOCTL](https://embetronicx.com/tutorials/linux/device-drivers/ioctl-tutorial-in-linux/)
- [ProcFS](https://embetronicx.com/tutorials/linux/device-drivers/procfs-in-linux/)
- [Kernel source code](https://elixir.bootlin.com/linux/v5.10.122/source)
- [Kernel coding style](https://www.kernel.org/doc/html/latest/process/coding-style.html)
- [Major kernel utilities](https://www.kernel.org/doc/html/latest/kernel-hacking/hacking.html#)
- [Locking in the Linux Kernel](https://www.kernel.org/doc/html/latest/kernel-hacking/locking.html)
- [Linux Kernel API](https://linux-kernel-labs.github.io/refs/heads/master/labs/kernel_api.html#linux-kernel-api)
- [Hashtable usage in the Linux Kernel](https://lwn.net/Articles/510271/)
- [Process Management](https://raw.gpm.name/teaching/2021-aosv/AOSV2021-10-ProcessManagement-v2.pdf)
- [Scheduling in the Linux Kernel](https://raw.gpm.name/teaching/2021-aosv/AOSV2021-11-Scheduling-v2.pdf)
- [How to retrieve non-exported symbols in the Linux Kernel (register_kprobe)](https://stackoverflow.com/a/70934260)
- [Wait queues in the Linux Kernel](https://embetronicx.com/tutorials/linux/device-drivers/waitqueue-in-linux-device-driver-tutorial/)
- [Reference counters for Kernel objects (kref)](https://www.kernel.org/doc/html/latest/core-api/kref.html)
- Many other useful resources that have been lost.
