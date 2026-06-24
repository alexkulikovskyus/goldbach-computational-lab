# Wall-spectrum predictor research note

Status: active hypothesis, not a claimed result.

## Persistence protocol

To avoid losing work across context-window changes:

- Always read the short restart memory first:
  `docs/wall-spectrum-session-handoff.md`
- Keep detailed experiment records in this file:
  `docs/wall-spectrum-predictor-research-note.md`
- Keep the current compact conclusion in:
  `docs/wall-spectrum-research-conclusion.md`
- Keep generated benchmark outputs under:
  `results/wall-spectrum/`
- Keep reproducible public C/C++ tools under:
  `tools/`

After every meaningful experiment, update both the short handoff file and this
research note. The handoff file should contain the current state, headline
result, exact files, and next step. This research note should contain the
details, parameters, tables, interpretation, and negative results.

## Goal

Test whether a q-free / pre-witness wall-spectrum can predict large minimal
Goldbach witnesses.

For an even integer `N`, the minimal Goldbach witness is

```text
p(N) = min { p prime : N - p is prime }.
```

The target is not to verify Goldbach and not to find any decomposition. The
target is narrower:

```text
Before computing p(N), can cheap information about small divisors of N - p
for early admissible prime p values identify numbers likely to have large p(N)?
```

## Prior-art guardrail

This branch starts from the assumption that minimal Goldbach witnesses are not
new objects.

Checked sources:

- Oliveira e Silva / Herzog / Pardi project page:
  https://sweet.ua.pt/tos/goldbach.html
  - reports exhaustive verification up to `4 * 10^18`;
  - includes a table of `S(p)` and counts for small primes used in minimal
    Goldbach partitions;
  - gives top 50 largest least primes found so far below `4 * 10^18`;
  - states empirical bounds and approximations for `S(p)`.
- Deshouillers, te Riele, Saouter, "New Experimental Results Concerning the
  Goldbach Conjecture", ANTS-III, 1998:
  https://ir.cwi.nl/pub/1222/1222D.pdf
  - explicitly discusses the smallest prime `p` such that `e - p` is prime;
  - calls this the minimal Goldbach decomposition;
  - contrasts minimal-decomposition search with faster interval coverage.
- Granville, van de Lune, te Riele, "Checking the Goldbach Conjecture on a
  Vector Computer", 1989:
  https://dms.umontreal.ca/~andrew/PDF/Goldbach1.pdf
  - prior work on minimal Goldbach partitions and record behavior.
- OEIS A025017/A025018/A025019:
  https://oeis.org/A025017
  - records sequences related to least primes in Goldbach partitions.
- Juhasz and Bartalos, "Predicting the size ranking of minimal primes in the
  generalised Goldbach partitions", arXiv:2510.21870:
  https://arxiv.org/abs/2510.21870
  - recent work on ranking minimal primes in generalized Goldbach partitions;
  - confirms that minimal prime size has prior attention.

Consequence: do not claim novelty for minimal witnesses, record peaks, q-lines,
or the existence of a wall. A possible contribution would have to be a tested
pre-witness predictor or a negative result showing that this wall-spectrum does
not outperform baselines.

## Hypothesis

Large `p(N)` occurs when early admissible primes `p` are unusually blocked:
many values `N - p` have small prime divisors, and the first open positions in
the early candidate lane appear later or in a thinner pattern than usual.

Call this pre-witness pattern the wall-spectrum.

This must be computed without knowing `q = N - p` primality and without using
the true minimal witness except as a held-out label for evaluation.

## Candidate pre-witness features

For fixed limits:

```text
pLimit      early candidate prime bound, e.g. 1k, 5k, 10k, 50k, 100k
wallLimits  small-prime divisor bounds, e.g. 31, 101, 1000, 10000, 250000
```

For each admissible prime `p <= pLimit`, mark it blocked at wall limit `L` if
there is a prime `r <= L`, `r > 3`, such that

```text
N - p == 0 mod r
```

and `N - p` is not equal to `r` itself.

Raw features:

- `admissible`: count of candidate primes in allowed modulo-6 lanes.
- `blocked(L)`: count blocked by at least one small prime `<= L`.
- `open(L)`: candidates not blocked by small primes `<= L`.
- `blockedRatio(L) = blocked(L) / admissible`.
- `firstOpenP(L)`: first admissible `p` that survives wall `L`.
- `firstOpenIndex(L)`: candidate index of `firstOpenP`.
- `longestClosedRun(L)`: longest consecutive run of blocked admissible
  candidates.
- `openGapMean(L)`: average gap in candidate index between open positions.
- tentative `wallScore(L)`: a sorting score derived only from the raw features.

The score is not a theorem. It is a ranking statistic to be tested against
known peaks and controls.

## Benchmark design

Positive set:

- Silva / Oliveira e Silva top largest least primes from the public table.
- Local parsed copy currently exists at:
  `data/silva/t0-parsed.tsv` (ignored local research data).

Control set:

- Even values near the same numerical scale as positives but not known as top
  peaks.
- Neighbor windows around positives, e.g. `N +/- 2k`, are useful later, but
  the first benchmark should include independent controls too.

Labels:

- `knownP` is allowed only as an evaluation label.
- No feature may use `knownP` or exact primality of `N - p` before ranking.

## Success criteria

The first pass is interesting only if wall-spectrum ranking is clearly better
than simple baselines.

Metrics:

- fraction of known peaks captured in top `1%`, `0.1%`, `0.01%` by score;
- enrichment over random selection;
- comparison against simple baselines:
  - `N mod small primes`;
  - singular-factor-like small-prime divisibility features of `N`;
  - random control selection;
  - simple first-open position alone.

A useful result would be something like:

```text
Top 0.1% by wallScore captures a large share of known peak rows
while random or simple baselines do not.
```

A negative result is also useful:

```text
wall-spectrum features do not separate known peaks from controls
after proper baseline comparison.
```

## First implementation step

Create a native C++/GMP scorer:

```text
tools/wall_spectrum_scorer_gmp.cpp
```

It should read either one `--n=<decimal even N>` or a TSV batch with labels,
compute only pre-witness wall features, and output TSV rows suitable for later
ranking.

No long run should be launched before the benchmark input and metrics are
fixed.

## 2026-06-22 implementation status

Implemented:

- `tools/wall_spectrum_scorer_gmp.cpp`
- `tools/wall_benchmark_controls_gmp.cpp`
- `tools/wall_benchmark_windows_gmp.cpp`
- `tools/minimal_witness_batch_gmp.cpp`
- `tools/qfree_qline_scout_gmp.cpp`
- `tools/wall_benchmark_metrics.cpp`
- `tools/wall_local_contrast_metrics.cpp`
- `tools/wall_nms_policy_metrics.cpp`
- `tools/wall_multipoint_landing_metrics.cpp`
- `tools/wall_persistence_policy_metrics.cpp`
- `tools/wall_blocker_profile_gmp.cpp`
- `make wall-smoke`

The scorer is intentionally pre-witness only. It does not test whether
`N - p` is prime. It marks an early admissible prime `p` as blocked when
`N - p` has a small prime divisor under the chosen wall limit.

Important correctness guard:

```text
candidate p is considered only when q = N - p >= 2
```

Without this guard, very small `N` rows are polluted by impossible candidates
with `p > N`.

Current smoke command:

```text
./bin/wall-spectrum-scorer-gmp --n=2000000 --pLimit=1000 --wallLimits=31,101,1000
```

Benchmark support tools:

- `wall_benchmark_controls_gmp`: builds a deterministic same-scale benchmark
  from the Silva table by taking the largest known `p` rows and adding nearby
  random even controls.
- `wall_benchmark_windows_gmp`: builds contiguous local windows around known
  Silva peaks by emitting every even `N + offset` in a fixed radius.
- `minimal_witness_batch_gmp`: computes exact/probable-prime minimal witness
  labels for the benchmark rows. This is label generation only; its output is
  not allowed as a feature.
- `qfree_qline_scout_gmp`: post-landing q-line scout. It tests candidate
  `q=C-a` values with GMP and generates Goldbach witnesses along local lines.
  This is not a q-free feature for the predictor; it is a peak/ridge generator
  after an anchor has already been chosen.
- `wall_benchmark_metrics`: joins exact labels to pre-witness wall features
  and reports top-percent capture against random expectation and simple
  feature baselines.
- `wall_local_contrast_metrics`: evaluates moving-background q-free contrast
  scores inside labeled benchmark windows. It uses exact witnesses only as
  labels, not as features.
- `wall_nms_policy_metrics`: evaluates a landing/refinement policy by
  selecting top-K q-free local maxima with non-maximum suppression and measuring
  hit coverage and covered-row cost.
- `wall_multipoint_landing_metrics`: evaluates phase-swept multi-point landing
  policies: choose several grid anchors by q-free wall pressure, open local
  windows around them, and measure peak coverage and revealed-row cost.
- `wall_persistence_policy_metrics`: evaluates multi-layer q-free
  wall-persistence policies by combining local maxima/votes across several
  calibrated `blockedRatio` layers and measuring hit coverage versus opened
  rows.
- `wall_blocker_profile_gmp`: computes q-free composition features for the
  small-prime blockers that close the early admissible lane.

Metric guard:

```text
ranking ties are broken by label, not by exactP
```

An earlier draft of the metrics tool used `exactP` as the tie-breaker. That
would leak the answer into the ranking, so it was corrected before recording
the benchmark below.

## First sanity benchmarks

These are diagnostic checks, not publishable conclusions.

### Silva top-50 local neighbor test

Input:

```text
results/wall-spectrum/silva-top50-neighbors.tsv
```

For each of the 50 largest known Silva / Oliveira e Silva least-prime rows,
compare the known peak `N` with `N - 2` and `N + 2`.

Parameters:

```text
pLimit = 10000
wallLimit = 10000
score = blockedRatio * log1p(longestClosedRun) / log1p(admissible)
```

Observed:

```text
groups                         50
peak_gt_m2                     47
peak_gt_p2                     48
peak_best_of_triplet           45
peak_middle                     5
peak_worst                      0
mean_peak_minus_best_neighbor   0.050546
```

Global ordering inside the 150 triplet rows:

```text
top 10 by wallScore   9 peaks
top 25 by wallScore  22 peaks
top 50 by wallScore  40 peaks
```

Window interpretation:

The local wall shape is visible around many known peaks. This supports using
wall-spectrum as a local diagnostic after landing near an interesting area.
It does not yet show how to choose the landing point in a huge field.

### Silva full-table test

Input:

```text
results/wall-spectrum/silva-all-input.tsv
```

This uses the local parsed Silva table of `S(p)` rows. It is not a random
sample of all even numbers; it is already a table of special first-occurrence
events.

Result:

The current scalar `wallScore` does not rank the largest known `p` rows at the
top when the whole table is mixed across many scales. After the small-`N`
candidate guard, the highest scores are still mostly moderate `p` rows around
the low thousands rather than the top-50 largest `p` values.

Wide-window interpretation:

The current score is not a global formula for "larger wallScore means larger
minimal witness". Scale normalization is required, and local comparison is
more meaningful than mixing small and large `S(p)` rows.

### Silva high-scale subset

Input:

```text
results/wall-spectrum/silva-highscale-input.tsv
```

Filter:

```text
len(S(p)) >= 18
```

Observed:

```text
rows                              191
top50p_total                       50
pearson_knownP_wallScore       -0.136962
high_top1_contains_top50p           0
high_top10_contains_top50p          1
high_top25_contains_top50p          4
high_top50_contains_top50p         10
high_top100_contains_top50p        20
```

Individual feature correlations on this subset:

```text
knownP vs blockedRatio       +0.137349
knownP vs firstOpenIndex     -0.011017
knownP vs longestClosedRun   -0.173958
knownP vs openGapMean        +0.134707
knownP vs wallScore          -0.136962
```

Interpretation:

This is a negative result for the first scalar `wallScore`. It separates
known peaks from their immediate neighbors better than chance, but it does
not globally sort larger known witnesses above smaller known witnesses at the
same broad scale.

