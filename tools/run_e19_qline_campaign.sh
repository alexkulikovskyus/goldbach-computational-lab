#!/usr/bin/env bash
set -euo pipefail

start_block="${1:-0}"
end_block="${2:-99}"

cd "$(dirname "$0")/.."

out_dir="results/qline-coverage/e19-forward"
mkdir -p "$out_dir"

agg="$out_dir/e19-qline-campaign-blocks.tsv"
if [[ ! -f "$agg" ]]; then
  printf "block\trows\tminOffset\tmaxOffset\tcenterOffset\taLimit\tpLimit\tqlineQTests\tprimeLines\tqlineRows\tfallbackRows\tfallbackVerified\tunresolved\tfallbackQTests\tnoSkipQTests\tbaselineSampleRows\tbaselineSampleQTests\tavgBaselineQTests\testimatedBaselineQTests\tsavingPct\tspeedup\tseconds\n" > "$agg"
fi

append_summary() {
  local block="$1"
  local summary="$2"
  if awk -F '\t' -v b="$block" 'NR > 1 && $1 == b { found = 1 } END { exit(found ? 0 : 1) }' "$agg"; then
    return
  fi
  tail -n 1 "$summary" | awk -F '\t' -v OFS='\t' -v b="$block" '{ print b,$0 }' >> "$agg"
}

for ((block = start_block; block <= end_block; ++block)); do
  min_offset=$((2 + block * 2000000000))
  max_offset=$(((block + 1) * 2000000000))
  center_offset=$((min_offset + 49998))
  printf -v tag "%06d" "$block"

  summary="$out_dir/e19-qline-block${tag}-summary.tsv"
  fallback="$out_dir/e19-qline-block${tag}-fallback.tsv"

  if [[ -f "$summary" ]]; then
    append_summary "$block" "$summary"
    echo "skip block=$block existing=$summary"
    continue
  fi

  echo "run block=$block offsets=${min_offset}..${max_offset} center=${center_offset}"
  caffeinate -i ./bin/qfree-no-skip-qline-large-gmp \
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
    --workers=19 \
    --baselineSampleStride=0 \
    --progressEveryLines=0 \
    --summary="$summary" \
    --fallbackOutput="$fallback"
  append_summary "$block" "$summary"
done
