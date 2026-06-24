# e=15000 peak hunt

This branch tested whether moving from e=10000 to e=15000 makes
`p >= 1000000` easier to reach for numbers of the form:

`N = 2 * 10^15000 + offset`.

For e=15000:

- `ln(N) ~= 34539.469542091247`
- `p = 1000000` corresponds to `p / ln(N) ~= 28.952384424`
- the previous e=10000 record had `p / ln(N) ~= 27.537393274`

So a p>=1M result at e=15000 needs only about a 5% normalized
improvement over the current e=10000 record.

## Scout

1. q-free pressure scout:
   - offsets: `[-100000, 100000]`, step 2
   - pLimit=10000
   - factorA=97, factorB=997
   - workers=18
   - top=200
   - output: `e15000-offset-pm100000-pressure-p10000-top200.tsv`

2. q-free survivor-road scoring:
   - input: `e15000-offset-pressure-top200-input.tsv`
   - pLimit=1000000
   - smallLimit=250000
   - output: `e15000-pressure-top200-road-score-p1000000-small250000.tsv`

The selected exact basket mixed:

- top `N mod 6 = 4` rows by pAt1000 / lane-specific road features;
- top `N mod 6 = 2` rows by rank-sum features.

## Exact top10 to pLimit=1000000

Command used `minimal-witness-parallel-gmp` with:

- pLimit=1000000
- smallLimit=250000
- workers=18
- primalityReps=5

Output: `e15000-roadscore-selected10-exact-p1000000-r5.tsv`.

The key row:

- offset `-77368`: exactP=0, verified=0 at pLimit=1000000

This means no witness was found up to p=1000000 under the worker's
probable-prime test path.

Other top10 rows:

- offset `64214`: p=432737
- offset `69758`: p=386537
- offset `64268`: p=381047
- offset `41828`: p=358607
- offset `46362`: p=299191
- offset `89862`: p=190369
- offset `9138`: p=127837
- offset `-10134`: p=90373
- offset `-93048`: p=7459

## Deep exact for offset -77368

Single-row follow-up:

- input: `e15000-minus77368-input.tsv`
- output: `e15000-minus77368-exact-p2000000-r5.tsv`
- pLimit=2000000
- smallLimit=250000
- workers=18
- primalityReps=5
- runtime: 2257.19 s wall, 36087.63 s user

Result:

- `N = 2 * 10^15000 - 77368`
- `p(N) = 1223309`
- `p / ln(N) ~= 35.417712438`

Interpretation:

- this is the first local result above p=1000000 in the current workflow;
- it is also a new normalized maximum for this project branch;
- the result is still a probable-prime computational witness under GMP's
  primality routine with `primalityReps=5`, not a formal certificate.

Practical conclusion:

- moving from e=10000 to e=15000 was useful;
- the q-free survivor-road scorer did not give a no-skip formula, but it did
  select a basket containing a p>1M row;
- lane-specific selection matters: the useful high row was in the `N mod 6 = 4`
  road bucket, while the `N mod 6 = 2` rank-sum candidates were much noisier.