### Exact-labeled nearby-control benchmark

Purpose:

Test whether pre-witness wall features separate the known largest Silva peaks
from ordinary even numbers of the same local scale.

Construction:

```text
source positives       top 50 Silva rows by known p
positive threshold     exactP >= 8419
controls               deterministic random even offsets around each peak
max control offset     1,000,000,000
label pLimit           50,000
label smallLimit       50,000
label workers          18
feature pLimit         10,000
feature wallLimit      10,000
```

The label stage computes the true/probable-prime minimal witness within the
label `pLimit`. The feature stage does not use those labels.

Small benchmark:

```text
controls per peak       20
rows                  1050
verified              1050
unresolved               0
positives               50
controls              1000
```

Top-percent capture:

```text
feature          top 1%        top 0.1%       top 0.01%
wallScore        6 / 11         2 / 2          1 / 1
blockedRatio    11 / 11         2 / 2          1 / 1
openGapMean     11 / 11         2 / 2          1 / 1
openRatio low   11 / 11         2 / 2          1 / 1
```

Random expectation for top 1% was about `0.52` positives. On this constructed
benchmark, `blockedRatio`, `openGapMean`, and low `openRatio` are much better
than random and better than the first `wallScore`.

Larger benchmark:

```text
controls per peak      100
rows                  5050
verified              5050
unresolved               0
positives               50
controls              5000
```

Top-percent capture:

```text
feature          top 1%         top 0.1%       top 0.01%
wallScore         8 / 51         1 / 6          0 / 1
blockedRatio     37 / 51         6 / 6          1 / 1
openGapMean      36 / 51         6 / 6          1 / 1
openRatio low    37 / 51         6 / 6          1 / 1
```

Random expectation for top 1% was about `0.50` positives. The enrichment for
`blockedRatio` at top 1% was about `73x` relative to random expectation.

Local group-rank test:

For each Silva peak, compare the peak against its own 100 nearby controls.
This models a landing workflow: if an anchor has already put us near an
interesting area, can the wall-pressure feature select the strongest point
inside that local bucket?

```text
feature          rank 1        rank <= 5      rank <= 10     mean peak rank
wallScore         9 / 50        27 / 50        40 / 50        7.16
blockedRatio     43 / 50        49 / 50        49 / 50        1.50
openGapMean      42 / 50        49 / 50        49 / 50        1.56
openRatio low    43 / 50        49 / 50        49 / 50        1.50
```

Random expectation for rank 1 is about `0.50` peaks out of 50 groups, because
each group has 101 rows. The worst observed `blockedRatio` rank among the
known peaks was `16`, for the boundary case `p=8419`; the other non-rank-1
cases were ranks `2` to `4`.

Parameter sweep:

The same `50 peaks + 5000 controls` benchmark was rescored at several
`pLimit/wallLimit` settings. Exact labels were reused; only the q-free
pre-witness features were recomputed.

Group-rank result for `blockedRatio`:

```text
pLimit  wallLimit  rank 1   rank <= 5  mean peak rank
1000    1000        8 / 50   20 / 50    20.16
2000    1000       14 / 50   24 / 50    14.44
2000    2000       14 / 50   26 / 50    11.46
5000    1000       21 / 50   36 / 50     6.54
5000    5000       25 / 50   42 / 50     3.10
10000   1000       25 / 50   43 / 50     3.56
10000   5000       37 / 50   48 / 50     1.68
10000   10000      43 / 50   49 / 50     1.50
20000   10000      26 / 50   43 / 50     3.36
20000   20000      29 / 50   44 / 50     3.08
```

Interpretation:

The signal is not a one-parameter accident. It strengthens as the scan depth
approaches the known witness scale. But it is not monotone forever: extending
`pLimit` to `20000` worsens the local rank on this benchmark. That is
consistent with the feature being a wall-before-the-witness measurement. Once
the scan goes far beyond the relevant witness scale, it starts mixing in
post-witness structure that is not part of the obstruction.

Practical consequence:

```text
pressure scan depth should be tied to the witness scale being searched
```

For the Silva top-50 benchmark, the best tested setting is
`pLimit=10000, wallLimit=10000`, which matches the fact that the positive
witnesses lie in the range `8419..9781`.

### Contiguous local-window benchmark

Purpose:

The nearby-control benchmark uses random controls around each known peak. That
is useful, but it is easier than the real landing task. A stricter test is to
open a contiguous local window around a known peak and ask whether q-free wall
features rank the true peak near the top of that whole window.

Construction:

```text
source positives       top 10 Silva rows by known p
window radius          +/- 2,000
stride                 2
rows per window        2,001
total rows             20,010
label pLimit           50,000
label smallLimit       50,000
label workers          18
verified rows          20,010
positive rows          10
```

Each window contained exactly one row with `exactP >= 8419`: the known Silva
peak itself.

Single-depth result at `pLimit=10000, wallLimit=10000`:

```text
feature          global top 1%  group rank 1  group rank <=5  mean peak rank
blockedRatio      6 / 201        1 / 10        5 / 10          16.8
openGapMean       6 / 201        2 / 10        4 / 10          26.0
wallScore         1 / 201        0 / 10        1 / 10         259.0
```

The global top-1% random expectation is about `0.10` positives, so
`blockedRatio` is still strongly enriched. But exact local placement is much
harder than in the random-control benchmark.

Parameter sweep for `blockedRatio` on the same contiguous windows:

```text
pLimit  wallLimit  rank 1   rank <= 5  rank <= 10  mean peak rank
5000    5000        1 / 10    1 / 10     2 / 10     136.1
8000    8000        4 / 10    6 / 10     7 / 10      20.7
9000    9000        4 / 10    5 / 10     6 / 10      24.9
10000   10000       1 / 10    5 / 10     5 / 10      16.8
12000   12000       2 / 10    4 / 10     6 / 10      53.9
15000   15000       2 / 10    5 / 10     5 / 10      98.1
20000   20000       1 / 10    2 / 10     2 / 10      90.6
```

A simple multi-depth rank average over `8000`, `9000`, and `10000` did not
solve the localization problem:

```text
rank 1     4 / 10
rank <=10  7 / 10
worst case p=8941 had ensemble rank 106
```

Prefix survivor test:

The scorer was extended with q-free prefix survivor counts:

```text
openLe100, openLe500, openLe1000, openLe2000, openLe4000, openLe8000
openRatioLe100, ..., openRatioLe8000
```

These count how many admissible early `p` values survive the small-divisor
wall inside each prefix. On the same top-10 contiguous-window benchmark, these
prefix features did not beat the basic `blockedRatio/openRatio` rule. The
best prefix feature was effectively the full-depth prefix (`openLe8000`),
which is the same signal as `blockedRatio` at `pLimit=8000`.

Simple combinations with `firstOpenP`, `firstOpenIndex`, and `openGapMean`
were also tested informally. They did not improve group rank; most variants
made the exact peak rank worse. This is a useful negative result: the false
pressure maxima are not removed by merely demanding that the first open slot
appear later.

Local-radius test:

Although `blockedRatio` is not enough to rank the true peak first across a
wide `+/-2000` window, it behaves much better inside smaller local windows.
Using `pLimit=8000, wallLimit=8000`, the rank of the known peak by
`blockedRatio` changes as follows:

```text
radius  rank 1   rank <=3  rank <=5  mean peak rank
20       8 / 10   10 / 10   10 / 10   1.40
50       8 / 10    9 / 10   10 / 10   1.60
100      7 / 10    9 / 10    9 / 10   1.90
200      6 / 10    9 / 10    9 / 10   2.30
500      5 / 10    7 / 10    7 / 10   5.20
1000     4 / 10    5 / 10    7 / 10  10.60
2000     4 / 10    4 / 10    6 / 10  20.70
```

Local-radius interpretation:

This turns the local-window result from a simple failure into a useful
constraint. The wall-pressure rule can pick the peak very well after a close
landing, but it degrades as the opened window gets wider. In other words, the
current method can be a local refinement tool, not a wide-field locator.

Scout anchor test:

The next question was whether a cheap q-free scout can choose a close landing
inside a wider `+/-2000` window. A first, phase-aligned test sampled anchors
at fixed steps and ranked those anchors by `blockedRatio`.

If the anchor grid is allowed to pass through the true peak (`offset=0`), the
result looks very strong:

```text
anchor step  top K  hit radius  hit groups
40           10     20          10 / 10
50            5     20          10 / 10
100           3     20          10 / 10
200           2     20          10 / 10
400           2     20          10 / 10
```

But this is not a fair landing policy, because in an unknown field we do not
know the phase of the grid relative to the peak.

The same scout was therefore tested across all even grid phases. For each
phase, anchors were ranked by `blockedRatio`; success means at least one of
the selected top-K anchors landed within a given radius of the true peak.

Representative phase-swept results:

```text
anchor step  top K  hit radius  mean hit groups  min  max  enrichment
50            5     100         3.32 / 10        0    10   1.32x
100           5     100         2.86 / 10        0    10   1.13x
200           5     100         2.91 / 10        0    10   1.15x
400           5     100         2.69 / 10        0    10   1.07x
200           2      20         0.32 / 10        0    10   1.53x
400           1      20         0.19 / 10        0     9   1.77x
```

Interpretation:

The anchor scout is phase-sensitive. Once the grid phase is not secretly
aligned to the peak, the mean improvement over random anchor selection is
small, and the worst phases miss all known peaks in the top-10 benchmark.
This is a negative result for coarse fixed-grid landing.

Practical consequence:

```text
blockedRatio can refine a close landing, but a fixed coarse anchor grid does
not reliably create that landing
```

Dense q-free scout with local-max suppression:

The fixed-grid scout fails because of phase. A stronger q-free scout is to
compute `blockedRatio` densely over the whole wide window, then choose the top
local maxima with non-maximum suppression. This still does not compute exact
witnesses; it only uses the q-free wall map.

Tested on the same top-10 `+/-2000` windows using `pLimit=8000,
wallLimit=8000`.

```text
suppress radius  top K maxima  hit radius  hit groups  random expected
100              5             20          8 / 10      0.52 / 10
100              5             50          8 / 10      1.27 / 10
100              5             100         8 / 10      2.52 / 10
100              10            20          8 / 10      1.05 / 10
200              10            100         9 / 10      5.05 / 10
```

Representative selected maxima for `suppress=100, topK=5` show the remaining
failure mode:

```text
p=9629: selected offsets 1956, 1050, -1212, 1644, 294; peak offset 0 missed
p=8941: selected offsets 1424, 336, -868, 1556, 1736; peak offset 0 missed
```

Interpretation:

Dense q-free scouting is much better than fixed-grid scouting and avoids the
phase problem. It can reduce exact/refinement work substantially: selecting
five local maxima and opening radius `20` around each means roughly `105`
candidate even numbers per `2001`-row window, about `5%` of the dense window.
On this benchmark that covers `8/10` known peaks.

But it is still not a no-skip rule. Some windows contain false pressure maxima
whose `blockedRatio` is higher than the true peak. Dense scouting is therefore
a useful triage policy, not a proof-grade locator.

Top-50 close-window validation:

The close-window refinement claim was then tested on all top-50 Silva peaks
using contiguous `+/-200` windows.

```text
source positives       top 50 Silva rows by known p
window radius          +/- 200
stride                 2
rows per window        201
total rows             10,050
label pLimit           50,000
feature pLimit         8,000
feature wallLimit      8,000
verified rows          10,050
positive rows          50
```

Group-rank by `blockedRatio`:

```text
rank 1      35 / 50
rank <=5    45 / 50
rank <=10   49 / 50
mean rank    2.22
```

Global top-percent capture:

```text
top 1%       41 / 101
top 0.1%     11 / 11
top 0.01%     2 / 2
```

Dense NMS on these close windows:

```text
suppress radius  top K maxima  hit radius  hit groups  random expected
20               1             20          38 / 50      5.22 / 50
20               3             20          47 / 50     15.67 / 50
20               5             20          49 / 50     26.12 / 50
20               5             50          50 / 50     63.43 / 50
```

