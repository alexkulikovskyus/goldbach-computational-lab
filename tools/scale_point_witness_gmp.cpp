#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <gmp.h>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

struct Options {
  uint32_t coefficient = 2;
  uint32_t first_exponent = 1001;
  uint32_t last_exponent = 1100;
  uint32_t p_limit = 1000000;
  uint32_t workers = 1;
  uint32_t primality_reps = 25;
  uint32_t q_filter_limit = 0;
  std::string checkpoint = "results/scale-point-witnesses-gmp.jsonl";
};

struct QFilter {
  uint32_t prime = 0;
  uint32_t n_mod = 0;
};

struct Row {
  uint32_t exponent = 0;
  uint32_t p = 0;
  uint32_t attempts = 0;
  uint32_t q_tests = 0;
  uint32_t residue_skips = 0;
  uint32_t divisor_skips = 0;
  bool verified = false;
  std::string n;
  std::string q;
  double seconds = 0.0;
};

static bool starts_with(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

static Options parse_args(int argc, char **argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto eq = arg.find('=');
    std::string key = eq == std::string::npos ? arg : arg.substr(0, eq);
    std::string value = eq == std::string::npos ? "" : arg.substr(eq + 1);
    if (key == "--coefficient") opt.coefficient = static_cast<uint32_t>(std::stoul(value));
    else if (key == "--firstExponent") opt.first_exponent = static_cast<uint32_t>(std::stoul(value));
    else if (key == "--lastExponent") opt.last_exponent = static_cast<uint32_t>(std::stoul(value));
    else if (key == "--pLimit") opt.p_limit = static_cast<uint32_t>(std::stoul(value));
    else if (key == "--workers") opt.workers = static_cast<uint32_t>(std::stoul(value));
    else if (key == "--primalityReps") opt.primality_reps = static_cast<uint32_t>(std::stoul(value));
    else if (key == "--qFilterLimit") opt.q_filter_limit = static_cast<uint32_t>(std::stoul(value));
    else if (key == "--checkpoint") opt.checkpoint = value;
    else {
      std::cerr << "unknown argument: " << arg << "\n";
      std::exit(2);
    }
  }
  if (opt.first_exponent > opt.last_exponent) {
    std::cerr << "firstExponent must be <= lastExponent\n";
    std::exit(2);
  }
  if (opt.workers == 0) opt.workers = 1;
  return opt;
}

