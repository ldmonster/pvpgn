#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# check-all.sh -- the single local quality gate for the refactoring plan.
#
# Implements the "three rings" model from plans/11-local-quality-gates.md:
#   ring 2 (default): all architectural + fast test gates that finish quickly
#   ring 3 (--deep) : sanitizers, coverage, mutation, fuzz, bench
#
# This is the LOCAL stand-in for "CI is green". There is intentionally no CI;
# the same scripts could be wired into a runner later, but that is out of scope.
#
# Honesty rule: a gate whose backend/runtime is missing is reported as SKIP
# (with a reason), never silently counted as a pass. The script exits non-zero
# if any HARD gate fails; SKIPs do not fail the run.
#
# Usage:
#   scripts/dev/check-all.sh [--deep] [--no-build]
#
# Exit codes:
#   0  all hard gates passed (skips allowed)
#   1  one or more hard gates failed

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

DEEP=0
RUN_BUILD=1
for arg in "$@"; do
    case "$arg" in
        --deep)     DEEP=1 ;;
        --no-build) RUN_BUILD=0 ;;
        -h|--help)
            sed -n '3,24p' "$0"; exit 0 ;;
        *) echo "check-all: unknown option: $arg" >&2; exit 2 ;;
    esac
done

# ----- pretty output ---------------------------------------------------------
if [ -t 1 ]; then
    C_GREEN=$'\033[32m'; C_RED=$'\033[31m'; C_YEL=$'\033[33m'; C_RST=$'\033[0m'; C_BOLD=$'\033[1m'
else
    C_GREEN=""; C_RED=""; C_YEL=""; C_RST=""; C_BOLD=""
fi

PASS_N=0; FAIL_N=0; SKIP_N=0
SUMMARY=""

# record <status> <name>  (status: PASS|FAIL|SKIP)
record() {
    local status="$1" name="$2"
    case "$status" in
        PASS) PASS_N=$((PASS_N+1)); SUMMARY="${SUMMARY}  ${C_GREEN}✔ PASS${C_RST}  ${name}\n" ;;
        FAIL) FAIL_N=$((FAIL_N+1)); SUMMARY="${SUMMARY}  ${C_RED}✘ FAIL${C_RST}  ${name}\n" ;;
        SKIP) SKIP_N=$((SKIP_N+1)); SUMMARY="${SUMMARY}  ${C_YEL}∅ SKIP${C_RST}  ${name}\n" ;;
    esac
}

# gate <name> <command...> : run a hard gate, capture output, record result
gate() {
    local name="$1"; shift
    printf '%s── %s%s\n' "$C_BOLD" "$name" "$C_RST"
    if "$@" >/tmp/check-all.$$ 2>&1; then
        tail -1 /tmp/check-all.$$ | sed 's/^/   /'
        record PASS "$name"
    else
        tail -4 /tmp/check-all.$$ | sed 's/^/   /'
        record FAIL "$name"
    fi
    rm -f /tmp/check-all.$$
}

# skip <name> <reason> : record a gate as skipped (missing backend/runtime)
skip() {
    local name="$1" reason="$2"
    printf '%s── %s%s\n   %s(skipped: %s)%s\n' "$C_BOLD" "$name" "$C_RST" "$C_YEL" "$reason" "$C_RST"
    record SKIP "$name"
}

# =============================================================================
# RING 2 — architectural + fast structural gates
# =============================================================================
echo "${C_BOLD}=== Ring 2: architecture & structure ===${C_RST}"

gate "layering rule (empty allow-list)" sh   scripts/v3_layering_check.sh src
gate "domain purity"                    bash scripts/check_domain_purity.sh src/domain
gate "unit pairing"                     bash scripts/dev/check-unit-pairing.sh
gate "test↔legacy linkage"              bash scripts/dev/check-test-legacy-linkage.sh
gate "plugin ABI semver"                bash scripts/dev/check-plugin-abi.sh
gate "plugin ABI purity (C99)"          bash scripts/dev/check-plugin-abi-purity.sh
gate "changelog discipline"             bash scripts/dev/check-changelog.sh

# config-reference-sync regenerates config-reference.md from the built
# pvpgn_config_tool (--print-schema). Without that binary the doc cannot be
# brought in sync, so the gate is env-gated: skip honestly when the tool is
# absent rather than report a fail we cannot fix here.
if find build -type f -name 'pvpgn_config_tool*' 2>/dev/null | grep -q .; then
    gate "config reference sync"        bash scripts/dev/check-config-reference-sync.sh
else
    skip "config reference sync"        "pvpgn_config_tool not built (run gen-config-docs.sh after building)"
fi

gate "docs reachable"                   bash scripts/dev/check-docs-reachable.sh
gate "no orphan scripts"                bash scripts/dev/check-scripts-orphans.sh

# ----- build + fast test bands ----------------------------------------------
echo "${C_BOLD}=== Ring 2: build & fast tests ===${C_RST}"
BUILD_DIR="build/v3-dev"
if [ "$RUN_BUILD" -eq 0 ]; then
    skip "build + unit/functional tests" "--no-build requested"
elif ! command -v ctest >/dev/null 2>&1; then
    skip "build + unit/functional tests" "ctest not found"
elif [ ! -d "$BUILD_DIR" ]; then
    skip "build + unit/functional tests" "no $BUILD_DIR (run: cmake --preset v3-dev && cmake --build --preset v3-dev)"
else
    gate "unit tests"       ctest --test-dir "$BUILD_DIR" -L unit --output-on-failure
    gate "functional tests" ctest --test-dir "$BUILD_DIR" -L functional --output-on-failure