Interpretation:

This strengthens the close-refinement part of the strategy. Once a window has
already been narrowed to about `+/-200`, `blockedRatio` and dense local-max
selection recover nearly all known top-50 peaks. For example, taking only the
top 5 local-max centers already covers `49/50` peaks within radius `20`. If
the next stage tests only the centers, that is `5/201` rows per close window;
if it expands each center by radius `20`, the cost is higher and can approach
about half of the close window before overlap. The right expansion policy is
therefore a separate cost/coverage tradeoff, not solved here.

The caveat remains: this does not solve the earlier wide-window landing
problem. It validates a second-stage refinement tool, not a first-stage global
locator.

Depth calibration inside the same top-50 close windows:

The same exact labels were reused while rescoring the `+/-200` windows at
several matched `pLimit=wallLimit` depths. This tests whether "deeper is
always better" or whether the wall-pressure feature has a natural depth scale.

Result file:

```text
results/wall-spectrum/windows-top50/top50-r200-depth-summary.tsv
```

Blocked-ratio summary:

```text
pLimit  wallLimit  top 1%   top 0.1%  top 0.01%  rank 1   rank <=5  rank <=10  mean rank
8000    8000       41/101   11/11     2/2        35/50    45/50     49/50      2.22
9000    9000       41/101   10/11     2/2        35/50    47/50     48/50      2.06
10000   10000      37/101   10/11     2/2        38/50    46/50     49/50      2.50
12000   12000      36/101    9/11     2/2        36/50    45/50     48/50      3.72
```

Interpretation:

The refinement signal is stable across nearby depths, but the best depth
depends on the evaluation target. `8000` is strongest for global top-percent
capture, `9000` gives the best mean local rank, and `10000` gives the most
rank-1 placements inside the close windows. `12000` weakens the result. This
supports the earlier warning: scan depth is not a harmless knob. It should be
treated as part of the predictor and probably tied to the expected witness
scale or to a local spectrum, not simply pushed higher.

Local contrast / moving-background test:

A new metric tool was added:

```text
tools/wall_local_contrast_metrics.cpp
```

It computes q-free scores that compare each row's `blockedRatio` with its
nearby wall-pressure background inside the same source window. The variants
tested were local deltas and z-scores over radii `20`, `50`, `100`, `200`,
plus two shell backgrounds: `20..100` and `50..200`. This uses only the
pre-witness wall map; exact witnesses are still labels only.

Close top-50 `+/-200` summary:

```text
pLimit  score              top 1%   rank 1   rank <=5  rank <=10  mean rank
8000    blockedRatio       41/101   35/50    45/50     49/50      2.22
8000    localR100_delta    42/101   36/50    45/50     49/50      2.08
8000    shell50_200_delta  40/101   36/50    46/50     49/50      2.04
9000    blockedRatio       41/101   35/50    47/50     48/50      2.06
9000    localR20_delta     41/101   36/50    46/50     49/50      1.92
9000    shell20_100_delta  41/101   37/50    47/50     48/50      2.00
10000   blockedRatio       37/101   38/50    46/50     49/50      2.50
10000   localR50_delta     41/101   40/50    46/50     48/50      2.42
10000   localR100_delta    41/101   40/50    47/50     48/50      2.46
```

Wide top-10 `+/-2000` summary at `pLimit=wallLimit=8000`:

```text
score              top 1%   rank 1   rank <=5  rank <=10  mean rank
blockedRatio       7/201    4/10     6/10      7/10       20.7
localR100_delta    8/201    3/10     6/10      7/10       19.7
localR200_delta    8/201    3/10     6/10      7/10       19.5
shell50_200_delta  8/201    3/10     6/10      7/10       20.0
```

Interpretation:

Moving-background contrast is a small refinement, not the missing formula. In
close windows it can improve mean rank and sometimes rank-1 placement. In wide
windows it slightly improves global top-percent capture and mean rank, but it
does not remove false pressure maxima and does not improve the no-skip landing
problem. Z-score variants were usually worse than simple deltas.

NMS landing/refinement policy test:

The local-max selection stage was made reproducible with:

```text
tools/wall_nms_policy_metrics.cpp
```

This tool selects top-K q-free centers inside each benchmark window using
non-maximum suppression. It then measures:

- how many known peak groups are hit within a given radius;
- the average number of selected centers;
- the average number/fraction of rows covered after expanding around those
  centers;
- enrichment over a coverage-matched random expectation.

Result files:

```text
results/wall-spectrum/windows-top50/silva-top50-r200-nms-policy-p9000.tsv
results/wall-spectrum/windows/top10-r2000-nms-policy-p8000.tsv
```

Close top-50 `+/-200` windows at `pLimit=wallLimit=9000`:

```text
score              suppress  topK  hitR  hit groups  covered rows  covered fraction
blockedRatio       20        5     20    49/50       96.44         0.4798
localR20_delta     20        5     20    49/50       96.16         0.4784
shell20_100_delta  20        5     20    48/50       97.78         0.4865
localR20_delta     20       10     10    50/50      108.44         0.5395
blockedRatio       20       10     10    49/50      108.22         0.5384
blockedRatio       20       10     20    50/50      171.68         0.8541
```

Wide top-10 `+/-2000` windows at `pLimit=wallLimit=8000`:

```text
score              suppress  topK  hitR  hit groups  covered rows  covered fraction
blockedRatio       100       5     20    7/10        105.0         0.0525
localR100_delta    100       5     20    7/10        105.0         0.0525
shell50_200_delta  100       5     20    7/10        105.0         0.0525
localR50_delta      20      10     20    8/10        205.8         0.1028
blockedRatio        20      10     20    8/10        206.5         0.1032
localR50_delta      20      10    100    9/10        813.5         0.4065
```

Interpretation:

NMS makes the cost/coverage tradeoff explicit. In close windows, top-K local
maxima can cover nearly all peaks, but expanding around those centers is not
free: `49/50` at hit radius `20` already covers about `48%` of a `+/-200`
window; `50/50` at radius `20` with topK `10` covers about `85%`. In wide
windows, local contrast scores do not materially beat raw `blockedRatio`.
Getting from `7/10` to `8/10` or `9/10` requires selecting/covering much more
of the window. This confirms that NMS is useful for measured triage, but it is
not yet a no-skip landing formula.

Blocker-composition test:

The next hypothesis was that true peaks and false pressure maxima might be
distinguished by the composition of the wall: not only how many early
admissible `p` values are blocked, but which small primes do the blocking.

Added:

```text
tools/wall_blocker_profile_gmp.cpp
```

For each row, it computes the smallest small-prime blocker of each early
admissible `p`, then reports q-free features such as:

```text
uniqueBlockers
blockerEntropy
normalizedBlockerEntropy
blockerHerfindahl
topBlockerShare
low31Share, low101Share, low1000Share
meanBlocker, geomMeanBlocker
pressureEntropy
pressureAntiDominance
pressureConcentration
pressureLow31, pressureLow101, pressureLow1000
```

Result files:

```text
results/wall-spectrum/windows/top10-r2000-blocker-profile-p8000.tsv
results/wall-spectrum/windows/top10-r2000-blocker-profile-metrics-p8000.tsv
results/wall-spectrum/windows-top50/silva-top50-r200-blocker-profile-p9000.tsv
results/wall-spectrum/windows-top50/silva-top50-r200-blocker-profile-metrics-p9000.tsv
```

Wide top-10 `+/-2000` summary:

```text
feature           mean peak rank  rank <=10
blockedRatio      20.7            7/10
pressureLow1000   69.5            6/10
pressureEntropy  151.9            0/10
pressureLow101   164.9            1/10
```

Close top-50 `+/-200` summary:

```text
feature           mean peak rank  rank <=10
blockedRatio       2.06           48/50
pressureLow1000    6.22           44/50
pressureEntropy   13.16           25/50
pressureLow101    13.52           31/50
```

Interpretation:

This is a negative result for the simple blocker-composition hypothesis.
Composition features alone are much worse than `blockedRatio`, and the tested
pressure-times-composition variants also do not improve on raw wall pressure.
The best composition variant, `pressureLow1000`, still loses clearly in both
wide and close windows. At least in these benchmarks, the missing distinction
between true peaks and false pressure maxima is not captured by first-blocker
diversity, entropy, dominance, or small-blocker share.

Q-line transfer / post-landing generator:

Older q-line transfer results were consolidated into this note because they
remain relevant to the question "are q-leads useful here?"

Tool:

```text
tools/qfree_qline_scout_gmp.cpp
bin/qfree-qline-scout-gmp
```

For a local center `C` and even offset `t`, choose an odd `a`:

```text
C + t = (a + t) + (C - a)
```

If `C-a` is probable prime and `a+t` is prime, this line gives a Goldbach
witness for `C+t`. This does not prove minimality, because a smaller witness
may exist. Therefore q-line scouting is not the same thing as the q-free
pre-witness predictor. It is a post-landing candidate/ridge generator.

Existing transfer result files:

```text
results/qfree-landing-policy/qline-transfer/qline-transfer-comparison.md
results/qfree-landing-policy/qline-transfer/silva/qline-transfer-summary.tsv
results/qfree-landing-policy/qline-transfer/afterwall/qline-transfer-summary.tsv
results/qfree-landing-policy/e4999-probe/e4999-row154-summary.md
```

Common transfer settings:

```text
sort=maxP
aLimit=12000
qFilterLimit=250000
top=12000
qTestTop=12000
primalityReps=25
```

Silva/e18 top-50 result:

```text
windows tested                         50
known center peaks recovered exactly   50/50
local window maxima recovered exactly  50/50
center was local maximum               50/50
avg qTests to center q-line            221.14
median qTests to center q-line         225
max qTests to center q-line            265
full q-line scan average               792.30 qTests/window
exact local scan average               271.80 qTests/window
exact-minimal rows reproduced          76.06 / 101 rows average
```

Afterwall controls:

```text
windows tested                         4
known center peaks recovered exactly   4/4
local window maxima recovered exactly  4/4
center was local maximum               4/4
avg qTests to center q-line            595.25
full q-line scan average               799.25 qTests/window
exact local scan average               430.00 qTests/window
exact-minimal rows reproduced          146.25 / 201 rows average
```

e5000 ridge probe:

```text
center C                               2 * 10^5000 + 756
window                                 +/-100 even rows
exact pLimit                           50000
max verified p                         45953
q-line GMP tests                       686..824
high exact-minimal rows recovered      all rows with exact p >= 43000
```

Interpretation:

Q-lines are useful here, but not as the missing q-free wide-field locator. They
are a strong post-landing ridge generator: once a window is already centered
near a peak, q-line scouting recovers the known center peaks and local maxima
in these controls. On Silva/e18 the exact local scan is already cheap, so the
q-line layer is not cheaper there. On e5000, where exact local witness checks
were much more expensive, q-lines recovered the high ridge with much less
work. This supports a practical two-stage picture:

```text
q-free wall/pressure/scout -> choose or refine a landing window
q-line scout              -> generate high witness candidates inside that window
exact lower-p closure     -> decide whether the q-line witness is minimal
```

What remains unsolved is the first arrow: choosing the landing window in a
huge field without already knowing where the peak is.

Multi-point landing benchmark:

To test whether a small set of q-free anchor landings can beat dense NMS, a
phase-swept evaluator was added:

```text
tools/wall_multipoint_landing_metrics.cpp
```

Policy model:

```text
1. Choose a fixed grid step inside the top-10 wide +/-2000 windows.
2. For every even phase of that grid, rank grid anchors by blockedRatio.
3. Select top-K anchors.
4. Reveal +/-R around each selected anchor.
5. Count whether the true peak at offset 0 is covered.
```

This is intentionally phase-swept. If only the lucky grid phase works, the
policy is not a reliable landing rule.

Result files:

```text
results/wall-spectrum/windows/top10-r2000-multipoint-landing-p8000.tsv
results/wall-spectrum/windows/top10-r2000-multipoint-vs-nms-summary.tsv
```

