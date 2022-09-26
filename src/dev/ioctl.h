/**
 *  @file ioctl.h
 *  @brief Header file for the IOCTL API.
 *
 *  This header file contains the definitions of the command exposed in the IOCTL API for the UMS module.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef DEV_IOCTL_H
#define DEV_IOCTL_H

/** @brief Command to initialize a worker within the module.
 *
 *  This command registers the worker thread to the list of the process in UMS mode
 *  and modifies the state of the thread to TASK_IDLE to avoid the kernel scheduler
 *  routine to execute the thread.
 */
#define INIT_WORKER _IO(0x1337, 'a')
/** @brief Command to register a process to be in UMS mode.
 */
#define REGISTER_PROC _IO(0x1337, 'b')
/** @brief Command to unregister a process that is in UMS mode.
 */
#define TERMINATE_PROC _IO(0x1337, 'c')
/** @brief Command to register a scheduler thread.
 *  @param unsigned_int: The ID of the CPU on top of which the thread will run.
 */
#define REGISTER_SCHED _IOW(0x1337, 'd', unsigned int)
/** @brief Command to register a worker to a precise scheduler.
 *  @param ums_usr_worker: A tuple formed by the TID of the worker and the CPUID of the scheduler.
 */
#define REGISTER_WORKER _IOW(0x1337, 'e', struct ums_usr_worker)
/** @brief Command to execute a ums worker.
 *  @param pid_t: The PID of the worker thread to be executed.
 */
#define EXECUTE_THREAD _IOW(0x1337, 'f', pid_t)
/** @brief Command to yield the calling thread.
 */
#define THREAD_YIELD _IO(0x1337, 'g')
/** @brief Command to get the list of available workers.
 *  @param int: This parameter is the pointer to an array, which holds as the first value
 *      the number of workers to be dequeued.
 */
#define DEQUEUE_LIST _IOWR(0x1337, 'h', int)
/** @brief Command to terminate a running worker and restore the scheduler hosting it.
 */
#define TERMINATE_WORKER _IO(0x1337, 'i')


/** IOCTL implementation to handle interaction with the LKM.
 *
 *  @param file: The file associated with the device fd.
 *  @param cmd: The command to be executed.
 *  @param arg: A potential argument needed to execute the wanted command.
 *  @return SUCCESS or an error (< 0).
 */
long ums_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

#endif // !DEV_IOCTL_H
