#!/usr/bin/env python3
from __future__ import annotations

import re
from collections import OrderedDict
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT_DIR = ROOT / "out"
OUTPUT_PATH = OUT_DIR / "summary_of_summaries.txt"

RUN_FILES = OrderedDict(
    [
        ("baseline", OUT_DIR / "baseline.txt"),
        ("no_drink_and_drive", OUT_DIR / "scenario_never_drink_drive.txt"),
        ("no_binge", OUT_DIR / "scenario_no_binge.txt"),
        ("abstinence", OUT_DIR / "scenario_abstinence.txt"),
        ("baseline_seed_301", OUT_DIR / "baseline_seed_301.txt"),
        ("baseline_seed_302", OUT_DIR / "baseline_seed_302.txt"),
        ("baseline_seed_303", OUT_DIR / "baseline_seed_303.txt"),
        ("baseline_seed_304", OUT_DIR / "baseline_seed_304.txt"),
        ("baseline_seed_305", OUT_DIR / "baseline_seed_305.txt"),
        ("baseline_daily_mode", OUT_DIR / "baseline_daily_mode.txt"),
    ]
)

SECTIONS = OrderedDict(
    [
        ("Positive utilons", "--- Positive utilons (discounted lifetime) ---"),
        ("Negative utilons", "--- Negative utilons (discounted lifetime) ---"),
        ("Net utilons", "--- Net utilons = Positive - Negative (discounted lifetime) ---"),
        ("Negative breakdown: acute", "--- Negative breakdown: acute ---"),
        ("Negative breakdown: hangover", "--- Negative breakdown: hangover ---"),
        (
            "Negative breakdown: chronic health proxies",
            "--- Negative breakdown: chronic health proxies ---",
        ),
        ("Negative breakdown: AUD Markov", "--- Negative breakdown: AUD Markov ---"),
        (
            "IHD protection term",
            "--- IHD protection term (separate; not netted by default) ---",
        ),
    ]
)

P_ROWS = ["p01", "p05", "p10", "p25", "p50", "p75", "p90", "p95", "p99"]
P_RE = re.compile(r"^\s*(p\d{2}):\s*(-?\d+\.\d+)")


def extract_section_percentiles(text: str, section_header: str) -> dict[str, str]:
    lines = text.splitlines()
    try:
        start = lines.index(section_header)
    except ValueError:
        return {}

    values: dict[str, str] = {}
    for line in lines[start + 1 :]:
        if line.startswith("--- ") or line.startswith("=== "):
            break
        m = P_RE.match(line)
        if m:
            values[m.group(1)] = m.group(2)
    return values


def render_table(title: str, data: dict[str, dict[str, dict[str, str]]]) -> str:
    cols = list(RUN_FILES.keys())
    lines = [f"## {title}"]
    lines.append("| percentile | " + " | ".join(cols) + " |")
    lines.append("|---|" + "|".join(["---" for _ in cols]) + "|")
    for p in P_ROWS:
        row = [p]
        for col in cols:
            row.append(data.get(col, {}).get(title, {}).get(p, "NA"))
        lines.append("| " + " | ".join(row) + " |")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    per_run: dict[str, dict[str, dict[str, str]]] = {}
    missing_runs: list[str] = []

    for run_name, path in RUN_FILES.items():
        if not path.exists():
            missing_runs.append(run_name)
            continue

        text = path.read_text(encoding="utf-8")
        sections: dict[str, dict[str, str]] = {}
        for section_name, section_header in SECTIONS.items():
            sections[section_name] = extract_section_percentiles(text, section_header)
        per_run[run_name] = sections

    out_lines = [
        "# Summary of Summaries",
        "",
        "This file aggregates p01..p99 percentile outputs across major runs.",
        "",
        "Runs included: " + ", ".join(per_run.keys()),
    ]
    if missing_runs:
        out_lines.append("Missing run files: " + ", ".join(missing_runs))
    out_lines.append("")

    for section_name in SECTIONS.keys():
        out_lines.append(render_table(section_name, per_run))

    OUTPUT_PATH.write_text("\n".join(out_lines), encoding="utf-8")


if __name__ == "__main__":
    main()
