// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport (macOS): Linux helpers the renderer uses for thread identity. Force-included into the GPU
// library on macOS only (CMakeLists.txt), like spartanport_win_compat.h on Windows.
#pragma once
#ifdef __APPLE__
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <mach-o/dyld.h>
#ifdef __cplusplus
#include "common/polyfill_thread.h"
extern "C" {
#endif
/* The low 32 bits of the system-wide thread id: the renderer compares ids, nothing else. */
static inline int gettid(void) {
    uint64_t id = 0;
    pthread_threadid_np(NULL, &id);
    return (int)id;
}
static inline void spartanport_macos_vulkan_driver(void) {
    if (getenv("VK_DRIVER_FILES") || getenv("VK_ICD_FILENAMES")) return;
    char executable[PATH_MAX], manifest[PATH_MAX];
    uint32_t length=sizeof(executable);
    if (_NSGetExecutablePath(executable,&length)) return;
    char *slash=strrchr(executable,'/');
    if (!slash) return;
    *slash=0;
    snprintf(manifest,sizeof(manifest),"%s/vulkan/kosmickrisp_mesa_icd.json",executable);
    if (access(manifest,R_OK)) snprintf(manifest,sizeof(manifest),"%s/kosmickrisp_mesa_icd.json",executable);
    if (!access(manifest,R_OK)) setenv("VK_DRIVER_FILES",manifest,0);
}
#ifdef __cplusplus
}
#endif
#endif
