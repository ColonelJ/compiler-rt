// RUN: %clang_dfsan -m64 %s -o %t && DFSAN_OPTIONS="strict_data_dependencies=0" %t
// RUN: %clang_dfsan -mllvm -dfsan-args-abi -m64 %s -o %t && DFSAN_OPTIONS="strict_data_dependencies=0" %t
// RUN: %clang_dfsan -DSTRICT_DATA_DEPENDENCIES -m64 %s -o %t && %t
// RUN: %clang_dfsan -DSTRICT_DATA_DEPENDENCIES -mllvm -dfsan-args-abi -m64 %s -o %t && %t

// Tests custom implementations of various glibc functions.

#define _GNU_SOURCE
#include <sanitizer/dfsan_interface.h>

#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <link.h>
#include <poll.h>
#include <pthread.h>
#include <pwd.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

dfsan_label i_label = 0;
dfsan_label j_label = 0;
dfsan_label i_j_label = 0;

#define ASSERT_ZERO_LABEL(data) \
  assert(0 == dfsan_get_label((long) (data)))

#define ASSERT_READ_ZERO_LABEL(ptr, size) \
  assert(0 == dfsan_read_label(ptr, size))

#define ASSERT_LABEL(data, label) \
  assert(label == dfsan_get_label((long) (data)))

#define ASSERT_READ_LABEL(ptr, size, label) \
  assert(label == dfsan_read_label(ptr, size))

void test_stat() {
  int i = 1;
  dfsan_set_label(i_label, &i, sizeof(i));

  struct stat s;
  s.st_dev = i;
  assert(0 == stat("/", &s));
  ASSERT_ZERO_LABEL(s.st_dev);

  s.st_dev = i;
  assert(-1 == stat("/nonexistent", &s));
  ASSERT_LABEL(s.st_dev, i_label);
}

void test_fstat() {
  int i = 1;
  dfsan_set_label(i_label, &i, sizeof(i));

  struct stat s;
  int fd = open("/dev/zero", O_RDONLY);
  s.st_dev = i;
  int rv = fstat(fd, &s);
  assert(0 == rv);
  ASSERT_ZERO_LABEL(s.st_dev);
}

void test_memcmp() {
  char str1[] = "str1", str2[] = "str2";
  dfsan_set_label(i_label, &str1[3], 1);
  dfsan_set_label(j_label, &str2[3], 1);

  int rv = memcmp(str1, str2, sizeof(str1));
  assert(rv < 0);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(rv);
#else
  ASSERT_LABEL(rv, i_j_label);
#endif
}

void test_memcpy() {
  char str1[] = "str1";
  char str2[sizeof(str1)];
  dfsan_set_label(i_label, &str1[3], 1);

  ASSERT_ZERO_LABEL(memcpy(str2, str1, sizeof(str1)));
  assert(0 == memcmp(str2, str1, sizeof(str1)));
  ASSERT_ZERO_LABEL(str2[0]);
  ASSERT_LABEL(str2[3], i_label);
}

void test_memset() {
  char buf[8];
  int j = 'a';
  dfsan_set_label(j_label, &j, sizeof(j));

  ASSERT_ZERO_LABEL(memset(&buf, j, sizeof(buf)));
  for (int i = 0; i < 8; ++i) {
    ASSERT_LABEL(buf[i], j_label);
    assert(buf[i] == 'a');
  }
}

void test_strcmp() {
  char str1[] = "str1", str2[] = "str2";
  dfsan_set_label(i_label, &str1[3], 1);
  dfsan_set_label(j_label, &str2[3], 1);

  int rv = strcmp(str1, str2);
  assert(rv < 0);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(rv);
#else
  ASSERT_LABEL(rv, i_j_label);
#endif
}

void test_strlen() {
  char str1[] = "str1";
  dfsan_set_label(i_label, &str1[3], 1);

  int rv = strlen(str1);
  assert(rv == 4);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(rv);
#else
  ASSERT_LABEL(rv, i_label);
#endif
}

