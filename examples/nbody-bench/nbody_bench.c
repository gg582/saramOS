#include "nbody_bench.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <stdarg.h>

#if defined(NBODY_DEPLOY_RUN) && !defined(NATIVE_RUN)
#include <hal/esp32_wroom32.h>
static esp32_hal_uart_t g_bench_uart;
static void bench_io_init(void) {
    static bool initialized = false;
    if (!initialized) {
        esp32_hal_uart_init(&g_bench_uart, 115200U);
        initialized = true;
    }
}
static void bench_printf(const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (written <= 0) {
        return;
    }
    size_t len = (written > (int)(sizeof(buffer))) ? (sizeof(buffer)) : (size_t)written;
    esp32_hal_uart_write(&g_bench_uart, buffer, len);
}
#else
static void bench_io_init(void) {}
static void bench_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
#endif

#ifndef NBODY_CPU_HZ_FALLBACK
#define NBODY_CPU_HZ_FALLBACK 100000000.0
#endif

#ifndef NBODY_BYTES_PER_INTERACTION
#define NBODY_BYTES_PER_INTERACTION (sizeof(double) * 4.0)
#endif

#ifndef NBODY_BYTES_PER_BODY_UPDATE
#define NBODY_BYTES_PER_BODY_UPDATE (sizeof(double) * 9.0)
#endif

typedef struct {
    atomic_uint count;
    atomic_uint generation;
} NBodyBarrier;

typedef struct {
    uint64_t ready_cycle;
    bool ready_cycle_valid;
    bool started;
} TaskRuntime;

static Body g_bodies[NBODY_MAX_BODIES];
static double g_accel_buf[2][NBODY_MAX_BODIES][3];
static uint32_t g_accel_index;
static NBodySimConfig g_cfg;
static NBodyTaskConfig g_task_cfg[NBODY_MAX_TASKS];
static TaskRuntime g_task_runtime[NBODY_MAX_TASKS];
static BenchMetrics g_metrics;
static const NBodyOSHooks *g_hooks;
static NBodyBarrier g_barrier = { ATOMIC_VAR_INIT(0), ATOMIC_VAR_INIT(0) };
static uint64_t g_step_cycles[NBODY_MAX_STEPS];
static uint32_t g_body_count;
static uint32_t g_task_count;
static uint64_t g_run_start_cycle;
static uint64_t g_run_end_cycle;
static atomic_uint g_tasks_finished;
static atomic_uint_fast64_t g_scheduler_cycles_total;
static atomic_uint g_context_switch_voluntary;
static atomic_uint g_context_switch_involuntary;
static atomic_uint_fast64_t g_ready_latency_cycles_total;
static atomic_uint g_ready_latency_samples;
static atomic_uint_fast64_t g_fpu_cost_cycles_total;
static atomic_uint g_fpu_cost_events;
static atomic_uint_fast64_t g_interrupt_latency_cycles_max;
static atomic_uint_fast64_t g_critical_section_cycles_max;
static double g_cache_miss_estimate;
static atomic_uint g_stack_peak_bytes;
static atomic_llong g_tick_drift_cycles;
static atomic_uint_fast64_t g_sync_wait_cycles;
static double g_memory_bytes_total;
static atomic_uint_fast64_t g_energy_active_cycles;
static atomic_uint_fast64_t g_energy_total_cycles;

static inline void nbody_yield(void) {
    if (g_hooks && g_hooks->yield) {
        g_hooks->yield();
    }
}

static inline uint64_t nbody_rdcycle(void) {
#if defined(__riscv)
    uint64_t value;
    __asm__ volatile("rdcycle %0" : "=r"(value));
    return value;
#elif defined(__ARM_ARCH) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__thumb__)
    volatile uint32_t *demcr = (uint32_t *)0xE000EDFC;
    volatile uint32_t *dwt_ctrl = (uint32_t *)0xE0001000;
    volatile uint32_t *dwt_cyccnt = (uint32_t *)0xE0001004;
    if ((*demcr & (1u << 24)) == 0u) {
        *demcr |= (1u << 24);
    }
    if ((*dwt_ctrl & 1u) == 0u) {
        *dwt_ctrl |= 1u;
        *dwt_cyccnt = 0u;
    }
    return (uint64_t)(*dwt_cyccnt);
#elif defined(__x86_64__) || defined(__i386__)
    return __builtin_ia32_rdtsc();
#else
#endif
}

