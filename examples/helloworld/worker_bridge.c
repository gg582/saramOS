#include "rtos_port.h"
#include <ttak/thread/worker.h>
#include <ttak/thread/pool.h>
#include <ttak/async/task.h>
#include <ttak/mem/epoch.h>
#include <ttak/timing/timing.h>
#include <ttak/priority/queue.h>

#include <pthread.h>


static ttak_task_t *bridge_worker_steal(ttak_thread_pool_t *pool, size_t skip_shard, uint64_t now) {
    for (size_t s = 0; s < TTAK_THREAD_POOL_SHARDS; ++s) {
        if (s == skip_shard) continue;
        ttak_pool_shard_t *shard = &pool->shards[s];
        if (shard->queue.head == NULL) continue;
        if (pthread_mutex_trylock(&shard->lock) != 0) continue;
        ttak_task_t *task = shard->queue.pop(&shard->queue, now);
        pthread_mutex_unlock(&shard->lock);
        if (task) return task;
    }
    return NULL;
}

void ttak_worker_run_cooperative_bridge(ttak_worker_t *worker, uint64_t budget_ns) {
    if (!worker || !worker->pool) return;
    ttak_thread_pool_t *pool = worker->pool;
    ttak_pool_shard_t *pref = &pool->shards[worker->preferred_shard];
    uint64_t start_ns = ttak_get_tick_count_ns();

    while (!worker->should_stop && !pool->is_shutdown) {
        uint64_t now = ttak_get_tick_count();
        ttak_task_t *task = NULL;

        pthread_mutex_lock(&pref->lock);
        task = pref->queue.pop(&pref->queue, now);
        pthread_mutex_unlock(&pref->lock);

        if (!task) {
            task = bridge_worker_steal(pool, worker->preferred_shard, now);
        }
        if (!task) {
            break;
        }

        ttak_epoch_enter();
        ttak_task_execute(task, now);
        ttak_epoch_exit();
        ttak_task_destroy(task, now);

        if (budget_ns) {
            uint64_t elapsed = ttak_get_tick_count_ns() - start_ns;
            if (elapsed >= budget_ns) {
                break;
            }
        }
    }
}
