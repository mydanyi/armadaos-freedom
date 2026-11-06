#!/usr/bin/env python3
"""Execute production Infinity code with minimal host shims.

KERNEL_TREE must point to a kernel with the Infinity overlay applied.
The tests cover cgroup yield, cpumask bounds, capacity-aware idle selection,
and debugfs reader/removal/replacement ordering under ASan/UBSan.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

TREE = Path(os.environ["KERNEL_TREE"])


def function(source, signature):
    match = re.search(re.escape(signature) + r"[^;{]*\{", source)
    if not match:
        raise AssertionError("Missing function definition: " + signature)
    start = match.start()
    brace = match.end() - 1
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError("Unterminated function: " + signature)


HEAD = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
typedef uint64_t u64;
"""


class InfinityReviewTests(unittest.TestCase):
    def run_c(self, source):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "test.c").write_text(HEAD + source)
            cmd = shlex.split(os.environ.get("CC", "cc")) + [
                "-std=gnu11", "-O1", "-g", "-no-pie", "-pthread",
                "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                str(path / "test.c"), "-o", str(path / "test"),
            ]
            build = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(path / "test")], capture_output=True,
                                 text=True, timeout=10)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_yield_keeps_cgroup_task_on_cpu_root(self):
        src = (TREE / "kernel/sched/fair.c").read_text()
        body = function(src, "static bool yield_to_task_fair(")
        self.run_c(r"""
struct list_head { struct list_head *next, *prev; };
static void init_list(struct list_head *p) { p->next = p->prev = p; }
static bool list_empty(struct list_head *p) { return p->next == p; }
static void list_add(struct list_head *p, struct list_head *head) {
    p->next = head->next; p->prev = head;
    head->next->prev = p; head->next = p;
}
static void list_move(struct list_head *p, struct list_head *head) {
    p->next->prev = p->prev; p->prev->next = p->next;
    list_add(p, head);
}
struct sched_entity { bool on_rq, sched_delayed; struct list_head inf_node; };
struct cfs_rq {
    struct sched_entity *curr;
    struct list_head inf_list;
    unsigned int inf_yield_to_moves;
};
struct rq { struct cfs_rq cfs; };
struct task_struct { struct sched_entity se; };
static struct rq root;
static struct cfs_rq group;
static struct cfs_rq *buddy_rq;
static int yields;
static struct cfs_rq *cfs_rq_of(struct sched_entity *se) { return &group; }
static struct rq *task_rq(struct task_struct *p) { return &root; }
static void set_next_buddy(struct cfs_rq *rq, struct sched_entity *se) { buddy_rq = rq; }
static void yield_task_fair(struct rq *rq) { yields++; }
""" + body + r"""
int main(void) {
    struct task_struct target = { .se.on_rq = true };
    struct list_head other;
    init_list(&root.cfs.inf_list); init_list(&group.inf_list);
    list_add(&target.se.inf_node, &root.cfs.inf_list);
    list_add(&other, &root.cfs.inf_list);
    assert(yield_to_task_fair(&root, &target));
    assert(root.cfs.inf_list.next == &target.se.inf_node);
    assert(list_empty(&group.inf_list));
    assert(root.cfs.inf_yield_to_moves == 1 && group.inf_yield_to_moves == 0);
    assert(buddy_rq == &root.cfs && yields == 1);
    target.se.on_rq = false;
    assert(!yield_to_task_fair(&root, &target));
    target.se.on_rq = true; target.se.sched_delayed = true;
    assert(!yield_to_task_fair(&root, &target));
    assert(yields == 1 && root.cfs.inf_yield_to_moves == 1);
    return 0;
}
""")

    def test_mask_allocation_boundary_and_zero_shift(self):
        src = (TREE / "kernel/sched/sched.h").read_text()
        body = function(src, "static __always_inline u64 poc_cpumask_to_u64(")
        self.run_c(r"""
#ifndef __always_inline
#define __always_inline inline __attribute__((always_inline))
#endif
#define BITS_TO_LONGS(n) (((n) + 63) / 64)
#define static_branch_likely(p) (*(p))
#define cpumask_bits(p) ((p)->bits)
struct cpumask { unsigned long bits[1]; };
struct sched_domain_shared { int poc_cpu_base, poc_affinity_shift; };
static unsigned int nr_cpumask_bits;
static bool sched_poc_aligned;
""" + body + r"""
int main(void) {
    struct sched_domain_shared sd = { .poc_cpu_base = 8, .poc_affinity_shift = 8 };
    nr_cpumask_bits = 16;
    struct cpumask *mask = calloc(1, sizeof(unsigned long));
    assert(mask); mask->bits[0] = 0xff00;
    assert(poc_cpumask_to_u64(mask, &sd) == 0xff);
    free(mask);
    nr_cpumask_bits = 128;
    mask = calloc(2, sizeof(unsigned long));
    assert(mask); mask->bits[0] = 0xff00; mask->bits[1] = 1;
    assert(poc_cpumask_to_u64(mask, &sd) == UINT64_C(0x01000000000000ff));
    sd.poc_cpu_base = 64; sd.poc_affinity_shift = 0; mask->bits[1] = 0x1234;
    assert(poc_cpumask_to_u64(mask, &sd) == 0x1234);
    sched_poc_aligned = true;
    assert(poc_cpumask_to_u64(mask, &sd) == 0x1234);
    free(mask);
    return 0;
}
""")

    def test_asymmetric_recent_cpu_reaches_capacity_path(self):
        src = (TREE / "kernel/sched/fair.c").read_text()
        body = function(src, "static int select_idle_sibling(")
        marker = body.index("On asymmetric system, update task utilization")
        end = body.rindex("/*", 0, marker)
        prefix = body[:end] + "return -4242;\n}\n"
        self.run_c(r"""
#define CONFIG_SCHED_POC_SELECTOR 1
#define static_branch_likely(p) (*(p))
#define static_branch_unlikely(p) (*(p))
static bool poc_selector_active, sched_poc_early_select, asymmetric, allowed = true;
static unsigned int nr_cpumask_bits = 16;
struct sched_domain;
struct task_struct { int recent_used_cpu; void *cpus_ptr; };
static bool sched_asym_cpucap_active(void) { return asymmetric; }
static bool cpus_share_cache(int a, int b) { return true; }
static bool choose_idle_cpu(int cpu, struct task_struct *p) { return true; }
static bool cpumask_test_cpu(int cpu, void *mask) { return allowed; }
static bool is_idle_core(int cpu) { return true; }
""" + prefix + r"""
int main(void) {
    struct task_struct task = { .recent_used_cpu = 3 };
    asymmetric = true;
    assert(select_idle_sibling(&task, 1, 2, 0) == -4242);
    task.recent_used_cpu = 3; asymmetric = false;
    assert(select_idle_sibling(&task, 1, 2, 0) == 3);
    task.recent_used_cpu = 3; allowed = false;
    assert(select_idle_sibling(&task, 1, 2, 0) == -4242);
    return 0;
}
""")

    def test_debugfs_removal_allows_readers_and_serializes_replacement(self):
        src = (TREE / "drivers/gpu/drm/scheduler/sched_rq.c").read_text()
        start = src.index("static struct drm_gpu_scheduler *inf_tracked[")
        end = src.index(";", src.index("static struct dentry *inf_dbg_dentry", start)) + 1
        declarations = src[start:end]
        track = function(src, "void drm_sched_inf_track(")
        untrack = function(src, "void drm_sched_inf_untrack(")
        self.run_c(r"""
#define INF_MAX_TRACKED 64
#define DEFINE_MUTEX(name) pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
#define mutex_lock(p) pthread_mutex_lock(p)
#define mutex_unlock(p) pthread_mutex_unlock(p)
#define IS_ERR(p) false
struct drm_gpu_scheduler { int id; };
struct dentry { int id; };
static int inf_fops;
""" + declarations + r"""
static pthread_mutex_t sync_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sync_cond = PTHREAD_COND_INITIALIZER;
static bool removal_entered, removal_hold, removal_pending, replacement_attempted;
static int creations;
static struct dentry *debugfs_create_file(const char *name, int mode,
                                         void *parent, void *data, void *ops) {
    pthread_mutex_lock(&sync_lock);
    assert(!removal_pending);
    struct dentry *d = malloc(sizeof(*d));
    assert(d); d->id = ++creations;
    pthread_mutex_unlock(&sync_lock);
    return d;
}
static void debugfs_remove(struct dentry *d) {
    /* An active inf_show reader must be able to acquire this lock. */
    assert(pthread_mutex_trylock(&inf_tracked_lock) == 0);
    pthread_mutex_unlock(&inf_tracked_lock);
    pthread_mutex_lock(&sync_lock);
    removal_entered = removal_pending = true;
    pthread_cond_broadcast(&sync_cond);
    while (removal_hold) pthread_cond_wait(&sync_cond, &sync_lock);
    free(d);
    removal_pending = false;
    pthread_mutex_unlock(&sync_lock);
}
""" + track + "\n" + untrack + r"""
static void *remove_thread(void *arg) { drm_sched_inf_untrack(arg); return NULL; }
static void *replace_thread(void *arg) {
    pthread_mutex_lock(&sync_lock);
    replacement_attempted = true;
    pthread_cond_broadcast(&sync_cond);
    pthread_mutex_unlock(&sync_lock);
    drm_sched_inf_track(arg);
    return NULL;
}
static void await_flag(bool *flag) {
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until); until.tv_sec += 3;
    pthread_mutex_lock(&sync_lock);
    while (!*flag) assert(pthread_cond_timedwait(&sync_cond, &sync_lock, &until) == 0);
    pthread_mutex_unlock(&sync_lock);
}
int main(void) {
    struct drm_gpu_scheduler a = {1}, b = {2};
    pthread_t remover, replacement;
    drm_sched_inf_track(&a);
    assert(creations == 1);
    removal_hold = true;
    assert(!pthread_create(&remover, NULL, remove_thread, &a));
    await_flag(&removal_entered);
    assert(pthread_mutex_trylock(&inf_tracked_lock) == 0);
    pthread_mutex_unlock(&inf_tracked_lock);
    assert(!pthread_create(&replacement, NULL, replace_thread, &b));
    await_flag(&replacement_attempted);
    struct timespec pause = { .tv_nsec = 100000000 };
    nanosleep(&pause, NULL);
    pthread_mutex_lock(&sync_lock);
    assert(creations == 1);
    removal_hold = false;
    pthread_cond_broadcast(&sync_cond);
    pthread_mutex_unlock(&sync_lock);
    assert(!pthread_join(remover, NULL));
    assert(!pthread_join(replacement, NULL));
    assert(creations == 2 && inf_tracked_nr == 1 && inf_tracked[0] == &b);
    drm_sched_inf_untrack(&b);
    assert(inf_tracked_nr == 0 && inf_dbg_dentry == NULL);
    return 0;
}
""")


if __name__ == "__main__":
    unittest.main()
