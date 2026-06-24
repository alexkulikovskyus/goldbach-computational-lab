#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <gmp.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Options {
  std::string input;
  std::string output;
  uint32_t p_limit = 1000000;
  uint32_t small_limit = 250000;
  uint32_t bucket_size = 100000;
};

struct Row {
  std::string label;
  std::string n;
  std::string offset;
  std::string known_p;
};

struct Road {
  uint32_t n_mod6 = 0;
  uint32_t admissible = 0;
  uint32_t blocked_small = 0;
  uint32_t survivors = 0;
  uint32_t first_p = 0;
  uint32_t last_p = 0;
  uint32_t max_gap = 0;
  double mean_gap = 0.0;
  double first20_gap = 0.0;
  double last20_gap = 0.0;
  uint32_t p_at_500 = 0;
  uint32_t p_at_1000 = 0;
  uint32_t p_at_1500 = 0;
  uint32_t p_at_2000 = 0;
  uint32_t p_at_2500 = 0;
  uint32_t p_at_3000 = 0;
  std::vector<uint32_t> buckets;
};

static uint64_t parse_u64(const std::string &value, const std::string &name) {
  try {
    size_t pos = 0;
    const uint64_t out = std::stoull(value, &pos);
    if (pos != value.size()) throw std::invalid_argument("bad");
    return out;
  } catch (...) {
    throw std::runtime_error("bad unsigned integer for " + name + ": " + value);
  }
}

static Options parse_args(int argc, char **argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const auto eq = arg.find('=');
    const std::string key = eq == std::string::npos ? arg : arg.substr(0, eq);
    const std::string value = eq == std::string::npos ? "" : arg.substr(eq + 1);
    if (key == "--input") opt.input = value;
    else if (key == "--output") opt.output = value;
    else if (key == "--pLimit") opt.p_limit = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--smallLimit") opt.small_limit = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--bucketSize") opt.bucket_size = static_cast<uint32_t>(parse_u64(value, key));
    else throw std::runtime_error("unknown argument: " + arg);
  }
  if (opt.input.empty()) throw std::runtime_error("need --input");
  if (opt.output.empty()) throw std::runtime_error("need --output");
  if (opt.p_limit < 3) throw std::runtime_error("--pLimit must be >= 3");
  if (opt.bucket_size == 0) throw std::runtime_error("--bucketSize must be > 0");
  return opt;
}

static std::vector<std::string> split_tab(const std::string &line) {
  std::vector<std::string> out;
  std::stringstream ss(line);
  std::string item;
  while (std::getline(ss, item, '\t')) out.push_back(item);
  return out;
}

static int find_col(const std::vector<std::string> &header, const std::string &name) {
  for (size_t i = 0; i < header.size(); ++i) {
    if (header[i] == name) return static_cast<int>(i);
  }
  return -1;
}

