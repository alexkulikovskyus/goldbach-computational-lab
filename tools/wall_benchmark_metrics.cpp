#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct Options {
  std::string labels_path;
  std::string features_path;
  uint32_t positive_p = 8419;
};

struct LabelRow {
  std::string klass;
  std::string source_p;
  uint32_t exact_p = 0;
  bool verified = false;
};

struct FeatureRow {
  std::string label;
  std::string klass;
  std::string source_p;
  uint32_t exact_p = 0;
  bool positive = false;
  std::unordered_map<std::string, double> values;
};

struct FeatureSpec {
  std::string name;
  bool descending = true;
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

static double parse_double(const std::string &value) {
  try {
    size_t pos = 0;
    double out = std::stod(value, &pos);
    if (pos != value.size()) throw std::invalid_argument("bad");
    return out;
  } catch (...) {
    return 0.0;
  }
}

static Options parse_args(int argc, char **argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    const auto eq = arg.find('=');
    const std::string key = eq == std::string::npos ? arg : arg.substr(0, eq);
    const std::string value = eq == std::string::npos ? "" : arg.substr(eq + 1);
    if (key == "--labels") opt.labels_path = value;
    else if (key == "--features") opt.features_path = value;
    else if (key == "--positiveP") opt.positive_p = static_cast<uint32_t>(parse_u64(value, key));
    else throw std::runtime_error("unknown argument: " + arg);
  }
  if (opt.labels_path.empty()) throw std::runtime_error("need --labels");
  if (opt.features_path.empty()) throw std::runtime_error("need --features");
  return opt;
}

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

static std::unordered_map<std::string, LabelRow> load_labels(const Options &opt) {
  std::ifstream in(opt.labels_path);
  if (!in) throw std::runtime_error("cannot open labels: " + opt.labels_path);

  std::string line;
  if (!std::getline(in, line)) throw std::runtime_error("empty labels file");
  const auto h = header_index(line);
  const size_t label_i = h.at("label");
  const size_t class_i = h.at("class");
  const size_t source_i = h.at("sourceP");
  const size_t exact_i = h.at("exactP");
  const size_t verified_i = h.at("verified");

  std::unordered_map<std::string, LabelRow> labels;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    const auto parts = split(line, '\t');
    if (parts.size() <= std::max({label_i, class_i, source_i, exact_i, verified_i})) continue;
    labels[parts[label_i]] = {
        parts[class_i],
        parts[source_i],
        static_cast<uint32_t>(parse_u64(parts[exact_i], "exactP")),
        parse_u64(parts[verified_i], "verified") != 0};
  }
  if (labels.empty()) throw std::runtime_error("no label rows");
  return labels;
}

static std::vector<FeatureRow> load_features(
    const Options &opt,
    const std::unordered_map<std::string, LabelRow> &labels) {
  std::ifstream in(opt.features_path);
  if (!in) throw std::runtime_error("cannot open features: " + opt.features_path);

  std::string line;
  if (!std::getline(in, line)) throw std::runtime_error("empty features file");
  const auto h = header_index(line);
  const size_t label_i = h.at("label");
  const size_t class_i = h.at("class");

  const std::vector<std::string> numeric_columns{
      "blockedRatio", "openRatio", "firstOpenP", "firstOpenIndex",
      "longestClosedRun", "openGapMean", "wallScore",
      "openLe100", "openRatioLe100",
      "openLe500", "openRatioLe500",
      "openLe1000", "openRatioLe1000",
      "openLe2000", "openRatioLe2000",
      "openLe4000", "openRatioLe4000",
      "openLe8000", "openRatioLe8000",
      "uniqueBlockers", "uniqueBlockerRatio", "blockerEntropy",
      "normalizedBlockerEntropy", "blockerHerfindahl",
      "topBlockerShare", "low31Share", "low101Share", "low1000Share",
      "meanBlocker", "geomMeanBlocker",
      "pressureEntropy", "pressureAntiDominance", "pressureConcentration",
      "pressureLow31", "pressureLow101", "pressureLow1000"};

  std::vector<FeatureRow> rows;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    const auto parts = split(line, '\t');
    if (parts.size() <= std::max(label_i, class_i)) continue;
    const auto found = labels.find(parts[label_i]);
    if (found == labels.end() || !found->second.verified) continue;

    FeatureRow row;
    row.label = parts[label_i];
    row.klass = parts[class_i];
    row.source_p = found->second.source_p;
    row.exact_p = found->second.exact_p;
    row.positive = row.exact_p >= opt.positive_p;
    for (const auto &name : numeric_columns) {
      const auto idx = h.find(name);
      if (idx != h.end() && idx->second < parts.size()) {
        const double value = parse_double(parts[idx->second]);
        row.values[name] = value;
        row.values[name + "High"] = value;
        row.values[name + "Low"] = value;
      }
    }
    rows.push_back(std::move(row));
  }
  if (rows.empty()) throw std::runtime_error("no joined rows");
  return rows;
}

static uint32_t count_positives(const std::vector<FeatureRow> &rows) {
  uint32_t out = 0;
  for (const auto &row : rows) {
    if (row.positive) ++out;
  }
  return out;
}

static uint32_t top_count(size_t rows, double percent) {
  return std::max<uint32_t>(1, static_cast<uint32_t>(std::ceil(rows * percent / 100.0)));
}

static double value_for_feature(const FeatureRow &row, const FeatureSpec &feature) {
  const auto found = row.values.find(feature.name);
  return found == row.values.end() ? 0.0 : found->second;
}