Comparison against dense NMS on the same top-10 wide benchmark:

```text
policy      params                                      hit groups  min/max  covered rows  covered fraction
denseNMS    suppress=100 topK=5 hitR=20                 7/10        7/7      105.0         0.0525
denseNMS    suppress=20 topK=10 hitR=20                 8/10        8/8      206.5         0.1032
multipoint  step=40 topK=10 revealR=20 phase-swept      1.70/10     0/10     209.1         0.1045
multipoint  step=400 topK=1 revealR=200 phase-swept     1.21/10     0/9      196.9         0.0984
multipoint  step=400 topK=10 revealR=200 phase-swept   10.00/10    10/10    1950.6        0.9748
```

Interpretation:

The simple multi-point landing policy fails as a cheap wide-field locator. At
about the same revealed-row cost as dense NMS, phase-swept multi-point landing
hits far fewer peaks and has phases that miss all peaks. It can guarantee
coverage only by revealing almost the entire `+/-2000` window. This is another
negative result for a simple outer landing formula. The q-line scout remains
useful after landing, but this benchmark does not provide the missing landing
rule that would decide where to run it.

Reproducibility check:

```text
2026-06-22: make build wall-smoke passed.
2026-06-22: make qline-scout-smoke passed.
2026-06-22: make qline-smoke passed.
2026-06-22: wall_multipoint_landing_metrics reproduced
results/wall-spectrum/windows/top10-r2000-multipoint-landing-p8000.tsv
byte-for-byte from:
  results/wall-spectrum/windows/silva-top10-r2000-labels.tsv
  results/wall-spectrum/windows/features-top10-r2000-p8000-w8000.tsv
```

Multi-layer wall-persistence benchmark:

A distinct persistence evaluator was added:

```text
tools/wall_persistence_policy_metrics.cpp
```

The rule is q-free: it ranks rows only from pre-witness wall features already
computed at several `pLimit/wallLimit` depths. It does not test `q = N - p`
for primality. The exact witness labels are used only to score hit coverage.

Policy idea:

```text
1. For each calibrated depth, select local wall maxima by blockedRatio.
2. Let selected maxima vote for nearby rows.
3. Select final local maxima from the accumulated multi-layer vote map.
4. Measure whether the opened rows cover the known peak.
```

Primary top-10 wide benchmark:

```text
labels:
  results/wall-spectrum/windows/silva-top10-r2000-labels.tsv

5 calibrated layers:
  p5000/w5000
  p8000/w8000
  p9000/w9000
  p10000/w10000
  p12000/w12000

result file:
  results/wall-spectrum/windows/top10-r2000-persistence-policy-5layers.tsv
```

Comparable-cost result:

```text
policy                    params                                                   hit groups  covered rows
denseNMS baseline          p8000 suppress=20 topK=10 hitR=20                       8/10        206.5
5-layer persistence        layerSuppress=20 layerTopK=3 voteR=20
                           finalSuppress=100 finalTopK=10 hitR=20                 10/10       209.2
```

This is the first benchmark where a q-free rule beats raw dense NMS in the
wide `+/-2000` top-10 windows at comparable opened-row cost. The improvement
is not universal at all costs: around `105` opened rows, persistence gives
`6/10`, while the old dense NMS baseline gives `7/10`.

Depth check:

```text
7 layers = 5 calibrated layers plus p15000/w15000 and p20000/w20000
best observed 7-layer result: 9/10 at 210.0 covered rows
```

Interpretation: deeper is not automatically better. This supports the earlier
depth-calibration observation: the wall must be measured before/around the
expected witness scale, not arbitrarily far beyond it.

Close top-50 sanity check:

```text
labels:
  results/wall-spectrum/windows-top50/silva-top50-r200-labels.tsv

4 layers:
  p8000/w8000
  p9000/w9000
  p10000/w10000
  p12000/w12000

result file:
  results/wall-spectrum/windows-top50/silva-top50-r200-persistence-policy-4layers.tsv
```

Comparison:

```text
policy                    params                                                   hit groups  covered rows
denseNMS baseline          p9000 suppress=20 topK=10 hitR=20                       50/50       171.68
4-layer persistence        layerSuppress=50 layerTopK=10 voteR=100
                           finalSuppress=20 finalTopK=10 hitR=20                  50/50       139.5
```

Summary file:

```text
results/wall-spectrum/windows/persistence-vs-baseline-summary.tsv
```

Interpretation:

Multi-layer persistence is now the most promising q-free wall-spectrum
direction. It suppresses some false maxima that defeated one-layer dense NMS.
However, this is still a finite benchmark result. It is not a no-skip
algorithm, not a proof, and not yet a formula. The current policy was chosen
after looking at the top-10 wide benchmark, so the next required step is
validation with the policy frozen.

Frozen-policy holdout validation:

```text
Frozen policy chosen on top10:
  layerSuppress = 20
  layerTopK     = 3
  voteR         = 20
  finalSuppress = 100
  finalTopK     = 10
```

Two independent holdout sets were generated from Silva ranks not used to pick
the policy:

```text
results/wall-spectrum/validation-r11-r30-r2000/
results/wall-spectrum/validation-r31-r50-r2000/
```

Each holdout contains 20 center peaks and a contiguous `+/-2000` even window
around each center:

```text
rows per holdout: 40,020
label pLimit:     50,000
label smallLimit: 50,000
label workers:    18
feature layers:   5000, 8000, 9000, 10000, 12000
```

Summary file:

```text
results/wall-spectrum/validation-frozen5layer-summary.tsv
```

Strict `hitR=20` comparison:

```text
scope          policy                 hit groups  covered rows  interpretation
r11-r30-r2000  denseNMS comparable     19/20       205.0         baseline
r11-r30-r2000  frozen 5-layer          19/20       210.0         same strict coverage
r31-r50-r2000  denseNMS comparable     14/20       205.2         baseline
r31-r50-r2000  frozen 5-layer          18/20       209.75        improved strict coverage
```

Wider `hitR=100` comparison:

```text
scope          policy                 hit groups  covered rows
r11-r30-r2000  denseNMS comparable     19/20       815.45
r11-r30-r2000  frozen 5-layer          20/20       849.75
r31-r50-r2000  denseNMS comparable     19/20       769.85
r31-r50-r2000  frozen 5-layer          20/20       826.45
```

False negatives at strict `hitR=20`:

```text
r11-r30: p=8819, nearest selected offset +40
r31-r50: p=8443, nearest selected offset -28
r31-r50: p=8501, nearest selected offset -58
```

Interpretation:

The holdout validation is mixed but positive. The exact `10/10 vs 8/10`
advantage from the top10 benchmark did not transfer uniformly: on ranks
11-30, frozen persistence ties comparable-cost dense NMS at `19/20`.
However, on ranks 31-50 it improves strict coverage from `14/20` to `18/20`
at essentially the same opened-row cost. The strict misses are near misses,
not complete failures of the wall geometry: selected centers landed within
`28`, `40`, and `58` of the true peak.

This suggests that multi-layer persistence is a real enrichment/refinement
signal, but not yet a strict no-skip locator. Raising the reveal radius to
`100` catches all holdout centers, but opens about 40% of each `+/-2000`
window, so the next problem is cost control rather than basic detectability.

Adaptive reveal:

The near misses above suggested a cheaper q-free reveal rule:

```text
1. Start from frozen 5-layer vote centers.
2. Open base radius 20 around all 10 selected centers.
3. Expand only the first 4 selected centers to radius 60.
```

This policy was chosen after seeing the ranks 11-50 near misses, so ranks
11-50 are diagnostic, not a fresh validation set. A fresh validation set was
then created from Silva ranks 51-100.

Result files:

```text
results/wall-spectrum/adaptive-reveal-summary.tsv
results/wall-spectrum/windows/top10-r2000-adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r11-r30-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r31-r50-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r51-r100-r2000/adaptive-reveal-5layers.tsv
```

Summary:

```text
scope          dense strict  dense wide   frozen adaptive  dense adaptive
top10-r2000      8/10         8/10          10/10             8/10
r11-r30         19/20        19/20          20/20            19/20
r31-r50         14/20        19/20          20/20            15/20
r51-r100        42/50        46/50          47/50            43/50
```

Mean opened rows:

```text
dense strict       about 205 rows/window
frozen adaptive    about 367 rows/window
dense wide         about 770..815 rows/window
```

Interpretation:

Adaptive reveal closes the ranks 11-50 near misses at about half the cost of
uniform `hitR=100`. On the fresh ranks 51-100 set it still improves over both
dense strict and the same adaptive reveal applied to dense p8000 centers:
`47/50` versus `42/50` and `43/50`. It also slightly beats the full dense-wide
`hitR=100` coverage count on ranks 51-100 (`47/50` versus `46/50`), while
using less than half the opened rows.

Fresh r51-r100 misses for frozen adaptive:

```text
p=7949 nearest selected offset +112
p=8161 selected offset +34 exists but is 10th center and not expanded
p=8291 nearest selected offset -186
```

This means adaptive reveal is a useful cost/coverage policy, but it is still
not no-skip. The remaining misses are no longer only a strict-radius artifact.

Fresh miss diagnostics:

The r51-r100 holdout was profiled with an added q-free group profile output:

```text
tools/wall_persistence_policy_metrics.cpp --groupProfileOutput=...
```

Result files:

```text
results/wall-spectrum/validation-r51-r100-r2000/frozen-adaptive-group-profile.tsv
results/wall-spectrum/validation-r51-r100-r2000/fresh-miss-diagnostics-summary.tsv
```

Aggregate:

```text
rows                      50
hits                      47
misses                     3
hitMeanNearestDistance     10.127660
missMeanNearestDistance   110.666667
hitPeakScoreZero           13
missPeakScoreZero           3
```

The key point is that `peakScore=0` is not enough to identify failures:
13 successful windows also have `peakScore=0`. Those successful hard cases
are caught because a selected vote center lands very close to the true peak,
usually at offset `+/-10`.

Miss profiles:

```text
p=7949:
  nearest selected offset: +112
  nearest selected order: 1
  peakScore: 0
  peakScoreRank: 2001
  peak layer ranks: 5,5,17,9,4

p=8161:
  nearest selected offset: +34
  nearest selected order: 10
  peakScore: 0
  peakScoreRank: 2001
  peak layer ranks: 608,32,173,98,115

p=8291:
  nearest selected offset: -186
  nearest selected order: 5
  peakScore: 0
  peakScoreRank: 2001
  peak layer ranks: 77,77,63,148,44
```

Interpretation:

The three failures are not the same failure mode.

- `p=7949` has a strong raw center profile in the individual layers, but the
  frozen per-layer top-3 NMS voting misses it. This points to displacement by
  stronger nearby maxima, not absence of a wall.
- `p=8161` has a selected center at `+34`, but it is selected too late
  (10th) to be expanded by the current adaptive reveal.
- `p=8291` is weaker and farther: the nearest selected center is at `-186`.

This suggests a possible mechanistic variant: relax per-layer vote capture
from `layerTopK=3` to something like `layerTopK=5`, then validate on a fresh
holdout. That must not be claimed from ranks 51-100, because ranks 51-100
were used to diagnose the failure.

### Fresh r101-r150 validation of the K5 hypothesis

The `layerTopK=5` idea above was then frozen before looking at a new holdout
and tested on Silva ranks 101-150 with contiguous `+/-2000` windows.

Inputs:

```text
source rows:
  results/wall-spectrum/validation-r101-r150-r2000/silva-r101-r150-source.tsv
windows:
  results/wall-spectrum/validation-r101-r150-r2000/silva-r101-r150-r2000.tsv
labels:
  results/wall-spectrum/validation-r101-r150-r2000/silva-r101-r150-r2000-labels.tsv
features:
  results/wall-spectrum/validation-r101-r150-r2000/features-p5000-w5000.tsv
  results/wall-spectrum/validation-r101-r150-r2000/features-p8000-w8000.tsv
  results/wall-spectrum/validation-r101-r150-r2000/features-p9000-w9000.tsv
  results/wall-spectrum/validation-r101-r150-r2000/features-p10000-w10000.tsv
  results/wall-spectrum/validation-r101-r150-r2000/features-p12000-w12000.tsv
```

