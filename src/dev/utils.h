/** @file utils.h
 *  @brief Headers for helper functions and data structures of LKM.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef DEV_UTILS_H
#define DEV_UTILS_H

#include "shared.h"
#include <linux/types.h>
#include <linux/kref.h>

/** @brief Registers the given process in UMS mode.
 *  @param pid: The pid of the process that holds resources (TGID).
 *  @return long: SUCCESS or an error (<0).
 */
long register_ums_process (pid_t pid);

/** @brief Unregisters the given process in UMS mode.
 *  @param pid: The pid of the process that holds resources (TGID).
 *  @return long: SUCCESS or an error (<0).
 */
long unregister_ums_process (pid_t pid);

/** @brief Initializes a worker node and saves it in the given process hashtable.
 *
 *  This function allocates the necessary data structures to represent a UMS worker
 *  and inserts it in the hashtable of the given process.
 *
 *  @return long: SUCCESS or an error (<0).
 */
long init_worker_node (void);

/** @brief Registers a new scheduler for the target process to be run on a specific cpu.
 *
 *  This function allocates the necessary memory to a struct ums_sched, and initializes
 *  it according to the function's parameters.
 *
 *  @param cpuid: The ID of the CPU on top of which the scheduler thread will run.
 *  @return long: SUCCESS or an error (<0).
 */
long register_ums_scheduler (unsigned int cpuid);

/** @brief Registers a worker thread on a specified scheduler's worker list.
 *  @param usr_worker: The worker's informations.
 *  @return long: SUCCESS or an error (<0).
 */
long register_usr_worker (struct ums_usr_worker *usr_worker);

/** @brief Executes the given worker on the current cpu.
 *
 *  Switches the execution from the scheduler thread to a worker thread, by swapping
 *  their pt_regs. This allows the scheduler's to effectively host the thread's execution.
 *
 *  @param tid: The PID of the worker to be executed.
 *  @return long: SUCCESS or an error (<0).
 */
long execute_thread (pid_t tid);

/** @brief Yields the current running worker and restores the scheduler's context.
 */
void thread_yield (void);

/** @brief Populates the array with the available worker's using their TIDs.
 *  @param tid_list: The array to be populated.
 */
void dequeue_list (unsigned int *tid_list);

/// @brief Terminates the current worker and restores the scheduler hosting it.
void terminate_worker (void);

/// Routine used to free worker when releasing the last kref.
void worker_release (struct kref *refcnt);

#endif // !DEV_UTILS_H
