/** @file utils.c
 *  @brief Contains the helper functions for the UMS library.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#include "utils.h"
#include "list.h"
#include "shared.h"
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

static void bind_to_cpu(unsigned int cpuid) {
        cpu_set_t *cpusetp;
        size_t size;

        cpusetp = CPU_ALLOC(ncpus);
        if (!cpusetp) {
                perror("Allocating cpu set");
                exit(EXIT_FAILURE);
        }
        size = CPU_ALLOC_SIZE(ncpus);
        CPU_ZERO_S(size, cpusetp);
        CPU_SET_S(cpuid, size, cpusetp);

        if (sched_setaffinity(0, size, cpusetp) != 0) {
                perror("Setting affinity for scheduler thread");
                exit(EXIT_FAILURE);
        }

        CPU_FREE(cpusetp);
}

int open_device ()
{
        int fd;

        fd = open(DRIVER_PATH, O_RDWR);
        if (fd < 0) {
                perror("Open DRIVER_PATH");
                return FAILURE;
        }
        return fd;
}

void *worker_wrap_routine (void *arg)
{
        struct ums_worker_arg *wrap_arg = (struct ums_worker_arg *)arg;
        struct ums_thread *thread = wrap_arg->ums_thread;

        // Get TID
        thread->tid = gettid();
        if (sem_post(wrap_arg->tid_sem) != 0) {
                perror("Incrementing semaphore");
                pthread_exit(NULL);
        }

        // Initialize worker and set it to IDLE state.
        ioctl(dev_fd, INIT_WORKER);

        // Execute worker function.
        wrap_arg->ums_routine(wrap_arg->arg);

        /* POST-ROUTINE PROCEDURE : */
        struct ums_worker *worker;
        unsigned int cpuid;

        getcpu(&cpuid, NULL);
        worker = ums_schedulers[cpuid]->current_worker;
        ums_schedulers[cpuid]->current_worker = NULL;
        // Set worker state as terminated
        pthread_mutex_lock(&worker->mutex);
        worker->state = WORKER_TERMINATED;
        pthread_mutex_unlock(&worker->mutex);

        // IOCTL call to restore scheduler and terminate worker.
        ioctl(dev_fd, TERMINATE_WORKER);

        // Cleanup
        free(wrap_arg);

        return NULL;
}

void *sched_wrap_routine (void *arg)
{
        long retval;
        struct ums_sched_arg *wrap_arg = (struct ums_sched_arg *)arg;

        // Get TID
        wrap_arg->ums_thread->tid = gettid();
        // Bind thread to CPU
        bind_to_cpu(wrap_arg->cpuid);
        // Register scheduler into LKM.
        retval = ioctl(dev_fd, REGISTER_SCHED, &wrap_arg->cpuid);
        if (retval != SUCCESS) {
                perror("Registering scheduler thread");
                pthread_exit(NULL);
        }

        // Signal on the semaphore so main thread can start registering workers.
        if (sem_post(wrap_arg->sem) != 0) {
                perror("Incrementing semaphore");
                pthread_exit(NULL);
        }
        // Execute scheduler
        wrap_arg->sched_routine();

        // Cleanup
        free(arg);

        return NULL;
}

int find_next_zero_bit(unsigned long *map, size_t size)
{
        int index = 0;
        for (index=0; (size_t)index<size; index++) {
                if (GET_BIT(map, index) == 0) break;
        }
        if ((size_t)index == size) {
                index = -1;
        }
        return index;
}

ums_worker_node_t *find_worker_tid (ums_list_head_t *head, pid_t tid)
{
        ums_worker_node_t *worker_node;

        pthread_rwlock_rdlock(&head->rwlock);
        list_for_each_entry(worker_node, &head->list, list) {
                if (worker_node->worker->thread.tid == tid) {
                        pthread_rwlock_unlock(&head->rwlock);
                        return worker_node;
                }
        }
        pthread_rwlock_unlock(&head->rwlock);

        return NULL;
}
