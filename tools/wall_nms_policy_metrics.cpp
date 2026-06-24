#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
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
      uint32_t count = 0;
      for (size_t other : idxs) {
        if (!in_band(rows[other].offset - rows[idx].offset, inner, outer)) continue;
        sum += rows[other].blocked_ratio;
        ++count;
      }
      const double mean = count == 0 ? 0.0 : sum / count;
      rows[idx].scores[prefix + "_delta"] = rows[idx].blocked_ratio - mean;
    }
  }
}

static double score_value(const Row &row, const ScoreSpec &score) {
  const auto found = row.scores.find(score.name);
  return found == row.scores.end() ? 0.0 : found->second;
}

static std::vector<Row> select_nms(
    std::vector<Row> group_rows,
    const ScoreSpec &score,
    uint32_t suppress_radius,
    uint32_t top_k) {
  std::sort(group_rows.begin(), group_rows.end(), [&](const Row &a, const Row &b) {
    const double av = score_value(a, score);
    const double bv = score_value(b, score);
    if (av != bv) return score.descending ? av > bv : av < bv;
    return a.label < b.label;
  });

  std::vector<Row> selected;
  for (const auto &candidate : group_rows) {
    bool suppressed = false;
    for (const auto &chosen : selected) {
      if (std::llabs(candidate.offset - chosen.offset) <= static_cast<int64_t>(suppress_radius)) {
        suppressed = true;
        break;
      }
    }
    if (suppressed) continue;
    selected.push_back(candidate);
    if (selected.size() >= top_k) break;
  }
  return selected;
}

static uint32_t covered_rows(const std::vector<Row> &group_rows, const std::vector<Row> &selected, uint32_t hit_radius) {
  std::set<std::string> covered;
  for (const auto &row : group_rows) {
    for (const auto &center : selected) {
      if (std::llabs(row.offset - center.offset) <= static_cast<int64_t>(hit_radius)) {
        covered.insert(row.label);
        break;
      }
    }
  }
  return static_cast<uint32_t>(covered.size());
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
        {"localR50_delta", true},
        {"localR100_delta", true},
        {"localR200_delta", true},
        {"shell20_100_delta", true},
        {"shell50_200_delta", true},
    };
    const std::vector<uint32_t> suppress_radii{20, 50, 100, 200};
    const std::vector<uint32_t> top_ks{1, 2, 3, 5, 10};
    const std::vector<uint32_t> hit_radii{10, 20, 50, 100};

    std::cout << "score\tsuppressRadius\ttopK\thitRadius\tgroups\thitGroups"
              << "\tmeanSelected\tmeanCoveredRows\tmeanCoveredFraction"
              << "\trandomExpectedGroups\tenrichment\n";

    for (const auto &score : scores) {
      for (uint32_t suppress_radius : suppress_radii) {
        for (uint32_t top_k : top_ks) {
          for (uint32_t hit_radius : hit_radii) {
            uint32_t group_count = 0;
            uint32_t hit_groups = 0;
            double selected_sum = 0.0;
            double covered_sum = 0.0;
            double covered_fraction_sum = 0.0;

            for (const auto &item : groups) {
              std::vector<Row> group_rows;
              group_rows.reserve(item.second.size());
              bool has_peak = false;
              for (size_t idx : item.second) {
                group_rows.push_back(rows[idx]);
                if (rows[idx].klass == "peak") has_peak = true;
              }
              if (!has_peak || group_rows.empty()) continue;

              const auto selected = select_nms(group_rows, score, suppress_radius, top_k);
              bool hit = false;
              for (const auto &center : selected) {
                if (std::llabs(center.offset) <= static_cast<int64_t>(hit_radius)) {
                  hit = true;
                  break;
                }
              }
              const uint32_t covered = covered_rows(group_rows, selected, hit_radius);
              ++group_count;
              if (hit) ++hit_groups;
              selected_sum += selected.size();
              covered_sum += covered;
              covered_fraction_sum += static_cast<double>(covered) / group_rows.size();
            }

            const double mean_selected = group_count == 0 ? 0.0 : selected_sum / group_count;
            const double mean_covered = group_count == 0 ? 0.0 : covered_sum / group_count;
            const double mean_covered_fraction = group_count == 0 ? 0.0 : covered_fraction_sum / group_count;
            const double random_expected = covered_fraction_sum;
            const double enrichment = random_expected == 0.0 ? 0.0 : hit_groups / random_expected;

            std::cout << score.name << '\t' << suppress_radius << '\t'
                      << top_k << '\t' << hit_radius << '\t'
                      << group_count << '\t' << hit_groups << '\t'
                      << mean_selected << '\t' << mean_covered << '\t'
                      << mean_covered_fraction << '\t' << random_expected
                      << '\t' << enrichment << '\n';
          }
        }
      }
    }
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
