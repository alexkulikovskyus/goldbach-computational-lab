# Goldbach Computational Lab

Native C/C++ tools and compact data for finite computational experiments with
minimal Goldbach witnesses, q-free wall-spectrum triage, and q-line interval
coverage.

For an even integer `N`, the minimal witness is

```text
p(N) = min { p prime : N - p is prime }.
```

This repository reports reproducible finite computations under explicitly
stated primality tests. It does not prove the Strong Goldbach conjecture. The
q-line coverage component is presented as a native implementation and
measurement of a classical interval-cover idea, not as a new theorem.

Repository: <https://github.com/alexkulikovskyus/goldbach-computational-lab>

## Included Native Tools

```text
native/goldbach_fast.c                  contiguous finite-range verifier in C
tools/scale_point_witness_gmp.cpp       C++/GMP worker for N = c * 10^e
tools/survivor_factor_scan_gmp.cpp      C++/GMP post-hoc survivor diagnostic
tools/scale_offset_pressure_scout.cpp   q-free sparse-offset pressure scout
tools/scale_offset_input_gmp.cpp        selected-offset input builder
tools/wall_spectrum_scorer_gmp.cpp      q-free small-divisor wall scorer
tools/wall_benchmark_windows_gmp.cpp    Silva/local-window benchmark builder
tools/minimal_witness_batch_gmp.cpp     batch labeler for benchmark windows
tools/minimal_witness_parallel_gmp.cpp  parallel exact minimal-witness worker
tools/wall_persistence_policy_metrics.cpp frozen K3 adaptive policy evaluator
tools/wall_frontier_summary.cpp         compact holdout frontier aggregator
tools/qfree_no_skip_qline_large_gmp.cpp C++/GMP q-line coverage worker
tools/qfree_qline_scout_gmp.cpp         small q-line scout/probe worker
tools/qline_extend_candidates_gmp.cpp   q-line candidate extension worker
tools/qfree_survivor_road_scorer_gmp.cpp survivor-road basket scorer
tools/run_e19_qline_campaign.sh         runner for the e19-forward campaign
tools/run_e19_qline_campaign_parallel.sh parallel runner used for the e19-forward campaign
```

## Included Data And Paper

```text
data/scale-point-2e2-2e5000-witnesses.csv
data/scale-point-2e2-2e5000-summary.json
data/goldbach-fast-20b-100b-w18-seg5m-faststats.json
data/goldbach-fast-faststats-100b-1t-w18-seg5m.json
data/e19-qline-coverage-current-summary.tsv
data/e19-qline-coverage-current-blocks.tsv
docs/qline-coverage-technology.md
docs/ai-context-for-paper.md
docs/wall-spectrum-session-handoff.md
docs/wall-spectrum-predictor-research-note.md
docs/wall-spectrum-frontier-summary.md
docs/wall-spectrum-research-conclusion.md
results/e15000-peak-hunt/
results/wall-spectrum/
paper/goldbach-witness-verifier.tex
paper/goldbach-witness-verifier.pdf
paper/figures/
```

## Build

Requirements:

```text
clang or gcc
clang++ or g++
GMP
pthread
tectonic, only for rebuilding the PDF
```

On Apple Silicon with Homebrew GMP installed at `/opt/homebrew/opt/gmp`:

```bash
make
```

If GMP is installed elsewhere:

```bash
make GMP_PREFIX=/path/to/gmp
```

## Smoke Tests

```bash
make smoke
make scale-smoke
make survivor-smoke
make wall-smoke
make qline-scout-smoke
make qline-smoke
```

## Contiguous Minimal-Witness Ranges

The C verifier scans every even value in a finite interval, tests candidate
prime witnesses in increasing order, and records the first successful
decomposition.

Example:

```bash
bin/goldbach-fast \
  --from=20000000000 \
  --to=100000000000 \
  --threads=18 \
  --segmentWidth=5000000 \
  --pLimit=100000 \
  --fastStats=1 \
  --out=results/range-example.json
```

## Decimal Scale Points

The C++/GMP worker checks sparse scale points

```text
N = coefficient * 10^e
```

and records the minimal witness found under GMP's primality test.

Example:

```bash
bin/scale-point-witness-gmp \
  --coefficient=2 \
  --firstExponent=4201 \
  --lastExponent=5000 \
  --pLimit=1000000 \
  --workers=18 \
  --primalityReps=25 \
  --checkpoint=results/scale-point-witnesses.jsonl
```

## Survivor Pressure

`tools/survivor_factor_scan_gmp.cpp` is a post-hoc diagnostic for a single
already chosen even `N`. It counts how many admissible candidate witnesses
before the first witness survive the small-prime wall and then fail later.

The paper's survivor-pressure tables used:

```text
smallLimit = 250000
factorLimit = 250000
```

The statistic is a ranking and diagnostic tool, not a proof and not an
independent formula for predicting `p(N)`.

## Q-free Wall-Spectrum Triage

The wall-spectrum tools test whether features computed before the exact
minimal-witness search can enrich for large minimal witnesses. The feature
stage is q-free: it uses small-divisor pressure in early admissible lanes, but
does not test whether `q = N - p` is prime and does not use the exact witness.

The current paper reports the frozen K3 adaptive policy on nine fresh
Oliveira e Silva--Herzog--Pardi least-prime holdout windows:

```text
K3 adaptive: 413/450 benchmark target rows at about 368 opened rows/window
dense wide:  346/450 benchmark target rows at about 808 opened rows/window
```

This is a triage/enrichment result, not a no-skip predictor and not a formula
for all large witnesses.

Related tools:

```text
bin/wall-spectrum-scorer-gmp
bin/wall-benchmark-windows-gmp
bin/minimal-witness-batch-gmp
bin/minimal-witness-parallel-gmp
bin/wall-persistence-policy-metrics
bin/wall-frontier-summary
```

## Sparse Offset Peak Hunt

The `e=15000` sparse offset branch is a q-free triage experiment followed by
exact minimal-witness checks on a selected basket. It is not a no-skip search.
The compact public snapshot is in:

```text
results/e15000-peak-hunt/
```

The headline row is:

```text
N = 2 * 10^15000 - 77368
p(N) = 1223309
p(N) / log(N) = 35.417712438
```

This is a probable-prime computational witness under GMP, not a formal
certificate.

## Q-line Coverage

`tools/qfree_no_skip_qline_large_gmp.cpp` implements the q-line interval
coverage experiment described in the paper. It verifies a weaker finite
statement than minimal-witness profiling: for every even `N` in a finite
block, find at least one decomposition `N = p + q` under the primality tests
used by the worker.

Rows missed by q-lines are checked by direct fallback. A block is accepted only
when:

```text
qlineRows + fallbackVerified = rows
unresolved = 0
```

The current public e19-forward snapshot covers `1.002 * 10^12` consecutive
even values starting at `2 * 10^19 + 2` under GMP probable-prime tests.

## Article

The paper is written in LaTeX:

```text
paper/goldbach-witness-verifier.tex
paper/goldbach-witness-verifier.pdf
```

Rebuild it with:

```bash
make paper
```

## License

MIT.