Label generation parameters:

```text
minimal-witness-batch-gmp
  pLimit=50000
  smallLimit=50000
  workers=18
```

Policy parameters:

```text
frozen adaptive K3:
  layers=5
  layerSuppress=20
  layerTopK=3
  voteR=20
  finalSuppress=100
  finalTopK=10
  baseR=20
  expandTop=4
  expandR=60

frozen adaptive K5:
  same, except layerTopK=5
```

Primary output:

```text
results/wall-spectrum/validation-r101-r150-r2000/validation-summary.tsv
results/wall-spectrum/validation-r101-r150-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r101-r150-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r101-r150-r2000/frozen-k3-adaptive-group-profile.tsv
results/wall-spectrum/validation-r101-r150-r2000/fresh-k3-miss-diagnostics-summary.tsv
results/wall-spectrum/frozen-adaptive-validation-summary.tsv
```

Summary:

```text
scope              policy                  hit groups  mean covered rows  covered fraction
r101-r150-r2000    dense strict             40/50       206.5              0.103198
r101-r150-r2000    dense wide               43/50       806.26             0.402929
r101-r150-r2000    frozen adaptive K3       47/50       368.26             0.184038
r101-r150-r2000    frozen adaptive K5       40/50       368.54             0.184178
r101-r150-r2000    dense p8000 adaptive     41/50       342.96             0.171394
```

Interpretation:

This is a negative result for the simplest "catch more per-layer maxima"
repair. Increasing `layerTopK` from 3 to 5 did not improve the r51-r100
failure mode on fresh data; it collapsed to the same `40/50` hit count as the
dense strict baseline, while opening about `368.5` rows per `+/-2000` window.
The likely reason is that the extra per-layer maxima add false persistent
centers faster than they recover displaced true centers. This is not a proof,
but it is the observed behavior on the fresh holdout.

Current best tested policy after this holdout remains:

```text
frozen 5-layer adaptive K3:
  47/50 on r51-r100
  47/50 on r101-r150
```

That is materially better than dense strict at comparable low cost and better
than dense wide at much lower cost, but it is still not no-skip.

K3 miss diagnostics on r101-r150:

```text
rows                      50
hits                      47
misses                     3
hitMeanNearestDistance    11.531915
missMeanNearestDistance  279.333333
hitPeakScoreZero          11
missPeakScoreZero          3
```

Miss profiles:

```text
p=7559:
  nearest selected offset: -48
  nearest selected order: 5
  nearest selected radius: 20
  peakScore: 0
  peakScoreRank: 2001
  peak layer ranks: 99,13,27,8,25

p=7727:
  nearest selected offset: -580
  nearest selected order: 9
  nearest selected radius: 20
  peakScore: 0
  peakScoreRank: 2001
  peak layer ranks: 142,11,24,62,7

p=7873:
  nearest selected offset: +210
  nearest selected order: 1
  nearest selected radius: 60
  peakScore: 0
  peakScoreRank: 2001
  peak layer ranks: 43,24,98,52,86
```

The repeated pattern is important:

- all three misses have `peakScore=0`;
- many successful rows also have `peakScore=0`;
- therefore `peakScore=0` is not a q-free rejection criterion;
- the misses are best described as displacement and priority failures.

Cheap expansion diagnostic from already computed policy tables:

```text
scope        K3 expandTop=4 R60  K3 expandTop=5 R60  K3 expandTop=4 R100  K3 expandTop=5 R100
top10        10/10, 367.2 rows   10/10, 407.2 rows   10/10, 506.2 rows    10/10, 574.4 rows
r11-r30      20/20, 366.85 rows  20/20, 406.35 rows  20/20, 504.95 rows   20/20, 579.75 rows
r31-r50      20/20, 368.4 rows   20/20, 404.7 rows   20/20, 503.9 rows    20/20, 567.7 rows
r51-r100     47/50, 367.6 rows   47/50, 406.4 rows   47/50, 503.18 rows   47/50, 572.82 rows
r101-r150    47/50, 368.26 rows  48/50, 407.98 rows  48/50, 501.08 rows   48/50, 571.48 rows
```

Interpretation:

Opening more around K3 centers is a cost/coverage knob, not a stable repair.
It recovers one additional peak on r101-r150, but it does nothing on
r51-r100. Wider reveal also increases opened rows from about `368` to
`408..571`, depending on the setting. Therefore this should be treated as a
fallback parameter, not as the next formula.

Consequence for the next experiment:

```text
Do not keep widening per-layer top-K as the main repair.
Look instead for a q-free local shape/priority correction that can decide
which selected centers deserve expansion, or when a displaced center implies
that a wider local reveal is needed.
```

### Fresh r151-r200 validation of shape-priority expansion

After the K5 failure, the next pre-declared mechanism was not to change which
K3 centers are selected. Instead, the selected centers remain the frozen K3
centers, and only the expansion priority is changed. The hypothesis was:

```text
If a selected wall center is part of a broader local wall island or shoulder,
it may deserve expanded radius before a narrower isolated selected center.
```

This is still q-free. The expansion priority uses only the frozen K3 vote
map:

```text
MassR100      sum of positive K3 vote scores within +/-100 of the center
ShoulderR100  sum of positive K3 vote scores at distance 20..100
MassR200      sum within +/-200
ShoulderR200  sum at distance 20..200
```

Implementation:

```text
tools/wall_persistence_policy_metrics.cpp
  local_mass_scores(...)
  evaluate_adaptive_reveal_by_expand_scores(...)
```

Fresh holdout:

```text
Silva ranks 151-200 by p
contiguous windows +/-2000
rows per group 2001
total rows 100050
labels pLimit=50000
labels smallLimit=50000
labels workers=18
feature layers: 5000, 8000, 9000, 10000, 12000
```

Input and output files:

```text
results/wall-spectrum/validation-r151-r200-r2000/silva-r151-r200-source.tsv
results/wall-spectrum/validation-r151-r200-r2000/silva-r151-r200-r2000.tsv
results/wall-spectrum/validation-r151-r200-r2000/silva-r151-r200-r2000-labels.tsv
results/wall-spectrum/validation-r151-r200-r2000/features-p5000-w5000.tsv
results/wall-spectrum/validation-r151-r200-r2000/features-p8000-w8000.tsv
results/wall-spectrum/validation-r151-r200-r2000/features-p9000-w9000.tsv
results/wall-spectrum/validation-r151-r200-r2000/features-p10000-w10000.tsv
results/wall-spectrum/validation-r151-r200-r2000/features-p12000-w12000.tsv
results/wall-spectrum/validation-r151-r200-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r151-r200-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r151-r200-r2000/validation-summary.tsv
results/wall-spectrum/validation-r151-r200-r2000/frozen-k3-adaptive-group-profile.tsv
results/wall-spectrum/validation-r151-r200-r2000/fresh-k3-miss-diagnostics-summary.tsv
```

Summary:

```text
policy                     hit groups  mean covered rows  covered fraction
dense strict                39/50       207.06             0.103478
dense wide                  44/50       805.98             0.402789
frozen adaptive K3          49/50       368.84             0.184328
frozen adaptive K5          44/50       368.50             0.184158
dense p8000 adaptive        40/50       347.38             0.173603
K3 MassR100 priority        49/50       365.56             0.182689
K3 ShoulderR100 priority    49/50       363.52             0.181669
K3 MassR200 priority        48/50       362.84             0.181329
K3 ShoulderR200 priority    47/50       364.12             0.181969
```

Interpretation:

This is a mixed result.

Positive:

- Frozen adaptive K3 received another fresh validation. It improved from
  dense strict `39/50` to `49/50`, and also beat dense wide `44/50` at less
  than half the opened rows.
- The K3 result is now stable across three fresh lower-rank holdouts:
  `47/50` on r51-r100, `47/50` on r101-r150, and `49/50` on r151-r200.

Negative/limited:

- K5 remains worse: `44/50` on r151-r200 after `40/50` on r101-r150.
- Shape-priority R100 did not improve hit count. It saved a small number of
  opened rows while preserving `49/50`; this is a minor cost optimization,
  not a new predictor.
- Shape-priority R200 worsened coverage. A broad local mass/shoulder score is
  not a stable expansion priority.

K3 miss diagnostic:

```text
rows                      50
hits                      49
misses                     1
hitMeanNearestDistance    12.9388
missMeanNearestDistance  700
hitPeakScoreZero          14
missPeakScoreZero          1

missed sourceP            7321
nearest selected offset   +700
nearest selected order    3
nearest selected radius   60
peakScore                 0
peakScoreRank             2001
peakLayerRanks            437,65,154,163,53
```

This is not a small priority miss. The selected wall island is displaced by
`700` from the true peak. A local expansion-priority rule cannot repair this
without opening a very wide part of the window. The next mechanism must target
far displacement or second-stage re-landing, not just "which selected center
gets radius 60".

### Fresh r201-r250 validation of gap reserve re-landing

The r151-r200 miss suggested a far-displacement failure: the selected K3 wall
island was far from the true peak, while dense p8000 `blockedRatio` still had
strong anchors near the true peak inside the uncovered gap. The next
pre-declared mechanism was therefore a second-stage q-free reserve:

```text
Primary:
  frozen adaptive K3
  finalSuppress=100
  finalTopK=10
  baseR=20
  expandTop=4
  expandR=60

Fallback:
  inspect only rows not covered by the primary K3 reveal
  score by dense p8000/w8000 blockedRatio
  fallbackSuppress=100
  fallbackTopK=1
  fallbackR=100
```

This still uses no exact witness and no primality of `q = N - p` in the
feature stage.

Implementation:

```text
tools/wall_persistence_policy_metrics.cpp
  is_covered_by_variable(...)
  evaluate_gap_relanding(...)
```

Diagnostic on already-used r151-r200:

```text
gap reserve top1 closed the single K3 miss:
  K3 adaptive      49/50, 368.84 rows
  gap reserve top1 50/50, 452.38 rows
```

This diagnostic motivated, but did not validate, the rule.

Fresh holdout:

```text
Silva ranks 201-250 by p
contiguous windows +/-2000
rows per group 2001
total rows 100050
labels pLimit=50000
labels smallLimit=50000
labels workers=18
feature layers: 5000, 8000, 9000, 10000, 12000
```

Input and output files:

```text
results/wall-spectrum/validation-r201-r250-r2000/silva-r201-r250-source.tsv
results/wall-spectrum/validation-r201-r250-r2000/silva-r201-r250-r2000.tsv
results/wall-spectrum/validation-r201-r250-r2000/silva-r201-r250-r2000-labels.tsv
results/wall-spectrum/validation-r201-r250-r2000/features-p5000-w5000.tsv
results/wall-spectrum/validation-r201-r250-r2000/features-p8000-w8000.tsv
results/wall-spectrum/validation-r201-r250-r2000/features-p9000-w9000.tsv
results/wall-spectrum/validation-r201-r250-r2000/features-p10000-w10000.tsv
results/wall-spectrum/validation-r201-r250-r2000/features-p12000-w12000.tsv
results/wall-spectrum/validation-r201-r250-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r201-r250-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r201-r250-r2000/validation-summary.tsv
results/wall-spectrum/validation-r201-r250-r2000/frozen-k3-adaptive-group-profile.tsv
results/wall-spectrum/validation-r201-r250-r2000/fresh-k3-miss-diagnostics-summary.tsv
```

Summary:

```text
policy                     hit groups  mean covered rows  covered fraction
dense strict                28/50       206.56             0.103228
dense wide                  33/50       801.10             0.400350
frozen adaptive K3          44/50       367.84             0.183828
frozen adaptive K5          41/50       367.84             0.183828
dense p8000 adaptive        30/50       345.74             0.172784
shape-priority MassR100     43/50       362.66             0.181239
gap dense reserve top1      44/50       456.42             0.228096
gap dense reserve top2      46/50       542.44             0.271084
K3 wide top5/R100           48/50       573.04             0.286377
```

