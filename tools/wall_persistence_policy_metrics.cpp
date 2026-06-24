#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct Options {
  std::string labels_path;
  std::vector<std::string> feature_paths;
  std::string detail_output_path;
  std::string group_profile_output_path;
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
  std::vector<double> blocked_ratios;
  std::vector<double> z_scores;
  std::vector<uint32_t> ranks;
};

struct Layer {
  std::string path;
  uint32_t p_limit = 0;
  uint32_t wall_limit = 0;
};

struct Metric {
  std::string policy;
  std::string params;
  uint32_t groups = 0;
  uint32_t hit_groups = 0;
  double selected_sum = 0.0;
  double covered_rows_sum = 0.0;
  double covered_fraction_sum = 0.0;
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

static std::vector<std::string> split(const std::string &text, char sep) {
  std::vector<std::string> out;
  std::string item;
  std::stringstream ss(text);
  while (std::getline(ss, item, sep)) {
    if (!item.empty()) out.push_back(item);
  }
  return out;
}

static std::unordered_map<std::string, size_t> header_index(const std::string &line) {
  std::unordered_map<std::string, size_t> out;
  const auto parts = split(line, '\t');
  for (size_t i = 0; i < parts.size(); ++i) out[parts[i]] = i;
  return out;
}

static Options parse_args(int argc, char **argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    const auto eq = arg.find('=');
    const std::string key = eq == std::string::npos ? arg : arg.substr(0, eq);
    const std::string value = eq == std::string::npos ? "" : arg.substr(eq + 1);
    if (key == "--labels") opt.labels_path = value;
    else if (key == "--features") opt.feature_paths = split(value, ',');
    else if (key == "--detailOutput") opt.detail_output_path = value;
    else if (key == "--groupProfileOutput") opt.group_profile_output_path = value;
    else if (key == "--positiveP") opt.positive_p = static_cast<uint32_t>(parse_u64(value, key));
    else throw std::runtime_error("unknown argument: " + arg);
  }
  if (opt.labels_path.empty()) throw std::runtime_error("need --labels");
  if (opt.feature_paths.size() < 2) throw std::runtime_error("need at least two --features files");
  return opt;
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
    const std::unordered_map<std::string, LabelRow> &labels,
    std::vector<Layer> &layers) {
  std::vector<Row> rows;
  std::unordered_map<std::string, size_t> row_by_label;

  for (size_t layer_i = 0; layer_i < opt.feature_paths.size(); ++layer_i) {
    const std::string &path = opt.feature_paths[layer_i];
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open features: " + path);

    std::string line;
    if (!std::getline(in, line)) throw std::runtime_error("empty features file: " + path);
    const auto h = header_index(line);
    const size_t label_i = h.at("label");
    const size_t class_i = h.at("class");
    const size_t br_i = h.at("blockedRatio");
    const size_t p_limit_i = h.at("pLimit");
    const size_t wall_limit_i = h.at("wallLimit");

    Layer layer;
    layer.path = path;
    bool saw_layer_limits = false;
    uint32_t joined = 0;

    while (std::getline(in, line)) {
      if (line.empty()) continue;
      const auto parts = split(line, '\t');
      if (parts.size() <= std::max({label_i, class_i, br_i, p_limit_i, wall_limit_i})) continue;
      const auto label_found = labels.find(parts[label_i]);
      if (label_found == labels.end() || !label_found->second.verified) continue;

      if (!saw_layer_limits) {
        layer.p_limit = static_cast<uint32_t>(parse_u64(parts[p_limit_i], "pLimit"));
        layer.wall_limit = static_cast<uint32_t>(parse_u64(parts[wall_limit_i], "wallLimit"));
        saw_layer_limits = true;
      }

      size_t row_index = 0;
      const auto row_found = row_by_label.find(parts[label_i]);
      if (row_found == row_by_label.end()) {
        if (layer_i != 0) continue;
        Row row;
        row.label = parts[label_i];
        row.klass = parts[class_i];
        row.source_p = label_found->second.source_p;
        row.offset = parse_offset(row.label);
        row.exact_p = label_found->second.exact_p;
        row.positive = row.exact_p >= opt.positive_p;
        row.blocked_ratios.assign(opt.feature_paths.size(), std::numeric_limits<double>::quiet_NaN());
        row.z_scores.assign(opt.feature_paths.size(), 0.0);
        row.ranks.assign(opt.feature_paths.size(), 0);
        row_index = rows.size();
        row_by_label[row.label] = row_index;
        rows.push_back(std::move(row));
      } else {
        row_index = row_found->second;
      }

      rows[row_index].blocked_ratios[layer_i] = parse_double(parts[br_i]);
      ++joined;
    }

    if (!saw_layer_limits) throw std::runtime_error("no joined rows for layer: " + path);
    layers.push_back(layer);
    if (joined == 0) throw std::runtime_error("layer joined zero rows: " + path);
  }

  rows.erase(std::remove_if(rows.begin(), rows.end(), [](const Row &row) {
    for (double value : row.blocked_ratios) {
      if (std::isnan(value)) return true;
    }
    return false;
  }), rows.end());

  if (rows.empty()) throw std::runtime_error("no complete multi-layer rows");
  return rows;
}

