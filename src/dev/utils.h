/** @file utils.h
 *  @brief Headers for helper functions and data structures of LKM.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef DEV_UTILS_H
#define DEV_UTILS_H

#include <linux/types.h>
#include <linux/kref.h>

/** @brief Registers the given process in UMS mode.
 *  @param pid: The pid of the process that holds resources (TGID).
 *  @return int: FAILURE or SUCCESS.
 */
int register_ums_process (pid_t pid);

/** @brief Unregisters the given process in UMS mode.
 *  @param pid: The pid of the process that holds resources (TGID).
 *  @return int: FAILURE or SUCCESS.
 */
int unregister_ums_process (pid_t pid);

void worker_release (struct kref *refcnt);

#endif // !DEV_UTILS_H
