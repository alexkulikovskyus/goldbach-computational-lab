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
  uint32_t skip = 0;
  uint32_t top = 10;
  uint64_t radius = 2000;
  uint64_t stride = 2;
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
    else if (key == "--skip") opt.skip = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--top") opt.top = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--radius") opt.radius = parse_u64(value, key);
    else if (key == "--stride") opt.stride = parse_u64(value, key);
    else throw std::runtime_error("unknown argument: " + arg);
  }
  if (opt.top == 0) throw std::runtime_error("--top must be > 0");
  if (opt.radius % 2 != 0) throw std::runtime_error("--radius must be even");
  if (opt.stride == 0 || opt.stride % 2 != 0) throw std::runtime_error("--stride must be a positive even number");
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
  if (opt.skip >= rows.size()) {
    throw std::runtime_error("--skip leaves no source rows");
  }
  rows.erase(rows.begin(), rows.begin() + opt.skip);
  if (rows.size() > opt.top) rows.resize(opt.top);
  return rows;
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

static std::string offset_label(int64_t offset) {
  if (offset == 0) return "peak";
  if (offset < 0) return "m" + std::to_string(-offset);
  return "p" + std::to_string(offset);
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

      const int64_t radius = static_cast<int64_t>(opt.radius);
      const int64_t stride = static_cast<int64_t>(opt.stride);
      for (int64_t offset = -radius; offset <= radius; offset += stride) {
        mpz_set(out, n);
        if (offset >= 0) mpz_add_ui(out, out, static_cast<unsigned long>(offset));
        else mpz_sub_ui(out, out, static_cast<unsigned long>(-offset));
        const std::string klass = offset == 0 ? "peak" : "window";
        const std::string label = "p" + std::to_string(row.p) + "_" + offset_label(offset);
        std::cout << label << '\t' << mpz_to_string(out) << '\t'
                  << klass << '\t' << row.p << '\n';
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
