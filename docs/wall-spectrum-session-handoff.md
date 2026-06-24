# Wall-spectrum session handoff

This file is the short restart memory for context-window changes. Read it at
the beginning of every new wall-spectrum / q-free predictor session before
running new experiments.

## Active objective

First read this file before doing anything else:

```text
docs/wall-spectrum-session-handoff.md
```

Then continue the actual research objective:

```text
Проверить, существует ли q-free / pre-witness wall-spectrum предиктор для
больших minimal Goldbach witnesses p(N): сначала зафиксировать prior-art по
minimal Goldbach partitions и известным Silva / Oliveira e Silva пикам, затем
построить benchmark из известных пиков и контрольных чисел того же масштаба,
реализовать и поддерживать воспроизводимые C/C++/GMP инструменты, считать
только признаки до полного поиска witness по ранним допустимым p и малым
делителям N-p, сравнить качество отбора с random/simple baselines по top 1%,
0.1%, 0.01%, group-rank, NMS/cost/coverage и false negatives.

Все важные промежуточные результаты, провалы, параметры, новые правила,
выводы и следующий шаг обязательно записывать в
docs/wall-spectrum-session-handoff.md в кратком виде, а подробные таблицы,
источники, параметры, интерпретации и negative results записывать в
docs/wall-spectrum-predictor-research-note.md. При смене окна новая сессия
должна сначала прочитать docs/wall-spectrum-session-handoff.md, затем при
необходимости подробный research note, и только после этого запускать новые
расчеты.

Не запускать долгие расчеты без ясной информационной пользы. Не называть
результат новым до prior-art проверки. Не считать цель выполненной без
воспроизводимого улучшения над baseline или честно зафиксированного
отрицательного результата.
```

The feature stage must not use the exact witness and must not use primality of
`q = N - p`. Exact witness computation is allowed only for labels and
evaluation.

## Where to write results

Use these files consistently:

- Short restart memory:
  `docs/wall-spectrum-session-handoff.md`
- Detailed scientific log:
  `docs/wall-spectrum-predictor-research-note.md`
- Current research conclusion:
  `docs/wall-spectrum-research-conclusion.md`
- Generated benchmark TSVs:
  `results/wall-spectrum/`
- C/C++ tools that should be reproducible/public:
  `tools/wall_*.cpp`, `tools/minimal_witness_batch_gmp.cpp`

At the end of every meaningful experiment, update this file with:

- what was tested;
- exact input/output files;
- the headline result;
- whether it improved over baseline;
- the next best step.

The detailed numbers and interpretation belong in
`docs/wall-spectrum-predictor-research-note.md`.

## Current verified state

Implemented C++/GMP tools:

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
- `tools/wall_frontier_summary.cpp`

Latest build/smoke status should be checked with:

```sh
make build wall-smoke qline-scout-smoke qline-smoke
```

Latest verified status:

```text
2026-06-22: make build wall-smoke passed after adding the NMS policy,
blocker-composition toolchain, and q-line scout to the reproducible build.
2026-06-22: q-line scout runtime smoke passed:
results/smoke/qline-scout.tsv
2026-06-22: make build wall-smoke passed again; make qline-scout-smoke
passed; make qline-smoke passed.
2026-06-22: wall_multipoint_landing_metrics reproduced
results/wall-spectrum/windows/top10-r2000-multipoint-landing-p8000.tsv
byte-for-byte from the saved labels/features.
2026-06-22: make build wall-smoke qline-scout-smoke qline-smoke passed
after adding wall_persistence_policy_metrics.
2026-06-22: frozen 5-layer persistence was validated without tuning on
Silva ranks 11-30 and 31-50, both `+/-2000` windows.
2026-06-22: adaptive reveal around frozen 5-layer votes was tested on top10,
ranks 11-30, ranks 31-50, and fresh ranks 51-100.
2026-06-22: make build wall-smoke qline-scout-smoke qline-smoke passed
after adding adaptive reveal metrics to wall_persistence_policy_metrics.
2026-06-22: make build wall-smoke qline-scout-smoke qline-smoke passed
after adding q-free group profile diagnostics to wall_persistence_policy_metrics.
2026-06-22: wall_persistence_policy_metrics was extended with a frozen
`adaptiveFrozen5LayerK5` diagnostic policy.
2026-06-22: make build wall-smoke qline-scout-smoke qline-smoke passed
after recording the fresh r101-r150 K3/K5 validation.
2026-06-22: make build wall-smoke qline-scout-smoke qline-smoke passed
after adding shape-priority expansion metrics and fresh r151-r200 validation.
2026-06-22: make build wall-smoke qline-scout-smoke qline-smoke passed
after adding gap reserve re-landing metrics and fresh r201-r250 validation.
2026-06-22: make build wall-smoke qline-scout-smoke qline-smoke passed
after adding confidence-tier reveal metrics, fresh r251-r300 validation, and
holdout frontier summary.
2026-06-22: make build wall-smoke qline-scout-smoke qline-smoke passed
after adding the reproducible wall_frontier_summary tool and regenerating
holdout-frontier-summary.tsv from frozen-adaptive-validation-summary.tsv.
2026-06-22: make build wall-smoke qline-scout-smoke qline-smoke passed
after adding `--skip` to wall_benchmark_windows_gmp, running fresh r301-r350,
and regenerating the six-holdout frontier through r350.
2026-06-22: make build wall-smoke qline-scout-smoke qline-smoke passed
after running fresh r351-r400 and regenerating the seven-holdout frontier
through r400.
2026-06-22: make build wall-smoke qline-scout-smoke qline-smoke passed
after running final fresh r401-r450 and regenerating the eight-holdout
frontier through r450.
2026-06-22: make build wall-smoke qline-scout-smoke qline-smoke passed
after running fresh r451-r500 candidate validation and regenerating the
nine-holdout frontier through r500.
```

