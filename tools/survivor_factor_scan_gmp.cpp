#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <gmp.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Options {
  std::string n_decimal;
  uint32_t p_limit = 20000;
  uint32_t small_limit = 250000;
  uint32_t factor_limit = 50000000;
  uint32_t primality_reps = 25;
};

struct PrimeMod {
  uint32_t prime = 0;
  uint32_t n_mod = 0;
};

static Options parse_args(int argc, char **argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto eq = arg.find('=');
    std::string key = eq == std::string::npos ? arg : arg.substr(0, eq);
    std::string value = eq == std::string::npos ? "" : arg.substr(eq + 1);
    if (key == "--n") opt.n_decimal = value;
    else if (key == "--pLimit") opt.p_limit = static_cast<uint32_t>(std::stoul(value));
    else if (key == "--smallLimit") opt.small_limit = static_cast<uint32_t>(std::stoul(value));
    else if (key == "--factorLimit") opt.factor_limit = static_cast<uint32_t>(std::stoul(value));
    else if (key == "--primalityReps") opt.primality_reps = static_cast<uint32_t>(std::stoul(value));
    else {
      std::cerr << "unknown argument: " << arg << "\n";
      std::exit(2);
    }
  }
  if (opt.n_decimal.empty()) {
    std::cerr << "--n=<even decimal integer> is required\n";
    std::exit(2);
  }
  if (opt.factor_limit < opt.small_limit) opt.factor_limit = opt.small_limit;
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

static bool candidate_can_leave_prime_q(uint32_t n_mod_6, uint32_t p) {
  if (p == 3) return true;
  uint32_t q_mod_6 = (n_mod_6 + 6 - (p % 6)) % 6;
  return q_mod_6 == 1 || q_mod_6 == 5;
}

static std::vector<PrimeMod> build_small_mods(
    const std::vector<uint32_t> &primes,
    uint32_t small_limit,
    const mpz_t n) {
  std::vector<PrimeMod> mods;
  for (uint32_t prime : primes) {
    if (prime > small_limit) break;
    if (prime <= 3) continue;
    mods.push_back({prime, static_cast<uint32_t>(mpz_fdiv_ui(n, prime))});
  }
  return mods;
}

static uint32_t first_small_blocker(const std::vector<PrimeMod> &mods, uint32_t p) {
  for (const PrimeMod &mod : mods) {
    if ((mod.n_mod + mod.prime - (p % mod.prime)) % mod.prime == 0) return mod.prime;
  }
  return 0;
}

static uint32_t first_factor_between(
    const std::vector<uint32_t> &primes,
    const mpz_t q,
    uint32_t start_exclusive,
    uint32_t factor_limit) {
  for (uint32_t prime : primes) {
    if (prime <= start_exclusive) continue;
    if (prime > factor_limit) break;
    if (mpz_divisible_ui_p(q, prime)) return prime;
  }
  return 0;
}

int main(int argc, char **argv) {
  Options opt = parse_args(argc, argv);
  auto started = std::chrono::steady_clock::now();
  std::vector<uint32_t> primes = build_primes(std::max(opt.factor_limit, opt.p_limit));

  mpz_t n;
  mpz_t q;
  mpz_init(n);
  mpz_init(q);
  if (mpz_set_str(n, opt.n_decimal.c_str(), 10) != 0) {
    std::cerr << "invalid N\n";
    return 2;
  }
  uint32_t n_mod_6 = static_cast<uint32_t>(mpz_fdiv_ui(n, 6));
  auto small_mods = build_small_mods(primes, opt.small_limit, n);

  uint32_t admissible = 0;
  uint32_t blocked_small = 0;
  uint32_t survivor_composite = 0;
  uint32_t survivor_prime = 0;
  uint32_t factored_after_small = 0;
  uint32_t unfactored_after_limit = 0;
  uint32_t first_witness = 0;
  std::vector<std::string> rows;

  for (uint32_t p : primes) {
    if (p > opt.p_limit) break;
    if (!candidate_can_leave_prime_q(n_mod_6, p)) continue;
    ++admissible;
    uint32_t small_blocker = first_small_blocker(small_mods, p);
    if (small_blocker) {
      ++blocked_small;
      continue;
    }

    mpz_sub_ui(q, n, p);
    int probable = mpz_probab_prime_p(q, opt.primality_reps);
    if (probable > 0) {
      ++survivor_prime;
      if (first_witness == 0) first_witness = p;
      std::ostringstream out;
      out << "{\"p\":" << p
          << ",\"status\":\"witness\""
          << ",\"factor\":0"
          << ",\"factorLimit\":" << opt.factor_limit
          << "}";
      rows.push_back(out.str());
      break;
    }

    ++survivor_composite;
    uint32_t factor = first_factor_between(primes, q, opt.small_limit, opt.factor_limit);
    if (factor) ++factored_after_small;
    else ++unfactored_after_limit;
    std::ostringstream out;
    out << "{\"p\":" << p
        << ",\"status\":\"composite_survivor\""
        << ",\"factor\":" << factor
        << ",\"factorLimit\":" << opt.factor_limit
        << "}";
    rows.push_back(out.str());
  }

  auto ended = std::chrono::steady_clock::now();
  double seconds = std::chrono::duration<double>(ended - started).count();

  std::cout << "{\"tool\":\"survivor-factor-scan-gmp\"";
  std::cout << ",\"n\":\"" << opt.n_decimal << "\"";
  std::cout << ",\"pLimit\":" << opt.p_limit;
  std::cout << ",\"smallLimit\":" << opt.small_limit;
  std::cout << ",\"factorLimit\":" << opt.factor_limit;
  std::cout << ",\"admissible\":" << admissible;
  std::cout << ",\"blockedSmall\":" << blocked_small;
  std::cout << ",\"survivorComposite\":" << survivor_composite;
  std::cout << ",\"survivorPrime\":" << survivor_prime;
  std::cout << ",\"factoredAfterSmall\":" << factored_after_small;
  std::cout << ",\"unfactoredAfterLimit\":" << unfactored_after_limit;
  std::cout << ",\"firstWitness\":" << first_witness;
  std::cout << ",\"seconds\":" << seconds;
  std::cout << ",\"rows\":[";
  for (size_t i = 0; i < rows.size(); ++i) {
    if (i) std::cout << ",";
    std::cout << rows[i];
  }
  std::cout << "]}\n";

  mpz_clear(q);
  mpz_clear(n);
  return 0;
}
