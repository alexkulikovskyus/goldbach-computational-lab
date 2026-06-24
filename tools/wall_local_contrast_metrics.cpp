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

struct Row {
  std::string label;
  std::string klass;
  std::string source_p;
  int64_t offset = 0;
  uint32_t exact_p = 0;
  bool positive = false;
  double blocked_ratio = 0.0;
  std::unordered_map<std::string, double> scores;
};

struct ScoreSpec {
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

static int64_t parse_offset(const std::string &label) {
  const auto pos = label.rfind('_');
  if (pos == std::string::npos) return 0;
  const std::string suffix = label.substr(pos + 1);
  if (suffix == "peak") return 0;
  if (suffix.size() >= 2 && suffix[0] == 'm') return -static_cast<int64_t>(parse_u64(suffix.substr(1), "offset"));
  if (suffix.size() >= 2 && suffix[0] == 'p') return static_cast<int64_t>(parse_u64(suffix.substr(1), "offset"));
  return 0;
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

static std::vector<Row> load_rows(
    const Options &opt,
    const std::unordered_map<std::string, LabelRow> &labels) {
  std::ifstream in(opt.features_path);
  if (!in) throw std::runtime_error("cannot open features: " + opt.features_path);

  std::string line;
  if (!std::getline(in, line)) throw std::runtime_error("empty features file");
  const auto h = header_index(line);
  const size_t label_i = h.at("label");
  const size_t class_i = h.at("class");
  const size_t br_i = h.at("blockedRatio");

  std::vector<Row> rows;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    const auto parts = split(line, '\t');
    if (parts.size() <= std::max({label_i, class_i, br_i})) continue;
    const auto found = labels.find(parts[label_i]);
    if (found == labels.end() || !found->second.verified) continue;

    Row row;
    row.label = parts[label_i];
    row.klass = parts[class_i];
    row.source_p = found->second.source_p;
    row.offset = parse_offset(row.label);
    row.exact_p = found->second.exact_p;
    row.positive = row.exact_p >= opt.positive_p;
    row.blocked_ratio = parse_double(parts[br_i]);
    row.scores["blockedRatio"] = row.blocked_ratio;
    rows.push_back(std::move(row));
  }
  if (rows.empty()) throw std::runtime_error("no joined rows");
  return rows;
}

static std::unordered_map<std::string, std::vector<size_t>> group_indices(const std::vector<Row> &rows) {
  std::unordered_map<std::string, std::vector<size_t>> groups;
  for (size_t i = 0; i < rows.size(); ++i) groups[rows[i].source_p].push_back(i);
  for (auto &item : groups) {
    std::sort(item.second.begin(), item.second.end(), [&](size_t a, size_t b) {
      if (rows[a].offset != rows[b].offset) return rows[a].offset < rows[b].offset;
      return rows[a].label < rows[b].label;
    });
  }
  return groups;
}

static bool in_band(int64_t distance, uint32_t inner, uint32_t outer) {
  const uint64_t d = static_cast<uint64_t>(std::llabs(distance));
  if (d == 0) return false;
  return d > inner && d <= outer;
}

static void add_band_scores(
    std::vector<Row> &rows,
    const std::unordered_map<std::string, std::vector<size_t>> &groups,
    uint32_t inner,
    uint32_t outer) {
  const std::string prefix = inner == 0
      ? "localR" + std::to_string(outer)
      : "shell" + std::to_string(inner) + "_" + std::to_string(outer);
  for (const auto &item : groups) {
    const auto &idxs = item.second;
    for (size_t idx : idxs) {
      double sum = 0.0;
      double sum_sq = 0.0;
      uint32_t count = 0;
      for (size_t other : idxs) {
        if (!in_band(rows[other].offset - rows[idx].offset, inner, outer)) continue;
        const double value = rows[other].blocked_ratio;
        sum += value;
        sum_sq += value * value;
        ++count;
      }
      if (count == 0) {
        rows[idx].scores[prefix + "_delta"] = 0.0;
        rows[idx].scores[prefix + "_z"] = 0.0;
        rows[idx].scores[prefix + "_neighborCount"] = 0.0;
        continue;
      }
      const double mean = sum / count;
      const double variance = std::max(0.0, sum_sq / count - mean * mean);
      const double sd = std::sqrt(variance);
      const double delta = rows[idx].blocked_ratio - mean;
      rows[idx].scores[prefix + "_delta"] = delta;
      rows[idx].scores[prefix + "_z"] = sd == 0.0 ? 0.0 : delta / sd;
      rows[idx].scores[prefix + "_neighborCount"] = count;
    }
  }
}

static uint32_t count_positives(const std::vector<Row> &rows) {
  uint32_t out = 0;
  for (const auto &row : rows) {
    if (row.positive) ++out;
  }
  return out;
}

static uint32_t top_count(size_t rows, double percent) {
  return std::max<uint32_t>(1, static_cast<uint32_t>(std::ceil(rows * percent / 100.0)));
}

static double score_value(const Row &row, const ScoreSpec &score) {
  const auto found = row.scores.find(score.name);
  return found == row.scores.end() ? 0.0 : found->second;
}

static void sort_by_score(std::vector<Row> &rows, const ScoreSpec &score) {
  std::sort(rows.begin(), rows.end(), [&](const Row &a, const Row &b) {
    const double av = score_value(a, score);
    const double bv = score_value(b, score);
    if (av != bv) return score.descending ? av > bv : av < bv;
    return a.label < b.label;
  });
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const auto labels = load_labels(opt);
    auto rows = load_rows(opt, labels);
    const auto groups = group_indices(rows);

    for (uint32_t radius : {20u, 50u, 100u, 200u}) add_band_scores(rows, groups, 0, radius);
    add_band_scores(rows, groups, 20, 100);
    add_band_scores(rows, groups, 50, 200);

    const std::vector<ScoreSpec> scores{
        {"blockedRatio", true},
        {"localR20_delta", true},
        {"localR20_z", true},
        {"localR50_delta", true},
        {"localR50_z", true},
        {"localR100_delta", true},
        {"localR100_z", true},
        {"localR200_delta", true},
        {"localR200_z", true},
        {"shell20_100_delta", true},
        {"shell20_100_z", true},
        {"shell50_200_delta", true},
        {"shell50_200_z", true},
    };

    const uint32_t positives = count_positives(rows);
    const std::vector<double> percents{1.0, 0.1, 0.01};
    std::cout << "metric\tscore\tcutoff\tselectedRows\tcapturedPositives"
              << "\ttotalPositives\trandomExpected\tenrichment\n";

    for (const auto &score : scores) {
      std::vector<Row> sorted = rows;
      sort_by_score(sorted, score);
      for (double percent : percents) {
        const uint32_t k = std::min<uint32_t>(top_count(sorted.size(), percent), sorted.size());
        uint32_t captured = 0;
        for (uint32_t i = 0; i < k; ++i) {
          if (sorted[i].positive) ++captured;
        }
        const double random_expected =
            static_cast<double>(k) * static_cast<double>(positives) / static_cast<double>(rows.size());
        const double enrichment = random_expected == 0.0 ? 0.0 : captured / random_expected;
        std::cout << "top_capture\t" << score.name << '\t'
                  << percent << '\t' << k << '\t' << captured << '\t'
                  << positives << '\t' << random_expected << '\t'
                  << enrichment << '\n';
      }
    }

    const std::vector<uint32_t> rank_limits{1, 5, 10};
    for (const auto &score : scores) {
      std::map<uint32_t, uint32_t> captured_by_rank;
      std::map<uint32_t, double> random_expected_by_rank;
      uint32_t group_count = 0;
      double rank_sum = 0.0;
      for (const auto &item : groups) {
        std::vector<Row> group_rows;
        for (size_t idx : item.second) group_rows.push_back(rows[idx]);
        sort_by_score(group_rows, score);

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
        std::cout << "group_rank_le\t" << score.name << '\t'
                  << rank_limit << '\t' << group_count << '\t'
                  << captured_by_rank[rank_limit] << '\t' << group_count << '\t'
                  << random_expected << '\t' << enrichment << '\n';
      }
      const double mean_rank = group_count == 0 ? 0.0 : rank_sum / group_count;
      std::cout << "group_mean_rank\t" << score.name << "\t0\t"
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
