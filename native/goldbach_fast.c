#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

typedef struct {
  uint64_t n;
  uint32_t p;
  uint64_t q;
  uint32_t attempts;
  uint8_t n_mod6;
  const char *label;
} Hit;

typedef struct {
  uint64_t count;
  uint64_t failures;
  long double attempts_sum;
  long double p_sum;
  uint32_t attempts_max;
  uint32_t p_max;
  uint64_t *attempts_hist;
  uint64_t *p_hist;
} Stats;

typedef struct {
  uint32_t *values;
  size_t len;
  size_t cap;
} PrimeList;

#define WHEEL_MOD 210
#define WHEEL_BLOCK_BITS 6720
#define WHEEL_BLOCK_WORDS (WHEEL_BLOCK_BITS / 64)

static uint64_t wheel_blocks[WHEEL_MOD][WHEEL_BLOCK_WORDS];

typedef struct {
  int worker_index;
  uint64_t from;
  uint64_t to;
  uint64_t segment_width;
  uint32_t p_limit;
  const uint32_t *base_primes;
  size_t base_prime_count;
  const uint32_t *lane1;
  size_t lane1_count;
  const uint32_t *lane5;
  size_t lane5_count;
  size_t attempts_hist_len;
  int full_stats;
  Stats stats;
  Stats route_stats[3];
  Hit hardest[50];
  size_t hardest_count;
  Hit *records;
  size_t record_count;
  size_t record_cap;
  double seconds;
} Worker;

