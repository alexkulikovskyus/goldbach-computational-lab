#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct Options {
  std::string input = "results/wall-spectrum/frozen-adaptive-validation-summary.tsv";
  std::string output;
  std::set<std::string> scopes = {
      "r51-r100-r2000",
      "r101-r150-r2000",
      "r151-r200-r2000",
      "r201-r250-r2000",
      "r251-r300-r2000",
      "r301-r350-r2000",
      "r351-r400-r2000",
      "r401-r450-r2000",
      "r451-r500-r2000"};
};

struct Row {
  std::string scope;
  std::string policy;
  std::string params;
  uint32_t hits = 0;
  uint32_t total = 0;
  double mean_rows = 0.0;
  double covered_fraction = 0.0;
};

struct Aggregate {
  std::string policy;
  uint32_t holdouts = 0;
  uint32_t hits = 0;
  uint32_t total = 0;
  double rows_sum = 0.0;
  double fraction_sum = 0.0;
};

static std::vector<std::string> split(const std::string &text, char sep) {
  std::vector<std::string> out;
  std::string item;
  std::stringstream ss(text);
  while (std::getline(ss, item, sep)) out.push_back(item);
  return out;
}

static std::unordered_map<std::string, size_t> header_index(const std::string &line) {
  std::unordered_map<std::string, size_t> out;
  const auto parts = split(line, '\t');
  for (size_t i = 0; i < parts.size(); ++i) out[parts[i]] = i;
  return out;
}

static uint32_t parse_u32(const std::string &value, const std::string &name) {
  try {
    size_t pos = 0;
    const uint64_t out = std::stoull(value, &pos);
    if (pos != value.size() || out > UINT32_MAX) throw std::invalid_argument("bad");
    return static_cast<uint32_t>(out);
  } catch (...) {
    throw std::runtime_error("bad uint32 for " + name + ": " + value);
  }
}

static double parse_double(const std::string &value, const std::string &name) {
  try {
    size_t pos = 0;
    const double out = std::stod(value, &pos);
    if (pos != value.size()) throw std::invalid_argument("bad");
    return out;
  } catch (...) {
    throw std::runtime_error("bad double for " + name + ": " + value);
  }
}

static std::pair<uint32_t, uint32_t> parse_hit_groups(const std::string &value) {
  const auto slash = value.find('/');
  if (slash == std::string::npos) {
    throw std::runtime_error("bad hitGroups value: " + value);
  }
  return {
      parse_u32(value.substr(0, slash), "hitGroups hits"),
      parse_u32(value.substr(slash + 1), "hitGroups total")};
}

static Options parse_args(int argc, char **argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    const auto eq = arg.find('=');
    const std::string key = eq == std::string::npos ? arg : arg.substr(0, eq);
    const std::string value = eq == std::string::npos ? "" : arg.substr(eq + 1);
    if (key == "--input") {
      opt.input = value;
    } else if (key == "--output") {
      opt.output = value;
    } else if (key == "--scopes") {
      opt.scopes.clear();
      for (const auto &scope : split(value, ',')) {
        if (!scope.empty()) opt.scopes.insert(scope);
      }
    } else {
      throw std::runtime_error("unknown argument: " + arg);
    }
  }
  if (opt.input.empty()) throw std::runtime_error("need --input");
  if (opt.scopes.empty()) throw std::runtime_error("need at least one --scopes entry");
  return opt;
}

static std::string normalize_policy(const Row &row) {
  if (row.policy == "denseNMS") {
    if (row.params.find("hitR=20") != std::string::npos) return "denseNMS_strict";
    if (row.params.find("hitR=100") != std::string::npos) return "denseNMS_wide";
  }
  if (row.policy == "frozen5LayerAdaptive") return "frozen5LayerAdaptiveK3";
  return row.policy;
}