## Current strongest positive result

On Silva top-50 close windows (`+/-200`), simple q-free wall pressure is a
strong local refinement signal.

Depth summary:

```text
pLimit  wallLimit  top1pct  top0_1pct  top0_01pct  rank1   rank5   rank10  meanRank
8000    8000       41/101   11/11      2/2         35/50   45/50   49/50   2.22
9000    9000       41/101   10/11      2/2         35/50   47/50   48/50   2.06
10000   10000      37/101   10/11      2/2         38/50   46/50   49/50   2.50
12000   12000      36/101    9/11      2/2         36/50   45/50   48/50   3.72
```

Interpretation: depth must be calibrated. Larger `pLimit` is not automatically
better.

## Current negative/limiting results

- `blockedRatio` is useful for close refinement but does not solve wide-field
  landing.
- Fixed-grid anchor scout is phase-sensitive and unreliable.
- Dense q-free scout/NMS is better than fixed grid, but still misses some
  peaks on wide windows.
- Simple phase-swept multi-point landing does not beat dense NMS at comparable
  revealed-row cost.
- Multi-layer wall persistence is the first tested wide-window improvement
  over dense NMS at comparable cost. It now has mixed but positive holdout
  evidence: ranks 11-30 match dense NMS at strict `hitR=20`; ranks 31-50 beat
  dense NMS at the same strict cost. This is still triage, not no-skip.
- Moving-background/local contrast is a modest refinement. It does not remove
  false pressure maxima in wide windows.
- Z-score contrast variants were generally worse than simple deltas.
- Blocker-composition features and pressure-times-composition variants were
  tested and did not beat raw `blockedRatio`.

## Latest NMS policy result

The reproducible NMS/landing policy evaluator has been added and run:

```text
tools/wall_nms_policy_metrics.cpp
```

Result files:

```text
results/wall-spectrum/windows-top50/silva-top50-r200-nms-policy-p9000.tsv
results/wall-spectrum/windows/top10-r2000-nms-policy-p8000.tsv
```

Close top-50 `+/-200` windows:

```text
blockedRatio, suppress=20, topK=5, hitR=20:
  hitGroups=49/50, meanCoveredRows=96.44, coveredFraction=0.4798

localR20_delta, suppress=20, topK=5, hitR=20:
  hitGroups=49/50, meanCoveredRows=96.16, coveredFraction=0.4784

blockedRatio, suppress=20, topK=10, hitR=20:
  hitGroups=50/50, meanCoveredRows=171.68, coveredFraction=0.8541
```

Wide top-10 `+/-2000` windows:

```text
blockedRatio, suppress=100, topK=5, hitR=20:
  hitGroups=7/10, meanCoveredRows=105.0, coveredFraction=0.0525

localR100_delta, suppress=100, topK=5, hitR=20:
  hitGroups=7/10, meanCoveredRows=105.0, coveredFraction=0.0525

localR50_delta, suppress=20, topK=10, hitR=20:
  hitGroups=8/10, meanCoveredRows=205.8, coveredFraction=0.1028

localR50_delta, suppress=20, topK=10, hitR=100:
  hitGroups=9/10, meanCoveredRows=813.5, coveredFraction=0.4065
```

Interpretation:

NMS makes the cost/coverage tradeoff explicit. Local contrast does not
materially beat raw `blockedRatio` in wide windows. Higher coverage is possible
only by opening much more of the window, so this is still triage, not a no-skip
landing formula.

## Latest blocker-composition result

The obstruction-composition hypothesis was tested with:

```text
tools/wall_blocker_profile_gmp.cpp
```

Result files:

```text
results/wall-spectrum/windows/top10-r2000-blocker-profile-p8000.tsv
results/wall-spectrum/windows/top10-r2000-blocker-profile-metrics-p8000.tsv
results/wall-spectrum/windows-top50/silva-top50-r200-blocker-profile-p9000.tsv
results/wall-spectrum/windows-top50/silva-top50-r200-blocker-profile-metrics-p9000.tsv
```

Wide top-10 `+/-2000`:

```text
feature           mean peak rank  rank <=10
blockedRatio      20.7            7/10
pressureLow1000   69.5            6/10
pressureEntropy  151.9            0/10
pressureLow101   164.9            1/10
```

Close top-50 `+/-200`:

```text
feature           mean peak rank  rank <=10
blockedRatio       2.06           48/50
pressureLow1000    6.22           44/50
pressureEntropy   13.16           25/50
pressureLow101    13.52           31/50
```

Interpretation:

This is a negative result for the simple blocker-composition idea. True peaks
are not separated from false pressure maxima by first-blocker diversity,
entropy, dominance, or low-prime share in the tested windows. Raw
`blockedRatio` remains the best q-free feature.

## Latest q-line transfer result

Older q-line transfer results were consolidated into the current research note.
The q-line scout is now part of the reproducible build:

```text
tools/qfree_qline_scout_gmp.cpp
bin/qfree-qline-scout-gmp
```