static std::map<std::string, std::vector<size_t>> group_indices(const std::vector<Row> &rows) {
  std::map<std::string, std::vector<size_t>> groups;
  for (size_t i = 0; i < rows.size(); ++i) groups[rows[i].source_p].push_back(i);
  for (auto &item : groups) {
    std::sort(item.second.begin(), item.second.end(), [&](size_t a, size_t b) {
      if (rows[a].offset != rows[b].offset) return rows[a].offset < rows[b].offset;
      return rows[a].label < rows[b].label;
    });
  }
  return groups;
}

static void compute_layer_stats(
    std::vector<Row> &rows,
    const std::map<std::string, std::vector<size_t>> &groups,
    size_t layer_count) {
  for (const auto &item : groups) {
    const auto &idxs = item.second;
    for (size_t layer = 0; layer < layer_count; ++layer) {
      double sum = 0.0;
      for (size_t idx : idxs) sum += rows[idx].blocked_ratios[layer];
      const double mean = idxs.empty() ? 0.0 : sum / idxs.size();

      double var_sum = 0.0;
      for (size_t idx : idxs) {
        const double delta = rows[idx].blocked_ratios[layer] - mean;
        var_sum += delta * delta;
      }
      const double stdev = idxs.size() <= 1 ? 0.0 : std::sqrt(var_sum / (idxs.size() - 1));
      for (size_t idx : idxs) rows[idx].z_scores[layer] = stdev == 0.0 ? 0.0 : (rows[idx].blocked_ratios[layer] - mean) / stdev;

      std::vector<size_t> sorted = idxs;
      std::sort(sorted.begin(), sorted.end(), [&](size_t a, size_t b) {
        const double av = rows[a].blocked_ratios[layer];
        const double bv = rows[b].blocked_ratios[layer];
        if (av != bv) return av > bv;
        return rows[a].label < rows[b].label;
      });
      for (size_t rank = 0; rank < sorted.size(); ++rank) {
        rows[sorted[rank]].ranks[layer] = static_cast<uint32_t>(rank + 1);
      }
    }
  }
}

