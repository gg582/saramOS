#ifndef NBODY_BENCH_H
#define NBODY_BENCH_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef NBODY_MAX_BODIES
#define NBODY_MAX_BODIES 256U
#endif

#ifndef NBODY_MAX_TASKS
#define NBODY_MAX_TASKS 8U
#endif

#ifndef NBODY_MAX_STEPS
#define NBODY_MAX_STEPS 2048U
#endif

#ifndef NBODY_FLOPS_PER_PAIR
#define NBODY_FLOPS_PER_PAIR 20.0
#endif

#ifndef NBODY_L1_CACHE_BYTES
#define NBODY_L1_CACHE_BYTES (32U * 1024U)
#endif

typedef struct {
    double mass;
    double pos[3];
    double vel[3];
} Body;

typedef struct {
    uint32_t task_id;
    uint32_t start_body;
    uint32_t end_body;
} NBodyTaskConfig;

typedef struct {
    uint32_t body_count;
    uint32_t task_count;
    uint32_t steps;
    double delta_t;
    double softening;
    double grav_const;
    double cpu_hz;
} NBodySimConfig;

typedef struct {
    double total_exec_time_s;
    uint64_t total_exec_cycles;
    double scheduler_overhead_s;
    uint64_t scheduler_cycles;
    uint32_t voluntary_context_switches;
    uint32_t involuntary_context_switches;
    double avg_step_time_ns;
    double step_jitter_ns2;
    double floating_point_throughput_gflops;
    double ready_to_run_latency_ns;
    uint32_t stack_peak_bytes;
    double fpu_context_cost_ns;
    double interrupt_latency_ns;
    double critical_section_ns;
    double memory_bandwidth_gbps;
    double cache_miss_estimate;
    double tick_drift_ns;
    double sync_overhead_ns;
    double energy_efficiency_ratio;
} BenchMetrics;

typedef struct {
    void (*yield)(void);
} NBodyOSHooks;

void nbody_bench_init(const NBodySimConfig *cfg,
                      const Body *initial_bodies,
                      uint32_t body_count);
void nbody_bench_bind_hooks(const NBodyOSHooks *hooks);
const NBodyTaskConfig *nbody_bench_get_task_table(uint32_t *count);
void nbody_bench_start(void);
void nbody_bench_wait_for_completion(void);
void nbody_bench_report(void);
void nbody_bench_get_metrics(BenchMetrics *out);

void nbody_task_entry(void *arg);

void nbody_mark_task_ready(uint32_t task_id, uint64_t ready_cycle);
void nbody_record_stack_peak(uint32_t task_id, uint32_t bytes);
void nbody_record_context_switch(bool involuntary);
void nbody_record_scheduler_window(uint64_t start_cycle, uint64_t end_cycle);
void nbody_record_fpu_context_cost(uint64_t cycles);
void nbody_record_interrupt_latency(uint64_t cycles);
void nbody_record_critical_section(uint64_t cycles);
void nbody_record_tick_drift(int64_t drift_cycles);
void nbody_record_cache_miss_estimate(double miss_ratio);
void nbody_record_energy_window(uint64_t active_cycles, uint64_t total_cycles);

#endif /* NBODY_BENCH_H */