Important distinction:

```text
q-free wall pressure      = pre-witness predictor/scout
q-line scout              = post-landing witness/ridge generator
exact lower-p closure     = minimality check
```

Q-line scout uses GMP to test `q=C-a`, so it is not a q-free feature for the
outer predictor. It is useful after landing.

Existing result files:

```text
results/qfree-landing-policy/qline-transfer/qline-transfer-comparison.md
results/qfree-landing-policy/qline-transfer/silva/qline-transfer-summary.tsv
results/qfree-landing-policy/qline-transfer/afterwall/qline-transfer-summary.tsv
results/qfree-landing-policy/e4999-probe/e4999-row154-summary.md
```

Silva/e18 Top50:

```text
known center peaks recovered exactly   50/50
local window maxima recovered exactly  50/50
avg qTests to center q-line            221.14
exact-minimal rows reproduced          76.06 / 101 rows average
```

Afterwall controls:

```text
known center peaks recovered exactly   4/4
local window maxima recovered exactly  4/4
avg qTests to center q-line            595.25
exact-minimal rows reproduced          146.25 / 201 rows average
```

e5000 ridge:

```text
q-line recovered the high verified ridge and max p=45953
q-line GMP tests = 686..824
all verified rows with exact p >= 43000 recovered
```

Interpretation:

Q-lines are not useless. They are currently the strongest post-landing peak
generator. They do not solve outer landing and do not prove minimality without
exact lower-p closure.

## Latest multi-point landing result

The simple multi-point landing benchmark was added:

```text
tools/wall_multipoint_landing_metrics.cpp
```

Result files:

```text
results/wall-spectrum/windows/top10-r2000-multipoint-landing-p8000.tsv
results/wall-spectrum/windows/top10-r2000-multipoint-vs-nms-summary.tsv
```

Comparison on top-10 wide `+/-2000` windows:

```text
policy      params                                      hit groups  min/max  covered rows  covered fraction
denseNMS    suppress=100 topK=5 hitR=20                 7/10        7/7      105.0         0.0525
denseNMS    suppress=20 topK=10 hitR=20                 8/10        8/8      206.5         0.1032
multipoint  step=40 topK=10 revealR=20 phase-swept      1.70/10     0/10     209.1         0.1045
multipoint  step=400 topK=1 revealR=200 phase-swept     1.21/10     0/9      196.9         0.0984
multipoint  step=400 topK=10 revealR=200 phase-swept   10.00/10    10/10    1950.6        0.9748
```

Interpretation:

At comparable cost, simple multi-point landing is much worse than dense NMS
and is phase-sensitive. It hits all phases only by revealing almost the entire
wide window. This is a negative result for the simple десант idea.

## Latest multi-layer persistence result

The multi-layer wall-persistence benchmark was added:

```text
tools/wall_persistence_policy_metrics.cpp
```

It tests whether local wall maxima that persist across several calibrated
`pLimit/wallLimit` layers distinguish true walls from false pressure maxima.
This is still q-free: it uses only pre-witness `blockedRatio` layers for
ranking. Exact witnesses are used only as labels/coverage evaluation.

Result files:

```text
results/wall-spectrum/windows/top10-r2000-persistence-policy-5layers.tsv
results/wall-spectrum/windows/top10-r2000-persistence-policy-7layers.tsv
results/wall-spectrum/windows-top50/silva-top50-r200-persistence-policy-4layers.tsv
results/wall-spectrum/windows/persistence-vs-baseline-summary.tsv
results/wall-spectrum/validation-r11-r30-r2000/validation-summary.tsv
results/wall-spectrum/validation-r31-r50-r2000/validation-summary.tsv
results/wall-spectrum/validation-frozen5layer-summary.tsv
results/wall-spectrum/adaptive-reveal-summary.tsv
results/wall-spectrum/validation-r51-r100-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r51-r100-r2000/frozen-adaptive-group-profile.tsv
results/wall-spectrum/validation-r51-r100-r2000/fresh-miss-diagnostics-summary.tsv
results/wall-spectrum/validation-r101-r150-r2000/validation-summary.tsv
results/wall-spectrum/validation-r101-r150-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r101-r150-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r101-r150-r2000/frozen-k3-adaptive-group-profile.tsv
results/wall-spectrum/validation-r101-r150-r2000/fresh-k3-miss-diagnostics-summary.tsv
results/wall-spectrum/validation-r151-r200-r2000/validation-summary.tsv
results/wall-spectrum/validation-r151-r200-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r151-r200-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r151-r200-r2000/frozen-k3-adaptive-group-profile.tsv
results/wall-spectrum/validation-r151-r200-r2000/fresh-k3-miss-diagnostics-summary.tsv
results/wall-spectrum/validation-r201-r250-r2000/validation-summary.tsv
results/wall-spectrum/validation-r201-r250-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r201-r250-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r201-r250-r2000/frozen-k3-adaptive-group-profile.tsv
results/wall-spectrum/validation-r201-r250-r2000/fresh-k3-miss-diagnostics-summary.tsv
results/wall-spectrum/validation-r251-r300-r2000/validation-summary.tsv
results/wall-spectrum/validation-r251-r300-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r251-r300-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r251-r300-r2000/frozen-k3-adaptive-group-profile.tsv
results/wall-spectrum/validation-r251-r300-r2000/fresh-k3-miss-diagnostics-summary.tsv
results/wall-spectrum/frozen-adaptive-validation-summary.tsv
results/wall-spectrum/holdout-frontier-summary.tsv
```

