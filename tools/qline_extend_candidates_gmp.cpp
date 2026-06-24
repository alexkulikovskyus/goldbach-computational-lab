#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <gmp.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Options {
  std::string input;
  std::string output;
  uint64_t a = 0;
  int64_t min_offset = 0;
  int64_t max_offset = 1000;
  int64_t step = 2;
  uint64_t top = 100;
  int primality_reps = 25;
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
    if (key == "--input") opt.input = value;
    else if (key == "--output") opt.output = value;
    else if (key == "--a") opt.a = parse_u64(value, key);
    else if (key == "--minOffset") opt.min_offset = parse_i64(value, key);
    else if (key == "--maxOffset") opt.max_offset = parse_i64(value, key);
    else if (key == "--step") opt.step = parse_i64(value, key);
    else if (key == "--top") opt.top = parse_u64(value, key);
    else if (key == "--primalityReps") opt.primality_reps = static_cast<int>(parse_u64(value, key));
    else throw std::runtime_error("unknown argument: " + arg);
  }
  if (opt.input.empty()) throw std::runtime_error("need --input");
  if (opt.output.empty()) throw std::runtime_error("need --output");
  if (opt.a < 3) throw std::runtime_error("need --a >= 3");
  if (opt.min_offset > opt.max_offset) throw std::runtime_error("minOffset > maxOffset");
  if (opt.step <= 0) throw std::runtime_error("step must be > 0");
  if (opt.top == 0) throw std::runtime_error("top must be > 0");
  return opt;
}

static std::vector<std::string> split_tab(const std::string &line) {
  std::vector<std::string> out;
  std::string item;
  std::stringstream ss(line);
  while (std::getline(ss, item, '\t')) out.push_back(item);
  return out;
}

static int find_col(const std::vector<std::string> &header, const std::string &name) {
  for (size_t i = 0; i < header.size(); ++i) {
    if (header[i] == name) return static_cast<int>(i);
  }
  return -1;
}

static std::string mpz_to_string(const mpz_t value) {
  char *raw = mpz_get_str(nullptr, 10, value);
  if (!raw) throw std::runtime_error("mpz_get_str failed");
  std::string out(raw);
  std::free(raw);
  return out;
}

static uint64_t mul_mod(uint64_t a, uint64_t b, uint64_t mod) {
  return static_cast<uint64_t>((static_cast<__uint128_t>(a) * b) % mod);
}

static uint64_t pow_mod(uint64_t a, uint64_t d, uint64_t mod) {
  uint64_t out = 1;
  while (d > 0) {
    if (d & 1U) out = mul_mod(out, a, mod);
    a = mul_mod(a, a, mod);
    d >>= 1U;
  }
  return out;
}

static bool is_prime_u64(uint64_t n) {
  if (n < 2) return false;
  static const uint64_t small[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
  for (uint64_t p : small) {
    if (n == p) return true;
    if (n % p == 0) return false;
  }
  uint64_t d = n - 1;
  uint32_t s = 0;
  while ((d & 1U) == 0) {
    d >>= 1U;
    ++s;
  }
  static const uint64_t bases[] = {2, 3, 5, 7, 11, 13, 17};
  for (uint64_t a : bases) {
    if (a % n == 0) continue;
    uint64_t x = pow_mod(a, d, n);
    if (x == 1 || x == n - 1) continue;
    bool witness = true;
    for (uint32_t r = 1; r < s; ++r) {
      x = mul_mod(x, x, n);
      if (x == n - 1) {
        witness = false;
        break;
      }
    }
    if (witness) return false;
  }
  return true;
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    std::ifstream in(opt.input);
    if (!in) throw std::runtime_error("cannot read " + opt.input);
    std::ofstream out(opt.output);
    if (!out) throw std::runtime_error("cannot write " + opt.output);

    std::string line;
    if (!std::getline(in, line)) throw std::runtime_error("empty input " + opt.input);
    const auto header = split_tab(line);
    int n_col = find_col(header, "n");
    if (n_col < 0) n_col = find_col(header, "N");
    if (n_col < 0) throw std::runtime_error("input needs n or N column");
    const int label_col = find_col(header, "label");

    if (!std::getline(in, line)) throw std::runtime_error("input has no data row");
    auto parts = split_tab(line);
    parts.resize(header.size());
    const std::string label =
        label_col >= 0 ? parts[static_cast<size_t>(label_col)] : "anchor";
    const std::string &n_decimal = parts[static_cast<size_t>(n_col)];

    mpz_t anchor;
    mpz_t q;
    mpz_t current;
    mpz_init(anchor);
    mpz_init(q);
    mpz_init(current);
    if (mpz_set_str(anchor, n_decimal.c_str(), 10) != 0) {
      throw std::runtime_error("bad decimal n in first data row");
    }
    mpz_set(q, anchor);
    mpz_sub_ui(q, q, static_cast<unsigned long>(opt.a));
    const int q_probable = mpz_probab_prime_p(q, opt.primality_reps);

    out << "label\tn\tclass\tsourceP\toffset\ta\tqProbable\n";
    uint64_t emitted = 0;
    for (int64_t off = opt.max_offset; off >= opt.min_offset; off -= opt.step) {
      const int64_t p_signed = static_cast<int64_t>(opt.a) + off;
      if (p_signed < 3) continue;
      const uint64_t p = static_cast<uint64_t>(p_signed);
      if (!is_prime_u64(p)) continue;

      mpz_set(current, anchor);
      if (off >= 0) {
        mpz_add_ui(current, current, static_cast<unsigned long>(off));
      } else {
        mpz_sub_ui(current, current, static_cast<unsigned long>(-off));
      }
      if (mpz_even_p(current) == 0) continue;
      out << label << "_a" << opt.a << "_d" << off << '\t'
          << mpz_to_string(current) << '\t'
          << "qline-ext" << '\t'
          << p << ";a=" << opt.a << '\t'
          << off << '\t'
          << opt.a << '\t'
          << q_probable << '\n';
      ++emitted;
      if (emitted >= opt.top) break;
    }

    std::cerr << "a=" << opt.a
              << " qProbable=" << q_probable
              << " emitted=" << emitted
              << " offsets=[" << opt.min_offset << "," << opt.max_offset << "]\n";
    mpz_clear(current);
    mpz_clear(q);
    mpz_clear(anchor);
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
