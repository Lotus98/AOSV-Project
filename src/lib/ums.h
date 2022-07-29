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
#include "shared.h"
#include "utils.h"


// Prototypes
/** @brief Initializes the driver interaction from Userspace.
 *
 *  @return 0 if successful.
 *  @return <0 if failed.
 */
int ums_init(void);

/** @brief Cleans up all UMS data
 */
void ums_destroy(void);

/** @brief Wrapper function to pthread_create.
 *
 *  This function is used as a wrapper to pthread_create, to instantiate a ums thread.
 *
 *  @param thread: A pointer to a struct ums_thread that will be instantiated.
 *  @param start_routine: The routine that will be executed by the UMS thread.
 *  @param arg: The argument needed by the UMS thread routine.
 *
 *  @return 0 if thread creation is successful.
 *  @return <0 if thread creation failed.
 */
int ums_thread_create (struct ums_thread *thread,
                       void *(*start_routine) (void *),
                       void *arg);


#endif // !LIB_UMS_H