Summary:

```text
scope        policy                   hit groups  covered rows  note
top10-r2000  denseNMS baseline         8/10        206.5         p8000, comparable cost
top10-r2000  5-layer persistence      10/10        209.2         first wide-window improvement
top10-r2000  7-layer persistence       9/10        210.0         deeper layers weaken
top50-r200   denseNMS baseline        50/50        171.68        close full coverage
top50-r200   4-layer persistence      50/50        139.5         same coverage, less opened window
```

Holdout validation with the top10 policy frozen:

```text
scope          policy                  hitR  hit groups  covered rows  note
r11-r30-r2000  denseNMS comparable      20    19/20       205.0         baseline
r11-r30-r2000  frozen 5-layer           20    19/20       210.0         same strict coverage
r11-r30-r2000  denseNMS comparable     100    19/20       815.45        wider reveal
r11-r30-r2000  frozen 5-layer          100    20/20       849.75        wider reveal, higher cost
r31-r50-r2000  denseNMS comparable      20    14/20       205.2         baseline
r31-r50-r2000  frozen 5-layer           20    18/20       209.75        strict improvement
r31-r50-r2000  denseNMS comparable     100    19/20       769.85        wider reveal
r31-r50-r2000  frozen 5-layer          100    20/20       826.45        wider reveal, higher cost
```

Interpretation:

This is the first positive result for a distinct q-free wall-spectrum rule
beyond raw dense NMS. The result suggests calibrated persistence across layers
can suppress some false maxima. Holdout validation did not preserve a universal
win at strict `hitR=20`, but it did preserve high coverage and produced a
clear improvement on ranks 31-50. Wider reveal (`hitR=100`) catches all
holdout peaks, but at much higher opened-row cost. This is still not a no-skip
locator and not a formula. The low-cost top10 `~105 rows` case still does not
beat the old dense NMS baseline (`6/10` versus `7/10`), and adding too-deep
layers (`15000`, `20000`) weakens the result.

## Latest adaptive reveal result

Adaptive reveal rule tested:

```text
Start from frozen 5-layer vote centers:
  layerSuppress=20, layerTopK=3, voteR=20,
  finalSuppress=100, finalTopK=10.

Open base radius 20 around all 10 selected centers.
Expand only the first 4 selected centers to radius 60.
```

Summary:

```text
scope          dense strict  dense wide   frozen adaptive  dense adaptive
top10-r2000      8/10         8/10          10/10             8/10
r11-r30         19/20        19/20          20/20            19/20
r31-r50         14/20        19/20          20/20            15/20
r51-r100        42/50        46/50          47/50            43/50
```

Mean opened rows for frozen adaptive were about `367` per `+/-2000` window,
versus about `205` for dense strict and about `770..815` for dense wide.

Interpretation:

Adaptive reveal is a real improvement over uniform `hitR=100`: it closes the
near-misses from ranks 11-50 at about half the opened-row cost. It also beats
the same adaptive reveal applied to dense p8000 centers. But fresh ranks
51-100 still have 3 misses:

```text
p=7949 nearest selected offset +112
p=8161 selected offset +34 exists but is 10th center, not expanded
p=8291 nearest selected offset -186
```

So adaptive reveal improves cost/coverage but is still not a no-skip rule.
The remaining misses are no longer just a `20` versus `60` radius issue.

## Latest miss diagnostics

The r51-r100 fresh misses were profiled with q-free group diagnostics from
`wall_persistence_policy_metrics --groupProfileOutput`.

Diagnostic files:

```text
results/wall-spectrum/validation-r51-r100-r2000/frozen-adaptive-group-profile.tsv
results/wall-spectrum/validation-r51-r100-r2000/fresh-miss-diagnostics-summary.tsv
```

Headline:

```text
rows                      50
hits                      47
misses                     3
hit mean nearest distance  10.13
miss mean nearest distance 110.67
hit peakScore=0           13
miss peakScore=0           3
```

Interpretation:

- All three misses have `peakScore=0`, but this is not a discriminator:
  13 successful rows also have `peakScore=0`.
- Successful `peakScore=0` rows are usually caught because a selected center
  lands at offset `+/-10`.
- `p=7949` is not a weak raw wall: its center layer ranks are `5,5,17,9,4`,
  but the per-layer top-3 NMS vote does not give the center any vote; the
  strongest selected wall is displaced to `+112`.
- `p=8161` has a selected center at `+34`, but it is the 10th selected center
  and therefore not expanded by the current top-4 adaptive reveal.
- `p=8291` is a harder miss: center ranks are weak and the nearest selected
  center is at `-186`.

Conclusion: current q-free diagnostics explain the misses as displacement and
priority failures, not as simple absence of a wall. No clean no-skip
discriminator is established yet.

## Latest fresh r101-r150 validation

The diagnostic idea from ranks 51-100 was tested on a fresh holdout:
Silva ranks 101-150, contiguous `+/-2000` windows. The mechanistic variant was
to relax per-layer capture from `layerTopK=3` to `layerTopK=5`, while keeping
the same adaptive reveal cost rule:

```text
layers=5
layerSuppress=20
voteR=20
finalSuppress=100
finalTopK=10
baseR=20
expandTop=4
expandR=60
```

Headline result:

