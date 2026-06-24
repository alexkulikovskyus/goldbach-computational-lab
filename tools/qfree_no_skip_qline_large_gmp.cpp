#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <gmp.h>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

struct Options {
  uint32_t coefficient = 2;
  uint32_t base = 10;
  uint32_t exponent = 19;
  int64_t min_offset = 2;
  int64_t max_offset = 2000000000LL;
  int64_t center_offset = 50000;
  uint32_t a_limit = 50000;
  uint32_t p_limit = 250000;
  uint32_t q_filter_limit = 250000;
  uint32_t small_limit = 250000;
  uint32_t workers = 19;
  uint32_t primality_reps = 25;
  uint64_t baseline_sample_stride = 1000;
  uint32_t progress_every_lines = 25;
  std::string summary;
  std::string fallback_output;
};

struct QLine {
  uint32_t a = 0;
  int probable = 0;
};

struct FallbackRow {
  int64_t offset = 0;
  uint32_t p = 0;
  uint32_t q_tests = 0;
  bool verified = false;
};

static uint64_t parse_u64(const std::string &value, const std::string &name) {
  try {
    size_t pos = 0;
    uint64_t out = std::stoull(value, &pos);
    if (pos != value.size()) throw std::invalid_argument("bad");
    return out;
  } catch (...) {
    throw std::runtime_error("bad uint64 for " + name + ": " + value);
  }
}

static int64_t parse_i64(const std::string &value, const std::string &name) {
  try {
    size_t pos = 0;
    int64_t out = std::stoll(value, &pos);
    if (pos != value.size()) throw std::invalid_argument("bad");
    return out;
  } catch (...) {
    throw std::runtime_error("bad int64 for " + name + ": " + value);
  }
}

static Options parse_args(int argc, char **argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    const auto eq = arg.find('=');
    const std::string key = eq == std::string::npos ? arg : arg.substr(0, eq);
    const std::string value = eq == std::string::npos ? "" : arg.substr(eq + 1);
    if (key == "--coefficient") opt.coefficient = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--base") opt.base = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--exponent") opt.exponent = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--minOffset") opt.min_offset = parse_i64(value, key);
    else if (key == "--maxOffset") opt.max_offset = parse_i64(value, key);
    else if (key == "--centerOffset") opt.center_offset = parse_i64(value, key);
    else if (key == "--aLimit") opt.a_limit = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--pLimit") opt.p_limit = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--qFilterLimit") opt.q_filter_limit = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--smallLimit") opt.small_limit = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--workers") opt.workers = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--primalityReps") opt.primality_reps = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--baselineSampleStride") opt.baseline_sample_stride = parse_u64(value, key);
    else if (key == "--progressEveryLines") opt.progress_every_lines = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--summary") opt.summary = value;
    else if (key == "--fallbackOutput") opt.fallback_output = value;
    else throw std::runtime_error("unknown argument: " + arg);
  }
  if (opt.base < 2) throw std::runtime_error("--base must be >= 2");
  if (opt.min_offset > opt.max_offset) throw std::runtime_error("--minOffset > --maxOffset");
  if ((opt.min_offset % 2) != 0 || (opt.max_offset % 2) != 0 || (opt.center_offset % 2) != 0) {
    throw std::runtime_error("offsets must be even");
  }
  if (opt.a_limit == 0 || opt.p_limit == 0 || opt.workers == 0) {
    throw std::runtime_error("limits/workers must be > 0");
  }
  if (opt.summary.empty()) throw std::runtime_error("need --summary");
  return opt;
}

static std::vector<uint32_t> build_primes(uint32_t limit) {
  std::vector<uint8_t> flags(limit + 1, 1);
  flags[0] = 0;
  if (limit >= 1) flags[1] = 0;
  for (uint64_t n = 4; n <= limit; n += 2) flags[n] = 0;
  for (uint64_t n = 3; n * n <= limit; n += 2) {
    if (!flags[n]) continue;
    for (uint64_t m = n * n; m <= limit; m += n * 2) flags[m] = 0;
  }
  std::vector<uint32_t> primes;
  for (uint32_t n = 2; n <= limit; ++n) {
    if (flags[n]) primes.push_back(n);
  }
  return primes;
}

static inline void clear_bit(std::vector<uint64_t> &bits, uint64_t idx) {
  bits[idx >> 6] &= ~(1ULL << (idx & 63));
}