static double now_seconds(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static uint64_t parse_u64_arg(const char *arg, const char *prefix, uint64_t current) {
  size_t len = strlen(prefix);
  if (strncmp(arg, prefix, len) == 0) {
    errno = 0;
    char *end = NULL;
    uint64_t value = strtoull(arg + len, &end, 10);
    if (errno || !end || *end) {
      fprintf(stderr, "invalid integer argument: %s\n", arg);
      exit(2);
    }
    return value;
  }
  return current;
}

static const char *parse_str_arg(const char *arg, const char *prefix, const char *current) {
  size_t len = strlen(prefix);
  if (strncmp(arg, prefix, len) == 0) return arg + len;
  return current;
}

static void prime_list_push(PrimeList *list, uint32_t value) {
  if (list->len == list->cap) {
    size_t next = list->cap ? list->cap * 2 : 1024;
    uint32_t *values = (uint32_t *)realloc(list->values, next * sizeof(uint32_t));
    if (!values) {
      fprintf(stderr, "out of memory growing prime list\n");
      exit(1);
    }
    list->values = values;
    list->cap = next;
  }
  list->values[list->len++] = value;
}

static uint8_t *build_small_sieve(uint32_t limit) {
  uint8_t *flags = (uint8_t *)malloc((size_t)limit + 1);
  if (!flags) {
    fprintf(stderr, "out of memory building small sieve\n");
    exit(1);
  }
  memset(flags, 1, (size_t)limit + 1);
  if (limit >= 0) flags[0] = 0;
  if (limit >= 1) flags[1] = 0;
  for (uint32_t n = 4; n <= limit; n += 2) flags[n] = 0;
  for (uint64_t n = 3; n * n <= limit; n += 2) {
    if (!flags[n]) continue;
    for (uint64_t composite = n * n; composite <= limit; composite += n * 2) {
      flags[composite] = 0;
    }
  }
  return flags;
}

static PrimeList collect_primes(const uint8_t *flags, uint32_t limit, int residue_filter) {
  PrimeList list = {0};
  for (uint32_t n = 2; n <= limit; n++) {
    if (!flags[n]) continue;
    if (residue_filter >= 0 && (n <= 3 || (int)(n % 6) != residue_filter)) continue;
    prime_list_push(&list, n);
  }
  return list;
}

static inline void bit_clear(uint64_t *bits, uint64_t index) {
  bits[index >> 6] &= ~(UINT64_C(1) << (index & 63));
}

static inline bool bit_test(const uint64_t *bits, uint64_t index) {
  return (bits[index >> 6] >> (index & 63)) & UINT64_C(1);
}

static inline void bit_set(uint64_t *bits, uint64_t index) {
  bits[index >> 6] |= UINT64_C(1) << (index & 63);
}

static void init_wheel_blocks(void) {
  for (uint32_t start_mod = 0; start_mod < WHEEL_MOD; start_mod++) {
    memset(wheel_blocks[start_mod], 0, sizeof(wheel_blocks[start_mod]));
    for (uint32_t bit = 0; bit < WHEEL_BLOCK_BITS; bit++) {
      uint32_t residue = (start_mod + 2 * bit) % WHEEL_MOD;
      if (residue % 3 != 0 && residue % 5 != 0 && residue % 7 != 0) {
        wheel_blocks[start_mod][bit >> 6] |= UINT64_C(1) << (bit & 63);
      }
    }
  }
}

static uint64_t odd_ceil(uint64_t value) {
  return value | UINT64_C(1);
}

static uint64_t odd_floor(uint64_t value) {
  return value % 2 ? value : value - 1;
}

static uint64_t div_ceil_u64(uint64_t a, uint64_t b) {
  return a / b + (a % b != 0);
}

static uint64_t *build_q_segment_bits(uint64_t q_from_raw, uint64_t q_to_raw, const uint32_t *base_primes, size_t base_count, uint64_t *odd_from_out, uint64_t *odd_count_out) {
  uint64_t odd_from = odd_ceil(q_from_raw <= 3 ? 3 : q_from_raw);
  uint64_t odd_to = odd_floor(q_to_raw < 3 ? 3 : q_to_raw);
  if (odd_to < odd_from) odd_to = odd_from;
  uint64_t odd_count = ((odd_to - odd_from) / 2) + 1;
  uint64_t word_count = (odd_count + 63) / 64;
  uint64_t *bits = (uint64_t *)malloc((size_t)word_count * sizeof(uint64_t));
  if (!bits) {
    fprintf(stderr, "out of memory for q segment (%" PRIu64 " odds)\n", odd_count);
    exit(1);
  }
  uint32_t wheel_start = (uint32_t)(odd_from % WHEEL_MOD);
  uint64_t full_blocks = word_count / WHEEL_BLOCK_WORDS;
  uint64_t remainder_words = word_count % WHEEL_BLOCK_WORDS;
  for (uint64_t block = 0; block < full_blocks; block++) {
    memcpy(bits + block * WHEEL_BLOCK_WORDS, wheel_blocks[wheel_start], WHEEL_BLOCK_WORDS * sizeof(uint64_t));
  }
  if (remainder_words) {
    memcpy(bits + full_blocks * WHEEL_BLOCK_WORDS, wheel_blocks[wheel_start], (size_t)remainder_words * sizeof(uint64_t));
  }
  uint64_t extra = word_count * 64 - odd_count;
  if (extra) bits[word_count - 1] &= (~UINT64_C(0) >> extra);

  const uint64_t small_primes[] = {3, 5, 7};
  for (size_t i = 0; i < sizeof(small_primes) / sizeof(small_primes[0]); i++) {
    uint64_t prime = small_primes[i];
    if (prime >= odd_from && prime <= odd_to) bit_set(bits, (prime - odd_from) / 2);
  }

  for (size_t i = 0; i < base_count; i++) {
    uint64_t prime = base_primes[i];
    if (prime <= 7) continue;
    uint64_t square = prime * prime;
    if (square > odd_to) break;
    uint64_t start = square > odd_from ? square : div_ceil_u64(odd_from, prime) * prime;
    if ((start & 1) == 0) start += prime;
    for (uint64_t composite = start; composite <= odd_to; composite += prime * 2) {
      bit_clear(bits, (composite - odd_from) / 2);
    }
  }

  *odd_from_out = odd_from;
  *odd_count_out = odd_count;
  return bits;
}

static void stats_init(Stats *stats, size_t attempts_hist_len, uint32_t p_limit) {
  memset(stats, 0, sizeof(*stats));
  stats->attempts_hist = (uint64_t *)calloc(attempts_hist_len, sizeof(uint64_t));
  stats->p_hist = (uint64_t *)calloc((size_t)p_limit + 1, sizeof(uint64_t));
  if (!stats->attempts_hist || !stats->p_hist) {
    fprintf(stderr, "out of memory for stats\n");
    exit(1);
  }
}

static void stats_free(Stats *stats) {
  free(stats->attempts_hist);
  free(stats->p_hist);
}

static void stats_add(Stats *stats, const Hit *hit, size_t attempts_hist_len, uint32_t p_limit) {
  stats->count++;
  if (!hit->p) {
    stats->failures++;
    return;
  }
  stats->attempts_sum += hit->attempts;
  stats->p_sum += hit->p;
  if (hit->attempts > stats->attempts_max) stats->attempts_max = hit->attempts;
  if (hit->p > stats->p_max) stats->p_max = hit->p;
  if (hit->attempts < attempts_hist_len) stats->attempts_hist[hit->attempts]++;
  if (hit->p <= p_limit) stats->p_hist[hit->p]++;
}

static void stats_merge(Stats *target, const Stats *source, size_t attempts_hist_len, uint32_t p_limit) {
  target->count += source->count;
  target->failures += source->failures;
  target->attempts_sum += source->attempts_sum;
  target->p_sum += source->p_sum;
  if (source->attempts_max > target->attempts_max) target->attempts_max = source->attempts_max;
  if (source->p_max > target->p_max) target->p_max = source->p_max;
  for (size_t i = 0; i < attempts_hist_len; i++) target->attempts_hist[i] += source->attempts_hist[i];
  for (uint32_t p = 0; p <= p_limit; p++) target->p_hist[p] += source->p_hist[p];
}

static uint32_t percentile_from_hist(const uint64_t *hist, size_t len, uint64_t total, double percentile) {
  if (!total) return 0;
  uint64_t target = (uint64_t)ceil((long double)total * percentile);
  uint64_t seen = 0;
  for (size_t i = 0; i < len; i++) {
    seen += hist[i];
    if (seen >= target) return (uint32_t)i;
  }
  return (uint32_t)(len - 1);
}

static int route_index(uint64_t n) {
  uint64_t residue = n % 6;
  if (residue == 0) return 0;
  if (residue == 2) return 1;
  return 2;
}

static const char *route_label_from_index(int index) {
  if (index == 0) return "0 mod 6";
  if (index == 1) return "1+1";
  return "5+5";
}

static const char *hit_label(uint32_t p, uint64_t q) {
  uint32_t pm = p % 6;
  uint32_t qm = (uint32_t)(q % 6);
  if (p == 3) return "3+q";
  if (pm == 1 && qm == 5) return "1+5";
  if (pm == 5 && qm == 1) return "5+1";
  if (pm == 1 && qm == 1) return "1+1";
  if (pm == 5 && qm == 5) return "5+5";
  return "other";
}

static bool q_is_prime_in_segment(uint64_t q, uint64_t odd_from, uint64_t odd_count, const uint64_t *q_bits) {
  if (q < odd_from) return false;
  uint64_t index = (q - odd_from) / 2;
  if (index >= odd_count) return false;
  return bit_test(q_bits, index);
}

static Hit first_witness(uint64_t n, uint64_t odd_from, uint64_t odd_count, const uint64_t *q_bits, const uint32_t *lane1, size_t lane1_count, const uint32_t *lane5, size_t lane5_count) {
  Hit hit = {0};
  hit.n = n;
  hit.n_mod6 = (uint8_t)(n % 6);
  int rindex = route_index(n);
  hit.label = route_label_from_index(rindex);

  if (n == 4) {
    hit.p = 2;
    hit.q = 2;
    hit.attempts = 1;
    hit.label = "2+2";
    return hit;
  }

  if (n >= 6) {
    hit.attempts++;
    uint64_t q = n - 3;
    if (q_is_prime_in_segment(q, odd_from, odd_count, q_bits)) {
      hit.p = 3;
      hit.q = q;
      hit.label = hit_label(hit.p, hit.q);
      return hit;
    }
  }

  size_t i1 = 0;
  size_t i5 = 0;
  while (i1 < lane1_count || i5 < lane5_count) {
    uint32_t p = 0;
    if (rindex == 1) {
      if (i1 >= lane1_count) break;
      p = lane1[i1++];
    } else if (rindex == 2) {
      if (i5 >= lane5_count) break;
      p = lane5[i5++];
    } else if (i5 >= lane5_count || (i1 < lane1_count && lane1[i1] < lane5[i5])) {
      p = lane1[i1++];
    } else {
      p = lane5[i5++];
    }
    if ((uint64_t)p > n / 2) break;
    hit.attempts++;
    uint64_t q = n - p;
    if (q_is_prime_in_segment(q, odd_from, odd_count, q_bits)) {
      hit.p = p;
      hit.q = q;
      hit.label = hit_label(hit.p, hit.q);
      return hit;
    }
  }
  return hit;
}

static void hardest_insert(Worker *worker, Hit hit) {
  if (!hit.p) return;
  if (worker->hardest_count < 50) {
    worker->hardest[worker->hardest_count++] = hit;
  } else {
    Hit *last = &worker->hardest[49];
    if (hit.attempts < last->attempts || (hit.attempts == last->attempts && hit.p <= last->p)) return;
    *last = hit;
  }
  for (size_t i = worker->hardest_count - 1; i > 0; i--) {
    Hit *left = &worker->hardest[i - 1];
    Hit *right = &worker->hardest[i];
    bool swap = right->attempts > left->attempts ||
      (right->attempts == left->attempts && right->p > left->p) ||
      (right->attempts == left->attempts && right->p == left->p && right->n > left->n);
    if (!swap) break;
    Hit tmp = *left;
    *left = *right;
    *right = tmp;
  }
}

static void record_push(Worker *worker, Hit hit) {
  if (worker->record_count == worker->record_cap) {
    size_t next = worker->record_cap ? worker->record_cap * 2 : 128;
    Hit *records = (Hit *)realloc(worker->records, next * sizeof(Hit));
    if (!records) {
      fprintf(stderr, "out of memory growing records\n");
      exit(1);
    }
    worker->records = records;
    worker->record_cap = next;
  }
  worker->records[worker->record_count++] = hit;
}

static void *worker_main(void *arg) {
  Worker *worker = (Worker *)arg;
  double started = now_seconds();
  stats_init(&worker->stats, worker->attempts_hist_len, worker->p_limit);
  if (worker->full_stats) {
    for (int i = 0; i < 3; i++) stats_init(&worker->route_stats[i], worker->attempts_hist_len, worker->p_limit);
  }

  uint32_t record_p = 0;
  for (uint64_t segment_from = worker->from; segment_from <= worker->to; ) {
    uint64_t segment_to = segment_from + worker->segment_width - 2;
    if (segment_to > worker->to || segment_to < segment_from) segment_to = worker->to;
    if (segment_to % 2) segment_to--;
    uint64_t odd_from = 0;
    uint64_t odd_count = 0;
    uint64_t q_from = segment_from > worker->p_limit ? segment_from - worker->p_limit : 0;
    uint64_t q_to = segment_to > 3 ? segment_to - 3 : 0;
    uint64_t *q_bits = build_q_segment_bits(q_from, q_to, worker->base_primes, worker->base_prime_count, &odd_from, &odd_count);

    for (uint64_t n = segment_from; n <= segment_to; n += 2) {
      Hit hit = first_witness(n, odd_from, odd_count, q_bits, worker->lane1, worker->lane1_count, worker->lane5, worker->lane5_count);
      stats_add(&worker->stats, &hit, worker->attempts_hist_len, worker->p_limit);
      if (worker->full_stats) stats_add(&worker->route_stats[route_index(n)], &hit, worker->attempts_hist_len, worker->p_limit);
      if (hit.p && (worker->hardest_count < 50 ||
        hit.attempts > worker->hardest[49].attempts ||
        (hit.attempts == worker->hardest[49].attempts && hit.p > worker->hardest[49].p))) {
        hardest_insert(worker, hit);
      }
      if (hit.p && hit.p > record_p) {
        record_p = hit.p;
        record_push(worker, hit);
      }
    }

    free(q_bits);
    if (segment_to == worker->to) break;
    segment_from = segment_to + 2;
  }

  worker->seconds = now_seconds() - started;
  return NULL;
}

static Hit *merge_hardest(Worker *workers, int thread_count, size_t *out_count) {
  Hit *hardest = (Hit *)calloc(50, sizeof(Hit));
  size_t count = 0;
  for (int w = 0; w < thread_count; w++) {
    for (size_t i = 0; i < workers[w].hardest_count; i++) {
      if (count < 50) {
        hardest[count++] = workers[w].hardest[i];
      } else {
        Hit *last = &hardest[49];
        Hit hit = workers[w].hardest[i];
        if (hit.attempts < last->attempts || (hit.attempts == last->attempts && hit.p <= last->p)) continue;
        *last = hit;
      }
      for (size_t j = count - 1; j > 0; j--) {
        Hit *left = &hardest[j - 1];
        Hit *right = &hardest[j];
        bool swap = right->attempts > left->attempts ||
          (right->attempts == left->attempts && right->p > left->p) ||
          (right->attempts == left->attempts && right->p == left->p && right->n > left->n);
        if (!swap) break;
        Hit tmp = *left;
        *left = *right;
        *right = tmp;
      }
    }
  }
  *out_count = count;
  return hardest;
}

static Hit *merge_records(Worker *workers, int thread_count, size_t *out_count) {
  Hit *records = NULL;
  size_t count = 0;
  size_t cap = 0;
  uint32_t record_p = 0;
  for (int w = 0; w < thread_count; w++) {
    for (size_t i = 0; i < workers[w].record_count; i++) {
      Hit hit = workers[w].records[i];
      if (hit.p <= record_p) continue;
      record_p = hit.p;
      if (count == cap) {
        cap = cap ? cap * 2 : 128;
        records = (Hit *)realloc(records, cap * sizeof(Hit));
        if (!records) {
          fprintf(stderr, "out of memory merging records\n");
          exit(1);
        }
      }
      records[count++] = hit;
    }
  }
  *out_count = count;
  return records;
}

static void print_stats_json(FILE *out, const Stats *stats, size_t attempts_hist_len, uint32_t p_limit, int indent) {
  uint64_t successes = stats->count - stats->failures;
  uint32_t median_attempts = percentile_from_hist(stats->attempts_hist, attempts_hist_len, successes, 0.5);
  uint32_t q99_attempts = percentile_from_hist(stats->attempts_hist, attempts_hist_len, successes, 0.99);
  uint32_t median_p = percentile_from_hist(stats->p_hist, (size_t)p_limit + 1, successes, 0.5);
  uint32_t q99_p = percentile_from_hist(stats->p_hist, (size_t)p_limit + 1, successes, 0.99);
  const char *sp = indent ? "    " : "";
  fprintf(out, "%s\"count\": %" PRIu64 ",\n", sp, stats->count);
  fprintf(out, "%s\"failures\": %" PRIu64 ",\n", sp, stats->failures);
  fprintf(out, "%s\"attempts\": {\"mean\": %.6Lf, \"median\": %" PRIu32 ", \"q99\": %" PRIu32 ", \"max\": %" PRIu32 "},\n",
    sp, successes ? stats->attempts_sum / successes : 0.0L, median_attempts, q99_attempts, stats->attempts_max);
  fprintf(out, "%s\"p\": {\"mean\": %.6Lf, \"median\": %" PRIu32 ", \"q99\": %" PRIu32 ", \"max\": %" PRIu32 "}",
    sp, successes ? stats->p_sum / successes : 0.0L, median_p, q99_p, stats->p_max);
}

static void write_json(const char *path, uint64_t from, uint64_t to, uint64_t segment_width, uint32_t p_limit, int thread_count, int full_stats, double seconds, const Stats *summary, const Stats route_stats[3], const Hit *hardest, size_t hardest_count, const Hit *records, size_t record_count, size_t attempts_hist_len) {
  FILE *out = fopen(path, "w");
  if (!out) {
    fprintf(stderr, "cannot write %s\n", path);
    exit(1);
  }
  fprintf(out, "{\n");
  fprintf(out, "  \"range\": {\"from\": %" PRIu64 ", \"to\": %" PRIu64 "},\n", from, to);
  fprintf(out, "  \"seconds\": %.3f,\n", seconds);
  fprintf(out, "  \"config\": {\"threads\": %d, \"segmentWidth\": %" PRIu64 ", \"pLimit\": %" PRIu32 ", \"fullStats\": %s, \"engine\": \"native-c-bitset-pthread-wheel210\"},\n", thread_count, segment_width, p_limit, full_stats ? "true" : "false");
  fprintf(out, "  \"summary\": {\n");
  print_stats_json(out, summary, attempts_hist_len, p_limit, 1);
  fprintf(out, "\n  },\n");
  if (full_stats) {
    fprintf(out, "  \"byRoute\": {\n");
    for (int i = 0; i < 3; i++) {
      fprintf(out, "    \"%s\": {\n", route_label_from_index(i));
      print_stats_json(out, &route_stats[i], attempts_hist_len, p_limit, 1);
      fprintf(out, "\n    }%s\n", i == 2 ? "" : ",");
    }
    fprintf(out, "  },\n");
  } else {
    fprintf(out, "  \"byRoute\": null,\n");
  }
  fprintf(out, "  \"records\": [\n");
  for (size_t i = 0; i < record_count; i++) {
    fprintf(out, "    {\"index\": %zu, \"n\": %" PRIu64 ", \"label\": \"%s\", \"p\": %" PRIu32 ", \"q\": %" PRIu64 ", \"attempts\": %" PRIu32 "}%s\n",
      i + 1, records[i].n, records[i].label, records[i].p, records[i].q, records[i].attempts, i + 1 == record_count ? "" : ",");
  }
  fprintf(out, "  ],\n");
  fprintf(out, "  \"hardest\": [\n");
  for (size_t i = 0; i < hardest_count; i++) {
    fprintf(out, "    {\"n\": %" PRIu64 ", \"label\": \"%s\", \"p\": %" PRIu32 ", \"q\": %" PRIu64 ", \"attempts\": %" PRIu32 "}%s\n",
      hardest[i].n, hardest[i].label, hardest[i].p, hardest[i].q, hardest[i].attempts, i + 1 == hardest_count ? "" : ",");
  }
  fprintf(out, "  ]\n");
  fprintf(out, "}\n");
  fclose(out);
}

int main(int argc, char **argv) {
  uint64_t from = 14;
  uint64_t to = 100000000;
  uint64_t segment_width = 5000000;
  uint64_t threads_arg = 18;
  uint64_t p_limit_arg = 100000;
  uint64_t fast_stats_arg = 0;
  const char *out_path = "data/goldbach-fast-result.json";

  for (int i = 1; i < argc; i++) {
    uint64_t before = from;
    from = parse_u64_arg(argv[i], "--from=", from);
    if (from != before) continue;
    before = to;
    to = parse_u64_arg(argv[i], "--to=", to);
    if (to != before) continue;
    before = segment_width;
    segment_width = parse_u64_arg(argv[i], "--segmentWidth=", segment_width);
    if (segment_width != before) continue;
    before = threads_arg;
    threads_arg = parse_u64_arg(argv[i], "--threads=", threads_arg);
    if (threads_arg != before) continue;
    before = p_limit_arg;
    p_limit_arg = parse_u64_arg(argv[i], "--pLimit=", p_limit_arg);
    if (p_limit_arg != before) continue;
    before = fast_stats_arg;
    fast_stats_arg = parse_u64_arg(argv[i], "--fastStats=", fast_stats_arg);
    if (fast_stats_arg != before) continue;
    const char *old_out = out_path;
    out_path = parse_str_arg(argv[i], "--out=", out_path);
    if (out_path != old_out) continue;
  }

  if (from % 2) from++;
  if (to % 2) to--;
  if (to < from) {
    fprintf(stderr, "empty even range\n");
    return 2;
  }
  if (p_limit_arg > UINT32_MAX) {
    fprintf(stderr, "pLimit too large\n");
    return 2;
  }
  uint32_t p_limit = (uint32_t)p_limit_arg;
  int full_stats = fast_stats_arg ? 0 : 1;
  int thread_count = (int)(threads_arg < 1 ? 1 : threads_arg);
  if (thread_count > 128) thread_count = 128;
  if (segment_width < 2) segment_width = 2;
  if (segment_width % 2) segment_width++;

  double started = now_seconds();
  init_wheel_blocks();
  uint32_t sqrt_limit = (uint32_t)floor(sqrt((long double)to)) + 1;
  uint32_t small_limit = sqrt_limit > p_limit ? sqrt_limit : p_limit;
  uint8_t *small_flags = build_small_sieve(small_limit);
  PrimeList base = collect_primes(small_flags, sqrt_limit, -1);
  PrimeList lane1 = collect_primes(small_flags, p_limit, 1);
  PrimeList lane5 = collect_primes(small_flags, p_limit, 5);
  free(small_flags);

  size_t attempts_hist_len = lane1.len + lane5.len + 3;
  uint64_t total_evens = ((to - from) / 2) + 1;
  if ((uint64_t)thread_count > total_evens) thread_count = (int)total_evens;
  Worker *workers = (Worker *)calloc((size_t)thread_count, sizeof(Worker));
  pthread_t *threads = (pthread_t *)calloc((size_t)thread_count, sizeof(pthread_t));
  if (!workers || !threads) {
    fprintf(stderr, "out of memory for workers\n");
    return 1;
  }

  fprintf(stderr, "native scan %" PRIu64 "..%" PRIu64 ", threads=%d, segmentWidth=%" PRIu64 ", p<=%" PRIu32 ", basePrimes=%zu\n",
    from, to, thread_count, segment_width, p_limit, base.len);

  uint64_t offset = 0;
  for (int i = 0; i < thread_count; i++) {
    uint64_t size = total_evens / (uint64_t)thread_count + ((uint64_t)i < total_evens % (uint64_t)thread_count ? 1 : 0);
    uint64_t worker_from = from + offset * 2;
    uint64_t worker_to = worker_from + (size - 1) * 2;
    offset += size;
    workers[i].worker_index = i + 1;
    workers[i].from = worker_from;
    workers[i].to = worker_to;
    workers[i].segment_width = segment_width;
    workers[i].p_limit = p_limit;
    workers[i].base_primes = base.values;
    workers[i].base_prime_count = base.len;
    workers[i].lane1 = lane1.values;
    workers[i].lane1_count = lane1.len;
    workers[i].lane5 = lane5.values;
    workers[i].lane5_count = lane5.len;
    workers[i].attempts_hist_len = attempts_hist_len;
    workers[i].full_stats = full_stats;
    int rc = pthread_create(&threads[i], NULL, worker_main, &workers[i]);
    if (rc != 0) {
      fprintf(stderr, "pthread_create failed: %s\n", strerror(rc));
      return 1;
    }
  }

  for (int i = 0; i < thread_count; i++) {
    int rc = pthread_join(threads[i], NULL);
    if (rc != 0) {
      fprintf(stderr, "pthread_join failed: %s\n", strerror(rc));
      return 1;
    }
  }

  Stats summary;
  Stats routes[3];
  stats_init(&summary, attempts_hist_len, p_limit);
  if (full_stats) {
    for (int i = 0; i < 3; i++) stats_init(&routes[i], attempts_hist_len, p_limit);
  } else {
    memset(routes, 0, sizeof(routes));
  }
  for (int i = 0; i < thread_count; i++) {
    stats_merge(&summary, &workers[i].stats, attempts_hist_len, p_limit);
    if (full_stats) {
      for (int route = 0; route < 3; route++) stats_merge(&routes[route], &workers[i].route_stats[route], attempts_hist_len, p_limit);
    }
  }
  size_t hardest_count = 0;
  Hit *hardest = merge_hardest(workers, thread_count, &hardest_count);
  size_t record_count = 0;
  Hit *records = merge_records(workers, thread_count, &record_count);
  double seconds = now_seconds() - started;
  write_json(out_path, from, to, segment_width, p_limit, thread_count, full_stats, seconds, &summary, routes, hardest, hardest_count, records, record_count, attempts_hist_len);

  uint64_t successes = summary.count - summary.failures;
  uint32_t median_attempts = percentile_from_hist(summary.attempts_hist, attempts_hist_len, successes, 0.5);
  uint32_t q99_attempts = percentile_from_hist(summary.attempts_hist, attempts_hist_len, successes, 0.99);
  printf("{\"outJson\":\"%s\",\"seconds\":%.3f,\"count\":%" PRIu64 ",\"failures\":%" PRIu64 ",\"meanAttempts\":%.6Lf,\"medianAttempts\":%" PRIu32 ",\"q99Attempts\":%" PRIu32 ",\"maxP\":%" PRIu32 ",\"recordCount\":%zu",
    out_path, seconds, summary.count, summary.failures, successes ? summary.attempts_sum / successes : 0.0L, median_attempts, q99_attempts, summary.p_max, record_count);
  if (hardest_count) {
    printf(",\"hardest\":{\"n\":%" PRIu64 ",\"label\":\"%s\",\"p\":%" PRIu32 ",\"q\":%" PRIu64 ",\"attempts\":%" PRIu32 "}",
      hardest[0].n, hardest[0].label, hardest[0].p, hardest[0].q, hardest[0].attempts);
  }
  printf("}\n");

  for (int i = 0; i < thread_count; i++) {
    stats_free(&workers[i].stats);
    if (full_stats) {
      for (int route = 0; route < 3; route++) stats_free(&workers[i].route_stats[route]);
    }
    free(workers[i].records);
  }
  stats_free(&summary);
  if (full_stats) {
    for (int route = 0; route < 3; route++) stats_free(&routes[route]);
  }
  free(hardest);
  free(records);
  free(workers);
  free(threads);
  free(base.values);
  free(lane1.values);
  free(lane5.values);
  return 0;
}
