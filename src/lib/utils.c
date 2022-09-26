/** @file utils.c
 *  @brief Contains the helper functions for the UMS library.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#include "ums.h"
#include "utils.h"
#include <stdlib.h>
#include <fcntl.h>

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
        if (ioctl(dev_fd, INIT_WORKER) != SUCCESS) {
                perror("Initializing worker");
                pthread_exit(NULL);
        }

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
        PRINTDBG("Worker(TID)[%d]: Calling IOCTL TERMINATE_WORKER\n", wrap_arg->ums_thread->tid);
        ioctl(dev_fd, TERMINATE_WORKER);
        PRINTDBG("Worker(TID)[%d]: Terminated its execution\n", wrap_arg->ums_thread->tid);

        // Cleanup
        free(wrap_arg);

        return NULL;
}

void *sched_wrap_routine (void *arg)
{
        long retval;
        struct ums_sched_arg *wrap_arg = (struct ums_sched_arg *)arg;
        ums_worker_node_t *worker_node;

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

        // Register worker list.
        list_for_each_entry(worker_node, wrap_arg->list, list) {
                struct ums_usr_worker *usr_worker;

                usr_worker = malloc(sizeof(*usr_worker));
                if (!usr_worker) {
                        perror("Allocating usr_worker to register worker in LKM");
                }
                usr_worker->tid = worker_node->worker->thread.tid;
                usr_worker->cpuid = (unsigned int)wrap_arg->cpuid;
                ioctl(dev_fd, REGISTER_WORKER, usr_worker);
                free(usr_worker);
        }

        // Signal on the semaphore so main thread can continue.
        if (sem_post(wrap_arg->sem) != 0) {
                perror("Incrementing semaphore");
                pthread_exit(NULL);
        }


        // Execute scheduler
        wrap_arg->sched_routine();

        /* POST-ROUTINE PROCEDURE : */
        struct ums_sched *scheduler = ums_schedulers[wrap_arg->cpuid];
        ums_worker_node_t *tmp;
        struct ums_worker *worker;

        // Cleanup completion list
        unsigned int __cpuid;
        getcpu(&__cpuid, NULL);
        PRINTDBG("CPU[%d] Cleaning up scheduler's completion list\n", __cpuid);
        list_for_each_entry_safe(worker_node, tmp, scheduler->worker_list, list) {
                worker = worker_node->worker;
                PRINTDBG("CPU[%d] Taking mutex lock for worker(TID)[%d]\n", __cpuid, worker->thread.tid);
                pthread_mutex_lock(&worker->mutex);
                PRINTDBG("CPU[%d] Decrementing refcnt to: %lu\n", __cpuid, worker->refcnt - 1);
                worker->refcnt--;
                if (worker->refcnt == 0) { // This should not happen
                        PRINTDBG("If this is happening you broke the global list");
                        pthread_mutex_unlock(&worker->mutex);
                        pthread_mutex_destroy(&worker->mutex);
                        // Join worker's pthread
                        pthread_join(worker->thread.pthread, NULL);
                        free(worker);
                } else {
                        PRINTDBG("CPU[%d] Releasing mutex lock\n", __cpuid);
                        pthread_mutex_unlock(&worker->mutex);
                }
                PRINTDBG("CPU[%d] Deleting node from list\n", __cpuid);
                list_del(&worker_node->list);
                PRINTDBG("CPU[%d] Freeing node\n", __cpuid);
                free(worker_node);
        }

        // Cleanup
        free(wrap_arg);

        PRINTDBG("CPU[%d] Scheduler is terminated\n", __cpuid);
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

struct ums_worker *find_worker_tid (struct list_head *head, pid_t tid)
{
        ums_worker_node_t *worker_node;

        list_for_each_entry(worker_node, head, list) {
                if (worker_node->worker->thread.tid == tid) {
                        return worker_node->worker;
                }
        }

        return NULL;
}

struct list_head *dup_worker_list (struct list_head *head) {
        struct list_head *dup_head;
        ums_worker_node_t *node;

        // Initialize head of dup list.
        dup_head = malloc(sizeof(*dup_head));
        INIT_LIST_HEAD(dup_head);

        list_for_each_entry(node, head, list) {
                ums_worker_list_insert(dup_head, node->worker);
        }

        return dup_head;
}