```text
policy                    hit groups  mean opened rows  covered fraction
dense strict               40/50       206.5             0.103198
dense wide                 43/50       806.26            0.402929
frozen adaptive K3         47/50       368.26            0.184038
frozen adaptive K5         40/50       368.54            0.184178
dense adaptive             41/50       342.96            0.171394
```

Interpretation:

The `layerTopK=5` hypothesis is a negative result. It did not repair the
r51-r100 miss type; on fresh r101-r150 it collapsed to `40/50`, essentially
the dense strict baseline, at much higher opened-row cost. The current best
tested policy remains frozen 5-layer adaptive K3.

K3 r101-r150 miss diagnostics:

```text
rows                       50
hits                       47
misses                      3
hitMeanNearestDistance     11.53
missMeanNearestDistance   279.33
hitPeakScoreZero           11
missPeakScoreZero           3
```

Miss profiles:

```text
p=7559 nearest selected offset -48, order 5, radius 20
p=7727 nearest selected offset -580, order 9, radius 20
p=7873 nearest selected offset +210, order 1, radius 60
```

The miss pattern repeats the earlier warning: all misses have `peakScore=0`,
but many hits also have `peakScore=0`. The failures are displacement/priority
failures, not a clean separable "no wall" class.

Cheap follow-up from already computed tables:

```text
scope        K3 expandTop=4 R60  K3 expandTop=5 R60  K3 expandTop=4 R100
r51-r100     47/50, 367.6 rows   47/50, 406.4 rows   47/50, 503.18 rows
r101-r150    47/50, 368.26 rows  48/50, 407.98 rows  48/50, 501.08 rows
```

Interpretation: simply opening more K3 centers is not a stable fix. It helps
one fresh holdout by one peak, but gives no gain on r51-r100. This can remain
a fallback cost/coverage knob, not a discovered formula.

## Latest fresh r151-r200 validation

A new q-free displacement/priority variant was added to
`tools/wall_persistence_policy_metrics.cpp`:

```text
Keep K3 selected centers unchanged.
Change only which selected centers receive expanded radius.
Expansion priority uses local wall-vote mass/shoulder around each selected
center, computed from the frozen K3 vote map, not from exact witness/q.
```

Fresh holdout: Silva ranks 151-200, contiguous `+/-2000` windows.

Result files:

```text
results/wall-spectrum/validation-r151-r200-r2000/validation-summary.tsv
results/wall-spectrum/validation-r151-r200-r2000/adaptive-reveal-5layers.tsv
results/wall-spectrum/validation-r151-r200-r2000/nms-policy-p8000.tsv
results/wall-spectrum/validation-r151-r200-r2000/fresh-k3-miss-diagnostics-summary.tsv
```

Headline:

```text
policy                     hit groups  mean opened rows  covered fraction
dense strict                39/50       207.06            0.103478
dense wide                  44/50       805.98            0.402789
frozen adaptive K3          49/50       368.84            0.184328
frozen adaptive K5          44/50       368.50            0.184158
dense adaptive              40/50       347.38            0.173603
K3 MassR100 priority        49/50       365.56            0.182689
K3 ShoulderR100 priority    49/50       363.52            0.181669
K3 MassR200 priority        48/50       362.84            0.181329
K3 ShoulderR200 priority    47/50       364.12            0.181969
```

Interpretation:

- K3 adaptive received another fresh positive validation: `49/50`, versus
  dense strict `39/50` and dense wide `44/50`.
- K5 again stayed worse (`44/50`), so do not revive it.
- Shape-priority R100 does not find a new peak, but slightly reduces cost at
  the same `49/50` coverage. This is a small cost optimization, not a formula.
- Shape-priority R200 worsens hit count, so broad shoulder/mass priority is
  not stable.

K3 miss diagnostic:

```text
misses                   1
missed sourceP           7321
nearest selected offset  +700
nearest order            3
nearest radius           60
peakScore                0
peakLayerRanks           437,65,154,163,53
```

This remaining miss is not a small near miss. The selected wall is displaced
far from the true peak, so local expansion priority cannot repair it without
opening a much larger part of the window.

## Latest fresh r201-r250 validation

A second-stage q-free reserve rule was added to
`tools/wall_persistence_policy_metrics.cpp`:

```text
Primary: frozen K3 adaptive centers, baseR=20, expandTop=4, expandR=60.
Fallback: select dense p8000/w8000 blockedRatio anchors only among rows not
already covered by the primary K3 reveal.
Predeclared main fallback: fallbackTopK=1, fallbackSuppress=100, fallbackR=100.
```

Diagnostic on the already-used r151-r200 holdout closed the one far miss
(`50/50`), but the fresh r201-r250 holdout did not generalize.

Fresh r201-r250 headline:

```text
policy                     hit groups  mean opened rows  covered fraction
dense strict                28/50       206.56            0.103228
dense wide                  33/50       801.10            0.400350
frozen adaptive K3          44/50       367.84            0.183828
frozen adaptive K5          41/50       367.84            0.183828
dense adaptive              30/50       345.74            0.172784
shape-priority MassR100     43/50       362.66            0.181239
gap dense reserve top1      44/50       456.42            0.228096
gap dense reserve top2      46/50       542.44            0.271084
K3 wide top5/R100           48/50       573.04            0.286377
```

Interpretation:

