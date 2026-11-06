#!/usr/bin/env python3
"""Check telemetry PM behavior using the actual attribute and PM helpers."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

TREE = Path(os.environ["KERNEL_TREE"])


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError("Unterminated function: " + signature)


class TelemetryPmTests(unittest.TestCase):
    def test_reads_balance_references_without_postponing_autosuspend(self):
        driver = (TREE / "drivers/gpu/drm/msm/msm_gpu_devfreq.c").read_text()
        pm = (TREE / "include/linux/pm_runtime.h").read_text()
        attribute = function(driver, "static ssize_t gpu_busy_time_ns_show(")
        put = function(pm, "static inline int __pm_runtime_put_autosuspend(")
        mark_and_put = function(pm, "static inline int pm_runtime_put_autosuspend(")
        source = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
typedef unsigned long long u64;
#define RPM_GET_PUT 1
#define RPM_ASYNC 2
#define RPM_AUTO 4
struct device { int refs, puts, queued; u64 last_busy, now; };
struct device_attribute { int unused; };
struct msm_gpu_devfreq {
    int lock;
    bool monitor_enabled, suspended;
    u64 monitor_busy_ns, monitor_busy_cycles;
};
struct msm_gpu;
struct gpu_funcs { u64 (*gpu_busy)(struct msm_gpu *, unsigned long *); };
struct msm_gpu { struct msm_gpu_devfreq devfreq; struct gpu_funcs *funcs; };
static struct msm_gpu gpu;
static int get_result = 1, hardware_reads, monitor_updates;
static struct msm_gpu *dev_to_gpu(struct device *dev) { return &gpu; }
static void mutex_lock(int *lock) { assert(!*lock); *lock = 1; }
static void mutex_unlock(int *lock) { assert(*lock); *lock = 0; }
static int pm_runtime_get_if_active(struct device *dev) {
    if (get_result > 0) dev->refs++;
    return get_result;
}
static void pm_runtime_mark_last_busy(struct device *dev) { dev->last_busy = dev->now; }
static int __pm_runtime_suspend(struct device *dev, int flags) {
    assert(flags == (RPM_GET_PUT | RPM_ASYNC | RPM_AUTO));
    assert(dev->refs > 0);
    dev->puts++;
    if (--dev->refs == 0) dev->queued++;
    return 0;
}
static u64 gpu_busy(struct msm_gpu *g, unsigned long *rate) {
    assert(g->devfreq.lock); hardware_reads++; *rate = 1; return 17;
}
static void msm_devfreq_monitor_update(struct msm_gpu *g) {
    assert(g->devfreq.lock); monitor_updates++;
}
static int sysfs_emit(char *buf, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buf, 128, format, args);
    va_end(args);
    return result;
}
""" + put + "\n" + mark_and_put + "\n" + attribute + r"""
int main(void) {
    struct gpu_funcs funcs = { .gpu_busy = gpu_busy };
    struct device dev = { .last_busy = 42, .now = 100 };
    char buf[128];
    gpu.funcs = &funcs;
    gpu.devfreq.monitor_busy_ns = 12345;
    for (int i = 0; i < 100; i++) {
        dev.now++;
        assert(gpu_busy_time_ns_show(&dev, NULL, buf) == 6);
        assert(!strcmp(buf, "12345\n"));
        assert(dev.refs == 0 && dev.puts == i + 1 && dev.queued == i + 1);
        assert(dev.last_busy == 42);
    }
    assert(hardware_reads == 1 && monitor_updates == 99);
    for (int i = 0; i < 2; i++) {
        get_result = i ? -11 : 0;
        memset(&gpu.devfreq, 0, sizeof(gpu.devfreq));
        hardware_reads = monitor_updates = 0;
        int before = dev.puts;
        gpu_busy_time_ns_show(&dev, NULL, buf);
        gpu_busy_time_ns_show(&dev, NULL, buf);
        assert(hardware_reads == 0 && monitor_updates == 0);
        assert(dev.puts == before && dev.refs == 0 && dev.last_busy == 42);
    }
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "test.c").write_text(source)
            cmd = shlex.split(os.environ.get("CC", "cc")) + [
                "-std=gnu11", "-O1", "-g", "-no-pie",
                "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                str(path / "test.c"), "-o", str(path / "test"),
            ]
            build = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(path / "test")], capture_output=True,
                                 text=True, timeout=10)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