static void nbody_barrier_wait(void) {
    uint64_t wait_start = nbody_rdcycle();
    uint32_t gen = atomic_load_explicit(&g_barrier.generation, memory_order_acquire);
    uint32_t arrived = atomic_fetch_add_explicit(&g_barrier.count, 1u, memory_order_acq_rel) + 1u;
    if (arrived == g_task_count) {
        atomic_store_explicit(&g_barrier.count, 0u, memory_order_relaxed);
        atomic_store_explicit(&g_barrier.generation, gen + 1u, memory_order_release);
    } else {
        while (atomic_load_explicit(&g_barrier.generation, memory_order_acquire) == gen) {
            nbody_yield();
        }
    }
    uint64_t wait_end = nbody_rdcycle();
    atomic_fetch_add_explicit(&g_sync_wait_cycles, wait_end - wait_start, memory_order_relaxed);
}

static void nbody_compute_acceleration(uint32_t start, uint32_t end, double accel[][3]) {
    for (uint32_t i = start; i < end; ++i) {
        double ax = 0.0;
        double ay = 0.0;
        double az = 0.0;
        const double px = g_bodies[i].pos[0];
        const double py = g_bodies[i].pos[1];
        const double pz = g_bodies[i].pos[2];
        for (uint32_t j = 0; j < g_body_count; ++j) {
            if (j == i) {
                continue;
            }
            double dx = g_bodies[j].pos[0] - px;
            double dy = g_bodies[j].pos[1] - py;
            double dz = g_bodies[j].pos[2] - pz;
            double dist_sqr = dx * dx + dy * dy + dz * dz + g_cfg.softening;
            double inv_dist = 1.0 / sqrt(dist_sqr);
            double inv_dist3 = inv_dist * inv_dist * inv_dist;
            double s = g_cfg.grav_const * g_bodies[j].mass * inv_dist3;
            ax += dx * s;
            ay += dy * s;
            az += dz * s;
        }
        accel[i][0] = ax;
        accel[i][1] = ay;
        accel[i][2] = az;
    }
}

static void nbody_update_pos_vel_half(uint32_t start, uint32_t end, double accel[][3]) {
    const double half_dt = 0.5 * g_cfg.delta_t;
    for (uint32_t i = start; i < end; ++i) {
        g_bodies[i].vel[0] += half_dt * accel[i][0];
        g_bodies[i].vel[1] += half_dt * accel[i][1];
        g_bodies[i].vel[2] += half_dt * accel[i][2];
        g_bodies[i].pos[0] += g_cfg.delta_t * g_bodies[i].vel[0];
        g_bodies[i].pos[1] += g_cfg.delta_t * g_bodies[i].vel[1];
        g_bodies[i].pos[2] += g_cfg.delta_t * g_bodies[i].vel[2];
    }
}

static void nbody_finalize_velocity(uint32_t start, uint32_t end, double accel[][3]) {
    const double half_dt = 0.5 * g_cfg.delta_t;
    for (uint32_t i = start; i < end; ++i) {
        g_bodies[i].vel[0] += half_dt * accel[i][0];
        g_bodies[i].vel[1] += half_dt * accel[i][1];
        g_bodies[i].vel[2] += half_dt * accel[i][2];
    }
}