void test_strdup() {
  char str1[] = "str1";
  dfsan_set_label(i_label, &str1[3], 1);

  char *strd = strdup(str1);
  ASSERT_ZERO_LABEL(strd[0]);
  ASSERT_LABEL(strd[3], i_label);
  free(strd);
}

void test_strncpy() {
  char str1[] = "str1";
  char str2[sizeof(str1)];
  dfsan_set_label(i_label, &str1[3], 1);

  char *strd = strncpy(str2, str1, 5);
  assert(strd == str2);
  assert(strcmp(str1, str2) == 0);
  ASSERT_ZERO_LABEL(strd);
  ASSERT_ZERO_LABEL(strd[0]);
  ASSERT_ZERO_LABEL(strd[1]);
  ASSERT_ZERO_LABEL(strd[2]);
  ASSERT_LABEL(strd[3], i_label);

  strd = strncpy(str2, str1, 3);
  assert(strd == str2);
  assert(strncmp(str1, str2, 3) == 0);
  ASSERT_ZERO_LABEL(strd);
  ASSERT_ZERO_LABEL(strd[0]);
  ASSERT_ZERO_LABEL(strd[1]);
  ASSERT_ZERO_LABEL(strd[2]);
}

void test_strncmp() {
  char str1[] = "str1", str2[] = "str2";
  dfsan_set_label(i_label, &str1[3], 1);
  dfsan_set_label(j_label, &str2[3], 1);

  int rv = strncmp(str1, str2, sizeof(str1));
  assert(rv < 0);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(rv);
#else
  ASSERT_LABEL(rv, dfsan_union(i_label, j_label));
#endif

  rv = strncmp(str1, str2, 3);
  assert(rv == 0);
  ASSERT_ZERO_LABEL(rv);
}

void test_strcasecmp() {
  char str1[] = "str1", str2[] = "str2", str3[] = "Str1";
  dfsan_set_label(i_label, &str1[3], 1);
  dfsan_set_label(j_label, &str2[3], 1);
  dfsan_set_label(j_label, &str3[2], 1);

  int rv = strcasecmp(str1, str2);
  assert(rv < 0);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(rv);
#else
  ASSERT_LABEL(rv, dfsan_union(i_label, j_label));
#endif

  rv = strcasecmp(str1, str3);
  assert(rv == 0);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(rv);
#else
  ASSERT_LABEL(rv, dfsan_union(i_label, j_label));
#endif
}

void test_strncasecmp() {
  char str1[] = "Str1", str2[] = "str2";
  dfsan_set_label(i_label, &str1[3], 1);
  dfsan_set_label(j_label, &str2[3], 1);

  int rv = strncasecmp(str1, str2, sizeof(str1));
  assert(rv < 0);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(rv);
#else
  ASSERT_LABEL(rv, dfsan_union(i_label, j_label));
#endif

  rv = strncasecmp(str1, str2, 3);
  assert(rv == 0);
  ASSERT_ZERO_LABEL(rv);
}

void test_strchr() {
  char str1[] = "str1";
  dfsan_set_label(i_label, &str1[3], 1);

  char *crv = strchr(str1, 'r');
  assert(crv == &str1[2]);
  ASSERT_ZERO_LABEL(crv);

  crv = strchr(str1, '1');
  assert(crv == &str1[3]);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(crv);
#else
  ASSERT_LABEL(crv, i_label);
#endif

  crv = strchr(str1, 'x');
  assert(!crv);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(crv);
#else
  ASSERT_LABEL(crv, i_label);
#endif
}

void test_calloc() {
  // With any luck this sequence of calls will cause calloc to return the same
  // pointer both times.  This is probably the best we can do to test this
  // function.
  char *crv = calloc(4096, 1);
  ASSERT_ZERO_LABEL(crv[0]);
  dfsan_set_label(i_label, crv, 100);
  free(crv);

  crv = calloc(4096, 1);
  ASSERT_ZERO_LABEL(crv[0]);
  free(crv);
}

