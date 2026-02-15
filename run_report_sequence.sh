#!/usr/bin/env bash
set -euo pipefail

# End-to-end runbook for utility_model_alcohol report generation.
# Produces baseline outputs, sensitivity analyses, decision scenarios,
# histogram plots, and reproducibility checks.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="${ROOT_DIR}/out"
SIM_BIN="${ROOT_DIR}/sim_cpp"
PLOT_SCRIPT="${ROOT_DIR}/plot_histograms.py"

mkdir -p "${OUT_DIR}"

if ! command -v g++ >/dev/null 2>&1; then
  echo "Error: g++ is required but not found in PATH." >&2
  exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
  echo "Error: python3 is required but not found in PATH." >&2
  exit 1
fi

echo "[1/7] Building simulator"
g++ -O3 -std=c++17 "${ROOT_DIR}/sim.cpp" -o "${SIM_BIN}"
"${SIM_BIN}" --help > "${OUT_DIR}/help.txt"
"${SIM_BIN}" --list-choice-params > "${OUT_DIR}/choice_params.txt"

echo "[2/7] Baseline run + histogram"
"${SIM_BIN}" \
  --mode expected \
  --drinks-per-day 1.5 \
  --runs 20000 \
  --seed 123 \
  --hist-data-out "${OUT_DIR}/baseline_hist.csv" \
  | tee "${OUT_DIR}/baseline.txt"
python3 "${PLOT_SCRIPT}" "${OUT_DIR}/baseline_hist.csv" --out "${OUT_DIR}/baseline_hist.png"

echo "[3/7] Intake sweep"
"${SIM_BIN}" \
  --mode expected \
  --sweep \
  --sweep-min 0 \
  --sweep-max 6 \
  --sweep-step 0.25 \
  --runs-per-point 5000 \
  --seed 123 \
  | tee "${OUT_DIR}/sweep_drinks_per_day.txt"

echo "[4/7] Parameter sensitivity"
"${SIM_BIN}" --mode expected --drinks-per-day 1.5 --runs 20000 --seed 124 \
  --discount-rate-choices 0,0.03,0.05 \
  --qaly-to-wellby-factor-choices 5,7,8 \
  | tee "${OUT_DIR}/sens_discount_qaly2wellby.txt"

"${SIM_BIN}" --mode expected --drinks-per-day 1.5 --runs 20000 --seed 125 \
  --causal-weight-choices 0.25,0.5,0.75,1.0 \
  --cancer-causal-weight-choices 0.75,1.0 \
  --mental-health-causal-weight-choices 0.25,0.5,0.75 \
  | tee "${OUT_DIR}/sens_causality.txt"

"${SIM_BIN}" --mode expected --drinks-per-day 1.5 --runs 20000 --seed 126 \
  --traffic-injury-rr-per-10g-choices 1.18,1.24,1.30 \
  --nontraffic-injury-rr-per-10g-choices 1.26,1.30,1.34 \
  --poisoning-prob-per-high-intensity-day-choices 1e-6,3e-6,1e-5,3e-5 \
  --hangover-ls-loss-per-day-choices 0.05,0.1,0.2,0.4 \
  | tee "${OUT_DIR}/sens_acute_risk.txt"

"${SIM_BIN}" --mode expected --drinks-per-day 1.5 --runs 20000 --seed 127 \
  --all-cancer-rr-per-10g-day-choices 1.02,1.04,1.06 \
  --cirrhosis-rr-mortality-at-25g-choices 2.0,2.65,3.2 \
  --af-rr-per-drink-day-choices 1.03,1.06,1.08 \
  --include-ihd-protection-choices false,true \
  --binge-negates-ihd-protection-choices true,false \
  | tee "${OUT_DIR}/sens_chronic_ihd.txt"

echo "[5/7] Decision-relevant scenarios"
# Scenario: never drink and drive (traffic alcohol RR forced to 1.0, no externality multiplier)
"${SIM_BIN}" \
  --mode expected \
  --drinks-per-day 1.5 \
  --runs 20000 \
  --seed 200 \
  --traffic-injury-rr-per-10g-choices 1.0 \
  --traffic-injury-externality-multiplier-choices 0.0 \
  --hist-data-out "${OUT_DIR}/scenario_never_drink_drive_hist.csv" \
  | tee "${OUT_DIR}/scenario_never_drink_drive.txt"
python3 "${PLOT_SCRIPT}" "${OUT_DIR}/scenario_never_drink_drive_hist.csv" --out "${OUT_DIR}/scenario_never_drink_drive_hist.png"

# Scenario: effectively no binge episodes
"${SIM_BIN}" \
  --mode expected \
  --drinks-per-day 1.5 \
  --runs 20000 \
  --seed 201 \
  --binge-threshold-drinks-choices 100 \
  --hist-data-out "${OUT_DIR}/scenario_no_binge_hist.csv" \
  | tee "${OUT_DIR}/scenario_no_binge.txt"

# Scenario: abstinence
"${SIM_BIN}" \
  --mode expected \
  --drinks-per-day 0 \
  --runs 20000 \
  --seed 202 \
  --hist-data-out "${OUT_DIR}/scenario_abstinence_hist.csv" \
  | tee "${OUT_DIR}/scenario_abstinence.txt"

echo "[6/7] Seed robustness"
for s in 301 302 303 304 305; do
  "${SIM_BIN}" --mode expected --drinks-per-day 1.5 --runs 20000 --seed "${s}" \
    | tee "${OUT_DIR}/baseline_seed_${s}.txt"
done

echo "[7/7] Daily-mode sanity run"
"${SIM_BIN}" \
  --mode daily \
  --drinks-per-day 1.5 \
  --runs 3000 \
  --seed 400 \
  | tee "${OUT_DIR}/baseline_daily_mode.txt"

echo "Done. Outputs are in: ${OUT_DIR}"