void nbody_bench_init(const NBodySimConfig *cfg,
                      const Body *initial_bodies,
                      uint32_t body_count) {
    if (!cfg || !initial_bodies) {
        return;
    }
    g_cfg = *cfg;
    if (g_cfg.cpu_hz <= 0.0) {
        g_cfg.cpu_hz = NBODY_CPU_HZ_FALLBACK;
    }
    g_body_count = (body_count > NBODY_MAX_BODIES) ? NBODY_MAX_BODIES : body_count;
    g_cfg.body_count = g_body_count;
    g_task_count = (g_cfg.task_count > NBODY_MAX_TASKS) ? NBODY_MAX_TASKS : g_cfg.task_count;
    if (g_task_count == 0u) {
        g_task_count = 1u;
    }
    memcpy(g_bodies, initial_bodies, g_body_count * sizeof(Body));
    memset(g_accel_buf, 0, sizeof(g_accel_buf));
    memset(g_task_runtime, 0, sizeof(g_task_runtime));
    memset(&g_metrics, 0, sizeof(g_metrics));
    memset(g_step_cycles, 0, sizeof(g_step_cycles));
    atomic_store(&g_barrier.count, 0u);
    atomic_store(&g_barrier.generation, 0u);
    atomic_store(&g_tasks_finished, 0u);
    g_accel_index = 0u;
    atomic_store(&g_scheduler_cycles_total, 0u);
    atomic_store(&g_context_switch_voluntary, 0u);
    atomic_store(&g_context_switch_involuntary, 0u);
    atomic_store(&g_ready_latency_cycles_total, 0u);
    atomic_store(&g_ready_latency_samples, 0u);
    atomic_store(&g_fpu_cost_cycles_total, 0u);
    atomic_store(&g_fpu_cost_events, 0u);
    atomic_store(&g_interrupt_latency_cycles_max, 0u);
    atomic_store(&g_critical_section_cycles_max, 0u);
    atomic_store(&g_tick_drift_cycles, 0);
    atomic_store(&g_sync_wait_cycles, 0u);
    atomic_store(&g_energy_active_cycles, 0u);
    atomic_store(&g_energy_total_cycles, 0u);
    atomic_store(&g_stack_peak_bytes, 0u);
    nbody_compute_acceleration(0u, g_body_count, g_accel_buf[g_accel_index]);
    uint32_t bodies_per_task = (g_body_count + g_task_count - 1u) / g_task_count;
    for (uint32_t t = 0; t < g_task_count; ++t) {
        uint32_t start = t * bodies_per_task;
        uint32_t end = start + bodies_per_task;
        if (start >= g_body_count) {
            start = g_body_count;
        }
        if (end > g_body_count) {
            end = g_body_count;
        }
        g_task_cfg[t].task_id = t;
        g_task_cfg[t].start_body = start;
        g_task_cfg[t].end_body = end;
        g_task_runtime[t].ready_cycle = 0u;
        g_task_runtime[t].ready_cycle_valid = false;
        g_task_runtime[t].started = false;
    }
    g_memory_bytes_total = 0.0;
    g_cache_miss_estimate = 0.0;
    bench_io_init();
}

void nbody_bench_bind_hooks(const NBodyOSHooks *hooks) {
    g_hooks = hooks;
}

const NBodyTaskConfig *nbody_bench_get_task_table(uint32_t *count) {
    if (count) {
        *count = g_task_count;
    }
    return g_task_cfg;
}

void nbody_bench_start(void) {
    g_run_start_cycle = nbody_rdcycle();
    g_run_end_cycle = g_run_start_cycle;
}

void nbody_bench_wait_for_completion(void) {
    while (atomic_load_explicit(&g_tasks_finished, memory_order_acquire) < g_task_count) {
        nbody_yield();
    }
    g_run_end_cycle = nbody_rdcycle();
}

static double nbody_total_time_seconds(void) {
    uint64_t cycles = (g_run_end_cycle > g_run_start_cycle)
                          ? (g_run_end_cycle - g_run_start_cycle)
                          : 0u;
    g_metrics.total_exec_cycles = cycles;
    g_metrics.total_exec_time_s = (double)cycles / g_cfg.cpu_hz;
    return g_metrics.total_exec_time_s;
}

