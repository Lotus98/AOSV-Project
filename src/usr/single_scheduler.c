#include "../lib/ums.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define WORKERS 128

void fun ()
{
        printf("[%lu]Hello, yielding from thread[TID]: %d\n", time(NULL), gettid());
        UmsThreadYield();
        printf("[%lu]Hello, I'm executing again from thread[TID]: %d\n", time(NULL), gettid());
}

void sched_fun ()
{
        struct list_head *completion_list;
        ums_worker_node_t *node, *tmp;
        unsigned int __cpuid;

        puts("Scheduler executing now");
        completion_list = DequeueUmsCompletionListItems(500);
        while(completion_list != NULL) {
                list_for_each_entry_safe(node, tmp, completion_list, list) {
                        getcpu(&__cpuid, NULL);
                        fprintf(stderr, "Scheduler CPU[%d] Executing worker[TID]: %d\n", __cpuid, node->worker->thread.tid);
                        ExecuteUmsThread(node->worker);
                        list_del(&node->list);
                        free(node);
                }
                free(completion_list);
                completion_list = DequeueUmsCompletionListItems(500);
        }

        puts("Done with the scheduling");

        return;
}

int main (void)
{
        int ret = SUCCESS;
        LIST_HEAD(head);
        struct ums_worker *worker;

        if (ums_init() != SUCCESS) {
                fprintf(stderr, "You are fucked mate\n");
                exit(FAILURE);
        }
        printf("Hello from PID: %d\n", getpid());

        // Create completion_list
        puts("Creating workers list");
        for (size_t i = 0; i < WORKERS; i++) {
                worker = malloc(sizeof(*worker));
                if (!worker) {
                        perror("Failing to allocate worker");
                }
                if (ums_worker_create(worker, fun, NULL) != SUCCESS) {
                        perror("Failed to create worker");
                } else {
                        ums_worker_list_insert(&head, worker);
                }
        }

        // Starting scheduler
        puts("Starting scheduler");
        EnterUmsSchedulingMode(sched_fun, &head);

        // Terminate UMS session
        puts("Waiting for session to terminate");
        ums_destroy();
        puts("Session terminated correctly");

        return ret;
}