void test_read() {
  char buf[16];
  dfsan_set_label(i_label, buf, 1);
  dfsan_set_label(j_label, buf + 15, 1);

  ASSERT_LABEL(buf[0], i_label);
  ASSERT_LABEL(buf[15], j_label);

  int fd = open("/dev/zero", O_RDONLY);
  int rv = read(fd, buf, sizeof(buf));
  assert(rv == sizeof(buf));
  ASSERT_ZERO_LABEL(rv);
  ASSERT_ZERO_LABEL(buf[0]);
  ASSERT_ZERO_LABEL(buf[15]);
  close(fd);
}

void test_pread() {
  char buf[16];
  dfsan_set_label(i_label, buf, 1);
  dfsan_set_label(j_label, buf + 15, 1);

  ASSERT_LABEL(buf[0], i_label);
  ASSERT_LABEL(buf[15], j_label);

  int fd = open("/bin/sh", O_RDONLY);
  int rv = pread(fd, buf, sizeof(buf), 0);
  assert(rv == sizeof(buf));
  ASSERT_ZERO_LABEL(rv);
  ASSERT_ZERO_LABEL(buf[0]);
  ASSERT_ZERO_LABEL(buf[15]);
  close(fd);
}

void test_dlopen() {
  void *map = dlopen(NULL, RTLD_NOW);
  assert(map);
  ASSERT_ZERO_LABEL(map);
  dlclose(map);
  map = dlopen("/nonexistent", RTLD_NOW);
  assert(!map);
  ASSERT_ZERO_LABEL(map);
}

void test_clock_gettime() {
  struct timespec tp;
  dfsan_set_label(j_label, ((char *)&tp) + 3, 1);
  int t = clock_gettime(CLOCK_REALTIME, &tp);
  assert(t == 0);
  ASSERT_ZERO_LABEL(t);
  ASSERT_ZERO_LABEL(((char *)&tp)[3]);
}

void test_ctime_r() {
  char *buf = (char*) malloc(64);
  time_t t = 0;

  char *ret = ctime_r(&t, buf);
  ASSERT_ZERO_LABEL(ret);
  assert(buf == ret);
  ASSERT_READ_ZERO_LABEL(buf, strlen(buf) + 1);

  dfsan_set_label(i_label, &t, sizeof(t));
  ret = ctime_r(&t, buf);
  ASSERT_ZERO_LABEL(ret);
  ASSERT_READ_LABEL(buf, strlen(buf) + 1, i_label);

  t = 0;
  dfsan_set_label(j_label, &buf, sizeof(&buf));
  ret = ctime_r(&t, buf);
  ASSERT_LABEL(ret, j_label);
  ASSERT_READ_ZERO_LABEL(buf, strlen(buf) + 1);
}

void test_fgets() {
  char *buf = (char*) malloc(128);
  FILE *f = fopen("/etc/passwd", "r");
  dfsan_set_label(j_label, buf, 1);
  char *ret = fgets(buf, sizeof(buf), f);
  assert(ret == buf);
  ASSERT_ZERO_LABEL(ret);
  ASSERT_READ_ZERO_LABEL(buf, 128);
  dfsan_set_label(j_label, &buf, sizeof(&buf));
  ret = fgets(buf, sizeof(buf), f);
  ASSERT_LABEL(ret, j_label);
  fclose(f);
}

void test_getcwd() {
  char buf[1024];
  char *ptr = buf;
  dfsan_set_label(i_label, buf + 2, 2);
  char* ret = getcwd(buf, sizeof(buf));
  assert(ret == buf);
  assert(ret[0] == '/');
  ASSERT_READ_ZERO_LABEL(buf + 2, 2);
  dfsan_set_label(i_label, &ptr, sizeof(ptr));
  ret = getcwd(ptr, sizeof(buf));
  ASSERT_LABEL(ret, i_label);
}

void test_get_current_dir_name() {
  char* ret = get_current_dir_name();
  assert(ret);
  assert(ret[0] == '/');
  ASSERT_READ_ZERO_LABEL(ret, strlen(ret) + 1);
}

void test_gethostname() {
  char buf[1024];
  dfsan_set_label(i_label, buf + 2, 2);
  assert(gethostname(buf, sizeof(buf)) == 0);
  ASSERT_READ_ZERO_LABEL(buf + 2, 2);
}