void nbody_bench_report(void) {
    BenchMetrics metrics;
    nbody_bench_get_metrics(&metrics);
    bench_printf("\n=== N-Body RTOS Benchmark ===\n");
    bench_printf("Total Execution Time       : %.6f s (%" PRIu64 " cycles)\n",
                 metrics.total_exec_time_s, metrics.total_exec_cycles);
    bench_printf("Scheduler Overhead         : %.6f s (%.2f%%)\n",
                 metrics.scheduler_overhead_s,
                 (metrics.total_exec_time_s > 0.0)
                     ? (metrics.scheduler_overhead_s / metrics.total_exec_time_s * 100.0)
                     : 0.0);
    bench_printf("Context Switches (V/I)     : %u / %u\n",
                 metrics.voluntary_context_switches,
                 metrics.involuntary_context_switches);
    bench_printf("Average Step Time          : %.3f ns\n", metrics.avg_step_time_ns);
    bench_printf("Step Time Variance         : %.3f ns^2\n", metrics.step_jitter_ns2);
    bench_printf("Floating Point Throughput  : %.3f GFLOPS\n",
                 metrics.floating_point_throughput_gflops);
    bench_printf("Ready-to-Run Latency       : %.3f ns\n", metrics.ready_to_run_latency_ns);
    bench_printf("Stack Peak Usage           : %u bytes\n", metrics.stack_peak_bytes);
    bench_printf("FPU Context Cost           : %.3f ns\n", metrics.fpu_context_cost_ns);
    bench_printf("Interrupt Latency (max)    : %.3f ns\n", metrics.interrupt_latency_ns);
    bench_printf("Critical Section (max)     : %.3f ns\n", metrics.critical_section_ns);
    bench_printf("Memory Bandwidth           : %.3f GB/s\n", metrics.memory_bandwidth_gbps);
    bench_printf("Cache Miss Estimate        : %.3f (ratio)\n", metrics.cache_miss_estimate);
    bench_printf("System Tick Drift          : %.3f ns\n", metrics.tick_drift_ns);
    bench_printf("Sync Overhead              : %.3f ns\n", metrics.sync_overhead_ns);
    bench_printf("Energy Efficiency Ratio    : %.3f\n", metrics.energy_efficiency_ratio);
    bench_printf("===============================\n");
}

static void nbody_finalize_metrics(void) {
    double total_time_s = nbody_total_time_seconds();
    uint64_t sched_cycles = atomic_load_explicit(&g_scheduler_cycles_total, memory_order_relaxed);
    g_metrics.scheduler_cycles = sched_cycles;
    g_metrics.scheduler_overhead_s = (double)sched_cycles / g_cfg.cpu_hz;
    g_metrics.voluntary_context_switches =
        atomic_load_explicit(&g_context_switch_voluntary, memory_order_relaxed);
    g_metrics.involuntary_context_switches =
        atomic_load_explicit(&g_context_switch_involuntary, memory_order_relaxed);

    double sum_cycles = 0.0;
    double sum_sq = 0.0;
    uint32_t steps_tracked = (g_cfg.steps > NBODY_MAX_STEPS) ? NBODY_MAX_STEPS : g_cfg.steps;
    for (uint32_t i = 0; i < steps_tracked; ++i) {
        double c = (double)g_step_cycles[i];
        sum_cycles += c;
        sum_sq += c * c;
    }
    double mean_cycles = (steps_tracked > 0u) ? (sum_cycles / (double)steps_tracked) : 0.0;
    double variance = (steps_tracked > 0u)
                          ? ((sum_sq / (double)steps_tracked) - (mean_cycles * mean_cycles))
                          : 0.0;
    g_metrics.avg_step_time_ns = (mean_cycles / g_cfg.cpu_hz) * 1e9;
    g_metrics.step_jitter_ns2 = (variance / (g_cfg.cpu_hz * g_cfg.cpu_hz)) * 1e18;

    double interactions = (double)g_body_count * (double)(g_body_count - 1u);
    double total_flops = interactions * (double)g_cfg.steps * NBODY_FLOPS_PER_PAIR;
    g_metrics.floating_point_throughput_gflops = (total_time_s > 0.0)
                                                     ? ((total_flops / total_time_s) / 1e9)
                                                     : 0.0;

    uint64_t ready_cycles =
        atomic_load_explicit(&g_ready_latency_cycles_total, memory_order_relaxed);
    uint32_t ready_samples =
        atomic_load_explicit(&g_ready_latency_samples, memory_order_relaxed);
    double ready_ns = (ready_samples > 0u)
                          ? (((double)ready_cycles / g_cfg.cpu_hz) * 1e9 / (double)ready_samples)
                          : 0.0;
    g_metrics.ready_to_run_latency_ns = ready_ns;

    g_metrics.stack_peak_bytes = atomic_load_explicit(&g_stack_peak_bytes, memory_order_relaxed);

    uint64_t fpu_cycles =
        atomic_load_explicit(&g_fpu_cost_cycles_total, memory_order_relaxed);
    uint32_t fpu_events =
        atomic_load_explicit(&g_fpu_cost_events, memory_order_relaxed);
    double fpu_ns = (fpu_events > 0u)
                        ? (((double)fpu_cycles / (double)fpu_events) / g_cfg.cpu_hz * 1e9)
                        : 0.0;
    g_metrics.fpu_context_cost_ns = fpu_ns;

    uint64_t max_int =
        atomic_load_explicit(&g_interrupt_latency_cycles_max, memory_order_relaxed);
    uint64_t max_crit =
        atomic_load_explicit(&g_critical_section_cycles_max, memory_order_relaxed);
    g_metrics.interrupt_latency_ns = ((double)max_int / g_cfg.cpu_hz) * 1e9;
    g_metrics.critical_section_ns = ((double)max_crit / g_cfg.cpu_hz) * 1e9;

    double bytes_total = g_memory_bytes_total;
    g_metrics.memory_bandwidth_gbps = (total_time_s > 0.0)
                                          ? ((bytes_total / total_time_s) / 1e9)
                                          : 0.0;

    double working_set = (double)g_body_count * sizeof(Body);
    double cache_ratio = NBODY_L1_CACHE_BYTES > 0
                             ? (working_set / (double)NBODY_L1_CACHE_BYTES)
                             : 0.0;
    g_metrics.cache_miss_estimate = (g_cache_miss_estimate > 0.0)
                                        ? g_cache_miss_estimate
                                        : cache_ratio;

    int64_t drift_cycles = atomic_load_explicit(&g_tick_drift_cycles, memory_order_relaxed);
    g_metrics.tick_drift_ns = ((double)drift_cycles / g_cfg.cpu_hz) * 1e9;
    uint64_t sync_cycles = atomic_load_explicit(&g_sync_wait_cycles, memory_order_relaxed);
    g_metrics.sync_overhead_ns = ((double)sync_cycles / g_cfg.cpu_hz) * 1e9;

    uint64_t energy_active =
        atomic_load_explicit(&g_energy_active_cycles, memory_order_relaxed);
    uint64_t energy_total =
        atomic_load_explicit(&g_energy_total_cycles, memory_order_relaxed);
    g_metrics.energy_efficiency_ratio = (energy_total > 0u)
                                            ? ((double)energy_active / (double)energy_total)
                                            : 0.0;
}

