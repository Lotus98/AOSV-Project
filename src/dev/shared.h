/**
 *  @file shared.h
 *  @brief Header file containing shared data between most LKM files.
 *
 *  This file contains the informations and data needed by all the files of the LKM.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef DEV_SHARED_H
#define DEV_SHARED_H

#include <asm-generic/errno.h>

#define DEVICE_NAME "umsdev"    ///< The device name in /dev
#define LOG_MSG "umsdev: "      ///< Log message to identify driver's messages in dmesg

#endif // !DEV_SHARED_H