static inline bool test_bit(const std::vector<uint64_t> &bits, uint64_t idx) {
  return (bits[idx >> 6] >> (idx & 63)) & 1ULL;
}

static inline void set_bit(std::vector<uint64_t> &bits, uint64_t idx) {
  bits[idx >> 6] |= 1ULL << (idx & 63);
}

static std::vector<uint64_t> build_odd_prime_bits(uint64_t max_p) {
  if ((max_p & 1ULL) == 0) --max_p;
  const uint64_t bit_count = (max_p + 1) / 2;
  std::vector<uint64_t> bits((bit_count + 63) / 64, ~0ULL);
  clear_bit(bits, 0);  // 1 is not prime.
  const uint64_t extra_bits = bits.size() * 64 - bit_count;
  if (extra_bits > 0) bits.back() &= (~0ULL >> extra_bits);

  const uint64_t sqrt_max = static_cast<uint64_t>(std::sqrt(static_cast<long double>(max_p))) + 1;
  for (uint64_t p = 3; p <= sqrt_max; p += 2) {
    const uint64_t p_idx = (p - 1) / 2;
    if (!test_bit(bits, p_idx)) continue;
    for (uint64_t m = p * p; m <= max_p; m += 2 * p) {
      clear_bit(bits, (m - 1) / 2);
    }
  }
  return bits;
}

static inline uint64_t get_shifted_word(const std::vector<uint64_t> &src, uint64_t bit) {
  const uint64_t word = bit >> 6;
  const uint32_t shift = static_cast<uint32_t>(bit & 63);
  uint64_t out = word < src.size() ? (src[word] >> shift) : 0ULL;
  if (shift != 0 && word + 1 < src.size()) out |= src[word + 1] << (64 - shift);
  return out;
}

static void or_slice(
    const std::vector<uint64_t> &src,
    std::vector<uint64_t> &dst,
    uint64_t src_start,
    uint64_t dst_start,
    uint64_t len) {
  if (len == 0) return;
  const uint64_t dst_end = dst_start + len;
  const uint64_t first_word = dst_start >> 6;
  const uint64_t last_word = (dst_end - 1) >> 6;
  for (uint64_t dw = first_word; dw <= last_word; ++dw) {
    const uint64_t word_start = dw << 6;
    const uint64_t seg_start = std::max(dst_start, word_start);
    const uint64_t seg_end = std::min(dst_end, word_start + 64);
    const uint32_t offset = static_cast<uint32_t>(seg_start - word_start);
    const uint32_t nbits = static_cast<uint32_t>(seg_end - seg_start);
    uint64_t mask = nbits == 64 ? ~0ULL : ((1ULL << nbits) - 1ULL);
    mask <<= offset;
    const uint64_t src_bit = src_start + (seg_start - dst_start);
    const uint64_t val = get_shifted_word(src, src_bit) << offset;
    dst[dw] |= val & mask;
  }
}

static bool candidate_can_leave_prime_q(uint32_t n_mod_6, uint32_t p) {
  if (p == 3) return true;
  const uint32_t q_mod_6 = (n_mod_6 + 6 - (p % 6)) % 6;
  return q_mod_6 == 1 || q_mod_6 == 5;
}

static std::vector<uint8_t> build_small_wall(
    const std::vector<uint32_t> &primes,
    uint32_t small_limit,
    uint32_t p_limit,
    const mpz_t n) {
  std::vector<uint8_t> wall(p_limit + 1, 0);
  for (uint32_t prime : primes) {
    if (prime > small_limit) break;
    if (prime <= 3) continue;
    uint64_t first = static_cast<uint32_t>(mpz_fdiv_ui(n, prime));
    if (first < 2) {
      const uint64_t delta = 2 - first;
      first += ((delta + prime - 1) / prime) * prime;
    }
    for (uint64_t p = first; p <= p_limit; p += prime) wall[static_cast<size_t>(p)] = 1;
  }
  return wall;
}