void nbody_bench_get_metrics(BenchMetrics *out) {
    if (!out) {
        return;
    }
    nbody_finalize_metrics();
    *out = g_metrics;
}

void nbody_mark_task_ready(uint32_t task_id, uint64_t ready_cycle) {
    if (task_id >= g_task_count) {
        return;
    }
    g_task_runtime[task_id].ready_cycle = ready_cycle;
    g_task_runtime[task_id].ready_cycle_valid = true;
}

void nbody_record_stack_peak(uint32_t task_id, uint32_t bytes) {
    (void)task_id;
    uint32_t prev = atomic_load_explicit(&g_stack_peak_bytes, memory_order_relaxed);
    while (bytes > prev &&
           !atomic_compare_exchange_weak_explicit(&g_stack_peak_bytes,
                                                  &prev,
                                                  bytes,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed)) {
    }
}

void nbody_record_context_switch(bool involuntary) {
    if (involuntary) {
        atomic_fetch_add_explicit(&g_context_switch_involuntary, 1u, memory_order_relaxed);
    } else {
        atomic_fetch_add_explicit(&g_context_switch_voluntary, 1u, memory_order_relaxed);
    }
}

void nbody_record_scheduler_window(uint64_t start_cycle, uint64_t end_cycle) {
    if (end_cycle > start_cycle) {
        atomic_fetch_add_explicit(&g_scheduler_cycles_total,
                                  end_cycle - start_cycle,
                                  memory_order_relaxed);
    }
}

void nbody_record_fpu_context_cost(uint64_t cycles) {
    atomic_fetch_add_explicit(&g_fpu_cost_cycles_total, cycles, memory_order_relaxed);
    atomic_fetch_add_explicit(&g_fpu_cost_events, 1u, memory_order_relaxed);
}

