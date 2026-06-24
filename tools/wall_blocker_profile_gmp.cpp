#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <gmp.h>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Options {
  std::string input_path;
  uint32_t p_limit = 8000;
  uint32_t wall_limit = 8000;
};

struct InputRow {
  std::string label;
  std::string n_decimal;
  std::string klass;
  std::string known_p;
};

struct Candidate {
  uint32_t p = 0;
  uint32_t first_blocker = 0;
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

static std::vector<std::string> split(const std::string &text, char sep) {
  std::vector<std::string> out;
  std::string item;
  std::stringstream ss(text);
  while (std::getline(ss, item, sep)) out.push_back(item);
  return out;
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
    else if (key == "--wallLimit") opt.wall_limit = static_cast<uint32_t>(parse_u64(value, key));
    else throw std::runtime_error("unknown argument: " + arg);
  }
  if (opt.input_path.empty()) throw std::runtime_error("need --input");
  if (opt.p_limit < 3) throw std::runtime_error("--pLimit must be >= 3");
  if (opt.wall_limit < 5) throw std::runtime_error("--wallLimit must be >= 5");
  return opt;
}

static bool starts_with(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

static std::vector<InputRow> load_input(const std::string &path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open input: " + path);

  std::vector<InputRow> rows;
  std::string line;
  uint64_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    if (line.empty()) continue;
    if (line_no == 1 && starts_with(line, "label\t")) continue;
    const auto parts = split(line, '\t');
    if (parts.size() < 2) continue;
    InputRow row;
    row.label = parts[0];
    row.n_decimal = parts[1];
    row.klass = parts.size() > 2 ? parts[2] : "unknown";
    row.known_p = parts.size() > 3 ? parts[3] : "";
    rows.push_back(std::move(row));
  }
  if (rows.empty()) throw std::runtime_error("no input rows");
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

static uint32_t first_blocker_for_p(
    const mpz_t n,
    uint32_t p,
    const std::vector<uint32_t> &small_primes,
    uint32_t wall_limit) {
  for (uint32_t r : small_primes) {
    if (r > wall_limit) break;
    if (r <= 3) continue;
    const uint32_t n_mod = static_cast<uint32_t>(mpz_fdiv_ui(n, r));
    if ((n_mod + r - (p % r)) % r != 0) continue;
    if (mpz_cmp_ui(n, static_cast<unsigned long>(p) + r) == 0) continue;
    return r;
  }
  return 0;
}

static std::vector<Candidate> build_candidates(
    const mpz_t n,
    uint32_t p_limit,
    uint32_t wall_limit,
    const std::vector<uint32_t> &candidate_primes,
    const std::vector<uint32_t> &small_primes) {
  const uint32_t n_mod_6 = static_cast<uint32_t>(mpz_fdiv_ui(n, 6));
  std::vector<Candidate> candidates;
  for (uint32_t p : candidate_primes) {
    if (p > p_limit) break;
    if (mpz_cmp_ui(n, static_cast<unsigned long>(p) + 1UL) <= 0) continue;
    if (!candidate_can_leave_prime_q(n_mod_6, p)) continue;
    candidates.push_back({p, first_blocker_for_p(n, p, small_primes, wall_limit)});
  }
  return candidates;
}

static void emit_profile(
    const InputRow &row,
    const mpz_t n,
    const std::vector<Candidate> &candidates,
    uint32_t p_limit,
    uint32_t wall_limit) {
  std::map<uint32_t, uint32_t> blocker_counts;
  uint32_t blocked = 0;
  uint32_t low31 = 0;
  uint32_t low101 = 0;
  uint32_t low1000 = 0;
  double log_sum = 0.0;
  double value_sum = 0.0;

  for (const auto &candidate : candidates) {
    if (candidate.first_blocker == 0 || candidate.first_blocker > wall_limit) continue;
    ++blocked;
    ++blocker_counts[candidate.first_blocker];
    if (candidate.first_blocker <= 31) ++low31;
    if (candidate.first_blocker <= 101) ++low101;
    if (candidate.first_blocker <= 1000) ++low1000;
    log_sum += std::log(static_cast<double>(candidate.first_blocker));
    value_sum += candidate.first_blocker;
  }

  uint32_t top_blocker = 0;
  uint32_t top_count = 0;
  double entropy = 0.0;
  double herfindahl = 0.0;
  for (const auto &item : blocker_counts) {
    if (item.second > top_count) {
      top_blocker = item.first;
      top_count = item.second;
    }
    const double share = blocked == 0 ? 0.0 : static_cast<double>(item.second) / blocked;
    if (share > 0.0) entropy -= share * std::log(share);
    herfindahl += share * share;
  }

  const uint32_t admissible = static_cast<uint32_t>(candidates.size());
  const uint32_t open = admissible >= blocked ? admissible - blocked : 0;
  const uint32_t unique = static_cast<uint32_t>(blocker_counts.size());
  const double blocked_ratio = admissible == 0 ? 0.0 : static_cast<double>(blocked) / admissible;
  const double open_ratio = admissible == 0 ? 0.0 : static_cast<double>(open) / admissible;
  const double unique_ratio = blocked == 0 ? 0.0 : static_cast<double>(unique) / blocked;
  const double top_share = blocked == 0 ? 0.0 : static_cast<double>(top_count) / blocked;
  const double low31_share = blocked == 0 ? 0.0 : static_cast<double>(low31) / blocked;
  const double low101_share = blocked == 0 ? 0.0 : static_cast<double>(low101) / blocked;
  const double low1000_share = blocked == 0 ? 0.0 : static_cast<double>(low1000) / blocked;
  const double norm_entropy = unique <= 1 ? 0.0 : entropy / std::log(static_cast<double>(unique));
  const double mean_blocker = blocked == 0 ? 0.0 : value_sum / blocked;
  const double geom_mean_blocker = blocked == 0 ? 0.0 : std::exp(log_sum / blocked);
  const double pressure_entropy = blocked_ratio * norm_entropy;
  const double pressure_anti_dominance = blocked_ratio * (1.0 - top_share);
  const double pressure_concentration = blocked_ratio * top_share;
  const double pressure_low31 = blocked_ratio * low31_share;
  const double pressure_low101 = blocked_ratio * low101_share;
  const double pressure_low1000 = blocked_ratio * low1000_share;
  const uint32_t n_mod_6 = static_cast<uint32_t>(mpz_fdiv_ui(n, 6));

  std::cout << row.label << '\t'
            << row.klass << '\t'
            << row.known_p << '\t'
            << n_mod_6 << '\t'
            << p_limit << '\t'
            << wall_limit << '\t'
            << admissible << '\t'
            << blocked << '\t'
            << open << '\t'
            << blocked_ratio << '\t'
            << open_ratio << '\t'
            << unique << '\t'
            << unique_ratio << '\t'
            << entropy << '\t'
            << norm_entropy << '\t'
            << herfindahl << '\t'
            << top_blocker << '\t'
            << top_count << '\t'
            << top_share << '\t'
            << low31_share << '\t'
            << low101_share << '\t'
            << low1000_share << '\t'
            << mean_blocker << '\t'
            << geom_mean_blocker << '\t'
            << pressure_entropy << '\t'
            << pressure_anti_dominance << '\t'
            << pressure_concentration << '\t'
            << pressure_low31 << '\t'
            << pressure_low101 << '\t'
            << pressure_low1000 << '\n';
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const auto rows = load_input(opt.input_path);
    const auto candidate_primes = build_primes(opt.p_limit);
    const auto small_primes = build_primes(opt.wall_limit);

    std::cout << "label\tclass\tknownP\tnMod6\tpLimit\twallLimit"
              << "\tadmissible\tblocked\topen\tblockedRatio\topenRatio"
              << "\tuniqueBlockers\tuniqueBlockerRatio\tblockerEntropy"
              << "\tnormalizedBlockerEntropy\tblockerHerfindahl"
              << "\ttopBlocker\ttopBlockerCount\ttopBlockerShare"
              << "\tlow31Share\tlow101Share\tlow1000Share"
              << "\tmeanBlocker\tgeomMeanBlocker"
              << "\tpressureEntropy\tpressureAntiDominance"
              << "\tpressureConcentration\tpressureLow31"
              << "\tpressureLow101\tpressureLow1000\n";

    mpz_t n;
    mpz_init(n);
    for (const auto &row : rows) {
      if (mpz_set_str(n, row.n_decimal.c_str(), 10) != 0) {
        throw std::runtime_error("bad N for label: " + row.label);
      }
      const auto candidates = build_candidates(
          n, opt.p_limit, opt.wall_limit, candidate_primes, small_primes);
      emit_profile(row, n, candidates, opt.p_limit, opt.wall_limit);
    }
    mpz_clear(n);
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