void test_getrlimit() {
  struct rlimit rlim;
  dfsan_set_label(i_label, &rlim, sizeof(rlim));
  assert(getrlimit(RLIMIT_CPU, &rlim) == 0);
  ASSERT_READ_ZERO_LABEL(&rlim, sizeof(rlim));
}

void test_getrusage() {
  struct rusage usage;
  dfsan_set_label(i_label, &usage, sizeof(usage));
  assert(getrusage(RUSAGE_SELF, &usage) == 0);
  ASSERT_READ_ZERO_LABEL(&usage, sizeof(usage));
}

void test_strcpy() {
  char src[] = "hello world";
  char dst[sizeof(src) + 2];
  dfsan_set_label(0, src, sizeof(src));
  dfsan_set_label(0, dst, sizeof(dst));
  dfsan_set_label(i_label, src + 2, 1);
  dfsan_set_label(j_label, src + 3, 1);
  dfsan_set_label(j_label, dst + 4, 1);
  dfsan_set_label(i_label, dst + 12, 1);
  char *ret = strcpy(dst, src);
  assert(ret == dst);
  assert(strcmp(src, dst) == 0);
  for (int i = 0; i < strlen(src) + 1; ++i) {
    assert(dfsan_get_label(dst[i]) == dfsan_get_label(src[i]));
  }
  // Note: if strlen(src) + 1 were used instead to compute the first untouched
  // byte of dest, the label would be I|J. This is because strlen() might
  // return a non-zero label, and because by default pointer labels are not
  // ignored on loads.
  ASSERT_LABEL(dst[12], i_label);
}

void test_strtol() {
  char buf[] = "1234578910";
  char *endptr = NULL;
  dfsan_set_label(i_label, buf + 1, 1);
  dfsan_set_label(j_label, buf + 10, 1);
  long int ret = strtol(buf, &endptr, 10);
  assert(ret == 1234578910);
  assert(endptr == buf + 10);
  ASSERT_LABEL(ret, i_j_label);
}

void test_strtoll() {
  char buf[] = "1234578910 ";
  char *endptr = NULL;
  dfsan_set_label(i_label, buf + 1, 1);
  dfsan_set_label(j_label, buf + 2, 1);
  long long int ret = strtoll(buf, &endptr, 10);
  assert(ret == 1234578910);
  assert(endptr == buf + 10);
  ASSERT_LABEL(ret, i_j_label);
}

void test_strtoul() {
  char buf[] = "0xffffffffffffaa";
  char *endptr = NULL;
  dfsan_set_label(i_label, buf + 1, 1);
  dfsan_set_label(j_label, buf + 2, 1);
  long unsigned int ret = strtol(buf, &endptr, 16);
  assert(ret == 72057594037927850);
  assert(endptr == buf + 16);
  ASSERT_LABEL(ret, i_j_label);
}

void test_strtoull() {
  char buf[] = "0xffffffffffffffaa";
  char *endptr = NULL;
  dfsan_set_label(i_label, buf + 1, 1);
  dfsan_set_label(j_label, buf + 2, 1);
  long long unsigned int ret = strtoull(buf, &endptr, 16);
  assert(ret == 0xffffffffffffffaa);
  assert(endptr == buf + 18);
  ASSERT_LABEL(ret, i_j_label);
}

void test_strtod() {
  char buf[] = "12345.76 foo";
  char *endptr = NULL;
  dfsan_set_label(i_label, buf + 1, 1);
  dfsan_set_label(j_label, buf + 6, 1);
  double ret = strtod(buf, &endptr);
  assert(ret == 12345.76);
  assert(endptr == buf + 8);
  ASSERT_LABEL(ret, i_j_label);
}

void test_time() {
  time_t t = 0;
  dfsan_set_label(i_label, &t, 1);
  time_t ret = time(&t);
  assert(ret == t);
  assert(ret > 0);
  ASSERT_ZERO_LABEL(t);
}

