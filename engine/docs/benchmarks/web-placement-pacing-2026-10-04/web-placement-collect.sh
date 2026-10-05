set -eu
export NODE_PATH=/Users/jhonny/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules
export REPEATS=3 WARMUP=10 METRICS=0 MEMORY_PROBE=1
BUNDLE_DIR="$PWD/tmp/web-placement-synthetic" VARIANTS_FILE="$PWD/tmp/web-placement-variants.json" CASES=bunny30k SECONDS=30 node scripts/web/benchmark/run.cjs tmp/web-placement-bunnies
BUNDLE_DIR="$PWD/tmp/web-placement-synthetic" VARIANTS_FILE="$PWD/tmp/web-placement-geometry-variants.json" CASES=geometry1000 SECONDS=30 node scripts/web/benchmark/run.cjs tmp/web-placement-geometry
BUNDLE_DIR="$PWD/tmp/web-placement-gameplay" VARIANTS_FILE="$PWD/tmp/web-placement-variants.json" CASES_FILE="$PWD/tmp/web-placement-game-cases.json" SECONDS=120 node scripts/web/benchmark/run.cjs tmp/web-placement-game
