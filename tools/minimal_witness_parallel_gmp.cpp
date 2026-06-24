#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <gmp.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

struct Options {
  std::string input_path;
  uint32_t p_limit = 1000000;
  uint32_t small_limit = 250000;
  uint32_t workers = 18;
  uint32_t primality_reps = 25;
};

struct InputRow {
  std::string label;
  std::string n_decimal;
  std::string klass = "unknown";
  std::string source_p;
};

struct Stats {
  uint64_t attempts = 0;
  uint64_t q_tests = 0;
  uint64_t divisor_skips = 0;
};

struct Result {
  uint32_t exact_p = 0;
  Stats stats;
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

static Options parse_args(int argc, char **argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    const auto eq = arg.find('=');
    const std::string key = eq == std::string::npos ? arg : arg.substr(0, eq);
    const std::string value = eq == std::string::npos ? "" : arg.substr(eq + 1);
    if (key == "--input") opt.input_path = value;
    else if (key == "--pLimit") opt.p_limit = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--smallLimit") opt.small_limit = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--workers") opt.workers = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--primalityReps") opt.primality_reps = static_cast<uint32_t>(parse_u64(value, key));
    else throw std::runtime_error("unknown argument: " + arg);
  }
  if (opt.input_path.empty()) throw std::runtime_error("need --input");
  if (opt.p_limit < 3) throw std::runtime_error("--pLimit must be >= 3");
  if (opt.workers == 0) opt.workers = 1;
  return opt;
}

static std::vector<std::string> split(const std::string &line, char sep) {
  std::vector<std::string> out;
  std::string item;
  std::stringstream ss(line);
  while (std::getline(ss, item, sep)) out.push_back(item);
  return out;
}

static bool is_comment_or_blank(const std::string &line) {
  for (unsigned char ch : line) {
    if (std::isspace(ch)) continue;
    return ch == '#';
  }
  return true;
}

static std::vector<InputRow> load_input(const Options &opt) {
  std::ifstream in(opt.input_path);
  if (!in) throw std::runtime_error("cannot open input: " + opt.input_path);

  std::vector<InputRow> rows;
  std::string line;
  uint64_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    if (is_comment_or_blank(line)) continue;
    if (line_no == 1 && line.rfind("label\t", 0) == 0) continue;
    const auto parts = split(line, '\t');
    if (parts.size() < 2) continue;
    InputRow row;
    row.label = parts[0];
    row.n_decimal = parts[1];
    row.klass = parts.size() > 2 ? parts[2] : "unknown";
    row.source_p = parts.size() > 3 ? parts[3] : "";
    rows.push_back(row);
  }
  if (rows.empty()) throw std::runtime_error("input has no rows");
  return rows;
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

static bool candidate_can_leave_prime_q(uint32_t n_mod6, uint32_t p) {
  if (p == 3) return true;
  const uint32_t q_mod6 = (n_mod6 + 6 - (p % 6)) % 6;
  return q_mod6 == 1 || q_mod6 == 5;
}

static void update_best(std::atomic<uint32_t> &best, uint32_t p) {
  uint32_t current = best.load(std::memory_order_relaxed);
  while (p < current &&
         !best.compare_exchange_weak(current, p, std::memory_order_relaxed)) {
  }
}

static Result compute_row(
    const InputRow &row,
    const Options &opt,
    const std::vector<uint32_t> &primes) {
  mpz_t n;
  mpz_init(n);
  if (mpz_set_str(n, row.n_decimal.c_str(), 10) != 0) {
    mpz_clear(n);
    throw std::runtime_error("bad N for label " + row.label);
  }
  if (mpz_odd_p(n)) {
    mpz_clear(n);
    throw std::runtime_error("N must be even for label " + row.label);
  }

  const uint32_t n_mod6 = static_cast<uint32_t>(mpz_fdiv_ui(n, 6));
  std::vector<uint8_t> q_blocked(opt.p_limit + 1, 0);
  for (uint32_t r : primes) {
    if (r > opt.small_limit) break;
    if (r <= 3) continue;
    uint32_t first = static_cast<uint32_t>(mpz_fdiv_ui(n, r));
    if (first < 2) first += ((2 - first + r - 1) / r) * r;
    for (uint32_t p = first; p <= opt.p_limit; p += r) q_blocked[p] = 1;
  }

  std::vector<uint32_t> lane;
  for (uint32_t p : primes) {
    if (p > opt.p_limit) break;
    if (candidate_can_leave_prime_q(n_mod6, p)) lane.push_back(p);
  }

  const uint32_t workers =
      std::min<uint32_t>(opt.workers, static_cast<uint32_t>(std::max<size_t>(1, lane.size())));
  std::atomic<uint32_t> best(opt.p_limit + 1);
  std::vector<Stats> locals(workers);
  std::vector<std::thread> threads;
  threads.reserve(workers);

  for (uint32_t worker = 0; worker < workers; ++worker) {
    threads.emplace_back([&, worker]() {
      mpz_t local_n;
      mpz_t q;
      mpz_init(local_n);
      mpz_init(q);
      mpz_set(local_n, n);
      Stats local;
      for (size_t i = worker; i < lane.size(); i += workers) {
        const uint32_t p = lane[i];
        if (p >= best.load(std::memory_order_relaxed)) continue;
        ++local.attempts;
        if (q_blocked[p]) {
          ++local.divisor_skips;
          continue;
        }
        ++local.q_tests;
        mpz_sub_ui(q, local_n, p);
        if (mpz_probab_prime_p(q, opt.primality_reps) > 0) update_best(best, p);
      }
      locals[worker] = local;
      mpz_clear(q);
      mpz_clear(local_n);
    });
  }

  for (auto &thread : threads) thread.join();
  mpz_clear(n);

  Result result;
  for (const Stats &local : locals) {
    result.stats.attempts += local.attempts;
    result.stats.q_tests += local.q_tests;
    result.stats.divisor_skips += local.divisor_skips;
  }
  const uint32_t best_p = best.load(std::memory_order_relaxed);
  if (best_p <= opt.p_limit) result.exact_p = best_p;
  return result;
}

int main(int argc, char **argv) {
  try {
    const auto started = std::chrono::steady_clock::now();
    const Options opt = parse_args(argc, argv);
    const auto rows = load_input(opt);
    const auto primes = build_primes(std::max(opt.p_limit, opt.small_limit));

    std::cout << "label\tclass\tsourceP\texactP\tverified\tattempts\tqTests"
              << "\tdivisorSkips\tworkers\tseconds\n";
    for (const InputRow &row : rows) {
      const auto row_started = std::chrono::steady_clock::now();
      const Result result = compute_row(row, opt, primes);
      const bool verified = result.exact_p != 0;
      const auto row_finished = std::chrono::steady_clock::now();
      const double seconds =
          std::chrono::duration<double>(row_finished - row_started).count();
      std::cout << row.label << '\t'
                << row.klass << '\t'
                << row.source_p << '\t'
                << result.exact_p << '\t'
                << (verified ? 1 : 0) << '\t'
                << result.stats.attempts << '\t'
                << result.stats.q_tests << '\t'
                << result.stats.divisor_skips << '\t'
                << opt.workers << '\t'
                << seconds << '\n'
                << std::flush;
    }
    const auto finished = std::chrono::steady_clock::now();
    std::cerr << "rows=" << rows.size()
              << " pLimit=" << opt.p_limit
              << " smallLimit=" << opt.small_limit
              << " workers=" << opt.workers
              << " seconds=" << std::chrono::duration<double>(finished - started).count()
              << "\n";
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