static std::vector<size_t> select_nms(
    const std::vector<size_t> &idxs,
    const std::vector<Row> &rows,
    const std::vector<double> &scores,
    uint32_t suppress_radius,
    uint32_t top_k) {
  std::vector<size_t> sorted = idxs;
  std::sort(sorted.begin(), sorted.end(), [&](size_t a, size_t b) {
    const double av = scores[a];
    const double bv = scores[b];
    if (av != bv) return av > bv;
    return rows[a].label < rows[b].label;
  });

  std::vector<size_t> selected;
  for (size_t candidate : sorted) {
    bool suppressed = false;
    for (size_t chosen : selected) {
      if (std::llabs(rows[candidate].offset - rows[chosen].offset) <= static_cast<int64_t>(suppress_radius)) {
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

static uint32_t covered_rows(
    const std::vector<size_t> &idxs,
    const std::vector<size_t> &selected,
    const std::vector<Row> &rows,
    uint32_t hit_radius) {
  std::set<std::string> covered;
  for (size_t idx : idxs) {
    for (size_t center : selected) {
      if (std::llabs(rows[idx].offset - rows[center].offset) <= static_cast<int64_t>(hit_radius)) {
        covered.insert(rows[idx].label);
        break;
      }
    }
  }
  return static_cast<uint32_t>(covered.size());
}

static uint32_t covered_rows_variable(
    const std::vector<size_t> &idxs,
    const std::vector<size_t> &selected,
    const std::vector<uint32_t> &radii,
    const std::vector<Row> &rows) {
  std::set<std::string> covered;
  for (size_t idx : idxs) {
    for (size_t i = 0; i < selected.size(); ++i) {
      if (std::llabs(rows[idx].offset - rows[selected[i]].offset) <= static_cast<int64_t>(radii[i])) {
        covered.insert(rows[idx].label);
        break;
      }
    }
  }
  return static_cast<uint32_t>(covered.size());
}

static bool is_covered_by_variable(
    size_t idx,
    const std::vector<size_t> &selected,
    const std::vector<uint32_t> &radii,
    const std::vector<Row> &rows) {
  for (size_t i = 0; i < selected.size(); ++i) {
    if (std::llabs(rows[idx].offset - rows[selected[i]].offset) <= static_cast<int64_t>(radii[i])) {
      return true;
    }
  }
  return false;
}

static std::vector<double> local_mass_scores(
    const std::vector<Row> &rows,
    const std::map<std::string, std::vector<size_t>> &groups,
    const std::vector<double> &scores,
    uint32_t outer_radius,
    uint32_t inner_radius) {
  std::vector<double> out(rows.size(), 0.0);
  for (const auto &item : groups) {
    const auto &idxs = item.second;
    for (size_t center : idxs) {
      double sum = 0.0;
      for (size_t idx : idxs) {
        const uint64_t distance = static_cast<uint64_t>(std::llabs(rows[idx].offset - rows[center].offset));
        if (distance > outer_radius || distance <= inner_radius) continue;
        if (scores[idx] > 0.0) sum += scores[idx];
      }
      out[center] = sum;
    }
  }
  return out;
}

static std::string join_selected_offsets(
    const std::vector<Row> &rows,
    const std::vector<size_t> &selected) {
  std::ostringstream out;
  for (size_t i = 0; i < selected.size(); ++i) {
    if (i) out << ',';
    out << rows[selected[i]].offset;
  }
  return out.str();
}

static std::string join_selected_labels(
    const std::vector<Row> &rows,
    const std::vector<size_t> &selected) {
  std::ostringstream out;
  for (size_t i = 0; i < selected.size(); ++i) {
    if (i) out << ',';
    out << rows[selected[i]].label;
  }
  return out.str();
}

static std::string join_selected_scores(
    const std::vector<size_t> &selected,
    const std::vector<double> &scores) {
  std::ostringstream out;
  for (size_t i = 0; i < selected.size(); ++i) {
    if (i) out << ',';
    out << scores[selected[i]];
  }
  return out.str();
}

static std::string join_u32_values(const std::vector<uint32_t> &values) {
  std::ostringstream out;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << values[i];
  }
  return out.str();
}

static std::string join_double_values(const std::vector<double> &values) {
  std::ostringstream out;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << values[i];
  }
  return out.str();
}

static uint32_t score_rank_in_group(
    const std::vector<size_t> &idxs,
    const std::vector<Row> &rows,
    const std::vector<double> &scores,
    size_t target) {
  std::vector<size_t> sorted = idxs;
  std::sort(sorted.begin(), sorted.end(), [&](size_t a, size_t b) {
    const double av = scores[a];
    const double bv = scores[b];
    if (av != bv) return av > bv;
    return rows[a].label < rows[b].label;
  });
  for (size_t rank = 0; rank < sorted.size(); ++rank) {
    if (sorted[rank] == target) return static_cast<uint32_t>(rank + 1);
  }
  return 0;
}

static void write_group_profile(
    const std::string &path,
    const std::vector<Row> &rows,
    const std::map<std::string, std::vector<size_t>> &groups,
    const std::vector<double> &scores,
    uint32_t final_suppress,
    uint32_t final_top_k,
    uint32_t base_radius,
    uint32_t expand_top,
    uint32_t expand_radius) {
  std::ofstream out(path);
  if (!out) throw std::runtime_error("cannot open group profile output: " + path);
  out << "sourceP\thit\tnearestOffset\tnearestDistance\tnearestOrder\tnearestRadius"
      << "\tpeakScore\tpeakScoreRank\tpeakLayerRanks\tpeakBlockedRatios"
      << "\tselectedOffsets\tselectedScores\tselectedRadii\tselectedLabels\n";

  for (const auto &item : groups) {
    size_t peak_idx = std::numeric_limits<size_t>::max();
    for (size_t idx : item.second) {
      if (rows[idx].klass == "peak" || rows[idx].offset == 0) {
        peak_idx = idx;
        break;
      }
    }
    if (peak_idx == std::numeric_limits<size_t>::max()) continue;

    const auto selected = select_nms(item.second, rows, scores, final_suppress, final_top_k);
    std::vector<uint32_t> radii;
    radii.reserve(selected.size());
    for (size_t i = 0; i < selected.size(); ++i) {
      radii.push_back(i < expand_top ? expand_radius : base_radius);
    }

    bool hit = false;
    int64_t nearest_offset = 0;
    uint64_t nearest_distance = std::numeric_limits<uint64_t>::max();
    uint32_t nearest_order = 0;
    uint32_t nearest_radius = 0;
    for (size_t i = 0; i < selected.size(); ++i) {
      const uint64_t distance = static_cast<uint64_t>(std::llabs(rows[selected[i]].offset));
      if (distance <= radii[i]) hit = true;
      if (distance < nearest_distance) {
        nearest_distance = distance;
        nearest_offset = rows[selected[i]].offset;
        nearest_order = static_cast<uint32_t>(i + 1);
        nearest_radius = radii[i];
      }
    }

    out << item.first << '\t'
        << (hit ? 1 : 0) << '\t'
        << nearest_offset << '\t'
        << nearest_distance << '\t'
        << nearest_order << '\t'
        << nearest_radius << '\t'
        << scores[peak_idx] << '\t'
        << score_rank_in_group(item.second, rows, scores, peak_idx) << '\t'
        << join_u32_values(rows[peak_idx].ranks) << '\t'
        << join_double_values(rows[peak_idx].blocked_ratios) << '\t'
        << join_selected_offsets(rows, selected) << '\t'
        << join_selected_scores(selected, scores) << '\t'
        << join_u32_values(radii) << '\t'
        << join_selected_labels(rows, selected) << '\n';
  }
}

static void add_metric(
    Metric &metric,
    const std::vector<Row> &rows,
    const std::vector<size_t> &idxs,
    const std::vector<size_t> &selected,
    uint32_t hit_radius) {
  bool has_peak = false;
  for (size_t idx : idxs) {
    if (rows[idx].klass == "peak") {
      has_peak = true;
      break;
    }
  }
  if (!has_peak) return;

  bool hit = false;
  for (size_t center : selected) {
    if (std::llabs(rows[center].offset) <= static_cast<int64_t>(hit_radius)) {
      hit = true;
      break;
    }
  }
  const uint32_t covered = covered_rows(idxs, selected, rows, hit_radius);
  ++metric.groups;
  if (hit) ++metric.hit_groups;
  metric.selected_sum += selected.size();
  metric.covered_rows_sum += covered;
  metric.covered_fraction_sum += static_cast<double>(covered) / idxs.size();
}

static std::vector<double> static_scores(
    const std::vector<Row> &rows,
    const std::string &policy,
    uint32_t rank_threshold) {
  std::vector<double> scores(rows.size(), 0.0);
  for (size_t i = 0; i < rows.size(); ++i) {
    const Row &row = rows[i];
    if (policy == "meanZ") {
      scores[i] = std::accumulate(row.z_scores.begin(), row.z_scores.end(), 0.0) / row.z_scores.size();
    } else if (policy == "minZ") {
      scores[i] = *std::min_element(row.z_scores.begin(), row.z_scores.end());
    } else if (policy == "rankVotes") {
      double votes = 0.0;
      double inverse_rank = 0.0;
      for (uint32_t rank : row.ranks) {
        if (rank <= rank_threshold) votes += 1.0;
        inverse_rank += 1.0 / static_cast<double>(rank);
      }
      scores[i] = votes * 1000.0 + inverse_rank;
    } else {
      throw std::runtime_error("bad static policy: " + policy);
    }
  }
  return scores;
}

static std::vector<double> nms_vote_scores(
    const std::vector<Row> &rows,
    const std::map<std::string, std::vector<size_t>> &groups,
    size_t layer_count,
    uint32_t layer_suppress,
    uint32_t layer_top_k,
    uint32_t vote_radius) {
  std::vector<double> out(rows.size(), 0.0);

  for (const auto &item : groups) {
    const auto &idxs = item.second;
    for (size_t layer = 0; layer < layer_count; ++layer) {
      std::vector<double> layer_scores(rows.size(), 0.0);
      for (size_t idx : idxs) layer_scores[idx] = rows[idx].blocked_ratios[layer];
      const auto selected = select_nms(idxs, rows, layer_scores, layer_suppress, layer_top_k);
      for (size_t rank = 0; rank < selected.size(); ++rank) {
        const size_t center = selected[rank];
        const double weight = 1.0 / static_cast<double>(rank + 1);
        for (size_t idx : idxs) {
          if (std::llabs(rows[idx].offset - rows[center].offset) <= static_cast<int64_t>(vote_radius)) {
            out[idx] += 1000.0 + weight;
          }
        }
      }
    }
  }
  return out;
}

static void evaluate_policy(
    const std::string &policy,
    const std::string &params,
    const std::vector<Row> &rows,
    const std::map<std::string, std::vector<size_t>> &groups,
    const std::vector<double> &scores,
    uint32_t final_suppress,
    uint32_t final_top_k,
    uint32_t hit_radius,
    std::ostream *detail_out) {
  Metric metric;
  metric.policy = policy;
  std::ostringstream param_out;
  param_out << params
            << " finalSuppress=" << final_suppress
            << " finalTopK=" << final_top_k
            << " hitR=" << hit_radius;
  metric.params = param_out.str();

  for (const auto &item : groups) {
    const auto selected = select_nms(item.second, rows, scores, final_suppress, final_top_k);
    bool hit = false;
    for (size_t center : selected) {
      if (std::llabs(rows[center].offset) <= static_cast<int64_t>(hit_radius)) {
        hit = true;
        break;
      }
    }
    if (detail_out) {
      *detail_out << policy << '\t'
                  << metric.params << '\t'
                  << item.first << '\t'
                  << (hit ? 1 : 0) << '\t'
                  << hit_radius << '\t'
                  << join_selected_offsets(rows, selected) << '\t'
                  << join_selected_labels(rows, selected) << '\n';
    }
    add_metric(metric, rows, item.second, selected, hit_radius);
  }

  const double mean_selected = metric.groups == 0 ? 0.0 : metric.selected_sum / metric.groups;
  const double mean_covered = metric.groups == 0 ? 0.0 : metric.covered_rows_sum / metric.groups;
  const double mean_covered_fraction = metric.groups == 0 ? 0.0 : metric.covered_fraction_sum / metric.groups;
  const double random_expected = metric.covered_fraction_sum;
  const double enrichment = random_expected == 0.0 ? 0.0 : metric.hit_groups / random_expected;

  std::cout << metric.policy << '\t'
            << metric.params << '\t'
            << metric.groups << '\t'
            << metric.hit_groups << '\t'
            << mean_selected << '\t'
            << mean_covered << '\t'
            << mean_covered_fraction << '\t'
            << random_expected << '\t'
            << enrichment << '\n';
}

static void evaluate_adaptive_reveal(
    const std::string &policy,
    const std::string &params,
    const std::vector<Row> &rows,
    const std::map<std::string, std::vector<size_t>> &groups,
    const std::vector<double> &scores,
    uint32_t final_suppress,
    uint32_t final_top_k,
    uint32_t base_radius,
    uint32_t expand_top,
    uint32_t expand_radius,
    std::ostream *detail_out) {
  Metric metric;
  metric.policy = policy;
  std::ostringstream param_out;
  param_out << params
            << " finalSuppress=" << final_suppress
            << " finalTopK=" << final_top_k
            << " baseR=" << base_radius
            << " expandTop=" << expand_top
            << " expandR=" << expand_radius;
  metric.params = param_out.str();

  for (const auto &item : groups) {
    const auto selected = select_nms(item.second, rows, scores, final_suppress, final_top_k);
    std::vector<uint32_t> radii;
    radii.reserve(selected.size());
    for (size_t i = 0; i < selected.size(); ++i) {
      radii.push_back(i < expand_top ? expand_radius : base_radius);
    }

    bool has_peak = false;
    for (size_t idx : item.second) {
      if (rows[idx].klass == "peak") {
        has_peak = true;
        break;
      }
    }
    if (!has_peak) continue;

    bool hit = false;
    for (size_t i = 0; i < selected.size(); ++i) {
      if (std::llabs(rows[selected[i]].offset) <= static_cast<int64_t>(radii[i])) {
        hit = true;
        break;
      }
    }
    const uint32_t covered = covered_rows_variable(item.second, selected, radii, rows);
    ++metric.groups;
    if (hit) ++metric.hit_groups;
    metric.selected_sum += selected.size();
    metric.covered_rows_sum += covered;
    metric.covered_fraction_sum += static_cast<double>(covered) / item.second.size();

    if (detail_out) {
      *detail_out << policy << '\t'
                  << metric.params << '\t'
                  << item.first << '\t'
                  << (hit ? 1 : 0) << '\t'
                  << expand_radius << '\t'
                  << join_selected_offsets(rows, selected) << '\t'
                  << join_selected_labels(rows, selected) << '\n';
    }
  }

  const double mean_selected = metric.groups == 0 ? 0.0 : metric.selected_sum / metric.groups;
  const double mean_covered = metric.groups == 0 ? 0.0 : metric.covered_rows_sum / metric.groups;
  const double mean_covered_fraction = metric.groups == 0 ? 0.0 : metric.covered_fraction_sum / metric.groups;
  const double random_expected = metric.covered_fraction_sum;
  const double enrichment = random_expected == 0.0 ? 0.0 : metric.hit_groups / random_expected;

  std::cout << metric.policy << '\t'
            << metric.params << '\t'
            << metric.groups << '\t'
            << metric.hit_groups << '\t'
            << mean_selected << '\t'
            << mean_covered << '\t'
            << mean_covered_fraction << '\t'
            << random_expected << '\t'
            << enrichment << '\n';
}

static void evaluate_adaptive_reveal_by_expand_scores(
    const std::string &policy,
    const std::string &params,
    const std::vector<Row> &rows,
    const std::map<std::string, std::vector<size_t>> &groups,
    const std::vector<double> &selection_scores,
    const std::vector<double> &expand_scores,
    uint32_t final_suppress,
    uint32_t final_top_k,
    uint32_t base_radius,
    uint32_t expand_top,
    uint32_t expand_radius,
    std::ostream *detail_out) {
  Metric metric;
  metric.policy = policy;
  std::ostringstream param_out;
  param_out << params
            << " finalSuppress=" << final_suppress
            << " finalTopK=" << final_top_k
            << " baseR=" << base_radius
            << " expandTop=" << expand_top
            << " expandR=" << expand_radius;
  metric.params = param_out.str();

  for (const auto &item : groups) {
    const auto selected = select_nms(item.second, rows, selection_scores, final_suppress, final_top_k);

    std::vector<size_t> expansion_order = selected;
    std::sort(expansion_order.begin(), expansion_order.end(), [&](size_t a, size_t b) {
      const double av = expand_scores[a];
      const double bv = expand_scores[b];
      if (av != bv) return av > bv;
      const double as = selection_scores[a];
      const double bs = selection_scores[b];
      if (as != bs) return as > bs;
      return rows[a].label < rows[b].label;
    });

    std::set<size_t> expanded;
    for (size_t i = 0; i < expansion_order.size() && i < expand_top; ++i) {
      expanded.insert(expansion_order[i]);
    }

    std::vector<uint32_t> radii;
    radii.reserve(selected.size());
    for (size_t idx : selected) {
      radii.push_back(expanded.count(idx) ? expand_radius : base_radius);
    }

    bool has_peak = false;
    for (size_t idx : item.second) {
      if (rows[idx].klass == "peak") {
        has_peak = true;
        break;
      }
    }
    if (!has_peak) continue;

    bool hit = false;
    for (size_t i = 0; i < selected.size(); ++i) {
      if (std::llabs(rows[selected[i]].offset) <= static_cast<int64_t>(radii[i])) {
        hit = true;
        break;
      }
    }
    const uint32_t covered = covered_rows_variable(item.second, selected, radii, rows);
    ++metric.groups;
    if (hit) ++metric.hit_groups;
    metric.selected_sum += selected.size();
    metric.covered_rows_sum += covered;
    metric.covered_fraction_sum += static_cast<double>(covered) / item.second.size();

    if (detail_out) {
      *detail_out << policy << '\t'
                  << metric.params << '\t'
                  << item.first << '\t'
                  << (hit ? 1 : 0) << '\t'
                  << expand_radius << '\t'
                  << join_selected_offsets(rows, selected) << '\t'
                  << join_selected_labels(rows, selected) << '\n';
    }
  }

  const double mean_selected = metric.groups == 0 ? 0.0 : metric.selected_sum / metric.groups;
  const double mean_covered = metric.groups == 0 ? 0.0 : metric.covered_rows_sum / metric.groups;
  const double mean_covered_fraction = metric.groups == 0 ? 0.0 : metric.covered_fraction_sum / metric.groups;
  const double random_expected = metric.covered_fraction_sum;
  const double enrichment = random_expected == 0.0 ? 0.0 : metric.hit_groups / random_expected;

  std::cout << metric.policy << '\t'
            << metric.params << '\t'
            << metric.groups << '\t'
            << metric.hit_groups << '\t'
            << mean_selected << '\t'
            << mean_covered << '\t'
            << mean_covered_fraction << '\t'
            << random_expected << '\t'
            << enrichment << '\n';
}

static void evaluate_tiered_reveal(
    const std::string &policy,
    const std::string &params,
    const std::vector<Row> &rows,
    const std::map<std::string, std::vector<size_t>> &groups,
    const std::vector<double> &scores,
    uint32_t final_suppress,
    uint32_t final_top_k,
    uint32_t top_radius,
    uint32_t mid_top,
    uint32_t mid_radius,
    uint32_t base_radius,
    std::ostream *detail_out) {
  Metric metric;
  metric.policy = policy;
  std::ostringstream param_out;
  param_out << params
            << " finalSuppress=" << final_suppress
            << " finalTopK=" << final_top_k
            << " topR=" << top_radius
            << " midTop=" << mid_top
            << " midR=" << mid_radius
            << " baseR=" << base_radius;
  metric.params = param_out.str();

  for (const auto &item : groups) {
    const auto selected = select_nms(item.second, rows, scores, final_suppress, final_top_k);
    std::vector<uint32_t> radii;
    radii.reserve(selected.size());
    for (size_t i = 0; i < selected.size(); ++i) {
      if (i == 0) radii.push_back(top_radius);
      else if (i < mid_top) radii.push_back(mid_radius);
      else radii.push_back(base_radius);
    }

    bool has_peak = false;
    for (size_t idx : item.second) {
      if (rows[idx].klass == "peak") {
        has_peak = true;
        break;
      }
    }
    if (!has_peak) continue;

    bool hit = false;
    for (size_t i = 0; i < selected.size(); ++i) {
      if (std::llabs(rows[selected[i]].offset) <= static_cast<int64_t>(radii[i])) {
        hit = true;
        break;
      }
    }
    const uint32_t covered = covered_rows_variable(item.second, selected, radii, rows);
    ++metric.groups;
    if (hit) ++metric.hit_groups;
    metric.selected_sum += selected.size();
    metric.covered_rows_sum += covered;
    metric.covered_fraction_sum += static_cast<double>(covered) / item.second.size();

    if (detail_out) {
      *detail_out << policy << '\t'
                  << metric.params << '\t'
                  << item.first << '\t'
                  << (hit ? 1 : 0) << '\t'
                  << top_radius << '\t'
                  << join_selected_offsets(rows, selected) << '\t'
                  << join_selected_labels(rows, selected) << '\n';
    }
  }

  const double mean_selected = metric.groups == 0 ? 0.0 : metric.selected_sum / metric.groups;
  const double mean_covered = metric.groups == 0 ? 0.0 : metric.covered_rows_sum / metric.groups;
  const double mean_covered_fraction = metric.groups == 0 ? 0.0 : metric.covered_fraction_sum / metric.groups;
  const double random_expected = metric.covered_fraction_sum;
  const double enrichment = random_expected == 0.0 ? 0.0 : metric.hit_groups / random_expected;

  std::cout << metric.policy << '\t'
            << metric.params << '\t'
            << metric.groups << '\t'
            << metric.hit_groups << '\t'
            << mean_selected << '\t'
            << mean_covered << '\t'
            << mean_covered_fraction << '\t'
            << random_expected << '\t'
            << enrichment << '\n';
}

static void evaluate_gap_relanding(
    const std::string &policy,
    const std::string &params,
    const std::vector<Row> &rows,
    const std::map<std::string, std::vector<size_t>> &groups,
    const std::vector<double> &primary_scores,
    const std::vector<double> &fallback_scores,
    uint32_t primary_suppress,
    uint32_t primary_top_k,
    uint32_t base_radius,
    uint32_t expand_top,
    uint32_t expand_radius,
    uint32_t fallback_suppress,
    uint32_t fallback_top_k,
    uint32_t fallback_radius,
    std::ostream *detail_out) {
  Metric metric;
  metric.policy = policy;
  std::ostringstream param_out;
  param_out << params
            << " primarySuppress=" << primary_suppress
            << " primaryTopK=" << primary_top_k
            << " baseR=" << base_radius
            << " expandTop=" << expand_top
            << " expandR=" << expand_radius
            << " fallbackSuppress=" << fallback_suppress
            << " fallbackTopK=" << fallback_top_k
            << " fallbackR=" << fallback_radius;
  metric.params = param_out.str();

  for (const auto &item : groups) {
    const auto primary = select_nms(item.second, rows, primary_scores, primary_suppress, primary_top_k);
    std::vector<uint32_t> primary_radii;
    primary_radii.reserve(primary.size());
    for (size_t i = 0; i < primary.size(); ++i) {
      primary_radii.push_back(i < expand_top ? expand_radius : base_radius);
    }

    std::vector<size_t> fallback_candidates;
    for (size_t idx : item.second) {
      if (!is_covered_by_variable(idx, primary, primary_radii, rows)) fallback_candidates.push_back(idx);
    }
    const auto fallback = select_nms(fallback_candidates, rows, fallback_scores, fallback_suppress, fallback_top_k);

    std::vector<size_t> selected = primary;
    selected.insert(selected.end(), fallback.begin(), fallback.end());
    std::vector<uint32_t> radii = primary_radii;
    for (size_t i = 0; i < fallback.size(); ++i) radii.push_back(fallback_radius);

    bool has_peak = false;
    for (size_t idx : item.second) {
      if (rows[idx].klass == "peak") {
        has_peak = true;
        break;
      }
    }
    if (!has_peak) continue;

    bool hit_by_offset = false;
    for (size_t i = 0; i < selected.size(); ++i) {
      if (std::llabs(rows[selected[i]].offset) <= static_cast<int64_t>(radii[i])) {
        hit_by_offset = true;
        break;
      }
    }

    const uint32_t covered = covered_rows_variable(item.second, selected, radii, rows);
    ++metric.groups;
    if (hit_by_offset) ++metric.hit_groups;
    metric.selected_sum += selected.size();
    metric.covered_rows_sum += covered;
    metric.covered_fraction_sum += static_cast<double>(covered) / item.second.size();

    if (detail_out) {
      *detail_out << policy << '\t'
                  << metric.params << '\t'
                  << item.first << '\t'
                  << (hit_by_offset ? 1 : 0) << '\t'
                  << fallback_radius << '\t'
                  << join_selected_offsets(rows, selected) << '\t'
                  << join_selected_labels(rows, selected) << '\n';
    }
  }

  const double mean_selected = metric.groups == 0 ? 0.0 : metric.selected_sum / metric.groups;
  const double mean_covered = metric.groups == 0 ? 0.0 : metric.covered_rows_sum / metric.groups;
  const double mean_covered_fraction = metric.groups == 0 ? 0.0 : metric.covered_fraction_sum / metric.groups;
  const double random_expected = metric.covered_fraction_sum;
  const double enrichment = random_expected == 0.0 ? 0.0 : metric.hit_groups / random_expected;

  std::cout << metric.policy << '\t'
            << metric.params << '\t'
            << metric.groups << '\t'
            << metric.hit_groups << '\t'
            << mean_selected << '\t'
            << mean_covered << '\t'
            << mean_covered_fraction << '\t'
            << random_expected << '\t'
            << enrichment << '\n';
}

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const auto labels = load_labels(opt);
    std::vector<Layer> layers;
    auto rows = load_rows(opt, labels, layers);
    const auto groups = group_indices(rows);
    compute_layer_stats(rows, groups, layers.size());

    std::ofstream detail_out_file;
    std::ostream *detail_out = nullptr;
    if (!opt.detail_output_path.empty()) {
      detail_out_file.open(opt.detail_output_path);
      if (!detail_out_file) throw std::runtime_error("cannot open detail output: " + opt.detail_output_path);
      detail_out = &detail_out_file;
      *detail_out << "policy\tparams\tsourceP\thit\thitRadius\tselectedOffsets\tselectedLabels\n";
    }

    std::cout << "policy\tparams\tgroups\thitGroups\tmeanSelected"
              << "\tmeanCoveredRows\tmeanCoveredFraction"
              << "\trandomExpectedGroups\tenrichment\n";

    const std::vector<uint32_t> final_suppress_values{20, 100};
    const std::vector<uint32_t> final_top_k_values{5, 10};
    const std::vector<uint32_t> hit_radius_values{20, 100};

    for (const std::string &policy : {"meanZ", "minZ"}) {
      const auto scores = static_scores(rows, policy, 0);
      for (uint32_t final_suppress : final_suppress_values) {
        for (uint32_t final_top_k : final_top_k_values) {
          for (uint32_t hit_radius : hit_radius_values) {
            evaluate_policy(policy, "layers=" + std::to_string(layers.size()),
                            rows, groups, scores, final_suppress, final_top_k, hit_radius, detail_out);
          }
        }
      }
    }

    for (uint32_t rank_threshold : {10u, 20u, 50u}) {
      const auto scores = static_scores(rows, "rankVotes", rank_threshold);
      for (uint32_t final_suppress : final_suppress_values) {
        for (uint32_t final_top_k : final_top_k_values) {
          for (uint32_t hit_radius : hit_radius_values) {
            evaluate_policy("rankVotes",
                            "layers=" + std::to_string(layers.size()) +
                                " rankLe=" + std::to_string(rank_threshold),
                            rows, groups, scores, final_suppress, final_top_k, hit_radius, detail_out);
          }
        }
      }
    }

    for (uint32_t layer_suppress : {20u, 50u, 100u}) {
      for (uint32_t layer_top_k : {3u, 5u, 10u}) {
        for (uint32_t vote_radius : {20u, 50u, 100u}) {
          const auto scores = nms_vote_scores(rows, groups, layers.size(), layer_suppress, layer_top_k, vote_radius);
          for (uint32_t final_suppress : final_suppress_values) {
            for (uint32_t final_top_k : final_top_k_values) {
              for (uint32_t hit_radius : hit_radius_values) {
                std::ostringstream params;
                params << "layers=" << layers.size()
                       << " layerSuppress=" << layer_suppress
                       << " layerTopK=" << layer_top_k
                       << " voteR=" << vote_radius;
                evaluate_policy("nmsVote", params.str(), rows, groups, scores,
                                final_suppress, final_top_k, hit_radius, detail_out);
              }
            }
          }
        }
      }
    }

    size_t baseline_layer = layers.size();
    for (size_t i = 0; i < layers.size(); ++i) {
      if (layers[i].p_limit == 8000 && layers[i].wall_limit == 8000) {
        baseline_layer = i;
        break;
      }
    }

    const auto frozen_scores = nms_vote_scores(rows, groups, layers.size(), 20, 3, 20);
    const auto frozen_scores_k5 = nms_vote_scores(rows, groups, layers.size(), 20, 5, 20);
    std::vector<double> dense_scores(rows.size(), 0.0);
    if (baseline_layer < layers.size()) {
      for (size_t i = 0; i < rows.size(); ++i) dense_scores[i] = rows[i].blocked_ratios[baseline_layer];
    }

    for (uint32_t expand_top : {1u, 2u, 3u, 4u, 5u}) {
      for (uint32_t expand_radius : {40u, 60u, 80u, 100u}) {
        evaluate_adaptive_reveal(
            "adaptiveFrozen5Layer",
            "layers=" + std::to_string(layers.size()) +
                " layerSuppress=20 layerTopK=3 voteR=20",
            rows, groups, frozen_scores, 100, 10, 20, expand_top, expand_radius, detail_out);
        evaluate_adaptive_reveal(
            "adaptiveFrozen5LayerK5",
            "layers=" + std::to_string(layers.size()) +
                " layerSuppress=20 layerTopK=5 voteR=20",
            rows, groups, frozen_scores_k5, 100, 10, 20, expand_top, expand_radius, detail_out);
        if (baseline_layer < layers.size()) {
          evaluate_adaptive_reveal(
              "adaptiveDenseP8000",
              "singleLayer=p8000w8000",
              rows, groups, dense_scores, 20, 10, 20, expand_top, expand_radius, detail_out);
        }
      }
    }

    const auto mass_scores_r100 = local_mass_scores(rows, groups, frozen_scores, 100, 0);
    const auto shoulder_scores_r100 = local_mass_scores(rows, groups, frozen_scores, 100, 20);
    const auto mass_scores_r200 = local_mass_scores(rows, groups, frozen_scores, 200, 0);
    const auto shoulder_scores_r200 = local_mass_scores(rows, groups, frozen_scores, 200, 20);
    for (uint32_t expand_radius : {60u, 120u}) {
      evaluate_adaptive_reveal_by_expand_scores(
          "adaptiveFrozen5LayerMassR100",
          "layers=" + std::to_string(layers.size()) +
              " layerSuppress=20 layerTopK=3 voteR=20 expandPriority=massR100",
          rows, groups, frozen_scores, mass_scores_r100, 100, 10, 20, 4, expand_radius, detail_out);
      evaluate_adaptive_reveal_by_expand_scores(
          "adaptiveFrozen5LayerShoulderR100",
          "layers=" + std::to_string(layers.size()) +
              " layerSuppress=20 layerTopK=3 voteR=20 expandPriority=shoulderR100",
          rows, groups, frozen_scores, shoulder_scores_r100, 100, 10, 20, 4, expand_radius, detail_out);
      evaluate_adaptive_reveal_by_expand_scores(
          "adaptiveFrozen5LayerMassR200",
          "layers=" + std::to_string(layers.size()) +
              " layerSuppress=20 layerTopK=3 voteR=20 expandPriority=massR200",
          rows, groups, frozen_scores, mass_scores_r200, 100, 10, 20, 4, expand_radius, detail_out);
      evaluate_adaptive_reveal_by_expand_scores(
          "adaptiveFrozen5LayerShoulderR200",
          "layers=" + std::to_string(layers.size()) +
              " layerSuppress=20 layerTopK=3 voteR=20 expandPriority=shoulderR200",
          rows, groups, frozen_scores, shoulder_scores_r200, 100, 10, 20, 4, expand_radius, detail_out);
    }

    if (baseline_layer < layers.size()) {
      evaluate_tiered_reveal(
          "adaptiveFrozen5LayerTiered",
          "layers=" + std::to_string(layers.size()) +
              " layerSuppress=20 layerTopK=3 voteR=20 tier=top1R100_top5R60",
          rows, groups, frozen_scores,
          100, 10, 100, 5, 60, 20, detail_out);
      evaluate_gap_relanding(
          "adaptiveFrozen5LayerGapDenseReserve",
          "layers=" + std::to_string(layers.size()) +
              " layerSuppress=20 layerTopK=3 voteR=20 fallbackScore=p8000w8000",
          rows, groups, frozen_scores, dense_scores,
          100, 10, 20, 4, 60,
          100, 1, 100, detail_out);
      evaluate_gap_relanding(
          "adaptiveFrozen5LayerGapDenseReserve",
          "layers=" + std::to_string(layers.size()) +
              " layerSuppress=20 layerTopK=3 voteR=20 fallbackScore=p8000w8000",
          rows, groups, frozen_scores, dense_scores,
          100, 10, 20, 4, 60,
          100, 2, 100, detail_out);
    }

    if (!opt.group_profile_output_path.empty()) {
      write_group_profile(opt.group_profile_output_path, rows, groups, frozen_scores,
                          100, 10, 20, 4, 60);
    }
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