void test_inet_pton() {
  char addr4[] = "127.0.0.1";
  dfsan_set_label(i_label, addr4 + 3, 1);
  struct in_addr in4;
  int ret4 = inet_pton(AF_INET, addr4, &in4);
  assert(ret4 == 1);
  ASSERT_READ_LABEL(&in4, sizeof(in4), i_label);
  assert(in4.s_addr == 0x0100007f);

  char addr6[] = "::1";
  dfsan_set_label(j_label, addr6 + 3, 1);
  struct in6_addr in6;
  int ret6 = inet_pton(AF_INET6, addr6, &in6);
  assert(ret6 == 1);
  ASSERT_READ_LABEL(((char *) &in6) + sizeof(in6) - 1, 1, j_label);
}

void test_localtime_r() {
  time_t t0 = 1384800998;
  struct tm t1;
  dfsan_set_label(i_label, &t0, sizeof(t0));
  struct tm* ret = localtime_r(&t0, &t1);
  assert(ret == &t1);
  assert(t1.tm_min == 56);
  ASSERT_LABEL(t1.tm_mon, i_label);
}

void test_getpwuid_r() {
  struct passwd pwd;
  char buf[1024];
  struct passwd *result;

  dfsan_set_label(i_label, &pwd, 4);
  int ret = getpwuid_r(0, &pwd, buf, sizeof(buf), &result);
  assert(ret == 0);
  assert(strcmp(pwd.pw_name, "root") == 0);
  assert(result == &pwd);
  ASSERT_READ_ZERO_LABEL(&pwd, 4);
}

void test_poll() {
  struct pollfd fd;
  fd.fd = 0;
  fd.events = POLLIN;
  dfsan_set_label(i_label, &fd.revents, sizeof(fd.revents));
  int ret = poll(&fd, 1, 1);
  ASSERT_ZERO_LABEL(fd.revents);
  assert(ret >= 0);
}

void test_select() {
  struct timeval t;
  fd_set fds;
  t.tv_sec = 2;
  FD_SET(0, &fds);
  dfsan_set_label(i_label, &fds, sizeof(fds));
  dfsan_set_label(j_label, &t, sizeof(t));
  int ret = select(1, &fds, NULL, NULL, &t);
  assert(ret >= 0);
  ASSERT_ZERO_LABEL(t.tv_sec);
  ASSERT_READ_ZERO_LABEL(&fds, sizeof(fds));
}

void test_sched_getaffinity() {
  cpu_set_t mask;
  dfsan_set_label(j_label, &mask, 1);
  int ret = sched_getaffinity(0, sizeof(mask), &mask);
  assert(ret == 0);
  ASSERT_READ_ZERO_LABEL(&mask, sizeof(mask));
}

void test_sigemptyset() {
  sigset_t set;
  dfsan_set_label(j_label, &set, 1);
  int ret = sigemptyset(&set);
  assert(ret == 0);
  ASSERT_READ_ZERO_LABEL(&set, sizeof(set));
}

void test_sigaction() {
  struct sigaction oldact;
  dfsan_set_label(j_label, &oldact, 1);
  int ret = sigaction(SIGUSR1, NULL, &oldact);
  assert(ret == 0);
  ASSERT_READ_ZERO_LABEL(&oldact, sizeof(oldact));
}

void test_gettimeofday() {
  struct timeval tv;
  struct timezone tz;
  dfsan_set_label(i_label, &tv, sizeof(tv));
  dfsan_set_label(j_label, &tz, sizeof(tz));
  int ret = gettimeofday(&tv, &tz);
  assert(ret == 0);
  ASSERT_READ_ZERO_LABEL(&tv, sizeof(tv));
  ASSERT_READ_ZERO_LABEL(&tz, sizeof(tz));
}

void *pthread_create_test_cb(void *p) {
  assert(p == (void *)1);
  ASSERT_ZERO_LABEL(p);
  return (void *)2;
}

void test_pthread_create() {
  pthread_t pt;
  pthread_create(&pt, 0, pthread_create_test_cb, (void *)1);
  void *cbrv;
  pthread_join(pt, &cbrv);
  assert(cbrv == (void *)2);
}

int dl_iterate_phdr_test_cb(struct dl_phdr_info *info, size_t size,
                            void *data) {
  assert(data == (void *)3);
  ASSERT_ZERO_LABEL(info);
  ASSERT_ZERO_LABEL(size);
  ASSERT_ZERO_LABEL(data);
  return 0;
}