Interpretation:

The second-stage reserve did not generalize as a clean repair.

Positive:

- K3 still strongly beats dense baselines on this harder lower-rank holdout:
  `44/50` versus dense strict `28/50` and dense wide `33/50`.
- A larger fallback (`fallbackTopK=2`) recovers two additional peaks, reaching
  `46/50`.

Negative/limited:

- The pre-declared fallbackTopK=1 rule gives `44/50`, exactly the same hit
  count as K3, while increasing mean opened rows from `367.84` to `456.42`.
- fallbackTopK=2 is still weaker than the expensive broad K3 comparison:
  K3 top5/R100 gives `48/50` at `573.04` rows.
- Shape-priority worsens hit count on this holdout.
- K5 remains worse.

K3 miss diagnostics:

```text
rows                      50
hits                      44
misses                     6
hitMeanNearestDistance    13.3636
missMeanNearestDistance  138.667
hitPeakScoreZero          17
missPeakScoreZero          6

miss sourceP values:
  6619, 6659, 6661, 6679, 6779, 6823
```

Miss profiles in short form:

```text
sourceP  nearestOffset  nearestOrder  nearestRadius  peakLayerRanks
6619     -56            5             20             13,6,11,43,12
6659     -498           5             20             84,305,523,421,657
6661     -100           5             20             29,20,9,22,18
6679     +100           1             60             10,8,8,22,36
6779     +26            5             20             18,7,13,22,71
6823     +52            8             20             118,38,32,25,36
```

This is not the same failure as r151-r200. The new miss set mixes:

- near priority/radius misses (`+26`, `+52`, `-56`);
- selected-but-not-wide-enough misses (`+/-100`);
- one farther displacement (`-498`).

The lesson is that the current misses are heterogeneous. A simple reserve
anchor repairs a specific far-displacement example but does not solve the
general failure class.

### Fresh r251-r300 validation of confidence-tier reveal

The r201-r250 miss set mixed near priority/radius misses and far displacement.
Before trying another reserve anchor, the next pre-declared mechanism was a
simple confidence-tier reveal using the same frozen K3 centers:

```text
selected K3 center #1     radius 100
selected K3 centers #2-5  radius 60
selected K3 centers #6+   radius 20
```

This rule is q-free: it uses only the selected K3 center order, not exact
witnesses or primality of `q`.

Diagnostic on already-used r201-r250:

```text
top5/R60                 46/50
tier top1R100/top5R60    47/50
```

This diagnostic motivated a fresh validation on ranks 251-300.

Implementation:

```text
tools/wall_persistence_policy_metrics.cpp
  evaluate_tiered_reveal(...)
```

Fresh holdout:

```text
Silva ranks 251-300 by p
contiguous windows +/-2000
rows per group 2001
total rows 100050
labels pLimit=50000
labels smallLimit=50000
labels workers=18
feature layers: 5000, 8000, 9000, 10000, 12000
```

Input and output files:

```text
results/wall-spectrum/validation-r251-r300-r2000/silva-r251-r300-source.tsv
results/wall-spectrum/validation-r251-r300-r2000/silva-r251-r300-r2000.tsv
results/wall-spectrum/validation-r251-r300-r2000/silva-r251-r300-r2000-labels.tsv
results/wall-spectrum/validation-r251-r300-r2000/features-p5000-w5000.tsv
results/wall-spectrum/validation-r251-r300-r2000/features-p8000-w8000.tsv
results/wall-spectrum/validation-r251-r300-r2000/features-p9000-w9000.tsv
results/wall-spectrum/validation-r251-r300-r2000/features-p10000-w10000.tsv
results/wall-spectrum/validation-r251-r300-r2000/features-p12000-w12000.tsv
results/wall-spectrum/validation-r251-r300-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r251-r300-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r251-r300-r2000/validation-summary.tsv
results/wall-spectrum/validation-r251-r300-r2000/frozen-k3-adaptive-group-profile.tsv
results/wall-spectrum/validation-r251-r300-r2000/fresh-k3-miss-diagnostics-summary.tsv
```

Summary:

```text
policy                     hit groups  mean covered rows  covered fraction
dense strict                34/50       207.74             0.103818
dense wide                  35/50       823.12             0.411354
frozen adaptive K3          46/50       367.94             0.183878
frozen adaptive K5          35/50       368.44             0.184128
dense p8000 adaptive        34/50       350.96             0.175392
shape-priority MassR100     46/50       364.12             0.181969
confidence tier             47/50       442.12             0.220950
gap reserve top1            47/50       453.24             0.226507
K3 wide top5/R100           48/50       578.42             0.289065
```

Interpretation:

This is a small positive result, but not a breakthrough.

Positive:

- K3 still beats dense baselines strongly: `46/50` versus dense strict
  `34/50` and dense wide `35/50`.
- The tier rule improves over standard K3 by one hit: `47/50` versus `46/50`.
- It is slightly cheaper than gap reserve top1 at the same hit count:
  `442.12` rows versus `453.24` rows.

Negative/limited:

- Tier is still worse than the expensive broad K3 top5/R100 comparison:
  `47/50` versus `48/50`.
- The improvement is exactly the kind expected from a radius class repair,
  not evidence of no-skip localization.
- K5 remains strongly negative on this lower-rank holdout.

K3 miss diagnostics:

```text
rows                      50
hits                      46
misses                     4
hitMeanNearestDistance    10.3913
missMeanNearestDistance   86
hitPeakScoreZero          16
missPeakScoreZero          4
```

Miss profiles in short form:

```text
sourceP  nearestOffset  nearestOrder  nearestRadius  peakLayerRanks
6221     -120           6             20             18,51,37,26,5
6247     +24            7             20             4,47,26,101,157
6379     -100           2             60             6,4,7,10,33
6421     -100           1             60             745,58,75,69,162
```

The tier rule directly addresses the `center #1 at distance 100` class, so it
can plausibly recover one of the four K3 misses. It does not address lower
priority near misses such as order 6/7 at `+24` or `-120`.

### Holdout frontier summary through r300

The repeated fresh holdout tests were consolidated into:

```text
results/wall-spectrum/holdout-frontier-summary.tsv
```

The key comparable policies across the five fresh `+/-2000` holdouts
`r51-r100`, `r101-r150`, `r151-r200`, `r201-r250`, and `r251-r300` are:

```text
policy               holdouts  hit groups  hit rate  mean covered rows
K3 adaptive          5         233/250     93.2%     368.10
dense wide           5         201/250     80.4%     807.06
dense adaptive       5         188/250     75.2%     347.24
dense strict         5         183/250     73.2%     206.92
```

Partly comparable newer variants:

```text
policy               holdouts  hit groups  hit rate  mean covered rows
K3 wide top5/R100    2          96/100     96.0%     575.73
confidence tier      1          47/50      94.0%     442.12
gap reserve top1     2          91/100     91.0%     454.83
K5                   4         160/200     80.0%     368.33
MassR100             3         138/150     92.0%     364.11
```

Interpretation:

The current evidence supports a narrower but real claim:

```text
Multi-layer K3 adaptive reveal is a strong q-free triage policy on these
Silva local holdouts.
```

It does not support a stronger claim:

```text
The method is not a no-skip predictor and does not reliably locate every peak.
```

Why this matters:

- K3 adaptive beats dense strict by a large coverage margin:
  `93.2%` versus `73.2%`.
- K3 adaptive also beats dense wide in coverage while opening less than half
  as many rows: `368.10` versus `807.06`.
- The experimental repairs after K3 are not yet compelling. K5 is clearly
  worse. Shape-priority and gap reserve do not generalize as clean repairs.
  Confidence tier gives one targeted improvement on one holdout, but does not
  change the overall conclusion.
- The observed remaining misses are heterogeneous: near radius/priority,
  selected-but-not-wide-enough, and far-displacement cases. A single scalar
  q-free score has not separated them.

Wide-window interpretation:

The pressure signal survives the harder contiguous-window test, but the
previous random-control numbers were optimistic. In a dense window there are
many false wall-pressure maxima: points with high `blockedRatio` but small
true `p(N)`. The known peak is still often near the top, but not reliably
enough to be a no-skip local locator.

This is the current practical picture:

```text
blockedRatio is good for enrichment and triage, not yet for exact peak picking
```

The next useful rule must distinguish a real pre-witness wall from a false
pressure maximum. A simple multi-depth average and prefix survivor counts are
not enough. A more promising route is to combine coarse landing with small
radius refinement instead of expecting one wide-window score to do both jobs.

Modulo-6 gate on the larger benchmark:

```text
positive nMod6 distribution:  nMod6=2: 30, nMod6=4: 20, nMod6=0: 0
control  nMod6 distribution:  nMod6=0: 1694, nMod6=2: 1664, nMod6=4: 1642
```

In this sample, the cheap gate `N mod 6 != 0` removes about one third of the
controls with no loss of the known positives. After this gate:

```text
rows after gate        3356
positives               50

feature          top 1%         top 0.1%       top 0.01%
blockedRatio     31 / 34         4 / 4          1 / 1
openGapMean      31 / 34         4 / 4          1 / 1
```

Interpretation:

This was the strongest positive result before the multi-layer persistence
benchmark. It remains useful background: near known large Silva peaks, the
early admissible lane has unusually high small-divisor pressure, and the
simplest pressure features separate those peaks from nearby random even
controls.

The first combined `wallScore` is not the best rule. The useful signal in
this benchmark is simpler:

```text
large p(N) candidate -> unusually high blockedRatio / low openRatio
```

What this does not prove:

- It does not tell us how to choose anchor points in a huge unlabelled field.
- It does not guarantee that all peaks would be captured.
- It does not yet compare against a full grid of all even numbers in an
  interval.
- It does not establish novelty; prior art still has to be checked before any
  claim beyond "local benchmark measurement".

## Current working conclusion

What appears real:

- Small-divisor wall structure around early admissible `p` values is not
  random decoration; known Silva peaks often look locally more closed than
  immediate neighbors.
- On exact-labeled nearby-control benchmarks, high `blockedRatio` and low
  `openRatio` strongly enrich for known large Silva peaks.
- On contiguous local windows, `blockedRatio` still enriches strongly, but it
  produces false local maxima and does not reliably rank the exact peak first.
- Within close local windows (`+/-20` to `+/-200` in the top-10 benchmark),
  `blockedRatio` ranks the known peak very near the top, suggesting a possible
  landing-then-refine workflow.
- The close-window result generalizes from top-10 to top-50: in `+/-200`
  windows, `blockedRatio` puts `49/50` peaks in the local top 10 and dense NMS
  with top-5 maxima plus radius 20 covers `49/50`.
- Depth calibration on top-50 close windows shows a stable signal from
  `8000` to `10000`, but `12000` already weakens the result. This argues for
  scale/depth calibration rather than a simple "use the largest pLimit" rule.
- Moving-background contrast is a modest local refinement. It improves some
  close-window ranks, but it does not solve wide-window landing and does not
  eliminate false pressure maxima.
- A fixed-step anchor scout is not robust enough: phase-aligned anchors look
  strong, but phase-swept anchors often miss the peak.
- Dense q-free local-max scouting is better and can reduce local exact work,
  but it still misses some peaks and is not no-skip.
- Multi-layer wall persistence is the first tested q-free rule that improves
  wide-window coverage over raw dense NMS at comparable opened-row cost:
  `10/10` top-10 `+/-2000` peaks at about `209` opened rows versus the
  dense-NMS baseline `8/10` at about `206.5` opened rows.
- Frozen-policy holdout validation is mixed but supports the signal:
  ranks 11-30 tie comparable dense NMS at `19/20`, while ranks 31-50 improve
  from `14/20` to `18/20` at essentially the same strict `hitR=20` cost.
- The remaining frozen-policy strict misses are near misses. The selected
  centers land within `28`, `40`, and `58` of the true center peaks.
