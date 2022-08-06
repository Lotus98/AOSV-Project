/** @file utils.h
 *  @brief Headers for helper functions and data structures of LKM.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef DEV_UTILS_H
#define DEV_UTILS_H

#include <linux/kprobes.h>

/** @brief Register process in UMS mode.
 *  @param pid: The pid of the process that holds resources (TGID).
 *  @return int: FAILURE or SUCCESS.
 */
int register_ums_process (pid_t pid);

#endif // !DEV_UTILS_H
