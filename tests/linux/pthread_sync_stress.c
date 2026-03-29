#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

enum {
  kCounterIters = 100000,
  kMessageRounds = 20000,
};

static pthread_barrier_t start_barrier;
static atomic_uint_fast64_t counter;
static atomic_uint phase;
static atomic_uint payload;
static atomic_uint ack;
static atomic_int failure;

static int pin_to_cpu(int cpu) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  return sched_setaffinity(0, sizeof(set), &set);
}

static void *worker(void *arg) {
  const int id = (int)(intptr_t)arg;
  if (pin_to_cpu(id) != 0) {
    perror("sched_setaffinity");
    atomic_store_explicit(&failure, 1, memory_order_relaxed);
    return NULL;
  }

  int rc = pthread_barrier_wait(&start_barrier);
  if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
    return NULL;
  }

  for (unsigned i = 0; i < kCounterIters; ++i) {
    atomic_fetch_add_explicit(&counter, 1, memory_order_acq_rel);
  }

  rc = pthread_barrier_wait(&start_barrier);
  if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
    return NULL;
  }

  if (id == 0) {
    for (unsigned i = 1; i <= kMessageRounds; ++i) {
      while (atomic_load_explicit(&ack, memory_order_acquire) != (i - 1u)) {
        if (atomic_load_explicit(&failure, memory_order_relaxed) != 0) {
          return NULL;
        }
        sched_yield();
      }
      atomic_store_explicit(&payload, i, memory_order_relaxed);
      atomic_store_explicit(&phase, i, memory_order_release);
    }
  } else {
    for (unsigned i = 1; i <= kMessageRounds; ++i) {
      while (atomic_load_explicit(&phase, memory_order_acquire) != i) {
        if (atomic_load_explicit(&failure, memory_order_relaxed) != 0) {
          return NULL;
        }
        sched_yield();
      }
      if (atomic_load_explicit(&payload, memory_order_relaxed) != i) {
        fprintf(stderr,
                "payload mismatch: got=%u expected=%u\n",
                atomic_load_explicit(&payload, memory_order_relaxed),
                i);
        atomic_store_explicit(&failure, 1, memory_order_relaxed);
        return NULL;
      }
      atomic_store_explicit(&ack, i, memory_order_release);
    }
  }

  return NULL;
}

int main(void) {
  const long nproc = sysconf(_SC_NPROCESSORS_ONLN);
  if (nproc < 2) {
    puts("PTHREAD-SYNC-SKIP");
    return 0;
  }

  if (pthread_barrier_init(&start_barrier, NULL, 2) != 0) {
    perror("pthread_barrier_init");
    return 1;
  }

  pthread_t threads[2];
  for (int i = 0; i < 2; ++i) {
    if (pthread_create(&threads[i], NULL, worker, (void *)(intptr_t)i) != 0) {
      perror("pthread_create");
      return 1;
    }
  }
  for (int i = 0; i < 2; ++i) {
    if (pthread_join(threads[i], NULL) != 0) {
      perror("pthread_join");
      return 1;
    }
  }

  if (atomic_load_explicit(&failure, memory_order_relaxed) != 0) {
    return 2;
  }

  const uint64_t final_counter = atomic_load_explicit(&counter, memory_order_relaxed);
  if (final_counter != (uint64_t)kCounterIters * 2u) {
    fprintf(stderr, "counter mismatch: got=%llu expected=%u\n",
            (unsigned long long)final_counter,
            kCounterIters * 2);
    return 3;
  }
  if (atomic_load_explicit(&phase, memory_order_relaxed) != kMessageRounds ||
      atomic_load_explicit(&ack, memory_order_relaxed) != kMessageRounds) {
    fprintf(stderr,
            "message mismatch: phase=%u ack=%u expected=%u\n",
            atomic_load_explicit(&phase, memory_order_relaxed),
            atomic_load_explicit(&ack, memory_order_relaxed),
            kMessageRounds);
    return 4;
  }

  printf("PTHREAD-SYNC PASS counter=%llu rounds=%u\n",
         (unsigned long long)final_counter,
         kMessageRounds);
  return 0;
}