void nbody_record_interrupt_latency(uint64_t cycles) {
    uint64_t prev = atomic_load_explicit(&g_interrupt_latency_cycles_max, memory_order_relaxed);
    while (cycles > prev &&
           !atomic_compare_exchange_weak_explicit(&g_interrupt_latency_cycles_max,
                                                  &prev,
                                                  cycles,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed)) {
    }
}

void nbody_record_critical_section(uint64_t cycles) {
    uint64_t prev = atomic_load_explicit(&g_critical_section_cycles_max, memory_order_relaxed);
    while (cycles > prev &&
           !atomic_compare_exchange_weak_explicit(&g_critical_section_cycles_max,
                                                  &prev,
                                                  cycles,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed)) {
    }
}

void nbody_record_tick_drift(int64_t drift_cycles) {
    atomic_fetch_add_explicit(&g_tick_drift_cycles, drift_cycles, memory_order_relaxed);
}

void nbody_record_cache_miss_estimate(double miss_ratio) {
    g_cache_miss_estimate = miss_ratio;
}

void nbody_record_energy_window(uint64_t active_cycles, uint64_t total_cycles) {
    atomic_fetch_add_explicit(&g_energy_active_cycles, active_cycles, memory_order_relaxed);
    atomic_fetch_add_explicit(&g_energy_total_cycles, total_cycles, memory_order_relaxed);
}

static void nbody_swap_accel_buffers(void) {
    g_accel_index ^= 1u;
}

static inline double (*nbody_get_active_accel(void))[3] {
    return g_accel_buf[g_accel_index];
}

static inline double (*nbody_get_next_accel(void))[3] {
    return g_accel_buf[g_accel_index ^ 1u];
}

void nbody_task_entry(void *arg) {
    const NBodyTaskConfig *cfg = (const NBodyTaskConfig *)arg;
    if (!cfg) {
        return;
    }
    uint32_t task_id = cfg->task_id;
    if (task_id >= g_task_count) {
        return;
    }
    TaskRuntime *runtime = &g_task_runtime[task_id];
    uint64_t start_cycle = nbody_rdcycle();
    if (runtime->ready_cycle_valid && start_cycle > runtime->ready_cycle) {
        atomic_fetch_add_explicit(&g_ready_latency_cycles_total,
                                  start_cycle - runtime->ready_cycle,
                                  memory_order_relaxed);
        atomic_fetch_add_explicit(&g_ready_latency_samples, 1u, memory_order_relaxed);
        runtime->ready_cycle_valid = false;
    }
    runtime->started = true;

    for (uint32_t step = 0; step < g_cfg.steps; ++step) {
        double (*acc_old)[3] = nbody_get_active_accel();
        double (*acc_new)[3] = nbody_get_next_accel();

        if ((task_id == 0u) && (step < NBODY_MAX_STEPS)) {
            g_step_cycles[step] = nbody_rdcycle();
        }
        nbody_barrier_wait();

        nbody_update_pos_vel_half(cfg->start_body, cfg->end_body, acc_old);
        nbody_barrier_wait();

        nbody_compute_acceleration(cfg->start_body, cfg->end_body, acc_new);
        nbody_barrier_wait();

        nbody_finalize_velocity(cfg->start_body, cfg->end_body, acc_new);
        nbody_barrier_wait();

        if (task_id == 0u) {
            if (step < NBODY_MAX_STEPS) {
                uint64_t end_cycle = nbody_rdcycle();
                uint64_t start_mark = g_step_cycles[step];
                g_step_cycles[step] = (end_cycle > start_mark) ? (end_cycle - start_mark) : 0u;
            }
            double bytes_per_step =
                ((double)g_body_count * NBODY_BYTES_PER_BODY_UPDATE) +
                ((double)g_body_count * (double)(g_body_count - 1u) * NBODY_BYTES_PER_INTERACTION);
            g_memory_bytes_total += bytes_per_step;
        }
        nbody_barrier_wait();

        if (task_id == 0u) {
            nbody_swap_accel_buffers();
        }
        nbody_barrier_wait();
    }

    atomic_fetch_add_explicit(&g_tasks_finished, 1u, memory_order_release);
}
