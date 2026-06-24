# AI Context for the Goldbach Computational Lab Paper

This note is the compact context file for an AI assistant, reviewer, or future
maintainer reading the paper:

`paper/goldbach-witness-verifier.pdf`

It explains what the project claims, what it does not claim, where the data
live, and which files should be read before changing the article.

## Read first

The paper is a finite computational note. It is not a proof of the Strong
Goldbach conjecture and it is not a certified extension of the Oliveira e
Silva--Herzog--Pardi verification up to `4 * 10^18`.

Use conservative language:

- "probable-prime computational witness" for rows accepted by GMP probable
  primality tests;
- "finite interval coverage" for q-line results;
- "triage", "enrichment", "benchmark", or "diagnostic" for wall-spectrum
  results;
- "formal certificate" only if independent primality certificates have been
  produced.

Avoid language such as "proof", "solution", "breakthrough", "new method", or
"discovery" unless a prior-art check has been repeated and the claim survives
that check.

## Main objects

For an even integer `N`, the minimal Goldbach witness is

```text
p(N) = min { p prime : N - p is prime }.
```

The project studies three different computational objects:

1. Minimal-witness measurements: find the first `p(N)` by increasing `p`.
2. Q-free wall-spectrum triage: try to select promising rows before computing
   the exact witness, using small-divisor features of `N - p` but not testing
   whether `q = N - p` is prime.
3. Q-line interval coverage: verify that each even number in a finite interval
   has at least one decomposition under the primality tests used by the worker.
   This does not compute the minimal witness profile.

These three objects answer different questions. Do not merge their claims.

## Current headline results

Scale-point minimal-witness table:

```text
N_e = 2 * 10^e, e = 2..5000
rows = 4999
largest absolute witness: p(2 * 10^4718) = 549667
largest normalized witness: p(2 * 10^3903) / log(2 * 10^3903) = 58.300567
GMP reps = 25 for accepted scale-point rows
```

Sparse offset experiment:

```text
N = 2 * 10^15000 - 77368
p(N) = 1223309
p(N) / log(N) = 35.417712438
GMP reps = 5 in this experimental branch
status = probable-prime computational witness, not formal certificate
```

Wall-spectrum triage benchmark:

```text
benchmark = Oliveira e Silva--Herzog--Pardi least-prime holdout windows
holdout ranks = 51..500, grouped into 9 windows of 50 rows
window radius = +/- 2000 rows
K3 adaptive = 413/450 hits, about 368 opened rows per group
dense wide baseline = 346/450 hits, about 808 opened rows per group
status = enrichment/triage result, not no-skip predictor
misses = 37/450 target rows
```

Q-line finite coverage campaign:

```text
interval start = 2 * 10^19 + 2
interval end = 2 * 10^19 + 2004000000000
even values checked = 1002000000000
q-line rows = 1001999948917
fallback rows = 51083
fallback verified = 51083
unresolved = 0
GMP reps = 25
status = probable-prime finite interval coverage, not formal certification
```

## Canonical data files

Scale points:

```text
data/scale-point-2e2-2e5000-witnesses.csv
data/scale-point-2e2-2e5000-summary.json
```

Contiguous finite-range summaries:

```text
data/goldbach-fast-20b-100b-w18-seg5m-faststats.json
data/goldbach-fast-faststats-100b-1t-w18-seg5m.json
```

Sparse `e=15000` peak hunt:

```text
results/e15000-peak-hunt/README.md
results/e15000-peak-hunt/e15000-offset-pm100000-pressure-p10000-top200.tsv
results/e15000-peak-hunt/e15000-pressure-top200-road-score-p1000000-small250000.tsv
results/e15000-peak-hunt/e15000-roadscore-selected10-summary.tsv
results/e15000-peak-hunt/e15000-minus77368-exact-p2000000-r5.tsv
```

Wall-spectrum:

```text
docs/wall-spectrum-session-handoff.md
docs/wall-spectrum-predictor-research-note.md
docs/wall-spectrum-frontier-summary.md
docs/wall-spectrum-research-conclusion.md
results/wall-spectrum/holdout-frontier-summary.tsv
results/wall-spectrum/frozen-adaptive-validation-summary.tsv
```

Q-line coverage:

```text
docs/qline-coverage-technology.md
data/e19-qline-coverage-current-summary.tsv
data/e19-qline-coverage-current-blocks.tsv
results/qline-coverage/e19-forward/
```

## Main native tools

```text
native/goldbach_fast.c
tools/scale_point_witness_gmp.cpp
tools/survivor_factor_scan_gmp.cpp
tools/scale_offset_pressure_scout.cpp
tools/scale_offset_input_gmp.cpp
tools/wall_spectrum_scorer_gmp.cpp
tools/wall_benchmark_windows_gmp.cpp
tools/minimal_witness_batch_gmp.cpp
tools/wall_persistence_policy_metrics.cpp
tools/wall_frontier_summary.cpp
tools/qfree_no_skip_qline_large_gmp.cpp
tools/qfree_qline_scout_gmp.cpp
tools/qline_extend_candidates_gmp.cpp
tools/minimal_witness_parallel_gmp.cpp
tools/qfree_survivor_road_scorer_gmp.cpp
```

Build with:

```bash
make
make paper
```

## Prior art boundary

The following are not new contributions of this project:

- minimal Goldbach partitions / least-prime partitions;
- exhaustive verification of Goldbach up to `4 * 10^18`;
- interval coverage by sets of primes `P + Q`;
- q-line-style reversal where large `q` values near an interval cover many
  even rows at once.

Important references:

- M.-K. Shen, "On checking the Goldbach conjecture", 1964.
- M. K. Sinisalo, "Checking the Goldbach conjecture up to `4 * 10^11`", 1993.
- A. Granville, H. J. J. te Riele, J. van de Lune, "Checking the Goldbach
  conjecture on a vector computer", 1989.
- J.-M. Deshouillers, H. J. J. te Riele, Y. Saouter, "New experimental results
  concerning the Goldbach conjecture", 1998.
- J. Richstein, "Verifying the Goldbach conjecture up to `4 * 10^14`", 2001.
- T. Oliveira e Silva, S. Herzog, S. Pardi, "Empirical verification of the even
  Goldbach conjecture and computation of prime gaps up to `4 * 10^18`", 2014.
- OEIS A025017, A025018, A025019 for least-prime Goldbach partition sequences.
- GNU MP manual for `mpz_probab_prime_p`.

## Current interpretation

The strongest honest summary is:

```text
This repository provides a compact native implementation and reproducible
finite computational measurements around minimal Goldbach witnesses, q-free
pre-witness triage, and q-line interval coverage. The most interesting
research signal is not a proof or formula, but a measurable enrichment effect:
the q-free wall-spectrum and survivor-road procedures can select small baskets
or neighborhoods that contain unusually delayed minimal witnesses more often
than simple baselines, while still missing some target rows.
```

The `e=15000` row is encouraging because it shows the triage workflow can land
on a `p > 10^6` sparse-scale witness. It is not enough to claim a universal
predictor.

The q-line result is valuable engineering and finite coverage, but it is a
different object from minimal-witness peak hunting.

## Current limitations

- Selected rows are accepted under GMP probable-prime tests unless a formal
  certificate is explicitly stated.
- The wall-spectrum and survivor-road procedures are enrichment diagnostics,
  not no-skip predictors.
- Q-line interval coverage verifies finite coverage under the worker's
  primality convention; it does not compute the minimal witness profile.
- Prior-art checks must stay ahead of any stronger public claim.