- K3 still beats dense baselines strongly on this harder lower-rank holdout.
- The predeclared reserveTop1 rule does not improve over K3 and costs more.
- reserveTop2 improves to `46/50`, but the broad K3 top5/R100 comparison gives
  `48/50` at higher cost, so reserve is not the clean repair.
- Shape-priority worsens hit count here.
- K5 remains negative.

K3 miss diagnostics on r201-r250:

```text
misses                     6
hitMeanNearestDistance     13.36
missMeanNearestDistance   138.67
hitPeakScoreZero           17
missPeakScoreZero           6
miss sourceP values        6619, 6659, 6661, 6679, 6779, 6823
```

The miss type shifted again: this holdout is not dominated by one far
displacement. Several misses are near-but-not-expanded (`26`, `52`, `56`,
`100`) plus one farther miss (`498`). This explains why a single dense reserve
anchor is not enough.

## Latest fresh r251-r300 validation

A q-free confidence-tier reveal rule was added to
`tools/wall_persistence_policy_metrics.cpp`:

```text
Keep the frozen K3 selected centers.
Give radius 100 to selected center #1.
Give radius 60 to selected centers #2..#5.
Give radius 20 to the remaining selected centers.
```

Reason: r201-r250 contained selected-but-not-wide-enough misses at distance
`100`, plus lower-priority near misses. On r201-r250 diagnostics this tier
would improve `46/50` top5/R60 to `47/50`, so it was frozen and tested on
fresh r251-r300.

Fresh r251-r300 headline:

```text
policy                     hit groups  mean opened rows  covered fraction
dense strict                34/50       207.74            0.103818
dense wide                  35/50       823.12            0.411354
frozen adaptive K3          46/50       367.94            0.183878
frozen adaptive K5          35/50       368.44            0.184128
dense adaptive              34/50       350.96            0.175392
shape-priority MassR100     46/50       364.12            0.181969
confidence tier             47/50       442.12            0.220950
gap reserve top1            47/50       453.24            0.226507
K3 wide top5/R100           48/50       578.42            0.289065
```

Interpretation:

- K3 still beats dense baselines strongly.
- Confidence tier gives a real but small improvement over standard K3:
  `47/50` versus `46/50`.
- It is cheaper than the gap reserve at the same `47/50`.
- It is still worse than broad K3 top5/R100 (`48/50`) and does not solve
  no-skip localization.

K3 miss diagnostics on r251-r300:

```text
misses                     4
hitMeanNearestDistance     10.39
missMeanNearestDistance    86.00
miss sourceP values        6221, 6247, 6379, 6421
```

Miss profiles in short:

```text
sourceP  nearestOffset  nearestOrder  nearestRadius  note
6221     -120           6             20             lower-priority near/far edge
6247     +24            7             20             lower-priority near miss
6379     -100           2             60             selected but not wide enough
6421     -100           1             60             selected but not wide enough
```

The confidence tier specifically targets the `center #1 at distance 100`
case, so it explains one recovered peak but leaves lower-priority near misses.

## Current holdout frontier summary

Summary file:

```text
results/wall-spectrum/holdout-frontier-summary.tsv
```

Comparable policies across the five fresh `+/-2000` holdouts
`r51-r100`, `r101-r150`, `r151-r200`, `r201-r250`, `r251-r300`:

```text
policy               hit groups  hit rate  mean opened rows
K3 adaptive           233/250     93.2%     368.10
dense wide            201/250     80.4%     807.06
dense adaptive        188/250     75.2%     347.24
dense strict          183/250     73.2%     206.92
```

Partly comparable / newer variants:

```text
K3 wide top5/R100      96/100     96.0%     575.73  (only r201-r300)
confidence tier        47/50      94.0%     442.12  (only r251-r300)
gap reserve top1       91/100     91.0%     454.83  (r201-r300)
K5                    160/200     80.0%     368.33  (r101-r300)
```

Interpretation:

K3 adaptive is now a real reproducible triage frontier improvement over dense
baselines on these Silva holdouts. It is not a no-skip locator: broad K3 can
buy more coverage with more opened rows, and no tested q-free rule removes all
misses.

## Current next step

Do not tune on ranks 51-100, 101-150, 151-200, 201-250, or 251-300. The fresh
K5 test failed repeatedly; shape-priority is not stable; gap-reserve did not
generalize cleanly; confidence-tier gave only a small targeted gain. Current
best status:

```text
multi-layer persistence plus adaptive reveal K3 is a strong q-free
enrichment/local coverage policy, but not a validated no-skip locator.
```

The next useful step is to decide whether to stop policy tweaking and turn the
current evidence into a clean research note/table: K3 adaptive is a strong
q-free triage policy, while K5, shape-priority, gap reserve, and tiered reveal
do not establish no-skip prediction. If continuing experimentally, the next
test should be pre-registered on a fresh holdout and should improve the
frontier, not merely add another wider reveal knob.

## 2026-06-22 reproducible frontier update

Added a C++ frontier aggregation tool:

```text
tools/wall_frontier_summary.cpp
bin/wall-frontier-summary
```

It regenerates:

```text
results/wall-spectrum/holdout-frontier-summary.tsv
```

from:

```text
results/wall-spectrum/frozen-adaptive-validation-summary.tsv
```

The generated output was checked against the previously saved table with
`diff -u` and matched byte-for-byte. The main result remains:

```text
K3 adaptive: 233/250 hits at 368.096 opened rows on average
dense strict: 183/250 hits at 206.920 opened rows on average
dense wide: 201/250 hits at 807.060 opened rows on average
dense adaptive: 188/250 hits at 347.240 opened rows on average
```

