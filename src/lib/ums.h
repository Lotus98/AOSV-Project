/** @file ums.h
 *  @brief Header file containing the main components needed for the UMS library.
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
#include "utils.h"
#include <fcntl.h>


// Prototypes
/** @brief Initializes UMS data for a the current process.
 *
 *  This function is to be called before starting a UMS session.
 *
 *  @return 0 if successful.
 *  @return <0 if failed.
 */
int ums_init(void);

/** @brief Cleans up all UMS data created by a process.
 *
 *  This function is to be called when a process that has been using UMS is exiting, or
 *  when all of his UMS threads are completed. Calling this function when a thread is still
 *  running may result in undesired behaviour such as a kernel panic.
 */
void ums_destroy(void);

/** @brief Wrapper to pthread_create.
 *
 *  This function is used as a wrapper for pthread_create, to instantiate a ums worker.
 *
 *  @param worker: A pointer to a struct ums_worker that will be initialized.
 *  @param start_routine: The routine that will be executed by the UMS thread.
 *  @param arg: The argument needed by the UMS thread routine.
 *
 *  @return 0 if worker creation is successful.
 *  @return <0 if worker creation failed.
 */
int ums_worker_create (struct ums_worker *worker,
                       void (*start_routine) (void *),
                       void *arg);

/** @brief Inserts a ums_worker into a worker list.
 *
 *  @param head: The head of the list.
 *  @param worker: The worker to be inserted as a node.
 *  @return SUCCESS
 *  @return FAILURE
 */
int ums_worker_list_insert(struct list_head *head, struct ums_worker *worker);

/** @brief Initializes the scheduler thread with its relative worker list.
 *
 *  This function creates a scheduler thread and assigns it to the first non used
 *  CPU for this process's UMS. It also assigns the given worker_list to that scheduler.
 *  It then registers the thread and its worker list to the LKM.
 *
 *  @param scheduler_routine: The scheduler component that will be executed by the scheduler thread.
 *  @param worker_list: The worker list to be assigned to the scheduler.
 */
int EnterUmsSchedulingMode(void (*scheduler_routine)(), struct list_head *worker_list);

/** @brief Executes the given worker in the scheduler's context.
 *  @param worker: The worker to be executed.
 *  @return SUCCESS: If the worker has been correctly executed.
 *  @return <0: If the worker could not be executed.
 */
int ExecuteUmsThread (struct ums_worker *worker);

/// @brief Yields the current worker and restores the scheduler's context
int UmsThreadYield (void);

/** @brief Returns a list of valid ums_worker_node_t workers that are available to be executed.
 *
 *  This function is a blocking function and will return a sublist of workers,
 *  from the ums scheduler's list, that are available to be executed on this scheduler.
 *  A worker in the list might become unavailable after the call, if it is shared
 *  with other completion lists, on different schedulers. To get a new valid list
 *  a new call to this function must be performed.
 *
 *  @return struct list_head *: A pointer to a list head that will contain valid workers.
 *  @return NULL: If all workers on the scheduler's completion list are terminated.
 */
struct list_head *DequeueUmsCompletionListItems (void);

#endif // !LIB_UMS_H
