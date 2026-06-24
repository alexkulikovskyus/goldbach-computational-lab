#include <algorithm>
#include <cctype>
#include <cmath>
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
  std::string n_decimal;
  std::string input_path;
  uint32_t p_limit = 10000;
  std::vector<uint32_t> wall_limits{31, 101, 1000};
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

struct PrefixStats {
  uint32_t limit = 0;
  uint32_t admissible = 0;
  uint32_t open = 0;
};

static bool starts_with(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

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

static std::vector<uint32_t> parse_limits(const std::string &value) {
  std::vector<uint32_t> limits;
  for (const std::string &part : split(value, ',')) {
    if (part.empty()) continue;
    limits.push_back(static_cast<uint32_t>(parse_u64(part, "--wallLimits")));
  }
  if (limits.empty()) throw std::runtime_error("--wallLimits must not be empty");
  std::sort(limits.begin(), limits.end());
  limits.erase(std::unique(limits.begin(), limits.end()), limits.end());
  return limits;
}

static Options parse_args(int argc, char **argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    const auto eq = arg.find('=');
    const std::string key = eq == std::string::npos ? arg : arg.substr(0, eq);
    const std::string value = eq == std::string::npos ? "" : arg.substr(eq + 1);
    if (key == "--n") opt.n_decimal = value;
    else if (key == "--input") opt.input_path = value;
    else if (key == "--pLimit") opt.p_limit = static_cast<uint32_t>(parse_u64(value, key));
    else if (key == "--wallLimits") opt.wall_limits = parse_limits(value);
    else throw std::runtime_error("unknown argument: " + arg);
  }
  if (opt.n_decimal.empty() && opt.input_path.empty()) {
    throw std::runtime_error("provide --n=<decimal N> or --input=<tsv>");
  }
  if (!opt.n_decimal.empty() && !opt.input_path.empty()) {
    throw std::runtime_error("use either --n or --input, not both");
  }
  if (opt.p_limit < 3) throw std::runtime_error("--pLimit must be >= 3");
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
  const uint32_t q_mod_6 = (n_mod_6 + 6 - (p % 6)) % 6;
  return q_mod_6 == 1 || q_mod_6 == 5;
}

static bool is_comment_or_blank(const std::string &line) {
  for (unsigned char ch : line) {
    if (std::isspace(ch)) continue;
    return ch == '#';
  }
  return true;
}

static std::vector<InputRow> load_input(const Options &opt) {
  if (!opt.n_decimal.empty()) {
    return {{"single", opt.n_decimal, "unknown", ""}};
  }

  std::ifstream in(opt.input_path);
  if (!in) throw std::runtime_error("cannot open input: " + opt.input_path);
  std::vector<InputRow> rows;
  std::string line;
  uint64_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    if (is_comment_or_blank(line)) continue;
    if (line_no == 1 && starts_with(line, "label\t")) continue;
    const auto parts = split(line, '\t');
    if (parts.size() == 1) rows.push_back({"row" + std::to_string(rows.size() + 1), parts[0], "unknown", ""});
    else if (parts.size() == 2) rows.push_back({parts[0], parts[1], "unknown", ""});
    else if (parts.size() == 3) rows.push_back({parts[0], parts[1], parts[2], ""});
    else rows.push_back({parts[0], parts[1], parts[2], parts[3]});
  }
  if (rows.empty()) throw std::runtime_error("input has no rows: " + opt.input_path);
  return rows;
}

