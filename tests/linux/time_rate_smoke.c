#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static long long timespec_diff_ms(const struct timespec* end, const struct timespec* start) {
  const long long sec = (long long)end->tv_sec - (long long)start->tv_sec;
  const long long nsec = (long long)end->tv_nsec - (long long)start->tv_nsec;
  return sec * 1000ll + nsec / 1000000ll;
}

static int read_rtc_since_epoch(long long* out_seconds) {
  FILE* fp = fopen("/sys/class/rtc/rtc0/since_epoch", "r");
  if (fp == NULL) {
    return -1;
  }

  long long value = 0;
  const int rc = fscanf(fp, "%lld", &value);
  fclose(fp);
  if (rc != 1) {
    errno = EIO;
    return -1;
  }

  *out_seconds = value;
  return 0;
}

int main(void) {
  struct timespec rt0 = {};
  struct timespec rt1 = {};
  struct timespec mono0 = {};
  struct timespec mono1 = {};
  struct timespec req = {
      .tv_sec = 2,
      .tv_nsec = 0,
  };
  long long rtc0 = 0;
  long long rtc1 = 0;

  printf("TIME-RATE-SMOKE-BEGIN\n");
  if (clock_gettime(CLOCK_REALTIME, &rt0) != 0) {
    perror("clock_gettime realtime start");
    return 1;
  }
  if (clock_gettime(CLOCK_MONOTONIC, &mono0) != 0) {
    perror("clock_gettime monotonic start");
    return 1;
  }
  if (read_rtc_since_epoch(&rtc0) != 0) {
    perror("read rtc start");
    return 1;
  }

  if (nanosleep(&req, NULL) != 0) {
    perror("nanosleep");
    return 1;
  }

  if (clock_gettime(CLOCK_REALTIME, &rt1) != 0) {
    perror("clock_gettime realtime end");
    return 1;
  }
  if (clock_gettime(CLOCK_MONOTONIC, &mono1) != 0) {
    perror("clock_gettime monotonic end");
    return 1;
  }
  if (read_rtc_since_epoch(&rtc1) != 0) {
    perror("read rtc end");
    return 1;
  }

  const long long rt_ms = timespec_diff_ms(&rt1, &rt0);
  const long long mono_ms = timespec_diff_ms(&mono1, &mono0);
  const long long rtc_ms = (rtc1 - rtc0) * 1000ll;
  const long long rt_vs_rtc = llabs(rt_ms - rtc_ms);
  const long long mono_vs_rt = llabs(mono_ms - rt_ms);

  printf("TIME-RATE-REALTIME-MS:%lld\n", rt_ms);
  printf("TIME-RATE-MONOTONIC-MS:%lld\n", mono_ms);
  printf("TIME-RATE-RTC-DELTA-S:%lld\n", rtc1 - rtc0);
  printf("TIME-RATE-RTC-MS:%lld\n", rtc_ms);
  printf("TIME-RATE-RT-VS-RTC-MS:%lld\n", rt_vs_rtc);
  printf("TIME-RATE-MONO-VS-RT-MS:%lld\n", mono_vs_rt);

  if (rt_ms < 1500ll || rt_ms > 3500ll) {
    fprintf(stderr, "realtime delta out of range: %lldms\n", rt_ms);
    return 1;
  }
  if (mono_ms < 1500ll || mono_ms > 3500ll) {
    fprintf(stderr, "monotonic delta out of range: %lldms\n", mono_ms);
    return 1;
  }
  if ((rtc1 - rtc0) < 1ll || (rtc1 - rtc0) > 3ll) {
    fprintf(stderr, "rtc delta out of range: %llds\n", rtc1 - rtc0);
    return 1;
  }
  if (rt_vs_rtc > 1500ll) {
    fprintf(stderr, "realtime and rtc diverged too much: %lldms\n", rt_vs_rtc);
    return 1;
  }
  if (mono_vs_rt > 750ll) {
    fprintf(stderr, "monotonic and realtime diverged too much: %lldms\n", mono_vs_rt);
    return 1;
  }

  printf("TIME-RATE-SMOKE PASS\n");
  return 0;
}
