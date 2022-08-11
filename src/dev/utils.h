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

/** @brief Finds a UMS process given its PID.
 *  @param pid: The pid of the process that holds resources (TGID).
 *  @return struct ums_proc: The wanted ums_proc struct.
 *  @return NULL: If the process is not in the UMS processes hashtable.
 */
struct ums_proc *find_ums_proc (pid_t pid);

/** @brief Initializes a worker node and saves it in the given process hashtable.
 *
 *  This function allocates the necessary data structures to represent a UMS worker
 *  and inserts it in the hashtable of the given process.
 *
 *  @param tid: The TID of the worker thread to be registered.
 *  @return long: SUCCESS or an error (<0).
 */
long init_worker_node (void);

/// Routine used to free worker when releasing the last kref.
void worker_release (struct kref *refcnt);

#endif // !DEV_UTILS_H
