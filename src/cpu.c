// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2017 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "cpu.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef __ANDROID__
#include <sys/syscall.h>
#include <unistd.h>
#include <stdint.h>
#endif

#ifdef __ANDROID__

// extract the ELF HW capabilities bitmap from /proc/self/auxv
static unsigned int get_elf_hwcap_from_proc_self_auxv()
{
    FILE* fp = fopen("/proc/self/auxv", "rb");
    if (!fp)
    {
        return 0;
    }

#define AT_HWCAP 16
#define AT_HWCAP2 26
#if __aarch64__

    struct { uint64_t tag; uint64_t value; } entry;
#else
    struct { unsigned int tag; unsigned int value; } entry;

#endif

    unsigned int result = 0;
    while (!feof(fp))
    {
        int nread = fread((char*)&entry, sizeof(entry), 1, fp);
        if (nread != 1)
            break;

        if (entry.tag == 0 && entry.value == 0)
            break;

        if (entry.tag == AT_HWCAP)
        {
            result = entry.value;
            break;
        }
    }

    fclose(fp);

    return result;
}

static unsigned int g_hwcaps = get_elf_hwcap_from_proc_self_auxv();

#if __aarch64__
// from arch/arm64/include/uapi/asm/hwcap.h
#define HWCAP_ASIMD     (1 << 1)
#define HWCAP_ASIMDHP   (1 << 10)
#else
// from arch/arm/include/uapi/asm/hwcap.h
#define HWCAP_NEON      (1 << 12)
#define HWCAP_VFPv4     (1 << 16)
#endif

#endif // __ANDROID__

int cpu_support_arm_neon()
{
#ifdef __ANDROID__
#if __aarch64__
    return g_hwcaps & HWCAP_ASIMD;
#else
    return g_hwcaps & HWCAP_NEON;
#endif
#else
    return 0;
#endif
}

int cpu_support_arm_vfpv4()
{
#ifdef __ANDROID__
#if __aarch64__
    // neon always enable fma and fp16
    return g_hwcaps & HWCAP_ASIMD;
#else
    return g_hwcaps & HWCAP_VFPv4;
#endif
#else
    return 0;
#endif
}

int cpu_support_arm_asimdhp()
{
#ifdef __ANDROID__
#if __aarch64__
    return g_hwcaps & HWCAP_ASIMDHP;
#else
    return 0;
#endif
#else
    return 0;
#endif
}

static int get_cpucount()
{
    int count = 0;
#ifdef __ANDROID__
    // get cpu count from /proc/cpuinfo
    FILE* fp = fopen("/proc/cpuinfo", "rb");
    if (!fp)
        return 1;

    char line[1024];
    while (!feof(fp))
    {
        char* s = fgets(line, 1024, fp);
        if (!s)
            break;

        if (memcmp(line, "processor", 9) == 0)
        {
            count++;
        }
    }

    fclose(fp);
#else
#ifdef _OPENMP
    count = omp_get_max_threads();
#else
    count = 1;
#endif // _OPENMP
#endif

    if (count < 1)
        count = 1;

    if (count > (int)sizeof(size_t) * 8)
    {
        fprintf(stderr, "more than %d cpu detected, thread affinity may not work properly :(\n", (int)sizeof(size_t) * 8);
    }

    return count;
}

static int g_cpucount = -1;

inline int get_cpu_count()
{
    // retrieve gpu count if not initialized
    if (g_cpucount == -1)
    {
        g_cpucount = get_cpucount();
    }
    return g_cpucount;
}

