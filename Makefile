.PHONY: all build clean smoke scale-smoke survivor-smoke qline-scout-smoke qline-smoke wall-smoke paper

CC ?= clang
CXX ?= clang++
CFLAGS ?= -O3 -flto -pthread -march=native -DNDEBUG
CXXFLAGS ?= -std=c++17 -O3 -flto -pthread -march=native -DNDEBUG
GMP_PREFIX ?= /opt/homebrew/opt/gmp
GMP_CFLAGS ?= -I$(GMP_PREFIX)/include
GMP_LDFLAGS ?= -L$(GMP_PREFIX)/lib
GMP_LIBS ?= -lgmp

all: build

build: bin/goldbach-fast bin/scale-point-witness-gmp bin/survivor-factor-scan-gmp bin/scale-offset-input-gmp bin/scale-offset-pressure-scout bin/qfree-qline-scout-gmp bin/qfree-no-skip-qline-large-gmp bin/qline-extend-candidates-gmp bin/wall-spectrum-scorer-gmp bin/qfree-survivor-road-scorer-gmp bin/wall-benchmark-controls-gmp bin/wall-benchmark-windows-gmp bin/minimal-witness-batch-gmp bin/minimal-witness-parallel-gmp bin/wall-benchmark-metrics bin/wall-local-contrast-metrics bin/wall-nms-policy-metrics bin/wall-multipoint-landing-metrics bin/wall-persistence-policy-metrics bin/wall-blocker-profile-gmp bin/wall-frontier-summary

bin/goldbach-fast: native/goldbach_fast.c
	mkdir -p bin
	$(CC) $(CFLAGS) $< -o $@

bin/scale-point-witness-gmp: tools/scale_point_witness_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/survivor-factor-scan-gmp: tools/survivor_factor_scan_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/scale-offset-input-gmp: tools/scale_offset_input_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/scale-offset-pressure-scout: tools/scale_offset_pressure_scout.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/qfree-qline-scout-gmp: tools/qfree_qline_scout_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/qfree-no-skip-qline-large-gmp: tools/qfree_no_skip_qline_large_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/qline-extend-candidates-gmp: tools/qline_extend_candidates_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/wall-spectrum-scorer-gmp: tools/wall_spectrum_scorer_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/qfree-survivor-road-scorer-gmp: tools/qfree_survivor_road_scorer_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/wall-benchmark-controls-gmp: tools/wall_benchmark_controls_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/wall-benchmark-windows-gmp: tools/wall_benchmark_windows_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/minimal-witness-batch-gmp: tools/minimal_witness_batch_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/minimal-witness-parallel-gmp: tools/minimal_witness_parallel_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/wall-benchmark-metrics: tools/wall_benchmark_metrics.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@

bin/wall-local-contrast-metrics: tools/wall_local_contrast_metrics.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@

bin/wall-nms-policy-metrics: tools/wall_nms_policy_metrics.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@

bin/wall-multipoint-landing-metrics: tools/wall_multipoint_landing_metrics.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@

bin/wall-persistence-policy-metrics: tools/wall_persistence_policy_metrics.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@

bin/wall-blocker-profile-gmp: tools/wall_blocker_profile_gmp.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(GMP_CFLAGS) $< $(GMP_LDFLAGS) $(GMP_LIBS) -o $@

bin/wall-frontier-summary: tools/wall_frontier_summary.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@

smoke: bin/goldbach-fast
	mkdir -p results/smoke
	./bin/goldbach-fast --from=1000000 --to=2000000 --threads=4 --segmentWidth=500000 --pLimit=10000 --fastStats=1 --out=results/smoke/range-smoke.json

scale-smoke: bin/scale-point-witness-gmp
	mkdir -p results/smoke
	rm -f results/smoke/scale-smoke.jsonl
	./bin/scale-point-witness-gmp --coefficient=2 --firstExponent=2 --lastExponent=5 --pLimit=10000 --workers=4 --primalityReps=25 --checkpoint=results/smoke/scale-smoke.jsonl

survivor-smoke: bin/survivor-factor-scan-gmp
	./bin/survivor-factor-scan-gmp --n=2000000 --pLimit=1000 --smallLimit=1000 --factorLimit=1000

qline-smoke: bin/qfree-no-skip-qline-large-gmp
	mkdir -p results/smoke
	./bin/qfree-no-skip-qline-large-gmp --coefficient=2 --base=10 --exponent=6 --minOffset=2 --maxOffset=2000 --centerOffset=500 --aLimit=500 --pLimit=5000 --qFilterLimit=1000 --smallLimit=1000 --workers=4 --baselineSampleStride=0 --progressEveryLines=0 --summary=results/smoke/qline-summary.tsv --fallbackOutput=results/smoke/qline-fallback.tsv

qline-scout-smoke: bin/qfree-qline-scout-gmp
	mkdir -p results/smoke
	./bin/qfree-qline-scout-gmp --coefficient=2 --base=10 --exponent=6 --minOffset=-20 --maxOffset=20 --aLimit=100 --qFilterLimit=1000 --top=10 --qTestTop=10 --output=results/smoke/qline-scout.tsv

wall-smoke: bin/wall-spectrum-scorer-gmp
	mkdir -p results/smoke
	./bin/wall-spectrum-scorer-gmp --n=2000000 --pLimit=1000 --wallLimits=31,101,1000 > results/smoke/wall-spectrum.tsv

paper:
	cd paper && tectonic goldbach-witness-verifier.tex

clean:
	rm -rf bin results/smoke