- Adaptive reveal around the top multi-layer vote centers closes the ranks
  11-50 near misses at about `367` opened rows per `+/-2000` window, roughly
  half the cost of uniform `hitR=100`.
- Fresh ranks 51-100 validation supports but limits the adaptive rule:
  frozen adaptive gives `47/50`, dense strict gives `42/50`, dense adaptive
  gives `43/50`, and dense wide gives `46/50`.
- Fresh r51-r100 diagnostics show three distinct miss modes: displaced strong
  wall (`p=7949`), low-priority near hit (`p=8161`), and far weak wall
  (`p=8291`).
- Fresh r101-r150 validation rejects the simplest K5 repair:
  `layerTopK=5` falls to `40/50` at the same opened-row cost where frozen K3
  gives `47/50`. The current best tested policy remains frozen adaptive K3.
- Fresh r101-r150 diagnostics repeat the same basic miss story: `peakScore=0`
  is not a safe discriminator, and the failures are displacement/priority
  misses rather than obvious absence of a wall.
- Fresh r151-r200 validation gives the strongest K3 holdout so far:
  `49/50`, versus dense strict `39/50` and dense wide `44/50`. Shape-priority
  R100 preserved `49/50` with slightly lower cost, but did not recover the
  remaining peak. Shape-priority R200 worsened coverage.
- Fresh r201-r250 validation is a harder negative/mixed result. K3 still beats
  dense baselines (`44/50` versus dense strict `28/50`), but the pre-declared
  gap reserve top1 does not improve hit count, and reserve top2 is still worse
  than expensive broad K3 top5/R100.
- Fresh r251-r300 validation gives a small positive result for confidence
  tiers: tier top1R100/top5R60 improves K3 from `46/50` to `47/50` at
  `442.12` rows, cheaper than gap reserve top1. It is still worse than broad
  K3 top5/R100 (`48/50`) and is not no-skip.
- The consolidated fresh holdout frontier through r300 supports K3 adaptive
  as a reproducible triage frontier improvement: `233/250` hits at about
  `368` opened rows, versus dense strict `183/250` at about `207` rows and
  dense wide `201/250` at about `807` rows.
- The same persistence idea improves close top-50 cost: `50/50` coverage at
  about `139.5` opened rows versus dense NMS `50/50` at about `171.68`.
- A landing-and-local-analysis strategy may still be useful after some other
  method chooses an anchor area.

What is not solved:

- We do not yet have a validated q-free landing rule for huge fields.
- We also do not yet have a no-skip q-free local peak picker inside a dense
  window.
- The coarse fixed-grid scout is not a reliable landing rule.
- Dense q-free scouting and multi-layer persistence are triage rules, not
  guarantees.
- The current scalar `wallScore` is not enough; `blockedRatio` is the better
  first ranking statistic in the current benchmark.
- Local background subtraction by itself is not enough. Multi-layer
  persistence plus adaptive reveal is now the best candidate policy, but it
  still misses some lower-rank Silva peaks in fresh validation.
- The current tests do not justify claiming a new mathematical method or a
  no-skip shortcut.

Next direction:

1. Treat `blockedRatio/openRatio` plus dense NMS as the baseline.
2. Treat multi-layer persistence as the current best q-free candidate rule.
3. Do not tune the frozen top10 policy on the holdout rows.
4. Do not tune adaptive reveal on ranks 51-100, 101-150, 151-200, 201-250,
   or 251-300. The pre-declared `layerTopK=5` variant failed,
   shape-priority expansion only produced a small cost saving on one holdout,
   gap reserve top1 failed to improve fresh r201-r250, and confidence tier
   produced only a small targeted improvement on r251-r300.
5. Next step should not be another ad hoc reveal tweak unless it improves the
   frontier on a fresh holdout. The current evidence is strong enough to write
   a clean negative/positive research summary: positive for q-free triage,
   negative for no-skip prediction.
6. Tie pressure-scan depth to the expected witness scale instead of making
   `pLimit` arbitrarily large.
7. Keep exact witness computation out of the feature stage; use it only for
   final labels.

## 2026-06-22 reproducible frontier aggregation

The consolidated holdout frontier is now generated by a dedicated C++ tool
instead of being a hand-copied table.

Tool:

```text
tools/wall_frontier_summary.cpp
bin/wall-frontier-summary
```

Input:

```text
results/wall-spectrum/frozen-adaptive-validation-summary.tsv
```

Output:

```text
results/wall-spectrum/holdout-frontier-summary.tsv
```

Command:

```text
./bin/wall-frontier-summary \
  --input=results/wall-spectrum/frozen-adaptive-validation-summary.tsv \
  --output=results/wall-spectrum/holdout-frontier-summary.tsv
```

The tool filters to the five fresh `+/-2000` holdouts:

```text
r51-r100-r2000
r101-r150-r2000
r151-r200-r2000
r201-r250-r2000
r251-r300-r2000
```

It normalizes old/new policy names where needed:

```text
denseNMS + hitR=20  -> denseNMS_strict
denseNMS + hitR=100 -> denseNMS_wide
frozen5LayerAdaptive -> frozen5LayerAdaptiveK3
```

The generated table was compared against the previously saved frontier with:

```text
diff -u results/wall-spectrum/holdout-frontier-summary.tsv \
       results/wall-spectrum/holdout-frontier-summary.rebuilt.tsv
```

No differences were reported, and the main TSV was regenerated from the tool.

Comparable all-five-holdout frontier:

```text
policy                  holdouts  hitGroups  hitRate   meanCoveredRows
frozen5LayerAdaptiveK3  5         233/250    0.932000  368.096000
denseNMS_wide           5         201/250    0.804000  807.060000
denseP8000Adaptive      5         188/250    0.752000  347.240000
denseNMS_strict         5         183/250    0.732000  206.920000
```

Interpretation:

- K3 adaptive is now reproducibly better than dense strict and dense wide on
  coverage in these fresh holdouts.
- K3 adaptive beats dense wide while opening less than half as many rows.
- This is still a q-free triage/enrichment result, not a no-skip peak finder.
- The failed repair attempts remain meaningful negative results: K5, shape
  priority, gap reserve, and confidence-tier reveal do not establish a
  general no-skip rule.

A compact restart-facing interpretation was added:

```text
docs/wall-spectrum-frontier-summary.md
```

Future experiments should treat this table as the current frontier. A new
policy should be pre-registered on a fresh holdout and compared against K3
adaptive, dense strict, dense wide, and dense adaptive by both coverage and
opened-row cost.

## 2026-06-22 fresh r301-r350 validation

To avoid tuning on the already used ranks, a new fresh Silva holdout was run
with the frozen policy set:

```text
Silva ranks 301-350 by p
contiguous windows +/-2000
rows per group 2001
total rows 100050
labels pLimit=50000
labels smallLimit=50000
labels workers=18
feature layers: 5000, 8000, 9000, 10000, 12000
```

`wall_benchmark_windows_gmp` was extended with `--skip` so the rank slice can
be generated without a manual `sort | sed` step:

```text
tools/wall_benchmark_windows_gmp.cpp
  --skip=300 --top=50
```

Input and output files:

```text
results/wall-spectrum/validation-r301-r350-r2000/silva-r301-r350-source.tsv
results/wall-spectrum/validation-r301-r350-r2000/silva-r301-r350-r2000.tsv
results/wall-spectrum/validation-r301-r350-r2000/silva-r301-r350-r2000-labels.tsv
results/wall-spectrum/validation-r301-r350-r2000/features-p5000-w5000.tsv
results/wall-spectrum/validation-r301-r350-r2000/features-p8000-w8000.tsv
results/wall-spectrum/validation-r301-r350-r2000/features-p9000-w9000.tsv
results/wall-spectrum/validation-r301-r350-r2000/features-p10000-w10000.tsv
results/wall-spectrum/validation-r301-r350-r2000/features-p12000-w12000.tsv
results/wall-spectrum/validation-r301-r350-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r301-r350-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r301-r350-r2000/frozen-k3-adaptive-group-profile.tsv
results/wall-spectrum/validation-r301-r350-r2000/fresh-k3-miss-diagnostics-summary.tsv
results/wall-spectrum/validation-r301-r350-r2000/validation-summary.tsv
```

Fresh r301-r350 headline:

```text
policy                     hit groups  mean opened rows  covered fraction
dense strict                24/50       206.26            0.103078
dense wide                  40/50       812.04            0.405817
frozen adaptive K3          42/50       368.62            0.184218
frozen adaptive K5          37/50       368.42            0.184118
dense p8000 adaptive        26/50       343.04            0.171434
shape-priority MassR100     42/50       364.54            0.182179
confidence tier             42/50       443.62            0.221699
gap reserve top1            42/50       453.60            0.226687
gap reserve top2            43/50       539.56            0.269645
K3 wide top5/R100           45/50       579.52            0.289615
```

Interpretation:

- This is the weakest fresh K3 holdout so far: `42/50`.
- K3 still beats the directly comparable dense baselines:
  `42/50` versus dense wide `40/50`, dense adaptive `26/50`, and dense
  strict `24/50`.
- The margin over dense wide is small on this slice, so this is an important
  anti-overclaim result.
- K5 remains worse than K3.
- MassR100 does not improve hit count.
- Confidence tier and reserve top1 do not improve hit count.
- Reserve top2 recovers one extra group at much higher cost, but remains
  worse than expensive K3 wide.
- Broad K3 top5/R100 reaches `45/50`, again showing that more coverage can
  be bought with more opened rows, not that we have a no-skip rule.

K3 miss diagnostics:

```text
rows                       50
hits                       42
misses                      8
hitMeanNearestDistance     10.6667
missMeanNearestDistance   122.5
hitPeakScoreZero           16
missPeakScoreZero           8
miss sourceP values        5717, 5813, 5857, 5867, 5927, 5981, 5987, 6079
```

Miss profiles in short:

```text
sourceP  nearestOffset  nearestDistance  nearestOrder  nearestRadius
5717     -156           156              9             20
5813     -106           106              1             60
5857      -32            32              6             20
5867      100           100              2             60
5927     -100           100              4             60
5981     -100           100              6             20
5987       40            40              7             20
6079     -346           346              2             60
```

The miss class remains heterogeneous:

- selected but just outside radius (`-106`, `+100`, `-100`);
- lower-priority near misses (`-32`, `+40`, `-100`);
- one larger displacement (`-346`);
- all eight misses have `peakScore=0`, but so do many hits, so this is not a
  safe rejection rule.

The combined fresh holdout frontier was regenerated with
`tools/wall_frontier_summary.cpp`, now including r301-r350 by default:

```text
policy                  holdouts  hitGroups  hitRate   meanCoveredRows
frozen5LayerAdaptiveK3  6         275/300    0.916667  368.183333
denseNMS_wide           6         241/300    0.803333  807.890000
denseP8000Adaptive      6         214/300    0.713333  346.540000
denseNMS_strict         6         207/300    0.690000  206.810000
```

Current conclusion after r301-r350:

```text
The K3 wall-persistence rule remains the best directly comparable q-free
triage policy, but r301-r350 lowers confidence in any stronger claim. The
result is enrichment, not no-skip prediction.
```

## 2026-06-22 fresh r351-r400 validation

One more pre-registered fresh slice was run with the same frozen policy set,
without changing the K3 rule after r301-r350.

```text
Silva ranks 351-400 by p
contiguous windows +/-2000
rows per group 2001
total rows 100050
labels pLimit=50000
labels smallLimit=50000
labels workers=18
feature layers: 5000, 8000, 9000, 10000, 12000
```

Input and output files:

