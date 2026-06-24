#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <gmp.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

struct Options {
  uint32_t coefficient = 2;
  uint32_t base = 10;
  uint32_t exponent = 5000;
  std::string base_decimal;
  int64_t min_offset = -100;
  int64_t max_offset = 100;
  int64_t step = 2;
  uint32_t a_limit = 50000;
  uint32_t q_filter_limit = 250000;
  uint32_t top = 500;
  uint32_t q_test_top = 500;
  uint32_t primality_reps = 25;
  uint32_t workers = 1;
  std::string sort_mode = "hit";
  std::string output;
};

struct Line {
  uint32_t a = 0;
  uint32_t hit_count = 0;
  uint32_t max_p = 0;
  int64_t max_offset = 0;
  bool q_small_blocked = false;
  bool q_tested = false;
  int q_probable = 0;
  std::vector<int64_t> offsets;
  std::vector<uint32_t> ps;
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
    else if (key == "--baseDecimal") opt.base_decimal = value;
    else if (key == "--minOffset") opt.min_offset = parse_i64(value, key);
    else if (key == "--maxOffset") opt.max_offset = parse_i64(value, key);
    else if (key == "--step") opt.step = parse_i64(value, key);
    else if (key == "--aLimit") opt.a_limit = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--qFilterLimit") opt.q_filter_limit = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--top") opt.top = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--qTestTop") opt.q_test_top = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--primalityReps") opt.primality_reps = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--workers") opt.workers = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--sort") opt.sort_mode = value;
    else if (key == "--output") opt.output = value;
    else throw std::runtime_error("unknown argument: " + arg);
  }
  if (opt.base < 2) throw std::runtime_error("--base must be >= 2");
  if (opt.min_offset > opt.max_offset) throw std::runtime_error("--minOffset > --maxOffset");
  if (opt.step <= 0) throw std::runtime_error("--step must be > 0");
  if (opt.a_limit == 0) throw std::runtime_error("--aLimit must be > 0");
  if (opt.workers == 0) opt.workers = 1;
  if (opt.sort_mode != "hit" && opt.sort_mode != "maxP" && opt.sort_mode != "aDesc") {
    throw std::runtime_error("--sort must be hit, maxP, or aDesc");
  }
  if (opt.output.empty()) throw std::runtime_error("need --output");
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

static std::vector<uint8_t> build_prime_flags(uint32_t limit) {
  std::vector<uint8_t> flags(limit + 1, 1);
  flags[0] = 0;
  if (limit >= 1) flags[1] = 0;
  for (uint64_t n = 4; n <= limit; n += 2) flags[n] = 0;
  for (uint64_t n = 3; n * n <= limit; n += 2) {
    if (!flags[n]) continue;
    for (uint64_t m = n * n; m <= limit; m += n * 2) flags[m] = 0;
  }
  return flags;
}

static std::string join_i64(const std::vector<int64_t> &values) {
  std::ostringstream out;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << values[i];
  }
  return out.str();
}

