#!/usr/bin/env bash
set -euo pipefail

start_block="${1:-0}"
end_block="${2:-1001}"
jobs="${3:-18}"
inner_workers="${4:-1}"

cd "$(dirname "$0")/.."

out_dir="results/qline-coverage/e19-forward"
mkdir -p "$out_dir"

run_one() {
  local block="$1"
  local min_offset max_offset center_offset tag summary fallback tmp_summary tmp_fallback
  min_offset=$((2 + block * 2000000000))
  max_offset=$(((block + 1) * 2000000000))
  center_offset=$((min_offset + 49998))
  printf -v tag "%06d" "$block"

  summary="$out_dir/e19-qline-block${tag}-summary.tsv"
  fallback="$out_dir/e19-qline-block${tag}-fallback.tsv"
  tmp_summary="${summary}.tmp.$$"
  tmp_fallback="${fallback}.tmp.$$"

  if [[ -f "$summary" ]]; then
    echo "skip block=$block existing=$summary"
    return 0
  fi

  echo "run block=$block offsets=${min_offset}..${max_offset} workers=${inner_workers}"
  ./bin/qfree-no-skip-qline-large-gmp \
    --coefficient=2 \
    --base=10 \
    --exponent=19 \
    --minOffset="$min_offset" \
    --maxOffset="$max_offset" \
    --centerOffset="$center_offset" \
    --aLimit=50000 \
    --pLimit=250000 \
    --qFilterLimit=250000 \
    --smallLimit=250000 \
    --workers="$inner_workers" \
    --baselineSampleStride=0 \
    --progressEveryLines=0 \
    --summary="$tmp_summary" \
    --fallbackOutput="$tmp_fallback"
  mv "$tmp_summary" "$summary"
  mv "$tmp_fallback" "$fallback"
}

export -f run_one
export out_dir inner_workers

seq "$start_block" "$end_block" | xargs -n 1 -P "$jobs" bash -c 'run_one "$0"'

agg="$out_dir/e19-qline-campaign-blocks.tsv"
printf "block\trows\tminOffset\tmaxOffset\tcenterOffset\taLimit\tpLimit\tqlineQTests\tprimeLines\tqlineRows\tfallbackRows\tfallbackVerified\tunresolved\tfallbackQTests\tnoSkipQTests\tbaselineSampleRows\tbaselineSampleQTests\tavgBaselineQTests\testimatedBaselineQTests\tsavingPct\tspeedup\tseconds\n" > "$agg"

for summary in "$out_dir"/e19-qline-block*-summary.tsv; do
  [[ -f "$summary" ]] || continue
  name="$(basename "$summary")"
  block="${name#e19-qline-block}"
  block="${block%-summary.tsv}"
  block="$((10#$block))"
  tail -n 1 "$summary" | awk -F '\t' -v OFS='\t' -v b="$block" '{ print b,$0 }'
done | sort -n -k1,1 >> "$agg"

echo "wrote $agg"