static std::vector<Row> load_rows(const Options &opt) {
  std::ifstream in(opt.input);
  if (!in) throw std::runtime_error("cannot open input: " + opt.input);

  std::string line;
  if (!std::getline(in, line)) throw std::runtime_error("empty input: " + opt.input);
  const auto h = header_index(line);
  const size_t scope_i = h.at("scope");
  const size_t policy_i = h.at("policy");
  const size_t params_i = h.at("params");
  const size_t hit_i = h.at("hitGroups");
  const size_t rows_i = h.at("meanCoveredRows");
  const size_t fraction_i = h.at("coveredFraction");

  std::vector<Row> rows;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    const auto parts = split(line, '\t');
    if (parts.size() <= std::max({scope_i, policy_i, params_i, hit_i, rows_i, fraction_i})) {
      continue;
    }
    Row row;
    row.scope = parts[scope_i];
    if (!opt.scopes.count(row.scope)) continue;
    row.policy = parts[policy_i];
    row.params = parts[params_i];
    const auto hit = parse_hit_groups(parts[hit_i]);
    row.hits = hit.first;
    row.total = hit.second;
    row.mean_rows = parse_double(parts[rows_i], "meanCoveredRows");
    row.covered_fraction = parse_double(parts[fraction_i], "coveredFraction");
    rows.push_back(std::move(row));
  }
  if (rows.empty()) throw std::runtime_error("no rows matched selected scopes");
  return rows;
}

static std::vector<Aggregate> aggregate_rows(const std::vector<Row> &rows) {
  std::map<std::string, Aggregate> by_policy;
  for (const auto &row : rows) {
    const std::string policy = normalize_policy(row);
    auto &agg = by_policy[policy];
    agg.policy = policy;
    agg.holdouts += 1;
    agg.hits += row.hits;
    agg.total += row.total;
    agg.rows_sum += row.mean_rows;
    agg.fraction_sum += row.covered_fraction;
  }

  std::vector<Aggregate> out;
  for (auto &entry : by_policy) out.push_back(entry.second);
  std::sort(out.begin(), out.end(), [](const Aggregate &a, const Aggregate &b) {
    const double ar = a.total == 0 ? 0.0 : static_cast<double>(a.hits) / a.total;
    const double br = b.total == 0 ? 0.0 : static_cast<double>(b.hits) / b.total;
    if (ar != br) return ar > br;
    const double a_rows = a.holdouts == 0 ? 0.0 : a.rows_sum / a.holdouts;
    const double b_rows = b.holdouts == 0 ? 0.0 : b.rows_sum / b.holdouts;
    if (a_rows != b_rows) return a_rows < b_rows;
    return a.policy < b.policy;
  });
  return out;
}

static void write_summary(std::ostream &out, const std::vector<Aggregate> &aggs) {
  out << "policy\tholdouts\thitGroups\ttotalGroups\thitRate\tmeanCoveredRows\tmeanCoveredFraction\n";
  out << std::fixed << std::setprecision(6);
  for (const auto &agg : aggs) {
    const double hit_rate = agg.total == 0 ? 0.0 : static_cast<double>(agg.hits) / agg.total;
    const double mean_rows = agg.holdouts == 0 ? 0.0 : agg.rows_sum / agg.holdouts;
    const double mean_fraction = agg.holdouts == 0 ? 0.0 : agg.fraction_sum / agg.holdouts;
    out << agg.policy << '\t'
        << agg.holdouts << '\t'
        << agg.hits << '\t'
        << agg.total << '\t'
        << hit_rate << '\t'
        << mean_rows << '\t'
        << mean_fraction << '\n';
  }
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const auto rows = load_rows(opt);
    const auto aggs = aggregate_rows(rows);
    if (opt.output.empty()) {
      write_summary(std::cout, aggs);
    } else {
      std::ofstream out(opt.output);
      if (!out) throw std::runtime_error("cannot write output: " + opt.output);
      write_summary(out, aggs);
    }
  } catch (const std::exception &ex) {
    std::cerr << "wall-frontier-summary: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
