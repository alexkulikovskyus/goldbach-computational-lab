# Wall-spectrum research conclusion

Status: current conclusion after the frozen fresh holdouts through
`r451-r500`.

## Question

For an even integer `N`, define the minimal Goldbach witness:

```text
p(N) = min { p prime : N - p is prime }.
```

The research question was not whether Goldbach is true, and not whether one
can find some Goldbach decomposition. The question was narrower:

```text
Before computing p(N), can q-free wall-spectrum features identify numbers
whose minimal witness p(N) is unusually large?
```

Here `q-free` means the feature stage does not test whether `q = N - p` is
prime and does not use the exact minimal witness. Exact witnesses are used only
later as labels for evaluation.

## Prior-art guardrail

Minimal Goldbach witnesses and least-prime records are not new objects.

The relevant prior-art boundary is:

- Oliveira e Silva, Herzog, and Pardi verified the even Goldbach conjecture up
  to `4 * 10^18` and publish least-prime / `S(p)` data:
  `https://sweet.ua.pt/tos/goldbach.html`
- Deshouillers, te Riele, and Saouter explicitly discuss the smallest prime
  `p` such that `e - p` is prime, calling the pair the minimal Goldbach
  decomposition:
  `https://ir.cwi.nl/pub/1222/1222D.pdf`
- OEIS A025017/A025018/A025019 record related least-prime Goldbach sequences:
  `https://oeis.org/A025017`,
  `https://oeis.org/A025018`,
  `https://oeis.org/A025019`

Therefore the possible contribution here is not a new Goldbach object, not a
new record table, and not a proof. The possible contribution is a tested
pre-witness triage/enrichment method, plus negative evidence about no-skip
prediction.

## Tested signal

The useful q-free signal is small-divisor pressure in the early admissible
prime lane.

For early prime candidates `p`, mark `p` blocked when `N - p` has a small
prime divisor under a chosen wall limit. A large minimal witness often appears
near a local pattern where many early admissible candidates are blocked. This
is the "wall-spectrum".

The current best frozen policy is:

```text
K3 adaptive wall persistence
```

In plain language:

1. Look at several wall-depth layers.
2. In each layer, find q-free local maxima of small-divisor pressure.
3. Keep centers that persist across layers.
4. Open a small adaptive neighborhood around the strongest centers.
5. Compare coverage against exact minimal-witness labels only after selection.

## Main finite result

The current comparable fresh holdout frontier uses nine Silva local windows:

```text
r51-r100
r101-r150
r151-r200
r201-r250
r251-r300
r301-r350
r351-r400
r401-r450
r451-r500
```

Each holdout group is a contiguous `+/-2000` window around a known Silva
least-prime row. The total comparable evaluation set is `450` groups.

Directly comparable policies:

| Policy | Hits | Hit rate | Mean opened rows | Mean opened fraction |
| --- | ---: | ---: | ---: | ---: |
| K3 adaptive | 413/450 | 91.78% | 368.30 | 18.41% |
| dense wide | 346/450 | 76.89% | 807.96 | 40.38% |
| dense adaptive | 298/450 | 66.22% | 347.36 | 17.36% |
| dense strict | 287/450 | 63.78% | 206.89 | 10.34% |

The main positive result:

```text
K3 adaptive covers substantially more known peaks than dense wide while
opening less than half as many rows.
```

This is a real enrichment result on these finite holdouts.

## Negative result

The result is not a no-skip predictor.

K3 adaptive still misses:

```text
37 / 450 known peaks
```

The remaining misses are not one simple failure mode. Across holdouts they
include:

- near centers that were selected but not opened far enough;
- lower-priority centers just outside the adaptive reveal order;
- displaced wall islands where the strongest q-free pressure is not centered
  on the true peak;
- occasional cases where broader reveal helps only by opening much more of
  the window.

So the current honest statement is:

```text
Wall persistence is useful for triage/enrichment, but the tested policies do
not establish a formula or no-skip localizer for all peaks.
```

## Candidate follow-up

Two inspected variants were given one fresh candidate-validation slice after
they looked promising in earlier inspected blocks. They are not established
improvements over K3:

| Candidate | Fresh r451-r500 result | Interpretation |
| --- | ---: | --- |
| MassR100 priority | 47/50, 365.20 rows | Slightly cheaper than K3, but loses one hit. |
| confidence tier | 48/50, 442.04 rows | Ties K3 hit count, but costs more. |

So the candidate check is neutral/negative: neither MassR100 nor confidence
tier should replace K3 on the current evidence.

## Practical meaning

For a large search problem, the current method can be used as a q-free
triage/landing heuristic:

```text
Use K3 adaptive to choose promising local neighborhoods,
then run exact witness work only inside the selected neighborhoods.
```

It should not be used as a proof that unselected neighborhoods contain no
peaks.

## Reproducible artifacts

Short restart memory:

```text
docs/wall-spectrum-session-handoff.md
```

Detailed research log:

```text
docs/wall-spectrum-predictor-research-note.md
```

Compact frontier:

```text
docs/wall-spectrum-frontier-summary.md
```

Generated frontier TSV:

```text
results/wall-spectrum/holdout-frontier-summary.tsv
```

Frontier generator:

```text
tools/wall_frontier_summary.cpp
```

Core policy evaluator:

```text
tools/wall_persistence_policy_metrics.cpp
```

Last verification command:

```text
make build wall-smoke qline-scout-smoke qline-smoke
```