Added compact restart/interpretation document:

```text
docs/wall-spectrum-frontier-summary.md
```

Use it when the next session needs the current frontier conclusion quickly:
K3 adaptive is a reproducible q-free triage improvement, but not a no-skip
predictor and not a proof/formula. The next experiment should be
pre-registered against a fresh holdout and must improve the frontier table,
not merely add another radius knob.

## 2026-06-22 fresh r301-r350 validation

Pre-registered fresh holdout run:

```text
Silva ranks 301-350 by p
windows +/-2000
rows per group 2001
labels pLimit=50000 smallLimit=50000 workers=18
feature layers 5000, 8000, 9000, 10000, 12000
```

`tools/wall_benchmark_windows_gmp.cpp` now supports `--skip`, so fresh rank
slices can be generated reproducibly:

```text
--skip=300 --top=50
```

Result directory:

```text
results/wall-spectrum/validation-r301-r350-r2000/
```

Important files:

```text
validation-summary.tsv
fresh-k3-miss-diagnostics-summary.tsv
frozen-k3-adaptive-group-profile.tsv
```

Headline:

```text
dense strict            24/50   mean rows 206.26
dense wide              40/50   mean rows 812.04
dense adaptive          26/50   mean rows 343.04
frozen adaptive K3      42/50   mean rows 368.62
frozen adaptive K5      37/50   mean rows 368.42
MassR100                42/50   mean rows 364.54
confidence tier         42/50   mean rows 443.62
gap reserve top1        42/50   mean rows 453.60
gap reserve top2        43/50   mean rows 539.56
K3 wide top5/R100       45/50   mean rows 579.52
```

This is the weakest fresh K3 validation so far. K3 still beats all directly
comparable dense baselines, but only narrowly beats dense wide on this slice.
This lowers confidence in any stronger claim and reinforces the current
conclusion:

```text
K3 wall persistence is a q-free triage/enrichment signal, not a no-skip
predictor.
```

Updated combined frontier through r350:

```text
K3 adaptive       275/300  91.6667%  mean rows 368.18
dense wide        241/300  80.3333%  mean rows 807.89
dense adaptive    214/300  71.3333%  mean rows 346.54
dense strict      207/300  69.0000%  mean rows 206.81
```

K3 miss diagnostics for r301-r350:

```text
misses                    8
missMeanNearestDistance   122.5
miss sourceP values       5717, 5813, 5857, 5867, 5927, 5981, 5987, 6079
```

Next best step:

Do not add another reveal knob based on r301-r350. If continuing, either:

1. run one more pre-registered fresh slice, r351-r400, using the same frozen
   policy set to measure stability; or
2. stop experiments and write the research conclusion as a positive triage
   result plus a negative no-skip result.

## 2026-06-22 fresh r351-r400 validation

Pre-registered fresh holdout run, same frozen policy set as r301-r350:

```text
Silva ranks 351-400 by p
windows +/-2000
rows per group 2001
labels pLimit=50000 smallLimit=50000 workers=18
feature layers 5000, 8000, 9000, 10000, 12000
```

The five feature layers were run as parallel independent
`wall-spectrum-scorer-gmp` processes. This changes runtime only, not the
method.

Result directory:

```text
results/wall-spectrum/validation-r351-r400-r2000/
```

Important files:

```text
validation-summary.tsv
fresh-k3-miss-diagnostics-summary.tsv
frozen-k3-adaptive-group-profile.tsv
```

Headline:

```text
dense strict            25/50   mean rows 206.80
dense wide              35/50   mean rows 795.92
dense adaptive          25/50   mean rows 344.46
frozen adaptive K3      44/50   mean rows 368.66
frozen adaptive K5      34/50   mean rows 368.88
K3 wide top5/R100       47/50   mean rows 574.80
MassR100                46/50   mean rows 365.48
confidence tier         46/50   mean rows 441.36
gap reserve top1        44/50   mean rows 449.74
gap reserve top2        44/50   mean rows 533.18
```

Interpretation:

K3 partially recovers after weak r301-r350 and clearly beats dense baselines
on this slice. K5 remains negative. MassR100 and confidence tier look good on
this one slice, but they are not stable/frozen replacements because earlier
holdouts did not show clear dominance. K3 wide buys extra coverage at higher
cost.

Updated combined frontier through r400:

```text
K3 adaptive       319/350  91.1429%  mean rows 368.25
dense wide        276/350  78.8571%  mean rows 806.18
dense adaptive    239/350  68.2857%  mean rows 346.24
dense strict      232/350  66.2857%  mean rows 206.81
```

K3 miss diagnostics for r351-r400:

```text
misses                    6
missMeanNearestDistance   73.6667
miss sourceP values       5303, 5333, 5393, 5557, 5581, 5647
```

Interim conclusion at that point:

```text
K3 remains the best directly comparable q-free triage baseline. The last two
fresh slices show variance, but not collapse. The result is still
enrichment/triage, not a no-skip predictor.
```

Next best step:

Do not tune on r301-r350 or r351-r400. Either freeze the experimental result
and write the research conclusion, or run one final fresh slice r401-r450 with
the same frozen policy set only to estimate stability. If trying to turn
MassR100/tier into a new candidate, it must be pre-registered and validated on
new holdouts, not selected from these blocks.

