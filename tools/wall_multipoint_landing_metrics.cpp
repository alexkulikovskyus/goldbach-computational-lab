#include <algorithm>
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
};

struct LabelRow {
  std::string klass;
  std::string source_p;
  bool verified = false;
};

struct Row {
  std::string label;
  std::string klass;
  std::string source_p;
  int64_t offset = 0;
  double blocked_ratio = 0.0;
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
  const size_t verified_i = h.at("verified");

  std::unordered_map<std::string, LabelRow> labels;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    const auto parts = split(line, '\t');
    if (parts.size() <= std::max({label_i, class_i, source_i, verified_i})) continue;
    labels[parts[label_i]] = {
        parts[class_i],
        parts[source_i],
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
    row.blocked_ratio = parse_double(parts[br_i]);
    rows.push_back(std::move(row));
  }
  if (rows.empty()) throw std::runtime_error("no joined rows");
  return rows;
}

static std::unordered_map<std::string, std::vector<Row>> group_rows(const std::vector<Row> &rows) {
  std::unordered_map<std::string, std::vector<Row>> groups;
  for (const auto &row : rows) groups[row.source_p].push_back(row);
  for (auto &item : groups) {
    std::sort(item.second.begin(), item.second.end(), [](const Row &a, const Row &b) {
      if (a.offset != b.offset) return a.offset < b.offset;
      return a.label < b.label;
    });
  }
  return groups;
}

static int64_t positive_mod(int64_t value, int64_t mod) {
  const int64_t out = value % mod;
  return out < 0 ? out + mod : out;
}

static std::vector<Row> select_anchors(
    const std::vector<Row> &rows,
    uint32_t anchor_step,
    uint32_t phase,
    uint32_t top_k) {
  std::vector<Row> anchors;
  for (const auto &row : rows) {
    if (positive_mod(row.offset, anchor_step) != phase) continue;
    anchors.push_back(row);
  }
  std::sort(anchors.begin(), anchors.end(), [](const Row &a, const Row &b) {
    if (a.blocked_ratio != b.blocked_ratio) return a.blocked_ratio > b.blocked_ratio;
    return a.label < b.label;
  });
  if (anchors.size() > top_k) anchors.resize(top_k);
  return anchors;
}

static uint32_t count_phase_anchors(
    const std::vector<Row> &rows,
    uint32_t anchor_step,
    uint32_t phase) {
  uint32_t count = 0;
  for (const auto &row : rows) {
    if (positive_mod(row.offset, anchor_step) == phase) ++count;
  }
  return count;
}

static uint32_t covered_rows(
    const std::vector<Row> &rows,
    const std::vector<Row> &anchors,
    uint32_t reveal_radius) {
  std::set<std::string> covered;
  for (const auto &row : rows) {
    for (const auto &anchor : anchors) {
      if (std::llabs(row.offset - anchor.offset) <= static_cast<int64_t>(reveal_radius)) {
        covered.insert(row.label);
        break;
      }
    }
  }
  return static_cast<uint32_t>(covered.size());
}

static bool peak_hit(const std::vector<Row> &anchors, uint32_t reveal_radius) {
  for (const auto &anchor : anchors) {
    if (std::llabs(anchor.offset) <= static_cast<int64_t>(reveal_radius)) return true;
  }
  return false;
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const auto labels = load_labels(opt);
    const auto rows = load_rows(opt, labels);
    const auto groups = group_rows(rows);

    const std::vector<uint32_t> anchor_steps{40, 50, 100, 200, 400};
    const std::vector<uint32_t> top_ks{1, 2, 3, 5, 10, 20};
    const std::vector<uint32_t> reveal_radii{10, 20, 50, 100, 200};

    std::cout << "anchorStep\ttopK\trevealRadius\tphases\tgroups"
              << "\tmeanHitGroups\tminHitGroups\tmaxHitGroups"
              << "\tmeanAnchorCandidates\tmeanSelected"
              << "\tmeanCoveredRows\tmeanCoveredFraction"
              << "\trandomExpectedGroups\tenrichment\n";

    for (uint32_t anchor_step : anchor_steps) {
      for (uint32_t top_k : top_ks) {
        for (uint32_t reveal_radius : reveal_radii) {
          uint32_t phase_count = 0;
          double hit_sum = 0.0;
          uint32_t min_hit = UINT32_MAX;
          uint32_t max_hit = 0;
          double anchor_candidates_sum = 0.0;
          double selected_sum = 0.0;
          double covered_rows_sum = 0.0;
          double covered_fraction_sum = 0.0;
          double random_expected_sum = 0.0;

          for (uint32_t phase = 0; phase < anchor_step; phase += 2) {
            uint32_t group_count = 0;
            uint32_t hit_groups = 0;
            double phase_anchor_candidates = 0.0;
            double phase_selected = 0.0;
            double phase_covered_rows = 0.0;
            double phase_covered_fraction = 0.0;

            for (const auto &item : groups) {
              const auto &group = item.second;
              bool has_peak = false;
              for (const auto &row : group) {
                if (row.klass == "peak") {
                  has_peak = true;
                  break;
                }
              }
              if (!has_peak || group.empty()) continue;

              const auto anchors = select_anchors(group, anchor_step, phase, top_k);
              const uint32_t anchor_candidates = count_phase_anchors(group, anchor_step, phase);
              const uint32_t covered = covered_rows(group, anchors, reveal_radius);
              const double covered_fraction = static_cast<double>(covered) / group.size();
              ++group_count;
              if (peak_hit(anchors, reveal_radius)) ++hit_groups;
              phase_anchor_candidates += anchor_candidates;
              phase_selected += anchors.size();
              phase_covered_rows += covered;
              phase_covered_fraction += covered_fraction;
            }

            if (group_count == 0) continue;
            ++phase_count;
            hit_sum += hit_groups;
            min_hit = std::min(min_hit, hit_groups);
            max_hit = std::max(max_hit, hit_groups);
            anchor_candidates_sum += phase_anchor_candidates / group_count;
            selected_sum += phase_selected / group_count;
            covered_rows_sum += phase_covered_rows / group_count;
            covered_fraction_sum += phase_covered_fraction / group_count;
            random_expected_sum += phase_covered_fraction;
          }

          if (phase_count == 0) continue;
          const double mean_hit = hit_sum / phase_count;
          const double mean_anchor_candidates = anchor_candidates_sum / phase_count;
          const double mean_selected = selected_sum / phase_count;
          const double mean_covered_rows = covered_rows_sum / phase_count;
          const double mean_covered_fraction = covered_fraction_sum / phase_count;
          const double random_expected = random_expected_sum / phase_count;
          const double enrichment = random_expected == 0.0 ? 0.0 : mean_hit / random_expected;

          std::cout << anchor_step << '\t'
                    << top_k << '\t'
                    << reveal_radius << '\t'
                    << phase_count << '\t'
                    << groups.size() << '\t'
                    << mean_hit << '\t'
                    << min_hit << '\t'
                    << max_hit << '\t'
                    << mean_anchor_candidates << '\t'
                    << mean_selected << '\t'
                    << mean_covered_rows << '\t'
                    << mean_covered_fraction << '\t'
                    << random_expected << '\t'
                    << enrichment << '\n';
        }
      }
    }
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
