# Wall-spectrum holdout frontier summary

This note is the compact, reproducible summary of the current q-free
wall-spectrum triage result. It should be read after
`docs/wall-spectrum-session-handoff.md` when the question is: what have we
actually shown, and what is still missing?

## Scope

The experiment is finite and computational. It tests whether features computed
before the exact minimal witness search can enrich for large minimal Goldbach
witnesses `p(N)`.

The feature stage is q-free in the operational sense used in this project:

- it does not use the exact minimal witness `p(N)`;
- it does not test whether `q = N - p` is prime;
- exact witness values are used only as labels/evaluation targets.

This is not a proof of the Goldbach conjecture, not a no-skip search
algorithm, and not a formula for all peaks.

## Reproducible artifacts

Input summary:

```text
results/wall-spectrum/frozen-adaptive-validation-summary.tsv
```

Reproducible frontier generator:

```text
tools/wall_frontier_summary.cpp
bin/wall-frontier-summary
```

Generated frontier:

```text
results/wall-spectrum/holdout-frontier-summary.tsv
```

Generation command:

```text
./bin/wall-frontier-summary \
  --input=results/wall-spectrum/frozen-adaptive-validation-summary.tsv \
  --output=results/wall-spectrum/holdout-frontier-summary.tsv
```

The frontier summary uses the nine fresh `+/-2000` Silva holdout windows:

```text
r51-r100-r2000
r101-r150-r2000
r151-r200-r2000
r201-r250-r2000
r251-r300-r2000
r301-r350-r2000
r351-r400-r2000
r401-r450-r2000
r451-r500-r2000
```

## Comparable frontier

These policies are directly comparable across all nine fresh holdouts.

| Policy | Holdouts | Hits | Hit rate | Mean opened rows | Mean opened fraction |
| --- | ---: | ---: | ---: | ---: | ---: |
| K3 adaptive | 9 | 413/450 | 91.8% | 368.30 | 18.41% |
| dense wide | 9 | 346/450 | 76.9% | 807.96 | 40.38% |
| dense adaptive | 9 | 298/450 | 66.2% | 347.36 | 17.36% |
| dense strict | 9 | 287/450 | 63.8% | 206.89 | 10.34% |

Interpretation:

- K3 adaptive is the current best tested q-free triage policy.
- It covers substantially more known peaks than dense strict: `413/450`
  versus `287/450`.
- It also covers more than dense wide while opening less than half as many
  rows: `368.30` versus `807.96`.
- This is enrichment/triage, not a no-skip locator. K3 adaptive still misses
  `37/450` known peaks in these fresh holdouts.
- The fresh r301-r350 holdout was weaker for K3 (`42/50`) than the previous
  frontier (`233/250`), which is a useful warning against overclaiming.
- The following r351-r400 holdout partially recovered (`44/50`) and widened
  the K3 advantage over dense wide on that slice (`35/50`).
- The r401-r450 stability slice was stronger (`46/50`) and widened the
  K3 advantage over dense wide again (`33/50`).
- The fresh r451-r500 candidate-validation slice was stronger still for K3
  (`48/50`) and did not promote MassR100 or confidence tier over K3.

## Partial variants

These rows were tested on only some fresh holdouts, so they are diagnostics,
not a replacement frontier.

| Policy | Holdouts | Hits | Hit rate | Mean opened rows | Comment |
| --- | ---: | ---: | ---: | ---: | --- |
| K3 wide top5/R100 | 6 | 286/300 | 95.3% | 576.79 | More coverage, higher cost. |
| confidence tier | 5 | 231/250 | 92.4% | 442.02 | Fresh check tied K3 but cost more. |
| MassR100 priority | 7 | 321/350 | 91.7% | 364.56 | Fresh check lost one hit versus K3. |
| gap reserve top2 | 5 | 228/250 | 91.2% | 539.15 | Helps specific cases, not general. |
| gap reserve top1 | 6 | 271/300 | 90.3% | 453.42 | No clean advantage over K3. |
| K5 adaptive | 8 | 312/400 | 78.0% | 368.28 | Negative result; worse than K3. |

## Negative result that matters

The failed repairs are as important as the positive result:

- K5 did not repair K3 misses and repeatedly worsened coverage.
- Shape-priority expansion produced only local cost changes and did not
  reliably improve hit count.
- Gap reserve and tiered reveal can recover selected cases, but they do not
  remove false negatives at an acceptable stable cost.
- MassR100 and confidence tier did not beat K3 on the first fresh candidate
  validation block.
- Remaining misses are heterogeneous: some are near-but-not-expanded, some
  are low-priority centers, and some are displaced from the strongest wall.

So the current evidence supports this statement:

```text
Multi-layer wall persistence is a reproducible q-free triage signal on these
Silva holdouts, but the tested policies do not establish a no-skip predictor.
```

## Next useful step

Do not add another reveal-radius tweak unless it is pre-registered against a
fresh holdout and improves the frontier table. The current scientifically
useful directions are:

1. Freeze K3 adaptive as the current triage baseline.
2. Compare any new rule against K3 adaptive, dense strict, dense wide, and
   dense adaptive on a fresh holdout.
3. Report both coverage and opened-row cost.
4. Treat a no-improvement result as a real negative result, not as a failure
   to be hidden.
5. Do not run MassR100 or confidence-tier rescue checks unless there is a new
   pre-registered rule. The first fresh candidate validation did not improve
   the K3 frontier.