static std::vector<Row> read_rows(const std::string &path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot read " + path);
  std::string line;
  if (!std::getline(in, line)) throw std::runtime_error("empty input " + path);
  const auto header = split_tab(line);
  const int label_col = find_col(header, "label");
  int n_col = find_col(header, "n");
  if (n_col < 0) n_col = find_col(header, "N");
  int offset_col = find_col(header, "offset");
  if (offset_col < 0) offset_col = find_col(header, "class");
  int known_col = find_col(header, "exactP");
  if (known_col < 0) known_col = find_col(header, "knownP");
  if (known_col < 0) known_col = find_col(header, "sourceP");
  if (label_col < 0 || n_col < 0) throw std::runtime_error("input needs label and n/N columns");

  std::vector<Row> rows;
  uint64_t row_no = 0;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    auto parts = split_tab(line);
    parts.resize(header.size());
    ++row_no;
    Row row;
    row.label = parts[static_cast<size_t>(label_col)];
    if (row.label.empty()) row.label = "row" + std::to_string(row_no);
    row.n = parts[static_cast<size_t>(n_col)];
    row.offset = offset_col >= 0 ? parts[static_cast<size_t>(offset_col)] : "";
    row.known_p = known_col >= 0 ? parts[static_cast<size_t>(known_col)] : "";
    rows.push_back(std::move(row));
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

static double mean_range(const std::vector<uint32_t> &values, size_t first, size_t last) {
  if (first >= last || first >= values.size()) return 0.0;
  last = std::min(last, values.size());
  double sum = 0.0;
  for (size_t i = first; i < last; ++i) sum += values[i];
  return sum / static_cast<double>(last - first);
}

static void set_milestone(Road &road, uint32_t survivor_index, uint32_t p) {
  if (survivor_index == 500) road.p_at_500 = p;
  else if (survivor_index == 1000) road.p_at_1000 = p;
  else if (survivor_index == 1500) road.p_at_1500 = p;
  else if (survivor_index == 2000) road.p_at_2000 = p;
  else if (survivor_index == 2500) road.p_at_2500 = p;
  else if (survivor_index == 3000) road.p_at_3000 = p;
}

static Road score_road(
    const Row &row,
    const Options &opt,
    const std::vector<uint32_t> &primes) {
  mpz_t n;
  mpz_init(n);
  if (mpz_set_str(n, row.n.c_str(), 10) != 0) {
    mpz_clear(n);
    throw std::runtime_error("bad N for " + row.label);
  }

  Road road;
  road.n_mod6 = static_cast<uint32_t>(mpz_fdiv_ui(n, 6));
  road.buckets.assign(static_cast<size_t>(opt.p_limit / opt.bucket_size) + 1, 0);

  std::vector<uint8_t> q_blocked(static_cast<size_t>(opt.p_limit) + 1, 0);
  for (uint32_t r : primes) {
    if (r > opt.small_limit) break;
    if (r <= 3) continue;
    uint32_t first = static_cast<uint32_t>(mpz_fdiv_ui(n, r));
    if (first < 2) first += ((2 - first + r - 1) / r) * r;
    for (uint32_t p = first; p <= opt.p_limit; p += r) q_blocked[p] = 1;
  }
  mpz_clear(n);

  std::vector<uint32_t> gaps;
  uint32_t prev = 0;
  for (uint32_t p : primes) {
    if (p > opt.p_limit) break;
    if (!candidate_can_leave_prime_q(road.n_mod6, p)) continue;
    ++road.admissible;
    if (q_blocked[p]) {
      ++road.blocked_small;
      continue;
    }
    ++road.survivors;
    if (road.first_p == 0) road.first_p = p;
    if (prev != 0) {
      const uint32_t gap = p - prev;
      gaps.push_back(gap);
      road.max_gap = std::max(road.max_gap, gap);
    }
    prev = p;
    road.last_p = p;
    set_milestone(road, road.survivors, p);
    const size_t bucket = static_cast<size_t>(p / opt.bucket_size);
    if (bucket >= road.buckets.size()) road.buckets.resize(bucket + 1, 0);
    ++road.buckets[bucket];
  }

  road.mean_gap = mean_range(gaps, 0, gaps.size());
  road.first20_gap = mean_range(gaps, 0, std::min<size_t>(20, gaps.size()));
  road.last20_gap = gaps.size() <= 20 ? road.mean_gap : mean_range(gaps, gaps.size() - 20, gaps.size());
  return road;
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const auto rows = read_rows(opt.input);
    const auto primes = build_primes(std::max(opt.p_limit, opt.small_limit));
    std::ofstream out(opt.output);
    if (!out) throw std::runtime_error("cannot write " + opt.output);

    const size_t bucket_count = static_cast<size_t>(opt.p_limit / opt.bucket_size) + 1;
    out << "label\toffset\tknownP\tnMod6\tpLimit\tsmallLimit"
        << "\tadmissible\tblockedSmall\tsurvivors\tsurvivorsPer100kP"
        << "\tfirstP\tlastP\tspan\tmeanGap\tfirst20Gap\tlast20Gap\tmaxGap"
        << "\tpAt500\tpAt1000\tpAt1500\tpAt2000\tpAt2500\tpAt3000";
    for (size_t i = 0; i < bucket_count; ++i) out << "\tbucket" << (i * opt.bucket_size);
    out << "\n";

    out << std::fixed << std::setprecision(6);
    for (const Row &row : rows) {
      const auto road = score_road(row, opt, primes);
      const double per_100k = opt.p_limit == 0 ? 0.0 :
          static_cast<double>(road.survivors) * 100000.0 / static_cast<double>(opt.p_limit);
      const uint32_t span = road.first_p == 0 ? 0 : road.last_p - road.first_p;
      out << row.label << '\t'
          << row.offset << '\t'
          << row.known_p << '\t'
          << road.n_mod6 << '\t'
          << opt.p_limit << '\t'
          << opt.small_limit << '\t'
          << road.admissible << '\t'
          << road.blocked_small << '\t'
          << road.survivors << '\t'
          << per_100k << '\t'
          << road.first_p << '\t'
          << road.last_p << '\t'
          << span << '\t'
          << road.mean_gap << '\t'
          << road.first20_gap << '\t'
          << road.last20_gap << '\t'
          << road.max_gap << '\t'
          << road.p_at_500 << '\t'
          << road.p_at_1000 << '\t'
          << road.p_at_1500 << '\t'
          << road.p_at_2000 << '\t'
          << road.p_at_2500 << '\t'
          << road.p_at_3000;
      for (size_t i = 0; i < bucket_count; ++i) {
        const uint32_t value = i < road.buckets.size() ? road.buckets[i] : 0;
        out << '\t' << value;
      }
      out << '\n';
    }
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
