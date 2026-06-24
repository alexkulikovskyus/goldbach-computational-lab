#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
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
  uint32_t p_limit = 50000;
  uint32_t small_limit = 50000;
  uint32_t workers = 1;
  uint32_t primality_reps = 25;
};

struct InputRow {
  std::string label;
  std::string n_decimal;
  std::string klass;
  std::string source_p;
};

struct Result {
  InputRow input;
  uint32_t exact_p = 0;
  uint32_t attempts = 0;
  uint32_t q_tests = 0;
  uint32_t divisor_skips = 0;
  bool verified = false;
  double seconds = 0.0;
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

static std::vector<std::string> split(const std::string &text, char sep) {
  std::vector<std::string> out;
  std::string item;
  std::stringstream ss(text);
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
    for (uint64_t p = first; p <= p_limit; p += prime) {
      wall[static_cast<size_t>(p)] = 1;
    }
  }
  return wall;
}

static Result compute_row(
    const InputRow &input,
    const Options &opt,
    const std::vector<uint32_t> &primes) {
  const auto start = std::chrono::steady_clock::now();
  Result result;
  result.input = input;

  mpz_t n;
  mpz_t q;
  mpz_init(n);
  mpz_init(q);
  if (mpz_set_str(n, input.n_decimal.c_str(), 10) != 0) {
    throw std::runtime_error("bad N for label " + input.label);
  }
  if (mpz_even_p(n) == 0) {
    throw std::runtime_error("N must be even for label " + input.label);
  }

  const uint32_t n_mod_6 = static_cast<uint32_t>(mpz_fdiv_ui(n, 6));
  const auto wall = build_small_wall(primes, opt.small_limit, opt.p_limit, n);

  for (uint32_t p : primes) {
    if (p > opt.p_limit) break;
    if (mpz_cmp_ui(n, static_cast<unsigned long>(p) + 1UL) <= 0) continue;
    ++result.attempts;
    if (!candidate_can_leave_prime_q(n_mod_6, p)) continue;
    if (wall[p]) {
      ++result.divisor_skips;
      continue;
    }
    ++result.q_tests;
    mpz_sub_ui(q, n, p);
    if (mpz_probab_prime_p(q, opt.primality_reps) > 0) {
      result.exact_p = p;
      result.verified = true;
      break;
    }
  }

  mpz_clear(q);
  mpz_clear(n);
  const auto end = std::chrono::steady_clock::now();
  result.seconds = std::chrono::duration<double>(end - start).count();
  return result;
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const auto rows = load_input(opt);
    const auto primes = build_primes(std::max(opt.p_limit, opt.small_limit));
    std::vector<Result> results(rows.size());
    std::atomic<size_t> next{0};
    std::atomic<bool> failed{false};

    auto worker = [&]() {
      while (!failed.load()) {
        const size_t index = next.fetch_add(1);
        if (index >= rows.size()) break;
        try {
          results[index] = compute_row(rows[index], opt, primes);
        } catch (const std::exception &ex) {
          failed.store(true);
          std::cerr << "worker error: " << ex.what() << "\n";
        }
      }
    };

    const uint32_t workers = std::min<uint32_t>(
        opt.workers, static_cast<uint32_t>(std::max<size_t>(1, rows.size())));
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < workers; ++i) threads.emplace_back(worker);
    for (auto &thread : threads) thread.join();
    if (failed.load()) return 1;

    std::cout << "label\tclass\tsourceP\texactP\tverified\tattempts\tqTests"
              << "\tdivisorSkips\tseconds\n";
    for (const auto &result : results) {
      std::cout << result.input.label << '\t'
                << result.input.klass << '\t'
                << result.input.source_p << '\t'
                << result.exact_p << '\t'
                << (result.verified ? 1 : 0) << '\t'
                << result.attempts << '\t'
                << result.q_tests << '\t'
                << result.divisor_skips << '\t'
                << result.seconds << '\n';
    }
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
