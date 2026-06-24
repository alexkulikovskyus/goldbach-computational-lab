#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <gmp.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Options {
  std::string input_path = "data/silva/t0-parsed.tsv";
  uint32_t top = 50;
  uint32_t controls_per_peak = 20;
  uint64_t max_offset = 1000000000ULL;
  uint64_t seed = 0x9e3779b97f4a7c15ULL;
};

struct SourceRow {
  uint32_t p = 0;
  std::string n_decimal;
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
    else if (key == "--top") opt.top = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--controlsPerPeak") {
      opt.controls_per_peak = static_cast<uint32_t>(parse_u64(value, key));
    } else if (key == "--maxOffset") {
      opt.max_offset = parse_u64(value, key);
    } else if (key == "--seed") {
      opt.seed = parse_u64(value, key);
    } else {
      throw std::runtime_error("unknown argument: " + arg);
    }
  }
  if (opt.top == 0) throw std::runtime_error("--top must be > 0");
  if (opt.max_offset < 2) throw std::runtime_error("--maxOffset must be >= 2");
  return opt;
}

static std::vector<std::string> split(const std::string &text, char sep) {
  std::vector<std::string> out;
  std::string item;
  std::stringstream ss(text);
  while (std::getline(ss, item, sep)) out.push_back(item);
  return out;
}

static std::vector<SourceRow> load_source(const Options &opt) {
  std::ifstream in(opt.input_path);
  if (!in) throw std::runtime_error("cannot open input: " + opt.input_path);

  std::vector<SourceRow> rows;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    const auto parts = split(line, '\t');
    if (parts.size() < 3) continue;
    SourceRow row;
    row.p = static_cast<uint32_t>(parse_u64(parts[1], "p"));
    row.n_decimal = parts[2];
    rows.push_back(row);
  }
  if (rows.empty()) throw std::runtime_error("no source rows");
  std::sort(rows.begin(), rows.end(), [](const SourceRow &a, const SourceRow &b) {
    if (a.p != b.p) return a.p > b.p;
    return a.n_decimal < b.n_decimal;
  });
  if (rows.size() > opt.top) rows.resize(opt.top);
  return rows;
}

static uint64_t splitmix64(uint64_t &state) {
  uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
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

static uint64_t make_even_offset(uint64_t raw, uint64_t max_offset) {
  const uint64_t slots = max_offset / 2;
  uint64_t value = ((raw % slots) + 1) * 2;
  if (value > max_offset) value = max_offset & ~1ULL;
  return value == 0 ? 2 : value;
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const auto rows = load_source(opt);

    std::cout << "label\tn\tclass\tknownP\n";

    mpz_t n;
    mpz_t out;
    mpz_init(n);
    mpz_init(out);

    for (const auto &row : rows) {
      if (mpz_set_str(n, row.n_decimal.c_str(), 10) != 0) {
        throw std::runtime_error("bad N for p=" + std::to_string(row.p));
      }

      const std::string peak_label = "p" + std::to_string(row.p) + "_peak";
      std::cout << peak_label << '\t' << row.n_decimal << "\tpeak\t" << row.p << '\n';

      uint64_t state = opt.seed ^ (static_cast<uint64_t>(row.p) << 32) ^ row.p;
      for (uint32_t i = 0; i < opt.controls_per_peak; ++i) {
        const uint64_t raw = splitmix64(state);
        const uint64_t offset = make_even_offset(raw, opt.max_offset);
        const bool plus = (splitmix64(state) & 1ULL) != 0;
        mpz_set(out, n);
        if (plus) mpz_add_ui(out, out, static_cast<unsigned long>(offset));
        else mpz_sub_ui(out, out, static_cast<unsigned long>(offset));
        if (mpz_even_p(out) == 0) mpz_add_ui(out, out, 1);
        const std::string control_label =
            "p" + std::to_string(row.p) + "_ctrl" + std::to_string(i + 1);
        std::cout << control_label << '\t' << mpz_to_string(out)
                  << "\tcontrol\t" << row.p << '\n';
      }
    }

    mpz_clear(out);
    mpz_clear(n);
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