static std::pair<bool, uint32_t> direct_search(
    const Options &opt,
    const std::vector<uint32_t> &primes,
    const mpz_t base_value,
    int64_t offset,
    uint32_t &q_tests) {
  mpz_t n;
  mpz_t q;
  mpz_init(n);
  mpz_init(q);
  mpz_set(n, base_value);
  if (offset >= 0) mpz_add_ui(n, n, static_cast<unsigned long>(offset));
  else mpz_sub_ui(n, n, static_cast<unsigned long>(-offset));
  const uint32_t n_mod_6 = static_cast<uint32_t>(mpz_fdiv_ui(n, 6));
  const auto wall = build_small_wall(primes, opt.small_limit, opt.p_limit, n);
  for (uint32_t p : primes) {
    if (p > opt.p_limit) break;
    if (!candidate_can_leave_prime_q(n_mod_6, p)) continue;
    if (wall[p]) continue;
    ++q_tests;
    mpz_sub_ui(q, n, p);
    if (mpz_probab_prime_p(q, opt.primality_reps) > 0) {
      mpz_clear(q);
      mpz_clear(n);
      return {true, p};
    }
  }
  mpz_clear(q);
  mpz_clear(n);
  return {false, 0};
}

int main(int argc, char **argv) {
  try {
    const auto started = std::chrono::steady_clock::now();
    const Options opt = parse_args(argc, argv);
    const uint64_t rows = static_cast<uint64_t>((opt.max_offset - opt.min_offset) / 2 + 1);
    const int64_t min_rel = opt.min_offset - opt.center_offset;
    const int64_t max_rel = opt.max_offset - opt.center_offset;
    if (max_rel + static_cast<int64_t>(opt.a_limit) < 3) {
      throw std::runtime_error("q-line p range is empty; move center or raise aLimit");
    }
    const uint64_t max_p = static_cast<uint64_t>(max_rel + static_cast<int64_t>(opt.a_limit));
    const uint32_t prime_limit = std::max<uint32_t>(
        opt.p_limit,
        std::max(opt.small_limit, opt.q_filter_limit));
    const auto primes = build_primes(prime_limit);

    mpz_t base_value;
    mpz_t center_value;
    mpz_init(base_value);
    mpz_init(center_value);
    mpz_ui_pow_ui(base_value, opt.base, opt.exponent);
    mpz_mul_ui(base_value, base_value, opt.coefficient);
    mpz_set(center_value, base_value);
    if (opt.center_offset >= 0) mpz_add_ui(center_value, center_value, static_cast<unsigned long>(opt.center_offset));
    else mpz_sub_ui(center_value, center_value, static_cast<unsigned long>(-opt.center_offset));

    std::cerr << "large run rows=" << rows
              << " offsets=" << opt.min_offset << ".." << opt.max_offset
              << " centerOffset=" << opt.center_offset
              << " maxP=" << max_p
              << " workers=" << opt.workers << "\n";

    const auto sieve_started = std::chrono::steady_clock::now();
    auto odd_prime_bits = build_odd_prime_bits(max_p);
    const auto sieve_ended = std::chrono::steady_clock::now();
    std::cerr << "prime-bit sieve seconds="
              << std::fixed << std::setprecision(2)
              << std::chrono::duration<double>(sieve_ended - sieve_started).count()
              << " bytes=" << (odd_prime_bits.size() * sizeof(uint64_t)) << "\n";

    std::vector<uint8_t> q_small_blocked(opt.a_limit + 1, 0);
    for (uint32_t r : primes) {
      if (r > opt.q_filter_limit) break;
      if (r <= 3) continue;
      const uint32_t rem = static_cast<uint32_t>(mpz_fdiv_ui(center_value, r));
      for (uint64_t a = rem == 0 ? r : rem; a <= opt.a_limit; a += r) {
        q_small_blocked[static_cast<size_t>(a)] = 1;
      }
    }

    std::vector<QLine> qlines;
    for (uint32_t a = 1; a <= opt.a_limit; a += 2) {
      if (!q_small_blocked[a]) qlines.push_back({a, 0});
    }
    std::atomic<size_t> next_q{0};
    const uint32_t q_workers = std::min<uint32_t>(opt.workers, std::max<size_t>(1, qlines.size()));
    std::vector<std::thread> q_threads;
    for (uint32_t w = 0; w < q_workers; ++w) {
      q_threads.emplace_back([&]() {
        mpz_t q;
        mpz_init(q);
        while (true) {
          const size_t i = next_q.fetch_add(1);
          if (i >= qlines.size()) break;
          mpz_sub_ui(q, center_value, qlines[i].a);
          qlines[i].probable = mpz_probab_prime_p(q, opt.primality_reps);
        }
        mpz_clear(q);
      });
    }
    for (auto &thread : q_threads) thread.join();

    std::vector<QLine> prime_qlines;
    for (const auto &line : qlines) {
      if (line.probable > 0) prime_qlines.push_back(line);
    }
    std::cerr << "qline stage qTests=" << qlines.size()
              << " primeLines=" << prime_qlines.size() << "\n";

    std::vector<uint64_t> covered((rows + 63) / 64, 0ULL);
    const auto count_covered_rows = [&]() {
      uint64_t total = 0;
      const uint64_t tail_bits = rows & 63ULL;
      for (size_t i = 0; i < covered.size(); ++i) {
        uint64_t word = covered[i];
        if (i + 1 == covered.size() && tail_bits != 0) {
          word &= ((1ULL << tail_bits) - 1ULL);
        }
        total += static_cast<uint64_t>(__builtin_popcountll(word));
      }
      return total;
    };
    const auto cover_started = std::chrono::steady_clock::now();
    for (size_t li = 0; li < prime_qlines.size(); ++li) {
      const int64_t p_start = static_cast<int64_t>(prime_qlines[li].a) + min_rel;
      int64_t src_start_signed = (p_start - 1) / 2;
      uint64_t dst_start = 0;
      uint64_t src_start = 0;
      if (src_start_signed < 1) {
        dst_start = static_cast<uint64_t>(1 - src_start_signed);
        src_start = 1;
      } else {
        src_start = static_cast<uint64_t>(src_start_signed);
      }
      if (dst_start < rows) {
        const uint64_t len = rows - dst_start;
        or_slice(odd_prime_bits, covered, src_start, dst_start, len);
      }
      if (opt.progress_every_lines > 0 && ((li + 1) % opt.progress_every_lines == 0 || li + 1 == prime_qlines.size())) {
        const uint64_t covered_count = count_covered_rows();
        std::cerr << "coverage progress lines=" << (li + 1) << "/" << prime_qlines.size()
                  << " covered=" << covered_count
                  << " uncovered=" << (rows - covered_count)
                  << "\n";
      }
    }
    const auto cover_ended = std::chrono::steady_clock::now();

    if ((rows & 63ULL) != 0) {
      covered.back() &= ((1ULL << (rows & 63ULL)) - 1ULL);
    }
    const uint64_t covered_rows = count_covered_rows();
    const uint64_t uncovered_rows = rows - covered_rows;
    std::cerr << "coverage done covered=" << covered_rows
              << " uncovered=" << uncovered_rows
              << " seconds=" << std::fixed << std::setprecision(2)
              << std::chrono::duration<double>(cover_ended - cover_started).count()
              << "\n";

    std::vector<int64_t> fallback_offsets;
    fallback_offsets.reserve(static_cast<size_t>(std::min<uint64_t>(uncovered_rows, 1000000)));
    for (uint64_t wi = 0; wi < covered.size(); ++wi) {
      uint64_t missing = ~covered[wi];
      if (wi + 1 == covered.size() && (rows & 63ULL) != 0) {
        missing &= ((1ULL << (rows & 63ULL)) - 1ULL);
      }
      while (missing) {
        const uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(missing));
        const uint64_t row = (wi << 6) + bit;
        if (row < rows) fallback_offsets.push_back(opt.min_offset + static_cast<int64_t>(2 * row));
        missing &= missing - 1;
      }
    }

    std::vector<uint64_t> sample_rows;
    if (opt.baseline_sample_stride > 0) {
      for (uint64_t row = 0; row < rows; row += opt.baseline_sample_stride) sample_rows.push_back(row);
    }
    std::atomic<size_t> next_sample{0};
    std::atomic<uint64_t> sample_qtests{0};
    std::atomic<uint64_t> sample_verified{0};
    const uint32_t sample_workers = std::min<uint32_t>(opt.workers, std::max<size_t>(1, sample_rows.size()));
    std::vector<std::thread> sample_threads;
    for (uint32_t w = 0; w < sample_workers; ++w) {
      sample_threads.emplace_back([&]() {
        while (true) {
          const size_t i = next_sample.fetch_add(1);
          if (i >= sample_rows.size()) break;
          const int64_t offset = opt.min_offset + static_cast<int64_t>(2 * sample_rows[i]);
          uint32_t q_tests = 0;
          auto found = direct_search(opt, primes, base_value, offset, q_tests);
          sample_qtests += q_tests;
          if (found.first) ++sample_verified;
        }
      });
    }
    for (auto &thread : sample_threads) thread.join();
    const double avg_baseline_qtests = sample_rows.empty() ? 0.0 :
        static_cast<double>(sample_qtests.load()) / static_cast<double>(sample_rows.size());
    const double estimated_baseline_qtests = avg_baseline_qtests * static_cast<double>(rows);

    std::vector<FallbackRow> fallback_results(fallback_offsets.size());
    std::atomic<size_t> next_fb{0};
    std::atomic<uint64_t> fallback_qtests{0};
    std::atomic<uint64_t> fallback_verified{0};
    const uint32_t fb_workers = std::min<uint32_t>(opt.workers, std::max<size_t>(1, fallback_offsets.size()));
    std::vector<std::thread> fb_threads;
    for (uint32_t w = 0; w < fb_workers; ++w) {
      fb_threads.emplace_back([&]() {
        while (true) {
          const size_t i = next_fb.fetch_add(1);
          if (i >= fallback_offsets.size()) break;
          uint32_t q_tests = 0;
          auto found = direct_search(opt, primes, base_value, fallback_offsets[i], q_tests);
          fallback_results[i] = {fallback_offsets[i], found.second, q_tests, found.first};
          fallback_qtests += q_tests;
          if (found.first) ++fallback_verified;
        }
      });
    }
    for (auto &thread : fb_threads) thread.join();

    uint64_t unresolved = 0;
    for (const auto &row : fallback_results) {
      if (!row.verified) ++unresolved;
    }

    if (!opt.fallback_output.empty()) {
      std::ofstream fb(opt.fallback_output);
      if (!fb) throw std::runtime_error("cannot write " + opt.fallback_output);
      fb << "offset\tp\tverified\tqTests\n";
      for (const auto &row : fallback_results) {
        fb << row.offset << '\t' << row.p << '\t' << (row.verified ? 1 : 0)
           << '\t' << row.q_tests << '\n';
      }
    }

    const uint64_t qline_qtests = qlines.size();
    const uint64_t no_skip_qtests = qline_qtests + fallback_qtests.load();
    const double saving = estimated_baseline_qtests == 0.0 ? 0.0 :
        100.0 * (1.0 - static_cast<double>(no_skip_qtests) / estimated_baseline_qtests);
    const double speedup = no_skip_qtests == 0 ? 0.0 :
        estimated_baseline_qtests / static_cast<double>(no_skip_qtests);
    const auto ended = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(ended - started).count();

    std::ofstream out(opt.summary);
    if (!out) throw std::runtime_error("cannot write " + opt.summary);
    out << "rows\tminOffset\tmaxOffset\tcenterOffset\taLimit\tpLimit\tqlineQTests"
        << "\tprimeLines\tqlineRows\tfallbackRows\tfallbackVerified\tunresolved"
        << "\tfallbackQTests\tnoSkipQTests\tbaselineSampleRows\tbaselineSampleQTests"
        << "\tavgBaselineQTests\testimatedBaselineQTests\tsavingPct\tspeedup"
        << "\tseconds\n";
    out << rows << '\t'
        << opt.min_offset << '\t'
        << opt.max_offset << '\t'
        << opt.center_offset << '\t'
        << opt.a_limit << '\t'
        << opt.p_limit << '\t'
        << qline_qtests << '\t'
        << prime_qlines.size() << '\t'
        << covered_rows << '\t'
        << fallback_offsets.size() << '\t'
        << fallback_verified.load() << '\t'
        << unresolved << '\t'
        << fallback_qtests.load() << '\t'
        << no_skip_qtests << '\t'
        << sample_rows.size() << '\t'
        << sample_qtests.load() << '\t'
        << std::fixed << std::setprecision(9) << avg_baseline_qtests << '\t'
        << std::fixed << std::setprecision(3) << estimated_baseline_qtests << '\t'
        << std::fixed << std::setprecision(6) << saving << '\t'
        << std::fixed << std::setprecision(6) << speedup << '\t'
        << std::fixed << std::setprecision(6) << seconds << '\n';

    std::cerr << "large summary"
              << " rows=" << rows
              << " qlineRows=" << covered_rows
              << " fallbackRows=" << fallback_offsets.size()
              << " unresolved=" << unresolved
              << " noSkipQTests=" << no_skip_qtests
              << " estBaselineQTests=" << std::fixed << std::setprecision(0) << estimated_baseline_qtests
              << " savingPct=" << std::fixed << std::setprecision(2) << saving
              << " speedup=" << std::fixed << std::setprecision(2) << speedup
              << " seconds=" << std::fixed << std::setprecision(2) << seconds
              << "\n";

    mpz_clear(center_value);
    mpz_clear(base_value);
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