static std::vector<uint32_t> build_primes(uint32_t limit) {
  std::vector<uint8_t> flags(limit + 1, 1);
  if (limit >= 0) flags[0] = 0;
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

static void gmp_free_string(char *value) {
  if (!value) return;
  void (*freefunc)(void *, size_t);
  mp_get_memory_functions(nullptr, nullptr, &freefunc);
  freefunc(value, std::strlen(value) + 1);
}

static std::string mpz_to_string(const mpz_t value) {
  char *raw = mpz_get_str(nullptr, 10, value);
  std::string out(raw);
  gmp_free_string(raw);
  return out;
}

static uint32_t scale_point_mod_6(uint32_t coefficient, uint32_t exponent) {
  uint32_t power_mod = exponent == 0 ? 1 : 4;
  return ((coefficient % 6) * power_mod) % 6;
}

static bool candidate_can_leave_prime_q(uint32_t n_mod_6, uint32_t p) {
  if (p == 3) return true;
  uint32_t q_mod_6 = (n_mod_6 + 6 - (p % 6)) % 6;
  return q_mod_6 == 1 || q_mod_6 == 5;
}

static uint32_t pow_mod_u32(uint32_t base, uint32_t exponent, uint32_t mod) {
  uint64_t result = 1 % mod;
  uint64_t value = base % mod;
  while (exponent > 0) {
    if (exponent & 1) result = (result * value) % mod;
    value = (value * value) % mod;
    exponent >>= 1;
  }
  return static_cast<uint32_t>(result);
}

static std::vector<QFilter> build_q_filters(
    const Options &opt,
    const std::vector<uint32_t> &primes,
    const mpz_t n,
    uint32_t exponent) {
  std::vector<QFilter> filters;
  for (uint32_t prime : primes) {
    if (prime > opt.q_filter_limit) break;
    if (prime <= 3) continue;
    if (mpz_cmp_ui(n, static_cast<unsigned long>(opt.p_limit + prime)) <= 0) continue;
    uint32_t n_mod = static_cast<uint32_t>(
        (static_cast<uint64_t>(opt.coefficient % prime) * pow_mod_u32(10, exponent, prime)) % prime);
    filters.push_back({prime, n_mod});
  }
  return filters;
}

static bool q_has_small_divisor(const std::vector<QFilter> &filters, uint32_t p) {
  for (const QFilter &filter : filters) {
    if ((filter.n_mod + filter.prime - (p % filter.prime)) % filter.prime == 0) return true;
  }
  return false;
}

static std::unordered_set<uint32_t> load_completed_exponents(const std::string &path) {
  std::unordered_set<uint32_t> completed;
  std::ifstream in(path);
  if (!in) return completed;
  std::string line;
  const std::string marker = "\"exponent\":";
  while (std::getline(in, line)) {
    auto pos = line.find(marker);
    if (pos == std::string::npos) continue;
    pos += marker.size();
    while (pos < line.size() && line[pos] == ' ') ++pos;
    size_t end = pos;
    while (end < line.size() && std::isdigit(static_cast<unsigned char>(line[end]))) ++end;
    if (end > pos) completed.insert(static_cast<uint32_t>(std::stoul(line.substr(pos, end - pos))));
  }
  return completed;
}

static std::string json_escape(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (char ch : value) {
    if (ch == '\\' || ch == '"') {
      out.push_back('\\');
      out.push_back(ch);
    } else if (ch == '\n') {
      out += "\\n";
    } else {
      out.push_back(ch);
    }
  }
  return out;
}

static std::string row_to_json(const Options &opt, const Row &row) {
  std::ostringstream out;
  out << "{\"coefficient\":" << opt.coefficient
      << ",\"exponent\":" << row.exponent
      << ",\"formula\":\"" << opt.coefficient << "*10^" << row.exponent << "\""
      << ",\"N\":\"" << json_escape(row.n) << "\""
      << ",\"p\":" << row.p
      << ",\"q\":\"" << json_escape(row.q) << "\""
      << ",\"attempts\":" << row.attempts
      << ",\"qTests\":" << row.q_tests
      << ",\"residueSkips\":" << row.residue_skips
      << ",\"divisorSkips\":" << row.divisor_skips
      << ",\"verified\":" << (row.verified ? "true" : "false")
      << ",\"seconds\":" << row.seconds << "}";
  return out.str();
}

static Row compute_row(const Options &opt, const std::vector<uint32_t> &primes, uint32_t exponent) {
  auto start = std::chrono::steady_clock::now();
  Row row;
  row.exponent = exponent;

  mpz_t n;
  mpz_t q;
  mpz_init(n);
  mpz_init(q);

  mpz_ui_pow_ui(n, 10, exponent);
  mpz_mul_ui(n, n, opt.coefficient);

  uint32_t n_mod_6 = scale_point_mod_6(opt.coefficient, exponent);
  std::vector<QFilter> q_filters = build_q_filters(opt, primes, n, exponent);
  for (uint32_t p : primes) {
    if (p > opt.p_limit) break;
    ++row.attempts;
    if (!candidate_can_leave_prime_q(n_mod_6, p)) {
      ++row.residue_skips;
      continue;
    }
    if (q_has_small_divisor(q_filters, p)) {
      ++row.divisor_skips;
      continue;
    }
    ++row.q_tests;
    mpz_sub_ui(q, n, p);
    if (mpz_probab_prime_p(q, opt.primality_reps) > 0) {
      row.p = p;
      row.verified = true;
      row.n = mpz_to_string(n);
      row.q = mpz_to_string(q);
      break;
    }
  }

  if (!row.verified) {
    row.n = mpz_to_string(n);
    row.q = "";
  }

  mpz_clear(q);
  mpz_clear(n);
  auto end = std::chrono::steady_clock::now();
  row.seconds = std::chrono::duration<double>(end - start).count();
  return row;
}

int main(int argc, char **argv) {
  Options opt = parse_args(argc, argv);
  auto completed = load_completed_exponents(opt.checkpoint);
  std::vector<uint32_t> exponents;
  for (uint32_t e = opt.first_exponent; e <= opt.last_exponent; ++e) {
    if (!completed.count(e)) exponents.push_back(e);
  }

  std::cerr << "checkpoint=" << opt.checkpoint << " completed=" << completed.size()
            << " remaining=" << exponents.size() << " workers=" << opt.workers << "\n";

  auto primes = build_primes(opt.p_limit);
  std::atomic<size_t> next{0};
  std::mutex write_mutex;
  std::ofstream out(opt.checkpoint, std::ios::app);
  if (!out) {
    std::cerr << "cannot open checkpoint: " << opt.checkpoint << "\n";
    return 1;
  }

  auto worker = [&]() {
    while (true) {
      size_t index = next.fetch_add(1);
      if (index >= exponents.size()) break;
      uint32_t exponent = exponents[index];
      Row row = compute_row(opt, primes, exponent);
      std::string json = row_to_json(opt, row);
      {
        std::lock_guard<std::mutex> lock(write_mutex);
        out << json << "\n";
        out.flush();
        std::cout << opt.coefficient << "*10^" << row.exponent << " -> p="
                  << (row.verified ? std::to_string(row.p) : std::string("unresolved"))
                  << ", attempts=" << row.attempts
                  << ", qTests=" << row.q_tests
                  << ", residueSkips=" << row.residue_skips
                  << ", divisorSkips=" << row.divisor_skips
                  << ", seconds=" << row.seconds << "\n";
        std::cout.flush();
      }
    }
  };

  std::vector<std::thread> threads;
  uint32_t workers = std::min<uint32_t>(opt.workers, std::max<size_t>(1, exponents.size()));
  for (uint32_t i = 0; i < workers; ++i) threads.emplace_back(worker);
  for (auto &thread : threads) thread.join();
  return 0;
}
