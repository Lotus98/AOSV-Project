/** @file ums.h
 *  @brief Header file exposing the main user API to interact with the UMS driver.
 *
 *  This file contains the main data structure and variables needed by the UMS library
 *  to interact with the IOCTL driver. In particular it implements the main endpoints
 *  used by the user applications.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef LIB_UMS_H
#define LIB_UMS_H

// Includes
#include "list.h"
#include "shared.h"

// Prototypes
/** @brief Initializes the UMS driver for the current process.
 *
 *  This function is to be called by the main thread before starting a UMS session.
 *
 *  @return 0 if successful <0 if failed.
 */
int ums_init(void);

/** @brief Cleans up all UMS data created by a process.
 *
 *  This function is to be called when a process is terminating its UMS session.
 *  Calling this function when a thread is still running may result in unexpected behaviour.
 */
void ums_destroy(void);

/** @brief Creates a UMS worker.
 *
 *  This function is used as a wrapper for pthread_create, to instantiate a UMS thread.
 *
 *  @param worker: A pointer to a struct ums_worker that will be initialized.
 *  @param start_routine: The routine that will be executed by the UMS thread.
 *  @param arg: The argument needed by the UMS thread routine.
 *  @return 0 if successful <0 if failed.
 */
int ums_worker_create (struct ums_worker *worker,
                       void (*start_routine) (void *),
                       void *arg);

/** @brief Inserts a UMS worker into a completion list.
 *  @param head: The head of the list.
 *  @param worker: The worker to be inserted as a node.
 *  @return 0 if successful <0 if failed.
 */
int ums_worker_list_insert(struct list_head *head, struct ums_worker *worker);

/** @brief Initializes the scheduler thread with its relative worker list.
 *
 *  This function creates a scheduler thread and assigns it to the first non used
 *  CPU for this process's UMS. If there are no more CPUs available to the process
 *  it wil return an error. It also assigns the given worker_list to that scheduler,
 *  performing additional operations to register both components to the UMS driver.
 *  This function must only be called by the main thread, any other usage, may cause
 *  unwanted behaviour.
 *
 *  @param scheduler_routine: The scheduling function that will be executed by the scheduler thread.
 *  @param worker_list: The worker list to be assigned to the scheduler.
 *  @return 0 if successful <0 if failed.
 */
int EnterUmsSchedulingMode(void (*scheduler_routine)(), struct list_head *worker_list);

/** @brief Executes the given worker in the scheduler's context.
 *  @param worker: The worker to be executed.
 *  @return 0 if successful <0 if failed.
 */
int ExecuteUmsThread (struct ums_worker *worker);

/// @brief Yields the current worker and restores the scheduler's context
int UmsThreadYield (void);

/** @brief Returns a list of valid workers that are available to be executed.
 *
 *  This function is a blocking function and will return a sublist of workers from
 *  the scheduler's completion list, that are available to be executed on this scheduler.
 *
 *  @param nworkers: The number of workers wanted in the queue. If the value is either equal to 0 or greater then the number available workers, the latter value will be used instead.
 *  @return struct list_head *: A pointer to a list head that will contain valid workers. The list has to be freed by the user.
 *  @return A pointer to a struct list_head representing the dequeued list or NULL if there are no more workers to be executed.
 */
struct list_head *DequeueUmsCompletionListItems (size_t nworkers);

#endif // !LIB_UMS_H
