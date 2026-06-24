#include <cstdlib>
#include <fstream>
#include <gmp.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Options {
  unsigned long coefficient = 2;
  unsigned long base = 10;
  unsigned long exponent = 10000;
  std::string offsets;
  std::string output;
  unsigned long limit = 0;
};

static unsigned long parse_ul(const std::string &value, const std::string &name) {
  try {
    size_t pos = 0;
    unsigned long out = std::stoul(value, &pos);
    if (pos != value.size()) throw std::invalid_argument("bad");
    return out;
  } catch (...) {
    throw std::runtime_error("bad unsigned integer for " + name + ": " + value);
  }
}

static Options parse_args(int argc, char **argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    const auto eq = arg.find('=');
    const std::string key = eq == std::string::npos ? arg : arg.substr(0, eq);
    const std::string value = eq == std::string::npos ? "" : arg.substr(eq + 1);
    if (key == "--coefficient") opt.coefficient = parse_ul(value, key);
    else if (key == "--base") opt.base = parse_ul(value, key);
    else if (key == "--exponent") opt.exponent = parse_ul(value, key);
    else if (key == "--offsets") opt.offsets = value;
    else if (key == "--output") opt.output = value;
    else if (key == "--limit") opt.limit = parse_ul(value, key);
    else throw std::runtime_error("unknown argument: " + arg);
  }
  if (opt.base < 2) throw std::runtime_error("--base must be >= 2");
  if (opt.offsets.empty()) throw std::runtime_error("need --offsets");
  if (opt.output.empty()) throw std::runtime_error("need --output");
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

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    std::ifstream in(opt.offsets);
    if (!in) throw std::runtime_error("cannot read " + opt.offsets);
    std::ofstream out(opt.output);
    if (!out) throw std::runtime_error("cannot write " + opt.output);

    std::string line;
    if (!std::getline(in, line)) throw std::runtime_error("empty offsets file");
    const auto header = split_tab(line);
    const int offset_col = find_col(header, "offset");
    const int rank_col = find_col(header, "rank");
    if (offset_col < 0) throw std::runtime_error("offsets file needs offset column");

    mpz_t base_value;
    mpz_t n;
    mpz_init(base_value);
    mpz_init(n);
    mpz_ui_pow_ui(base_value, opt.base, opt.exponent);
    mpz_mul_ui(base_value, base_value, opt.coefficient);

    out << "label\tn\toffset\tsourceRank\n";
    unsigned long written = 0;
    while (std::getline(in, line)) {
      if (line.empty()) continue;
      auto parts = split_tab(line);
      parts.resize(header.size());
      const long long offset = std::stoll(parts[static_cast<size_t>(offset_col)]);
      const std::string rank = rank_col >= 0 ? parts[static_cast<size_t>(rank_col)] : "";
      mpz_set(n, base_value);
      if (offset >= 0) {
        mpz_add_ui(n, n, static_cast<unsigned long>(offset));
      } else {
        mpz_sub_ui(n, n, static_cast<unsigned long>(-offset));
      }
      if (mpz_even_p(n) == 0) continue;
      out << "e" << opt.exponent << "_d" << offset
          << '\t' << mpz_to_string(n)
          << '\t' << offset
          << '\t' << rank << '\n';
      ++written;
      if (opt.limit != 0 && written >= opt.limit) break;
    }

    mpz_clear(n);
    mpz_clear(base_value);
    std::cerr << "written=" << written << " output=" << opt.output << "\n";
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