```text
results/wall-spectrum/validation-r351-r400-r2000/silva-r351-r400-source.tsv
results/wall-spectrum/validation-r351-r400-r2000/silva-r351-r400-r2000.tsv
results/wall-spectrum/validation-r351-r400-r2000/silva-r351-r400-r2000-labels.tsv
results/wall-spectrum/validation-r351-r400-r2000/features-p5000-w5000.tsv
results/wall-spectrum/validation-r351-r400-r2000/features-p8000-w8000.tsv
results/wall-spectrum/validation-r351-r400-r2000/features-p9000-w9000.tsv
results/wall-spectrum/validation-r351-r400-r2000/features-p10000-w10000.tsv
results/wall-spectrum/validation-r351-r400-r2000/features-p12000-w12000.tsv
results/wall-spectrum/validation-r351-r400-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r351-r400-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r351-r400-r2000/frozen-k3-adaptive-group-profile.tsv
results/wall-spectrum/validation-r351-r400-r2000/fresh-k3-miss-diagnostics-summary.tsv
results/wall-spectrum/validation-r351-r400-r2000/validation-summary.tsv
```

The five feature layers were run in parallel as independent scorer processes.
This did not change the method; it only avoided waiting for the layers
sequentially.

Fresh r351-r400 headline:

```text
policy                     hit groups  mean opened rows  covered fraction
dense strict                25/50       206.80            0.103348
dense wide                  35/50       795.92            0.397761
frozen adaptive K3          44/50       368.66            0.184238
frozen adaptive K5          34/50       368.88            0.184348
dense p8000 adaptive        25/50       344.46            0.172144
K3 wide top5/R100           47/50       574.80            0.287256
shape-priority MassR100     46/50       365.48            0.182649
confidence tier             46/50       441.36            0.220570
gap reserve top1            44/50       449.74            0.224758
gap reserve top2            44/50       533.18            0.266457
```

Interpretation:

- K3 partially recovers after the weaker r301-r350 holdout: `44/50`.
- K3 clearly beats the directly comparable dense baselines on this slice:
  dense wide is `35/50`, dense adaptive `25/50`, dense strict `25/50`.
- K5 remains worse than K3.
- MassR100 and confidence tier both reach `46/50` on this slice, but they are
  not yet a frozen replacement for K3 because prior slices did not show stable
  dominance.
- K3 wide reaches `47/50` at higher cost, consistent with the earlier
  observation that broader reveal buys coverage but does not prove no-skip.

K3 miss diagnostics:

```text
rows                       50
hits                       44
misses                      6
hitMeanNearestDistance     12.6364
missMeanNearestDistance    73.6667
hitPeakScoreZero           18
missPeakScoreZero           5
miss sourceP values        5303, 5333, 5393, 5557, 5581, 5647
```

Miss profiles in short:

```text
sourceP  nearestOffset  nearestDistance  nearestOrder  nearestRadius
5303       26            26              5             20
5333       22            22             10             20
5393       56            56              5             20
5557     -190           190              6             20
5581     -100           100              2             60
5647       48            48              7             20
```

This miss set is mostly near/radius/priority failure, not absence of a wall.
It explains why broader/tiered reveal recovers some rows, but it still does
not supply a no-skip rule.

The combined fresh holdout frontier was regenerated with
`tools/wall_frontier_summary.cpp`, now including r351-r400 by default:

```text
policy                  holdouts  hitGroups  hitRate   meanCoveredRows
frozen5LayerAdaptiveK3  7         319/350    0.911429  368.251429
denseNMS_wide           7         276/350    0.788571  806.180000
denseP8000Adaptive      7         239/350    0.682857  346.242857
denseNMS_strict         7         232/350    0.662857  206.808571
```

Current conclusion after r351-r400:

```text
K3 remains the best directly comparable q-free triage baseline. The last two
fresh slices show variance, but not collapse. The right claim remains
enrichment/triage, not a complete predictor and not a no-skip algorithm.
```

## 2026-06-22 fresh r401-r450 validation

One final stability slice was run with the same frozen policy set. This was
not a search for a new rule; it was a stability check after the r301-r350 and
r351-r400 variance.

```text
Silva ranks 401-450 by p
contiguous windows +/-2000
rows per group 2001
total rows 100050
labels pLimit=50000
labels smallLimit=50000
labels workers=18
feature layers: 5000, 8000, 9000, 10000, 12000
```

Input and output files:

```text
results/wall-spectrum/validation-r401-r450-r2000/silva-r401-r450-source.tsv
results/wall-spectrum/validation-r401-r450-r2000/silva-r401-r450-r2000.tsv
results/wall-spectrum/validation-r401-r450-r2000/silva-r401-r450-r2000-labels.tsv
results/wall-spectrum/validation-r401-r450-r2000/features-p5000-w5000.tsv
results/wall-spectrum/validation-r401-r450-r2000/features-p8000-w8000.tsv
results/wall-spectrum/validation-r401-r450-r2000/features-p9000-w9000.tsv
results/wall-spectrum/validation-r401-r450-r2000/features-p10000-w10000.tsv
results/wall-spectrum/validation-r401-r450-r2000/features-p12000-w12000.tsv
results/wall-spectrum/validation-r401-r450-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r401-r450-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r401-r450-r2000/frozen-k3-adaptive-group-profile.tsv
results/wall-spectrum/validation-r401-r450-r2000/fresh-k3-miss-diagnostics-summary.tsv
results/wall-spectrum/validation-r401-r450-r2000/validation-summary.tsv
```

Fresh r401-r450 headline:

```text
policy                     hit groups  mean opened rows  covered fraction
dense strict                24/50       207.50            0.103698
dense wide                  33/50       811.52            0.405557
frozen adaptive K3          46/50       368.66            0.184238
frozen adaptive K5          39/50       368.24            0.184028
dense p8000 adaptive        26/50       350.74            0.175282
K3 wide top5/R100           49/50       577.40            0.288556
shape-priority MassR100     48/50       364.38            0.182099
confidence tier             48/50       440.96            0.220370
gap reserve top1            46/50       455.40            0.227586
gap reserve top2            47/50       540.68            0.270205
```

Interpretation:

- K3 is strong on this final stability slice: `46/50`.
- Dense wide is only `33/50` despite opening more than twice as many rows.
- K5 remains worse than K3.
- MassR100 and confidence tier again outperform K3 on this slice. This is
  interesting, but it is not enough to promote either as the published
  baseline because these rules were inspected/developed after earlier blocks.
  A fair claim would require freezing one of them and validating on new
  holdouts.
- K3 wide reaches `49/50` at higher cost; again, broader reveal buys coverage
  but does not prove no-skip.

K3 miss diagnostics:

```text
rows                       50
hits                       46
misses                      4
hitMeanNearestDistance     12.7826
missMeanNearestDistance    59.5
hitPeakScoreZero           18
missPeakScoreZero           2
miss sourceP values        4943, 4999, 5153, 5233
```

Miss profiles in short:

```text
sourceP  nearestOffset  nearestDistance  nearestOrder  nearestRadius
4943       58            58              7             20
4999      -22            22              5             20
5153       58            58              5             20
5233     -100           100              4             60
```

This miss set is almost entirely near/radius/priority failure. The wall
signal lands near the target, but the frozen K3 reveal geometry does not
always open far enough or prioritize the near center early enough.

The combined fresh holdout frontier was regenerated with
`tools/wall_frontier_summary.cpp`, now including r401-r450 by default:

```text
policy                  holdouts  hitGroups  hitRate   meanCoveredRows
frozen5LayerAdaptiveK3  8         365/400    0.912500  368.302500
denseNMS_wide           8         309/400    0.772500  806.847500
denseP8000Adaptive      8         265/400    0.662500  346.805000
denseNMS_strict         8         256/400    0.640000  206.895000
```

Interim conclusion after r401-r450:

```text
K3 adaptive is a robust q-free triage baseline on these Silva holdouts:
365/400 coverage at about 368 opened rows per window, versus dense wide
309/400 at about 807 rows. It is not a no-skip predictor: 35/400 known peaks
are still missed, and broader reveal can buy extra coverage only by opening
more rows.
```

Candidate note:

MassR100 and confidence-tier reveal now deserve a clearly separated future
test if the work continues. They must be frozen before testing and compared
on new holdouts. They should not be presented as established from these same
inspection blocks.

## 2026-06-22 fresh r451-r500 candidate validation

After r401-r450, MassR100 and confidence-tier reveal looked tempting. To avoid
hindsight tuning, they were treated as frozen candidates and checked on a new
fresh slice not used to select them.

```text
Silva ranks 451-500 by p
contiguous windows +/-2000
rows per group 2001
total rows 100050
labels pLimit=50000
labels smallLimit=50000
labels workers=18
feature layers: 5000, 8000, 9000, 10000, 12000
```

Input and output files:

```text
results/wall-spectrum/validation-r451-r500-r2000/silva-r451-r500-source.tsv
results/wall-spectrum/validation-r451-r500-r2000/silva-r451-r500-r2000.tsv
results/wall-spectrum/validation-r451-r500-r2000/silva-r451-r500-r2000-labels.tsv
results/wall-spectrum/validation-r451-r500-r2000/features-p5000-w5000.tsv
results/wall-spectrum/validation-r451-r500-r2000/features-p8000-w8000.tsv
results/wall-spectrum/validation-r451-r500-r2000/features-p9000-w9000.tsv
results/wall-spectrum/validation-r451-r500-r2000/features-p10000-w10000.tsv
results/wall-spectrum/validation-r451-r500-r2000/features-p12000-w12000.tsv
results/wall-spectrum/validation-r451-r500-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r451-r500-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r451-r500-r2000/frozen-k3-adaptive-group-profile.tsv
results/wall-spectrum/validation-r451-r500-r2000/fresh-k3-miss-diagnostics-summary.tsv
results/wall-spectrum/validation-r451-r500-r2000/validation-summary.tsv
```

Fresh r451-r500 headline:

```text
policy                     hit groups  mean opened rows  covered fraction
dense strict                31/50       206.82            0.103358
dense wide                  37/50       816.88            0.408236
frozen adaptive K3          48/50       368.32            0.184068
frozen adaptive K5          42/50       367.34            0.183578
dense p8000 adaptive        33/50       351.80            0.175812
K3 wide top5/R100           49/50       577.56            0.288636
MassR100                    47/50       365.20            0.182509
confidence tier             48/50       442.04            0.220910
gap reserve top1            48/50       452.12            0.225947
gap reserve top2            48/50       539.88            0.269805
```

Candidate interpretation:

- K3 remains the best simple frozen baseline on this fresh block: `48/50`
  at `368.32` opened rows.
- MassR100 is a little cheaper but misses one more known peak: `47/50`.
- Confidence tier ties K3 on coverage but costs more: `442.04` rows.
- Gap reserve variants tie K3 but cost still more.
- K3 wide reaches `49/50`, again confirming that wider reveal buys coverage
  but does not create a no-skip rule.

So this candidate validation is neutral/negative. MassR100 and confidence
tier should not be promoted over K3.

K3 miss diagnostics:

```text
rows                       50
hits                       48
misses                      2
hitMeanNearestDistance     13
missMeanNearestDistance    124
hitPeakScoreZero           13
missPeakScoreZero           2
miss sourceP values        4507, 4783
```

Miss profiles in short:

```text
sourceP  nearestOffset  nearestDistance  nearestOrder  nearestRadius
4507      148           148              8             20
4783     -100           100              5             20
```

Both misses are near-selected-center failures. The true offset appears among
the selected offsets, but the frozen K3 opened radius at that center was only
`20`, while the true offset was `100` or `148` away. This reinforces the
current model: q-free wall persistence is useful for local triage, but the
reveal geometry is not safe enough to skip the rest of the field.

The combined fresh holdout frontier was regenerated with
`tools/wall_frontier_summary.cpp`, now including r451-r500 by default:

```text
policy                  holdouts  hitGroups  hitRate   meanCoveredRows
frozen5LayerAdaptiveK3  9         413/450    0.917778  368.304444
denseNMS_wide           9         346/450    0.768889  807.962222
denseP8000Adaptive      9         298/450    0.662222  347.360000
denseNMS_strict         9         287/450    0.637778  206.886667
```

Current conclusion after r451-r500:

```text
K3 adaptive remains the defensible q-free triage baseline: 413/450 known
peaks covered at about 368 opened rows per window. Dense wide covers only
346/450 while opening about 808 rows per window. The result is enrichment,
not a formula and not a no-skip predictor: 37/450 known peaks are still
missed.
```
