#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

struct Options {
  uint32_t coefficient = 2;
  uint32_t base = 10;
  uint32_t exponent = 10000;
  int64_t min_offset = -100000;
  int64_t max_offset = 100000;
  int64_t step = 2;
  uint32_t p_limit = 10000;
  uint32_t factor_a = 97;
  uint32_t factor_b = 997;
  uint32_t workers = 18;
  uint32_t top = 100;
  std::string output;
};

struct Pressure {
  uint32_t cand = 0;
  uint32_t blocked = 0;
  uint32_t first_surv = 0;
  uint32_t longest_run = 0;
  double frac = 0.0;
};

struct Score {
  int64_t offset = 0;
  Pressure a;
  Pressure b;
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
    else if (key == "--step") opt.step = parse_i64(value, key);
    else if (key == "--pLimit") opt.p_limit = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--factorA") opt.factor_a = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--factorB") opt.factor_b = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--workers") opt.workers = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--top") opt.top = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--output") opt.output = value;
    else throw std::runtime_error("unknown argument: " + arg);
  }
  if (opt.base < 2) throw std::runtime_error("--base must be >= 2");
  if (opt.min_offset > opt.max_offset) throw std::runtime_error("--minOffset > --maxOffset");
  if (opt.step <= 0) throw std::runtime_error("--step must be > 0");
  if ((opt.min_offset % 2) || (opt.max_offset % 2) || (opt.step % 2)) {
    throw std::runtime_error("offset range and step must be even");
  }
  if (opt.workers == 0) opt.workers = 1;
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

static uint32_t signed_offset_mod(int64_t offset, uint32_t mod) {
  int64_t rem = offset % static_cast<int64_t>(mod);
  if (rem < 0) rem += mod;
  return static_cast<uint32_t>(rem);
}

static bool lane_admissible(uint32_t n_mod6, uint32_t lane) {
  const uint32_t q_mod6 = (n_mod6 + 6 - lane) % 6;
  return q_mod6 == 1 || q_mod6 == 5;
}

static Pressure pressure_for(
    int64_t offset,
    const std::vector<uint32_t> &primes,
    const std::vector<uint32_t> &factors,
    const std::vector<uint32_t> &base_mods,
    uint32_t base_mod6,
    uint32_t p_limit) {
  Pressure out;
  const uint32_t n_mod6 = (base_mod6 + signed_offset_mod(offset, 6)) % 6;
  uint32_t run = 0;
  for (uint32_t p : primes) {
    if (p > p_limit) break;
    if (p <= 3) continue;
    if (!lane_admissible(n_mod6, p % 6)) continue;
    ++out.cand;

    bool blocked = false;
    for (size_t i = 0; i < factors.size(); ++i) {
      const uint32_t r = factors[i];
      const uint32_t n_mod = (base_mods[i] + signed_offset_mod(offset, r)) % r;
      if ((n_mod + r - (p % r)) % r == 0) {
        blocked = true;
        break;
      }
    }

    if (blocked) {
      ++out.blocked;
      ++run;
    } else {
      if (out.first_surv == 0) out.first_surv = out.cand;
      run = 0;
    }
    out.longest_run = std::max(out.longest_run, run);
  }
  if (out.first_surv == 0) out.first_surv = out.cand + 1;
  out.frac = out.cand == 0 ? 0.0 : static_cast<double>(out.blocked) / out.cand;
  return out;
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const uint32_t factor_limit = std::max(opt.factor_a, opt.factor_b);
    const auto primes = build_primes(std::max(opt.p_limit, factor_limit));

    std::vector<uint32_t> factors_a;
    std::vector<uint32_t> factors_b;
    std::vector<uint32_t> mods_a;
    std::vector<uint32_t> mods_b;
    for (uint32_t p : primes) {
      if (p > factor_limit) break;
      if (p <= 3) continue;
      const uint32_t base_mod = static_cast<uint32_t>(
          (static_cast<uint64_t>(opt.coefficient % p) *
           pow_mod_u32(opt.base, opt.exponent, p)) % p);
      if (p <= opt.factor_a) {
        factors_a.push_back(p);
        mods_a.push_back(base_mod);
      }
      if (p <= opt.factor_b) {
        factors_b.push_back(p);
        mods_b.push_back(base_mod);
      }
    }
    const uint32_t base_mod6 = static_cast<uint32_t>(
        (static_cast<uint64_t>(opt.coefficient % 6) *
         pow_mod_u32(opt.base, opt.exponent, 6)) % 6);

    const uint64_t rows = static_cast<uint64_t>((opt.max_offset - opt.min_offset) / opt.step + 1);
    std::vector<Score> scores(rows);
    std::atomic<uint64_t> next{0};
    const uint32_t worker_count = std::min<uint32_t>(opt.workers, static_cast<uint32_t>(std::max<uint64_t>(1, rows)));
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (uint32_t worker = 0; worker < worker_count; ++worker) {
      workers.emplace_back([&]() {
        while (true) {
          const uint64_t idx = next.fetch_add(1);
          if (idx >= rows) break;
          const int64_t offset = opt.min_offset + static_cast<int64_t>(idx) * opt.step;
          Score score;
          score.offset = offset;
          score.a = pressure_for(offset, primes, factors_a, mods_a, base_mod6, opt.p_limit);
          score.b = pressure_for(offset, primes, factors_b, mods_b, base_mod6, opt.p_limit);
          scores[idx] = score;
        }
      });
    }
    for (auto &worker : workers) worker.join();

    std::sort(scores.begin(), scores.end(), [](const Score &x, const Score &y) {
      if (x.b.frac != y.b.frac) return x.b.frac > y.b.frac;
      if (x.b.longest_run != y.b.longest_run) return x.b.longest_run > y.b.longest_run;
      if (x.a.frac != y.a.frac) return x.a.frac > y.a.frac;
      return x.offset < y.offset;
    });

    std::ofstream out(opt.output);
    if (!out) throw std::runtime_error("cannot write " + opt.output);
    out << "rank\toffset\tpLimit\tp" << opt.factor_a << "Frac\tp" << opt.factor_b
        << "Frac\tp" << opt.factor_a << "Run\tp" << opt.factor_b
        << "Run\tp" << opt.factor_a << "FirstSurv\tp" << opt.factor_b
        << "FirstSurv\tcand\n";
    const size_t limit = opt.top == 0 ? scores.size() : std::min<size_t>(opt.top, scores.size());
    for (size_t i = 0; i < limit; ++i) {
      const Score &s = scores[i];
      out << (i + 1) << '\t' << s.offset << '\t' << opt.p_limit
          << '\t' << std::fixed << std::setprecision(12) << s.a.frac
          << '\t' << s.b.frac
          << '\t' << s.a.longest_run
          << '\t' << s.b.longest_run
          << '\t' << s.a.first_surv
          << '\t' << s.b.first_surv
          << '\t' << s.b.cand << '\n';
    }

    std::cerr << "rows=" << rows
              << " workers=" << worker_count
              << " output=" << opt.output << "\n";
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
