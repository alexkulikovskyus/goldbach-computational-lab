# Q-line Coverage Technology

This note describes the q-line coverage method used in the e19-forward finite
Goldbach coverage campaign.

The method is separate from the minimal-witness profile. It does not compute

```text
p(N) = min { p prime : N - p is prime }.
```

Instead, it tries to verify a weaker but still meaningful finite statement:

```text
For every even N in a tested finite block, find at least one decomposition
N = p + q where p and q pass the primality test used by the worker.
```

In short:

```text
minimal-witness profile = find the first p(N), useful for background and peaks
q-line coverage         = find any valid p + q, useful for fast block coverage
```

## Plain-language picture

Direct search tries to solve each even number separately.

Q-line coverage does something different. It first finds many large prime
numbers `q` near the tested block. Each such `q` acts like a ruler laid across
the whole block. If `N - q` is prime at a particular position, that position is
covered.

So instead of asking one billion times:

```text
Which p works for this N?
```

the q-line worker asks:

```text
Can this one fixed prime q cover many N at once?
```

After many such rulers are laid down, almost every row is covered. The few
holes are then checked one by one by direct fallback.

## Block Model

The campaign scans consecutive blocks of even values near

```text
B = 2 * 10^19.
```

For each block we use:

```text
rows = 1,000,000,000 even values
minOffset = 2 + block * 2,000,000,000
maxOffset = (block + 1) * 2,000,000,000
N = B + offset
```

Each block has a center offset:

```text
centerOffset = minOffset + 49,998
N0 = B + centerOffset
```

The center is intentionally near the beginning of the block, not in the middle.
This lets small values of `a <= 50000` produce valid positive candidate
primes `p` across almost the whole block.

## Q-lines

Pick an odd value `a` with

```text
1 <= a <= aLimit
aLimit = 50,000
```

Define

```text
q_a = N0 - a.
```

If `q_a` is prime, then for every even `N` in the block the line gives a
candidate value

```text
N = p_a(N) + q_a
```

where

```text
p_a(N) = N - q_a
       = N - (N0 - a)
       = a + (N - N0).
```

As `N` moves by `2`, `p_a(N)` also moves by `2`. Therefore one prime `q_a`
creates a whole line of possible decompositions across the block. The line
covers exactly the rows for which `p_a(N)` is also prime and at least `3`.
This is why we call it a q-line.

For a row `N`, if both conditions hold:

```text
q_a is prime
p_a(N) is prime
```

then that row is covered:

```text
N = p_a(N) + q_a.
```

## How Q-lines Are Built

The worker does not test every `q_a` expensively from scratch.

First it applies a small-prime filter:

```text
qFilterLimit = 250,000
```

For each small prime `r`, if

```text
q_a = N0 - a == 0 mod r
```

then `q_a` is composite, so this `a` is blocked.

Only the survivors are sent to GMP:

```text
mpz_probab_prime_p(q_a, 25)
```

Every survivor accepted by GMP becomes a q-line.

In the current e19-forward campaign, typical blocks have around:

```text
about 3,300 to 3,500 q tests
about 1,070 to 1,190 accepted q-lines
```

These accepted q-lines are then used to cover one billion rows at once.

## Prime Bitset For p

For a q-line `q_a`, the only remaining question is whether

```text
p_a(N) = a + (N - N0)
```

is prime for each row.

Instead of calling GMP for each row, the worker builds a bitset of ordinary
prime values in the possible `p` range. For the current block geometry this is
roughly:

```text
p <= 2,000,000,000
```

The worker builds an odd-prime bitset using a sieve, so primality of `p` can be
checked by bit lookup. Then each q-line becomes a shifted slice of that prime
bitset. The coverage bitset for the block is updated by OR-ing these shifted
slices.

Conceptually:

```text
coverage[row] = coverage[row] OR isPrime(p_a(N_row))
```

for every accepted q-line.

After all q-lines are applied:

```text
coverage[row] = 1  means the row has at least one q-line decomposition
coverage[row] = 0  means q-line did not cover the row
```

## Direct Fallback