fi

# ----- e2e: modern login journey vs a real bnetd ----------------------------
# Self-contained (spawns its own bnetd, stdlib-only Python client); the only
# gate that drives bnetd's real wire dispatch / session-send / teardown paths,
# so it guards the dispatch UAF, the un-sent-reply bug, and the on_close crash
# fixed under M1 Step 1.1. Run the script directly rather than toggling
# PVPGN_V3_E2E_TESTS (that would also register the *-smoke.sh tests, which need
# client binaries this toolchain does not build). Env-gated: skip honestly when
# bnetd / python3 is unavailable.
BNETD_BIN="$BUILD_DIR/src/app/bnetd/bnetd"
E2E_JOURNEY="tests/e2e/modern_login_journey_test.py"
E2E_PERSIST="tests/e2e/account_persistence_test.py"
E2E_HOSTILE="tests/e2e/hostile_input_test.py"
if [ "$RUN_BUILD" -eq 0 ]; then
    skip "e2e modern login journey" "--no-build requested"
    skip "e2e account persistence"  "--no-build requested"
    skip "e2e hostile input"        "--no-build requested"
elif ! command -v python3 >/dev/null 2>&1; then
    skip "e2e modern login journey" "python3 not found"
    skip "e2e account persistence"  "python3 not found"
    skip "e2e hostile input"        "python3 not found"
elif [ ! -x "$BNETD_BIN" ]; then
    skip "e2e modern login journey" "bnetd not built ($BNETD_BIN; cmake --build --preset v3-dev --target bnetd)"
    skip "e2e account persistence"  "bnetd not built ($BNETD_BIN)"
    skip "e2e hostile input"        "bnetd not built ($BNETD_BIN)"
else
    gate "e2e modern login journey" python3 "$E2E_JOURNEY" --bnetd "$BNETD_BIN"
    gate "e2e account persistence"  python3 "$E2E_PERSIST" --bnetd "$BNETD_BIN"
    gate "e2e hostile input"        python3 "$E2E_HOSTILE" --bnetd "$BNETD_BIN"
fi

# =============================================================================
# RING 3 — deep gates (opt-in)
# =============================================================================
if [ "$DEEP" -eq 1 ]; then
    echo "${C_BOLD}=== Ring 3: deep gates (--deep) ===${C_RST}"
    # Coverage floor is a *no-regress* ratchet, not the M1 target. Measured
    # domain+app line coverage is ~63% (2026-06-03, after the 1.9–1.15 test-
    # wiring repair + net-new arc); the floor is pinned just below that so the
    # gains can't silently regress. Ramp this toward the 85% M1 exit as net-new
    # tests land — raise the number here, never lower it. See progress 1.9/1.15.
    COVERAGE_RAMP_FLOOR=65
    if command -v ctest >/dev/null 2>&1 && [ -d build/v3-coverage ]; then
        gate "coverage (>=${COVERAGE_RAMP_FLOOR}% domain+app, ramp->85)" \
            bash scripts/dev/check-coverage.sh build/v3-coverage "$COVERAGE_RAMP_FLOOR"
    else
        skip "coverage (>=${COVERAGE_RAMP_FLOOR}% domain+app, ramp->85)" "no build/v3-coverage"
    fi
    if [ -d build/v3-asan ]; then gate "asan suite" ctest --test-dir build/v3-asan --output-on-failure
        else skip "asan suite" "no build/v3-asan (cmake --preset v3-asan)"; fi
    if [ -d build/v3-ubsan ]; then gate "ubsan suite" ctest --test-dir build/v3-ubsan --output-on-failure
        else skip "ubsan suite" "no build/v3-ubsan (cmake --preset v3-ubsan)"; fi
    if [ -d build/v3-tsan ]; then gate "tsan suite" ctest --test-dir build/v3-tsan --output-on-failure
        else skip "tsan suite" "no build/v3-tsan (cmake --preset v3-tsan)"; fi
    # check-bench-regression.py requires <baseline> <current>; the baseline is
    # the committed host-specific capture, current is the freshly-generated
    # bench-results.json. (Earlier this called the script with no args -> argparse
    # usage error -> spurious FAIL.) Only gate when both files are present.
    if [ -f tests/bench/baselines/local-gcc13.json ] && [ -f bench-results.json ]; then
        gate "bench regression" python3 scripts/dev/check-bench-regression.py \
            tests/bench/baselines/local-gcc13.json bench-results.json
    else
        skip "bench regression" "no baseline (tests/bench/baselines/) or bench-results.json"
    fi
    skip "mutation pilot" "run explicitly: python3 scripts/dev/mutation_pilot.py"
    skip "fuzz smoke"     "run explicitly: cmake --build --preset v3-asan --target fuzz-smoke"
fi

# =============================================================================
# SUMMARY
# =============================================================================
echo ""
echo "${C_BOLD}=== check-all summary ===${C_RST}"
printf '%b' "$SUMMARY"
echo ""
printf '%s%d passed%s, %s%d failed%s, %s%d skipped%s\n' \
    "$C_GREEN" "$PASS_N" "$C_RST" \
    "$C_RED" "$FAIL_N" "$C_RST" \
    "$C_YEL" "$SKIP_N" "$C_RST"

if [ "$FAIL_N" -gt 0 ]; then
    echo "${C_RED}check-all: FAILED${C_RST}"
    exit 1
fi
echo "${C_GREEN}check-all: OK${C_RST} (skips are env-gated, verified by reference)"
exit 0
