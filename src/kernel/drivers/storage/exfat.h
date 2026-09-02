/**
 * CardboardOS - exFAT Filesystem Driver Header
 */

#ifndef EXFAT_H
#define EXFAT_H

#include <stdint.h>
#include <stdbool.h>
#include "../../fs/vfs.h"

extern struct vfs_ops exfat_ops;

bool init_exfat(void);

#endif // EXFAT_H