static uint32_t first_blocker_for_p(
    const mpz_t n,
    uint32_t p,
    const std::vector<uint32_t> &small_primes,
    uint32_t max_wall_limit) {
  for (uint32_t r : small_primes) {
    if (r > max_wall_limit) break;
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
    const std::vector<uint32_t> &candidate_primes,
    const std::vector<uint32_t> &small_primes,
    uint32_t max_wall_limit) {
  const uint32_t n_mod_6 = static_cast<uint32_t>(mpz_fdiv_ui(n, 6));
  std::vector<Candidate> candidates;
  for (uint32_t p : candidate_primes) {
    if (p > p_limit) break;
    if (mpz_cmp_ui(n, static_cast<unsigned long>(p) + 1UL) <= 0) continue;
    if (!candidate_can_leave_prime_q(n_mod_6, p)) continue;
    candidates.push_back({p, first_blocker_for_p(n, p, small_primes, max_wall_limit)});
  }
  return candidates;
}

static void emit_for_limit(
    const InputRow &row,
    const mpz_t n,
    const std::vector<Candidate> &candidates,
    uint32_t p_limit,
    uint32_t wall_limit) {
  std::vector<PrefixStats> prefixes{
      {100, 0, 0},
      {500, 0, 0},
      {1000, 0, 0},
      {2000, 0, 0},
      {4000, 0, 0},
      {8000, 0, 0},
  };
  uint32_t blocked = 0;
  uint32_t open = 0;
  uint32_t first_open_p = 0;
  uint32_t first_open_index = 0;
  uint32_t longest_closed_run = 0;
  uint32_t current_closed_run = 0;
  uint64_t open_gap_sum = 0;
  uint32_t open_gap_count = 0;
  uint32_t last_open_index = 0;

  for (uint32_t i = 0; i < candidates.size(); ++i) {
    const bool is_blocked =
        candidates[i].first_blocker != 0 && candidates[i].first_blocker <= wall_limit;
    const uint32_t index = i + 1;
    for (auto &prefix : prefixes) {
      if (candidates[i].p <= prefix.limit) {
        ++prefix.admissible;
        if (!is_blocked) ++prefix.open;
      }
    }
    if (is_blocked) {
      ++blocked;
      ++current_closed_run;
      longest_closed_run = std::max(longest_closed_run, current_closed_run);
      continue;
    }

    ++open;
    current_closed_run = 0;
    if (first_open_p == 0) {
      first_open_p = candidates[i].p;
      first_open_index = index;
    }
    if (last_open_index != 0) {
      open_gap_sum += index - last_open_index;
      ++open_gap_count;
    }
    last_open_index = index;
  }

  const uint32_t admissible = static_cast<uint32_t>(candidates.size());
  const double blocked_ratio = admissible == 0 ? 0.0 : static_cast<double>(blocked) / admissible;
  const double open_ratio = admissible == 0 ? 0.0 : static_cast<double>(open) / admissible;
  const double open_gap_mean = open_gap_count == 0 ? 0.0 :
      static_cast<double>(open_gap_sum) / static_cast<double>(open_gap_count);
  const double score = admissible == 0 ? 0.0 :
      blocked_ratio * std::log1p(static_cast<double>(longest_closed_run)) /
      std::log1p(static_cast<double>(admissible));
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
            << first_open_p << '\t'
            << first_open_index << '\t'
            << longest_closed_run << '\t'
            << open_gap_mean << '\t'
            << score;
  for (const auto &prefix : prefixes) {
    const double prefix_open_ratio = prefix.admissible == 0 ? 0.0 :
        static_cast<double>(prefix.open) / static_cast<double>(prefix.admissible);
    std::cout << '\t' << prefix.open << '\t' << prefix_open_ratio;
  }
  std::cout << '\n';
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const uint32_t max_wall_limit = opt.wall_limits.back();
    const uint32_t sieve_limit = std::max(opt.p_limit, max_wall_limit);
    const auto primes = build_primes(sieve_limit);
    const auto rows = load_input(opt);

    std::cout << "label\tclass\tknownP\tnMod6\tpLimit\twallLimit\tadmissible"
              << "\tblocked\topen\tblockedRatio\topenRatio\tfirstOpenP"
              << "\tfirstOpenIndex\tlongestClosedRun\topenGapMean\twallScore"
              << "\topenLe100\topenRatioLe100"
              << "\topenLe500\topenRatioLe500"
              << "\topenLe1000\topenRatioLe1000"
              << "\topenLe2000\topenRatioLe2000"
              << "\topenLe4000\topenRatioLe4000"
              << "\topenLe8000\topenRatioLe8000\n";

    mpz_t n;
    mpz_init(n);
    for (const auto &row : rows) {
      if (mpz_set_str(n, row.n_decimal.c_str(), 10) != 0) {
        throw std::runtime_error("invalid decimal N for label " + row.label);
      }
      if (mpz_even_p(n) == 0) {
        throw std::runtime_error("N must be even for label " + row.label);
      }
      const auto candidates = build_candidates(n, opt.p_limit, primes, primes, max_wall_limit);
      for (uint32_t wall_limit : opt.wall_limits) {
        emit_for_limit(row, n, candidates, opt.p_limit, wall_limit);
      }
    }
    mpz_clear(n);
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