static std::string join_u32(const std::vector<uint32_t> &values) {
  std::ostringstream out;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << values[i];
  }
  return out.str();
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const uint32_t p_limit = static_cast<uint32_t>(
        std::max<int64_t>(2, static_cast<int64_t>(opt.a_limit) + opt.max_offset));
    const auto p_is_prime = build_prime_flags(p_limit);
    const auto primes = build_primes(std::max(opt.q_filter_limit, p_limit));

    mpz_t base_value;
    mpz_init(base_value);
    if (!opt.base_decimal.empty()) {
      if (mpz_set_str(base_value, opt.base_decimal.c_str(), 10) != 0) {
        throw std::runtime_error("bad --baseDecimal");
      }
    } else {
      mpz_ui_pow_ui(base_value, opt.base, opt.exponent);
      mpz_mul_ui(base_value, base_value, opt.coefficient);
    }

    std::vector<uint8_t> q_small_blocked(opt.a_limit + 1, 0);
    for (uint32_t r : primes) {
      if (r > opt.q_filter_limit) break;
      if (r <= 3) continue;
      const uint32_t rem = static_cast<uint32_t>(mpz_fdiv_ui(base_value, r));
      for (uint64_t a = rem == 0 ? r : rem; a <= opt.a_limit; a += r) {
        q_small_blocked[static_cast<size_t>(a)] = 1;
      }
    }

    std::vector<Line> lines;
    lines.reserve(opt.a_limit / 2);
    for (uint32_t a = 1; a <= opt.a_limit; a += 2) {
      Line line;
      line.a = a;
      line.q_small_blocked = q_small_blocked[a] != 0;
      for (int64_t offset = opt.min_offset; offset <= opt.max_offset; offset += opt.step) {
        const int64_t p64 = static_cast<int64_t>(a) + offset;
        if (p64 < 2 || p64 > static_cast<int64_t>(p_limit)) continue;
        const uint32_t p = static_cast<uint32_t>(p64);
        if (!p_is_prime[p]) continue;
        ++line.hit_count;
        line.offsets.push_back(offset);
        line.ps.push_back(p);
        if (p > line.max_p) {
          line.max_p = p;
          line.max_offset = offset;
        }
      }
      if (line.hit_count > 0) lines.push_back(std::move(line));
    }

    if (opt.sort_mode == "maxP") {
      std::sort(lines.begin(), lines.end(), [](const Line &a, const Line &b) {
        if (a.max_p != b.max_p) return a.max_p > b.max_p;
        if (a.hit_count != b.hit_count) return a.hit_count > b.hit_count;
        return a.a > b.a;
      });
    } else if (opt.sort_mode == "aDesc") {
      std::sort(lines.begin(), lines.end(), [](const Line &a, const Line &b) {
        if (a.a != b.a) return a.a > b.a;
        return a.hit_count > b.hit_count;
      });
    } else {
      std::sort(lines.begin(), lines.end(), [](const Line &a, const Line &b) {
        if (a.hit_count != b.hit_count) return a.hit_count > b.hit_count;
        if (a.max_p != b.max_p) return a.max_p > b.max_p;
        return a.a < b.a;
      });
    }
    if (opt.top > 0 && lines.size() > opt.top) lines.resize(opt.top);

    std::vector<size_t> test_indices;
    test_indices.reserve(std::min<size_t>(opt.q_test_top, lines.size()));
    for (size_t i = 0; i < lines.size() && test_indices.size() < opt.q_test_top; ++i) {
      if (!lines[i].q_small_blocked) test_indices.push_back(i);
    }

    std::atomic<size_t> next_test{0};
    const uint32_t workers = std::min<uint32_t>(
        opt.workers, std::max<uint32_t>(1, static_cast<uint32_t>(test_indices.size())));
    std::vector<std::thread> threads;
    threads.reserve(workers);
    for (uint32_t worker = 0; worker < workers; ++worker) {
      threads.emplace_back([&]() {
        mpz_t local_base;
        mpz_t q;
        mpz_init(local_base);
        mpz_init(q);
        mpz_set(local_base, base_value);

        while (true) {
          const size_t pos = next_test.fetch_add(1);
          if (pos >= test_indices.size()) break;
          Line &line = lines[test_indices[pos]];
          mpz_sub_ui(q, local_base, line.a);
          line.q_probable = mpz_probab_prime_p(q, opt.primality_reps);
          line.q_tested = true;
        }

        mpz_clear(q);
        mpz_clear(local_base);
      });
    }
    for (auto &thread : threads) thread.join();

    std::ofstream out(opt.output);
    if (!out) throw std::runtime_error("cannot write " + opt.output);
    out << "rank\ta\thitCount\tmaxP\tmaxOffset\tqSmallBlocked\tqTested\tqProbable"
        << "\toffsets\tps\n";
    for (size_t i = 0; i < lines.size(); ++i) {
      const Line &line = lines[i];
      out << (i + 1) << '\t'
          << line.a << '\t'
          << line.hit_count << '\t'
          << line.max_p << '\t'
          << line.max_offset << '\t'
          << (line.q_small_blocked ? 1 : 0) << '\t'
          << (line.q_tested ? 1 : 0) << '\t'
          << line.q_probable << '\t'
          << join_i64(line.offsets) << '\t'
          << join_u32(line.ps) << '\n';
    }

    std::cerr << "lines=" << lines.size()
              << " qTested=" << test_indices.size()
              << " workers=" << workers
              << " output=" << opt.output << "\n";

    mpz_clear(base_value);
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