Q-line uncovered rows are not discarded and not treated as failures.

For every uncovered row, the worker runs direct fallback. Direct fallback scans
candidate prime witnesses `p` in increasing order up to:

```text
pLimit = 250,000
```

It applies the admissible modulo-6 lane and a small-prime wall:

```text
smallLimit = 250,000
```

For remaining candidates it tests:

```text
q = N - p
```

using GMP's probable-prime test.

A block is accepted only if every row is in one of these two states:

```text
q-line covered
direct fallback verified
```

The block is not accepted if any row remains unresolved.

## Pseudocode

For one block:

```text
input:
  B = 2 * 10^19
  minOffset, maxOffset
  centerOffset
  aLimit, pLimit, qFilterLimit, smallLimit

N0 = B + centerOffset

build q-line candidates:
  for odd a in [1, aLimit]:
    if N0 - a is not eliminated by small primes <= qFilterLimit:
      if GMP says N0 - a is probable prime:
        keep q-line a

build prime bitset for p values in the block's possible p range

coverage = all zero bits

for each accepted q-line a:
  q = N0 - a
  for every row N in the block, implicitly by shifted bitset:
    p = a + (N - N0)
    if p is prime:
      coverage[row] = 1

fallback:
  for every row with coverage[row] = 0:
    run direct search for some p <= pLimit
    if p and N - p pass primality checks:
      mark fallback verified
    else:
      mark unresolved

accept block only if unresolved = 0
```

## No-skip Condition

The key safety rule is:

```text
q-line miss is not a rejection.
```

A q-line miss only means:

```text
This row was not covered by the current q-line set.
```

The row must then be checked by direct fallback. Therefore the q-line method is
not a lossy filter for the final finite coverage result. It is an accelerator
followed by a direct verifier for the small residue.

The finite block condition is:

```text
qlineRows + fallbackVerified = rows
unresolved = 0
```

When this condition holds, every even `N` in the block has at least one
recorded Goldbach decomposition under the primality test used by the worker.

## What The Method Proves

For a completed finite block with `unresolved = 0`, q-line coverage proves the
following computational statement:

```text
Every even N in this finite block has at least one decomposition N = p + q,
where the reported p and q pass the worker's primality tests.
```

More precisely, because large `q` values are accepted by GMP's probable-prime
routine, the result should be described as:

```text
probable-prime computational coverage under GMP mpz_probab_prime_p
```

unless rows are later upgraded with independent formal primality certificates.

## Correctness Sketch For One Finite Block

This is the finite-block argument used by the worker.

Assumption:

```text
The primality tests used by the worker are accepted as the computational
primality oracle for this run.
```

Claim 1:

```text
If a row is q-line covered, then that row has a Goldbach decomposition.
```

Reason:

```text
The q-line was kept only after q_a = N0 - a passed the primality test.
The row was marked covered only when p_a(N) = N - q_a was prime in the p-bitset.
Therefore N = p_a(N) + q_a with both parts prime under the run's tests.
```

Claim 2:

```text
If a row is fallback verified, then that row has a Goldbach decomposition.
```

Reason:

```text
Fallback explicitly found a p and tested q = N - p.
The row is marked verified only if the pair passed the worker's checks.
```

Claim 3:

```text
If qlineRows + fallbackVerified = rows and unresolved = 0, then every even N
in the finite block has at least one recorded decomposition.
```

Reason:

```text
Every row is either covered by Claim 1 or verified by Claim 2.
There is no third unresolved class left.
```

## What The Method Does Not Prove

Q-line coverage does not prove the Strong Goldbach conjecture in general.

It does not verify untested values outside the finite block.

It does not compute the minimal witness `p(N)`.

It does not locate minimal-witness peaks, because the first q-line that covers
a row may use a much larger `p` than the true minimal witness, or a different
decomposition entirely.

It does not replace formal primality certificates unless certification is added
as a separate layer.

## What Q-line Holes Mean

A q-line hole is a row with

```text
coverage[row] = 0
```

after all accepted q-lines have been applied.

This does not mean the row has no Goldbach decomposition. It also does not mean
the row is a minimal-witness peak. It only means:

```text
None of the q-lines chosen for this block covered this row.
```

The direct fallback stage then checks the row normally. In the current
campaign, q-line holes are rare: usually only tens of rows per billion. This
makes them attractive diagnostic targets. After a coverage campaign finishes,
one useful follow-up is:

```text
For every q-line hole, compute or inspect its minimal witness p(N).
Compare those values with the local minimal-witness background.
```

Possible outcomes:

```text
holes are mostly low/background p(N):
  q-line holes are just geometry misses, not peak indicators.

holes are enriched for high p(N):
  q-line holes may become a cheap preselector for difficult rows.

holes include both:
  additional features are needed before holes can guide peak search.
```

## Why It Is Fast

A direct minimal-witness profile asks, for every row:

```text
try p = 3, 5, 7, 11, ...
until q = N - p is prime
```

That is the right calculation for background and peaks, but it is expensive.

Q-line coverage reverses the direction:

```text
1. Find many prime q_a near N0 once.
2. For each q_a, use a prime bitset to cover many rows at once.
3. Direct-check only the tiny residue not covered by q-lines.
```

In the completed e19-forward campaign, q-lines usually left only tens of rows
per billion for direct fallback.

## Completed Campaign Parameters

The public e19-forward campaign snapshot uses:

```text
B = 2 * 10^19
rows per block = 1,000,000,000
blocks = 1,002
aLimit = 50,000
pLimit = 250,000
qFilterLimit = 250,000
smallLimit = 250,000
GMP primality reps = 25
block-parallel jobs = 18
inner workers per block = 1
```

Worker:

```text
tools/qfree_no_skip_qline_large_gmp.cpp
bin/qfree-no-skip-qline-large-gmp
```

Runner:

```text
tools/run_e19_qline_campaign.sh
tools/run_e19_qline_campaign_parallel.sh
```

## Completed Public Snapshot

The public data files under `data/` summarize the completed finite campaign:

```text
data/e19-qline-coverage-current-summary.tsv
data/e19-qline-coverage-current-blocks.tsv
```

The aggregate result is:

```text
blocks = 1002
even values checked = 1,002,000,000,000
offset range = 2 .. 2,004,000,000,000
q-line rows = 1,001,999,948,917
q-line coverage = 99.999994902%
direct fallback rows = 51,083
direct fallback verified = 51,083
unresolved = 0
```

This is probable-prime computational coverage under GMP's primality test, not
formal certification.

Result files:

```text
results/qline-coverage/e19-forward/e19-qline-campaign-blocks.tsv
results/qline-coverage/e19-forward/e19-qline-blockNNNNNN-summary.tsv
results/qline-coverage/e19-forward/e19-qline-blockNNNNNN-fallback.tsv
```

The large per-block fallback files remain local unless a later publication
needs them.

## Fields In The Block Summary

Each block summary records:

```text
rows                 number of even values in the block
minOffset/maxOffset  tested offset interval from B
centerOffset         offset used to define N0
aLimit               maximum q-line a
pLimit               direct fallback p limit
qlineQTests          number of q_a candidates sent to GMP
primeLines           accepted prime q-lines
qlineRows            rows covered by q-lines
fallbackRows         rows not covered by q-lines
fallbackVerified     fallback rows verified directly
unresolved           rows not verified by either stage
fallbackQTests       GMP q tests spent in fallback
noSkipQTests         qlineQTests + fallbackQTests
seconds              wall time for the block
```

For a valid completed block:

```text
rows = qlineRows + fallbackVerified
fallbackRows = fallbackVerified
unresolved = 0
```

## How To Phrase The Result

Good wording:

```text
We performed a finite q-line coverage verification of consecutive even values
near 2 * 10^19. In each completed block, q-lines covered almost all rows and
direct fallback verified the remaining rows. No unresolved rows were observed
in the reported finite range.
```

Avoid wording like:

```text
This proves Goldbach.
This computes minimal witnesses.
This covers the entire e18-to-e19 layer.
```

Those statements would be too strong or wrong for this method.
