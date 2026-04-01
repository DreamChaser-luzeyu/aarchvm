#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/rtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static const long long kRtcToleranceSeconds = 2;

static int read_text_file(const char* path, char* buffer, size_t buffer_size) {
  FILE* f = fopen(path, "r");
  size_t n;
  if (f == NULL) {
    return 0;
  }
  n = fread(buffer, 1, buffer_size - 1, f);
  fclose(f);
  buffer[n] = '\0';
  return 1;
}

static long long read_since_epoch_sysfs(void) {
  char buffer[128];
  if (!read_text_file("/sys/class/rtc/rtc0/since_epoch", buffer, sizeof(buffer))) {
    return -1;
  }
  return strtoll(buffer, NULL, 10);
}

static int read_rtc_name(char* buffer, size_t buffer_size) {
  size_t len;
  if (!read_text_file("/sys/class/rtc/rtc0/name", buffer, buffer_size)) {
    return 0;
  }
  len = strlen(buffer);
  if (len > 0 && buffer[len - 1] == '\n') {
    buffer[len - 1] = '\0';
  }
  return 1;
}

static int rtc_read_epoch(int fd, long long* epoch_out) {
  struct rtc_time rtc_tm;
  struct tm tm_value;
  time_t epoch;

  memset(&rtc_tm, 0, sizeof(rtc_tm));
  if (ioctl(fd, RTC_RD_TIME, &rtc_tm) != 0) {
    return 0;
  }

  memset(&tm_value, 0, sizeof(tm_value));
  tm_value.tm_sec = rtc_tm.tm_sec;
  tm_value.tm_min = rtc_tm.tm_min;
  tm_value.tm_hour = rtc_tm.tm_hour;
  tm_value.tm_mday = rtc_tm.tm_mday;
  tm_value.tm_mon = rtc_tm.tm_mon;
  tm_value.tm_year = rtc_tm.tm_year;
  tm_value.tm_isdst = 0;

  errno = 0;
  epoch = timegm(&tm_value);
  if (epoch == (time_t)-1 && errno != 0) {
    return 0;
  }

  *epoch_out = (long long)epoch;
  return 1;
}

static int rtc_set_epoch(int fd, long long epoch) {
  const time_t value = (time_t)epoch;
  struct tm tm_value;
  struct rtc_time rtc_tm;

  memset(&tm_value, 0, sizeof(tm_value));
  if (gmtime_r(&value, &tm_value) == NULL) {
    return 0;
  }

  memset(&rtc_tm, 0, sizeof(rtc_tm));
  rtc_tm.tm_sec = tm_value.tm_sec;
  rtc_tm.tm_min = tm_value.tm_min;
  rtc_tm.tm_hour = tm_value.tm_hour;
  rtc_tm.tm_mday = tm_value.tm_mday;
  rtc_tm.tm_mon = tm_value.tm_mon;
  rtc_tm.tm_year = tm_value.tm_year;
  rtc_tm.tm_wday = tm_value.tm_wday;
  rtc_tm.tm_yday = tm_value.tm_yday;
  rtc_tm.tm_isdst = 0;

  return ioctl(fd, RTC_SET_TIME, &rtc_tm) == 0;
}

static int within_tolerance(long long lhs, long long rhs) {
  const long long diff = (lhs >= rhs) ? (lhs - rhs) : (rhs - lhs);
  return diff <= kRtcToleranceSeconds;
}

int main(void) {
  char name[128];
  long long sysfs_before;
  long long before_epoch = 0;
  long long after_epoch = 0;
  long long restored_epoch = 0;
  long long target_epoch;
  int fd;

  setenv("TZ", "UTC0", 1);
  tzset();

  printf("RTC-SMOKE-BEGIN\n");

  if (!read_rtc_name(name, sizeof(name))) {
    perror("read rtc name");
    return 1;
  }
  printf("RTC-NAME:%s\n", name);

  sysfs_before = read_since_epoch_sysfs();
  if (sysfs_before < 0) {
    perror("read since_epoch");
    return 1;
  }
  printf("RTC-SYSFS-BEFORE:%lld\n", sysfs_before);

  fd = open("/dev/rtc0", O_RDWR);
  if (fd < 0) {
    perror("open /dev/rtc0");
    return 1;
  }

  if (!rtc_read_epoch(fd, &before_epoch)) {
    perror("RTC_RD_TIME");
    close(fd);
    return 1;
  }
  printf("RTC-READ-BEFORE:%lld\n", before_epoch);
  if (!within_tolerance(before_epoch, sysfs_before)) {
    fprintf(stderr, "rtc/sysfs mismatch before set: rtc=%lld sysfs=%lld\n", before_epoch, sysfs_before);
    close(fd);
    return 1;
  }

  target_epoch = (before_epoch > 120) ? (before_epoch - 60) : (before_epoch + 60);
  if (!rtc_set_epoch(fd, target_epoch)) {
    perror("RTC_SET_TIME target");
    close(fd);
    return 1;
  }
  if (!rtc_read_epoch(fd, &after_epoch)) {
    perror("RTC_RD_TIME after set");
    close(fd);
    return 1;
  }
  printf("RTC-READ-AFTER:%lld\n", after_epoch);
  if (!within_tolerance(after_epoch, target_epoch)) {
    fprintf(stderr, "rtc set mismatch: got=%lld target=%lld\n", after_epoch, target_epoch);
    close(fd);
    return 1;
  }

  if (!rtc_set_epoch(fd, sysfs_before)) {
    perror("RTC_SET_TIME restore");
    close(fd);
    return 1;
  }
  if (!rtc_read_epoch(fd, &restored_epoch)) {
    perror("RTC_RD_TIME restore");
    close(fd);
    return 1;
  }
  printf("RTC-READ-RESTORED:%lld\n", restored_epoch);
  if (!within_tolerance(restored_epoch, sysfs_before)) {
    fprintf(stderr, "rtc restore mismatch: got=%lld restore=%lld\n", restored_epoch, sysfs_before);
    close(fd);
    return 1;
  }

  close(fd);
  printf("RTC-SMOKE PASS\n");
  return 0;
}