static void sort_by_feature(std::vector<FeatureRow> &rows, const FeatureSpec &feature) {
  std::sort(rows.begin(), rows.end(), [&](const FeatureRow &a, const FeatureRow &b) {
    const double av = value_for_feature(a, feature);
    const double bv = value_for_feature(b, feature);
    if (av != bv) return feature.descending ? av > bv : av < bv;
    return a.label < b.label;
  });
}

static std::unordered_map<std::string, std::vector<FeatureRow>> group_by_source(
    const std::vector<FeatureRow> &rows) {
  std::unordered_map<std::string, std::vector<FeatureRow>> groups;
  for (const auto &row : rows) groups[row.source_p].push_back(row);
  return groups;
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const auto labels = load_labels(opt);
    const auto rows = load_features(opt, labels);
    const uint32_t positives = count_positives(rows);

    const std::vector<FeatureSpec> features{
        {"wallScore", true},
        {"blockedRatio", true},
        {"openGapMean", true},
        {"longestClosedRun", true},
        {"firstOpenIndex", true},
        {"firstOpenP", true},
        {"openRatio", false},
        {"openLe100", false},
        {"openLe500", false},
        {"openLe1000", false},
        {"openLe2000", false},
        {"openLe4000", false},
        {"openLe8000", false},
        {"openRatioLe100", false},
        {"openRatioLe500", false},
        {"openRatioLe1000", false},
        {"openRatioLe2000", false},
        {"openRatioLe4000", false},
        {"openRatioLe8000", false},
        {"uniqueBlockers", true},
        {"uniqueBlockersLow", false},
        {"uniqueBlockerRatio", true},
        {"uniqueBlockerRatioLow", false},
        {"blockerEntropy", true},
        {"blockerEntropyLow", false},
        {"normalizedBlockerEntropy", true},
        {"normalizedBlockerEntropyLow", false},
        {"blockerHerfindahl", false},
        {"blockerHerfindahlHigh", true},
        {"topBlockerShare", false},
        {"topBlockerShareHigh", true},
        {"low31Share", false},
        {"low31ShareHigh", true},
        {"low101Share", false},
        {"low101ShareHigh", true},
        {"low1000Share", false},
        {"low1000ShareHigh", true},
        {"meanBlocker", true},
        {"meanBlockerLow", false},
        {"geomMeanBlocker", true},
        {"geomMeanBlockerLow", false},
        {"pressureEntropy", true},
        {"pressureEntropyLow", false},
        {"pressureAntiDominance", true},
        {"pressureAntiDominanceLow", false},
        {"pressureConcentration", true},
        {"pressureConcentrationLow", false},
        {"pressureLow31", true},
        {"pressureLow31Low", false},
        {"pressureLow101", true},
        {"pressureLow101Low", false},
        {"pressureLow1000", true},
        {"pressureLow1000Low", false},
    };
    const std::vector<double> percents{1.0, 0.1, 0.01};

    std::cout << "metric\tfeature\tcutoff\tselectedRows\tcapturedPositives"
              << "\ttotalPositives\trandomExpected\tenrichment\n";
    for (const auto &feature : features) {
      std::vector<FeatureRow> sorted = rows;
      sort_by_feature(sorted, feature);
      for (double percent : percents) {
        const uint32_t k = std::min<uint32_t>(top_count(sorted.size(), percent), sorted.size());
        uint32_t captured = 0;
        for (uint32_t i = 0; i < k; ++i) {
          if (sorted[i].positive) ++captured;
        }
        const double random_expected =
            static_cast<double>(k) * static_cast<double>(positives) / static_cast<double>(rows.size());
        const double enrichment = random_expected == 0.0 ? 0.0 : captured / random_expected;
        std::cout << "top_capture\t" << feature.name << '\t'
                  << percent << '\t' << k << '\t' << captured << '\t'
                  << positives << '\t' << random_expected << '\t'
                  << enrichment << '\n';
      }
    }

    const auto groups = group_by_source(rows);
    const std::vector<uint32_t> rank_limits{1, 5, 10};
    for (const auto &feature : features) {
      std::map<uint32_t, uint32_t> captured_by_rank;
      std::map<uint32_t, double> random_expected_by_rank;
      uint32_t group_count = 0;
      double rank_sum = 0.0;
      for (auto item : groups) {
        auto group_rows = item.second;
        sort_by_feature(group_rows, feature);
        uint32_t peak_rank = 0;
        for (uint32_t i = 0; i < group_rows.size(); ++i) {
          if (group_rows[i].klass == "peak") {
            peak_rank = i + 1;
            break;
          }
        }
        if (peak_rank == 0) continue;
        ++group_count;
        rank_sum += peak_rank;
        for (uint32_t rank_limit : rank_limits) {
          if (peak_rank <= rank_limit) ++captured_by_rank[rank_limit];
          random_expected_by_rank[rank_limit] +=
              static_cast<double>(std::min<uint32_t>(rank_limit, group_rows.size())) /
              static_cast<double>(group_rows.size());
        }
      }
      for (uint32_t rank_limit : rank_limits) {
        const double random_expected = random_expected_by_rank[rank_limit];
        const double enrichment = random_expected == 0.0
            ? 0.0
            : captured_by_rank[rank_limit] / random_expected;
        std::cout << "group_rank_le\t" << feature.name << '\t'
                  << rank_limit << '\t' << group_count << '\t'
                  << captured_by_rank[rank_limit] << '\t' << group_count << '\t'
                  << random_expected << '\t' << enrichment << '\n';
      }
      const double mean_rank = group_count == 0 ? 0.0 : rank_sum / group_count;
      std::cout << "group_mean_rank\t" << feature.name << "\t0\t"
                << group_count << "\t0\t" << group_count << '\t'
                << mean_rank << "\t0\n";
    }

    std::cout << "summary\trows\t0\t" << rows.size() << "\t0\t"
              << positives << "\t0\t0\n";
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