#ifdef __ANDROID__
static int get_max_freq_khz(int cpuid)
{
    // first try, for all possible cpu
    char path[256];
    sprintf(path, "/sys/devices/system/cpu/cpufreq/stats/cpu%d/time_in_state", cpuid);

    FILE* fp = fopen(path, "rb");

    if (!fp)
    {
        // second try, for online cpu
        sprintf(path, "/sys/devices/system/cpu/cpu%d/cpufreq/stats/time_in_state", cpuid);
        fp = fopen(path, "rb");

        if (fp)
        {
            int max_freq_khz = 0;
            while (!feof(fp))
            {
                int freq_khz = 0;
                int nscan = fscanf(fp, "%d %*d", &freq_khz);
                if (nscan != 1)
                    break;

                if (freq_khz > max_freq_khz)
                    max_freq_khz = freq_khz;
            }

            fclose(fp);

            if (max_freq_khz != 0)
                return max_freq_khz;

            fp = NULL;
        }

        if (!fp)
        {
            // third try, for online cpu
            sprintf(path, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", cpuid);
            fp = fopen(path, "rb");

            if (!fp)
                return -1;

            int max_freq_khz = -1;
            fscanf(fp, "%d", &max_freq_khz);

            fclose(fp);

            return max_freq_khz;
        }
    }

    int max_freq_khz = 0;
    while (!feof(fp))
    {
        int freq_khz = 0;
        int nscan = fscanf(fp, "%d %*d", &freq_khz);
        if (nscan != 1)
            break;

        if (freq_khz > max_freq_khz)
            max_freq_khz = freq_khz;
    }

    fclose(fp);

    return max_freq_khz;
}

static int set_sched_affinity(size_t thread_affinity_mask)
{
    // cpu_set_t definition
    // ref http://stackoverflow.com/questions/16319725/android-set-thread-affinity
#define CPU_SETSIZE 1024
#define __NCPUBITS  (8 * sizeof (unsigned long))
typedef struct
{
    unsigned long __bits[CPU_SETSIZE / __NCPUBITS];
} cpu_set_t;

#define CPU_SET(cpu, cpusetp) \
    ((cpusetp)->__bits[(cpu)/__NCPUBITS] |= (1UL << ((cpu) % __NCPUBITS)))

#define CPU_ZERO(cpusetp) \
    memset((cpusetp), 0, sizeof(cpu_set_t))

    // set affinity for thread
#ifdef __GLIBC__
    pid_t pid = syscall(SYS_gettid);
#else
#ifdef PI3
    pid_t pid = getpid();
#else
    pid_t pid = gettid();
#endif
#endif
    cpu_set_t mask;
    CPU_ZERO(&mask);
    for (int i=0; i<(int)sizeof(size_t) * 8; i++)
    {
        if (thread_affinity_mask & (1 << i))
            CPU_SET(i, &mask);
    }

    int syscallret = syscall(__NR_sched_setaffinity, pid, sizeof(mask), &mask);
    if (syscallret)
    {
        fprintf(stderr, "syscall error %d\n", syscallret);
        return -1;
    }

    return 0;
}
#endif // __ANDROID__

static int g_powersave = 0;

int get_cpu_powersave()
{
    return g_powersave;
}

int set_cpu_powersave(int powersave)
{
    if (powersave < 0 || powersave > 2)
    {
        fprintf(stderr, "powersave %d not supported\n", powersave);
        return -1;
    }

    size_t thread_affinity_mask = get_cpu_thread_affinity_mask(powersave);

    int ret = set_cpu_thread_affinity(thread_affinity_mask);
    if (ret != 0)
        return ret;

    g_powersave = powersave;

    return 0;
}

static size_t g_thread_affinity_mask_all = 0;
static size_t g_thread_affinity_mask_little = 0;
static size_t g_thread_affinity_mask_big = 0;

static int setup_thread_affinity_masks()
{
    g_thread_affinity_mask_all = (1 << get_cpu_count()) - 1;

#ifdef __ANDROID__
    int max_freq_khz_min = INT_MAX;
    int max_freq_khz_max = 0;
    vector_def(int) cpu_max_freq_khz;
    vector_init(cpu_max_freq_khz);
    vector_resize(cpu_max_freq_khz, get_cpu_count());
    for (int i=0; i<get_cpu_count(); i++)
    {
        int max_freq_khz = get_max_freq_khz(i);

//         fprintf(stderr, "%d max freq = %d khz\n", i, max_freq_khz);

        cpu_max_freq_khz[i] = max_freq_khz;

        if (max_freq_khz > max_freq_khz_max)
            max_freq_khz_max = max_freq_khz;
        if (max_freq_khz < max_freq_khz_min)
            max_freq_khz_min = max_freq_khz;
    }

    int max_freq_khz_medium = (max_freq_khz_min + max_freq_khz_max) / 2;
    if (max_freq_khz_medium == max_freq_khz_max)
    {
        g_thread_affinity_mask_little = 0;
        g_thread_affinity_mask_big = g_thread_affinity_mask_all;
        return 0;
    }

    for (int i=0; i<get_cpu_count(); i++)
    {
        if (cpu_max_freq_khz[i] < max_freq_khz_medium)
            g_thread_affinity_mask_little |= (1 << i);
        else
            g_thread_affinity_mask_big |= (1 << i);
    }
    vector_destroy(cpu_max_freq_khz);
#else
    // TODO implement me for other platforms
    g_thread_affinity_mask_little = 0;
    g_thread_affinity_mask_big = g_thread_affinity_mask_all;
#endif

    return 0;
}

size_t get_cpu_thread_affinity_mask(int powersave)
{
    if (g_thread_affinity_mask_all == 0)
    {
        setup_thread_affinity_masks();
    }

    if (g_thread_affinity_mask_little == 0)
    {
        // SMP cpu powersave not supported
        // fallback to all cores anyway
        return g_thread_affinity_mask_all;
    }

    if (powersave == 0)
        return g_thread_affinity_mask_all;

    if (powersave == 1)
        return g_thread_affinity_mask_little;

    if (powersave == 2)
        return g_thread_affinity_mask_big;

    fprintf(stderr, "powersave %d not supported\n", powersave);

    // fallback to all cores anyway
    return g_thread_affinity_mask_all;
}

int set_cpu_thread_affinity(size_t thread_affinity_mask)
{
#ifdef __ANDROID__
    int num_threads = 0;
    for (int i=0; i<(int)sizeof(size_t) * 8; i++)
    {
        if (thread_affinity_mask & (1 << i))
            num_threads++;
    }

#ifdef _OPENMP
    // set affinity for each thread
    set_omp_num_threads(num_threads);
    vector_def(int) ssarets;
    vector_init(ssarets);
    vector_resize(ssarets, num_threads);
    #pragma omp parallel for num_threads(num_threads)
    for (int i=0; i<num_threads; i++)
    {
        vector_get(ssarets, i) = set_sched_affinity(thread_affinity_mask);
    }
    for (int i=0; i<num_threads; i++)
    {
        if (vector_get(ssarets, i) != 0)
            return -1;
    }
    vector_destroy(ssarets);
#else
    int ssaret = set_sched_affinity(thread_affinity_mask);
    if (ssaret != 0)
        return -1;
#endif

    return 0;
#else
    // TODO
    (void)thread_affinity_mask;
    return -1;
#endif
}

int get_omp_num_threads()
{
#ifdef _OPENMP
    return omp_get_num_threads();
#else
    return 1;
#endif
}

void set_omp_num_threads(int num_threads)
{
#ifdef _OPENMP
    omp_set_num_threads(num_threads);
#else
    (void)num_threads;
#endif
}

int get_omp_dynamic()
{
#ifdef _OPENMP
    return omp_get_dynamic();
#else
    return 0;
#endif
}

void set_omp_dynamic(int dynamic)
{
#ifdef _OPENMP
    omp_set_dynamic(dynamic);
#else
    (void)dynamic;
#endif
}

int get_omp_thread_num()
{
#if _OPENMP
    return omp_get_thread_num();
#else
    return 0;
#endif
}