void test_dl_iterate_phdr() {
  dl_iterate_phdr(dl_iterate_phdr_test_cb, (void *)3);
}

void test_strrchr() {
  char str1[] = "str1str1";
  dfsan_set_label(i_label, &str1[7], 1);

  char *rv = strrchr(str1, 'r');
  assert(rv == &str1[6]);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(rv);
#else
  ASSERT_LABEL(rv, i_label);
#endif
}

void test_strstr() {
  char str1[] = "str1str1";
  dfsan_set_label(i_label, &str1[3], 1);
  dfsan_set_label(j_label, &str1[5], 1);

  char *rv = strstr(str1, "1s");
  assert(rv == &str1[3]);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(rv);
#else
  ASSERT_LABEL(rv, i_label);
#endif

  rv = strstr(str1, "2s");
  assert(rv == NULL);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(rv);
#else
  ASSERT_LABEL(rv, i_j_label);
#endif
}

void test_memchr() {
  char str1[] = "str1";
  dfsan_set_label(i_label, &str1[3], 1);
  dfsan_set_label(j_label, &str1[4], 1);

  char *crv = memchr(str1, 'r', sizeof(str1));
  assert(crv == &str1[2]);
  ASSERT_ZERO_LABEL(crv);

  crv = memchr(str1, '1', sizeof(str1));
  assert(crv == &str1[3]);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(crv);
#else
  ASSERT_LABEL(crv, i_label);
#endif

  crv = memchr(str1, 'x', sizeof(str1));
  assert(!crv);
#ifdef STRICT_DATA_DEPENDENCIES
  ASSERT_ZERO_LABEL(crv);
#else
  ASSERT_LABEL(crv, i_j_label);
#endif
}

void alarm_handler(int unused) {
  ;
}

void test_nanosleep() {
  struct timespec req, rem;
  req.tv_sec = 1;
  req.tv_nsec = 0;
  dfsan_set_label(i_label, &rem, sizeof(rem));

  // non interrupted
  int rv = nanosleep(&req, &rem);
  assert(rv == 0);
  ASSERT_ZERO_LABEL(rv);
  ASSERT_READ_LABEL(&rem, 1, i_label);

  // interrupted by an alarm
  signal(SIGALRM, alarm_handler);
  req.tv_sec = 3;
  alarm(1);
  rv = nanosleep(&req, &rem);
  assert(rv == -1);
  ASSERT_ZERO_LABEL(rv);
  ASSERT_READ_ZERO_LABEL(&rem, sizeof(rem));
}

void test_socketpair() {
  int fd[2];

  dfsan_set_label(i_label, fd, sizeof(fd));
  int rv = socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
  assert(rv == 0);
  ASSERT_ZERO_LABEL(rv);
  ASSERT_READ_ZERO_LABEL(fd, sizeof(fd));
}

int main(void) {
  i_label = dfsan_create_label("i", 0);
  j_label = dfsan_create_label("j", 0);
  i_j_label = dfsan_union(i_label, j_label);

  test_calloc();
  test_clock_gettime();
  test_ctime_r();
  test_dl_iterate_phdr();
  test_dlopen();
  test_fgets();
  test_fstat();
  test_get_current_dir_name();
  test_getcwd();
  test_gethostname();
  test_getpwuid_r();
  test_getrlimit();
  test_getrusage();
  test_gettimeofday();
  test_inet_pton();
  test_localtime_r();
  test_memchr();
  test_memcmp();
  test_memcpy();
  test_memset();
  test_nanosleep();
  test_poll();
  test_pread();
  test_pthread_create();
  test_read();
  test_sched_getaffinity();
  test_select();
  test_sigaction();
  test_sigemptyset();
  test_socketpair();
  test_stat();
  test_strcasecmp();
  test_strchr();
  test_strcmp();
  test_strcpy();
  test_strdup();
  test_strlen();
  test_strncasecmp();
  test_strncmp();
  test_strncpy();
  test_strrchr();
  test_strstr();
  test_strtod();
  test_strtol();
  test_strtoll();
  test_strtoul();
  test_strtoull();
  test_time();
}