## 2026-06-22 fresh r401-r450 validation

Final pre-registered stability holdout run, same frozen policy set:

```text
Silva ranks 401-450 by p
windows +/-2000
rows per group 2001
labels pLimit=50000 smallLimit=50000 workers=18
feature layers 5000, 8000, 9000, 10000, 12000
```

Result directory:

```text
results/wall-spectrum/validation-r401-r450-r2000/
```

Important files:

```text
validation-summary.tsv
fresh-k3-miss-diagnostics-summary.tsv
frozen-k3-adaptive-group-profile.tsv
```

Headline:

```text
dense strict            24/50   mean rows 207.50
dense wide              33/50   mean rows 811.52
dense adaptive          26/50   mean rows 350.74
frozen adaptive K3      46/50   mean rows 368.66
frozen adaptive K5      39/50   mean rows 368.24
K3 wide top5/R100       49/50   mean rows 577.40
MassR100                48/50   mean rows 364.38
confidence tier         48/50   mean rows 440.96
gap reserve top1        46/50   mean rows 455.40
gap reserve top2        47/50   mean rows 540.68
```

Updated combined frontier through r450:

```text
K3 adaptive       365/400  91.2500%  mean rows 368.30
dense wide        309/400  77.2500%  mean rows 806.85
dense adaptive    265/400  66.2500%  mean rows 346.81
dense strict      256/400  64.0000%  mean rows 206.90
```

K3 miss diagnostics for r401-r450:

```text
misses                    4
missMeanNearestDistance   59.5
miss sourceP values       4943, 4999, 5153, 5233
```

Current conclusion:

```text
K3 adaptive is a robust q-free triage baseline on these Silva holdouts:
365/400 coverage at about 368 opened rows per window, versus dense wide
309/400 at about 807 rows. It is not a no-skip predictor: 35/400 known peaks
are still missed.
```

Important candidate note:

MassR100 and confidence-tier reveal look promising after r351-r450, but they
must not be claimed from these inspected blocks. If continued, freeze one
candidate and validate it on new holdouts. Otherwise stop experiments and
write the conclusion as: positive q-free triage/enrichment; negative no-skip
predictor.

## 2026-06-22 fresh r451-r500 candidate validation

Fresh holdout run for the pre-registered candidate check:

```text
Silva ranks 451-500 by p
windows +/-2000
rows per group 2001
labels pLimit=50000 smallLimit=50000 workers=18
feature layers 5000, 8000, 9000, 10000, 12000
```

Result directory:

```text
results/wall-spectrum/validation-r451-r500-r2000/
```

Important files:

```text
validation-summary.tsv
fresh-k3-miss-diagnostics-summary.tsv
frozen-k3-adaptive-group-profile.tsv
```

Headline:

```text
dense strict            31/50   mean rows 206.82
dense wide              37/50   mean rows 816.88
dense adaptive          33/50   mean rows 351.80
frozen adaptive K3      48/50   mean rows 368.32
frozen adaptive K5      42/50   mean rows 367.34
K3 wide top5/R100       49/50   mean rows 577.56
MassR100                47/50   mean rows 365.20
confidence tier         48/50   mean rows 442.04
gap reserve top1        48/50   mean rows 452.12
gap reserve top2        48/50   mean rows 539.88
```

Interpretation:

This fresh block is a neutral/negative validation for the proposed
MassR100/tier candidates. MassR100 is slightly cheaper than K3 but loses one
known peak: `47/50` versus K3 `48/50`. Confidence tier ties K3 on hit count
but opens more rows: `442.04` versus `368.32`. Therefore neither candidate
should be promoted over K3 from this test.

Updated combined frontier through r500:

```text
K3 adaptive       413/450  91.7778%  mean rows 368.30
dense wide        346/450  76.8889%  mean rows 807.96
dense adaptive    298/450  66.2222%  mean rows 347.36
dense strict      287/450  63.7778%  mean rows 206.89
```

K3 miss diagnostics for r451-r500:

```text
misses                    2
missMeanNearestDistance   124
miss sourceP values       4507, 4783
```

Both K3 misses had the true target among selected centers, but outside the
opened radius. This supports the existing interpretation: the q-free wall
signal is useful for triage, but the frozen reveal geometry is not no-skip.

Candidate validation decision:

Stop this candidate-validation series here unless a new hypothesis is
pre-registered. The first fresh check did not improve the K3 frontier, so
running more blocks just to rescue MassR100/tier would be hindsight tuning.

## 2026-06-22 conclusion note

The current conclusion has been separated into:

```text
docs/wall-spectrum-research-conclusion.md
```

Read it after this handoff when the question is "what did we actually learn?"

Current final formulation:

```text
Positive: K3 adaptive is a robust q-free triage/enrichment baseline on the
fresh Silva holdouts: 413/450 coverage at about 368 opened rows per window,
versus dense wide 346/450 at about 808 rows.

Negative: K3 is not a no-skip predictor and not a formula for all peaks:
37/450 known peaks are still missed.
```

Important boundary:

MassR100 and confidence-tier reveal have now received one fresh candidate
validation slice. The result was not an improvement over K3: MassR100 lost one
hit, and confidence tier tied K3 at higher cost.

Next best step:

Stop running more MassR100/tier rescue checks unless a genuinely new rule is
pre-registered. The current defensible result is positive q-free triage with
K3 adaptive, and negative no-skip/formula evidence.
