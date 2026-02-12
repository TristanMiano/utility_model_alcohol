#!/usr/bin/env python3
"""
Monte Carlo simulation of alcohol welfare over a lifetime horizon.

This version includes:
  - POSITIVE utilons module (already in your script): daily LS uplift -> yearly utilons
  - NEGATIVE utilons module (from your Deep Research report): injuries, hangover,
    chronic health burden proxies, and an AUD Markov model.

Key merge-friendly design:
  - A single exposure knob in SCRIPT: drinks_per_day (mean standard drinks/day).
  - Daily drink-count variability is controlled by SCRIPT["day_count_model"].
  - Both modules consume the same implied daily distribution of drinks_today.

Units:
  - 1 utilon = +1 point on a 0–10 life satisfaction scale sustained for 1 person for 1 year.
  - We model per-day LS changes and convert to utilons by aggregating over a year, then discount.

Performance:
  - No day-by-day simulation over 60 years.
  - We use an analytic PMF over drinks_today (0..cap) and compute expected annual effects.
  - Lifetime simulation is annual timestep only, so 50k runs is feasible.

NOTE:
  - Several chronic channels require a "baseline_daly_rate" to translate RR into burden.
    The DR report calls this out; we include these as explicit discrete calibration menus.
"""

from __future__ import annotations

import math
from array import array
from dataclasses import dataclass
from typing import Dict, Iterable, List, Tuple

import random as _random
import argparse
import faulthandler
import sys


faulthandler.enable()


def make_histogram(values, bins: int, title: str, xlabel: str, ylabel: str) -> None:
    """Plot histogram if matplotlib is available; otherwise print a warning and continue."""
    import importlib.util

    if importlib.util.find_spec("matplotlib") is None:
        print("[warn] matplotlib is not installed; skipping histogram plot.")
        return

    try:
        import matplotlib.pyplot as plt
        plt.figure()
        plt.hist(values, bins=bins)
        plt.title(title)
        plt.xlabel(xlabel)
        plt.ylabel(ylabel)
        plt.tight_layout()
        plt.show()
    except Exception as exc:
        print(f"[warn] matplotlib plotting failed; skipping histogram plot. ({exc})", file=sys.stderr)


# =============================================================================
# 1) SCRIPT PARAMETERS (chosen by the user; NOT sampled)
# =============================================================================

SCRIPT: Dict[str, object] = {
    # Monte Carlo draws (one run = one simulated person's lifetime)
    "num_runs": 100,
    "seed": 12345,

    # Horizon
    "years": 60,
    "days_per_year": 365,

    # Exposure: key shared input for positives + negatives
    "drinks_per_day": 1.5,

    # Daily count model for drinks_today. (Must align with the negative module.)
    #   - "constant": drinks_today is fixed at round(drinks_per_day)
    #   - "poisson": drinks_today ~ Poisson(drinks_per_day) truncated at cap
    #   - "two_point": drinks_today = 0 w.p. p_zero, else = high_drinks chosen to preserve mean
    "day_count_model": "poisson",   # "constant" | "poisson" | "two_point"
    "two_point_p_zero": 0.5,        # only used if day_count_model == "two_point"
    "two_point_high_drinks": 6,     # only used if day_count_model == "two_point"
    "max_drinks_cap": 12,

    # Discounting (continuous exp(-r*t), per report)
    "discount_rate_annual": 0.03,

    # Outputs
    "hist_bins": 70,
    "hist_title": "Discounted Lifetime Utilons (Net = Positive - Negative)",
    "quantiles": [1, 5, 10, 25, 50, 75, 90, 95, 99],
}


# Dedicated RNG instance to avoid stdlib random.gauss cache weirdness / shadowing.
# --- replace your RNG line with a tiny helper so we can reseed during sweeps ---
def reseed(seed: int | None) -> None:
    global RNG
    RNG = _random.Random(seed) if seed is not None else _random.Random()

reseed(int(SCRIPT["seed"]) if SCRIPT["seed"] is not None else None)


# =============================================================================
# 2) UTILITY-MODEL PARAMETER MENUS (sampled discretely, uniformly)
# =============================================================================
# These are *model* parameters and should be sampled from discrete lists per run.

# ----- POSITIVE MODULE MENUS (from your positive model, slightly tightened) -----
POS_MODEL = {
    # Fraction of days that are "social context" (social positives are stronger)
    "p_social_day": [0.1, 0.2, 0.35, 0.5],

    "baseline_stress": [0.2, 0.4, 0.6, 0.8],
    "baseline_sociability": [0.2, 0.4, 0.6, 0.8],
    "social_setting_quality": [0.3, 0.5, 0.7, 0.9],

    "responsiveness": [0.6, 0.8, 1.0, 1.2, 1.4],
    "saturation_rate": [0.4, 0.7, 1.0, 1.3],

    # Cross-walk / scale: session_score -> LS uplift for that day
    "ls_per_session_score": [0.15, 0.25, 0.35, 0.50],

    "w_enjoyment": [0.8, 1.0, 1.2, 1.4],
    "w_relaxation": [0.6, 0.8, 1.0, 1.2],
    "w_social": [0.5, 0.8, 1.1, 1.4],
    "w_mood": [0.3, 0.5, 0.7, 0.9],

    # We do expected-value simulation, so noise averages out; keep for possible future daily mode.
    "noise_sigma_session_score": [0.15, 0.25, 0.35, 0.5],
    "max_daily_ls_uplift": [1.0, 1.5, 2.0],
}

# ----- NEGATIVE MODULE MENUS (from your DR report) -----
NEG_MODEL = {
    # Global conversion / exposure definitions
    "discount_rate_choices": [0.0, 0.015, 0.03, 0.05],
    "grams_ethanol_per_standard_drink_choices": [10, 14],
    "qaly_to_wellby_factor_choices": [5, 6, 7, 8],
    "causal_weight_choices": [0.25, 0.5, 0.75, 1.0],

    # Binge definitions (NIAAA style thresholds)
    "binge_threshold_drinks_choices": [4, 5],
    "high_intensity_multiplier_choices": [2, 3],

    # Chronic lag smoothing (half-life in years)
    "latency_half_life_years_choices": [2, 5, 10],
    "cancer_latency_half_life_years_choices": [5, 10, 15],
    "cirrhosis_latency_half_life_years_choices": [3, 5, 10],

    # Acute injury / violence RR anchors
    "traffic_injury_rr_per_10g_choices": [1.18, 1.24, 1.30],
    "nontraffic_injury_rr_per_10g_choices": [1.26, 1.30, 1.34],
    "intentional_injury_rr_per_drink_choices": [1.25, 1.38, 1.50],

    # Baseline event probabilities (calibration knobs)
    "injury_baseline_prob_per_drinking_day_choices": [1e-6, 5e-6, 2e-5, 1e-4],
    "violence_baseline_prob_per_binge_day_choices": [1e-7, 5e-7, 2e-6, 1e-5],

    # DALY/event severities (coarse)
    "injury_daly_per_nonfatal_event_choices": [0.005, 0.02, 0.05],
    "injury_case_fatality_choices": [0.002, 0.005, 0.01],
    "injury_daly_per_fatal_event_choices": [20, 30, 40],

    # Traffic externalities
    "traffic_injury_externality_multiplier_choices": [0.5, 1.0, 1.5],

    # Poisoning
    "poisoning_prob_per_high_intensity_day_choices": [1e-8, 1e-7, 1e-6],
    "poisoning_case_fatality_choices": [0.005, 0.01, 0.02],
    "poisoning_daly_nonfatal_choices": [0.01, 0.05, 0.2],

    # Hangover (direct wellbeing)
    "hangover_prob_given_binge_choices": [0.3, 0.5, 0.7, 0.9],
    "hangover_ls_loss_per_day_choices": [0.05, 0.1, 0.2, 0.4],
    "hangover_duration_days_choices": [1, 2],

    # Cancer slopes
    "breast_cancer_rr_per_10g_day_choices": [1.05, 1.07, 1.10],
    "all_cancer_rr_per_10g_day_choices": [1.02, 1.04, 1.06],
    "cancer_causal_weight_choices": [0.75, 1.0],

    # Cirrhosis RR anchors at 25/50/100g (mortality RR; we use as burden multiplier)
    "cirrhosis_rr_mortality_at_25g_choices": [2.0, 2.65, 3.2],
    "cirrhosis_rr_mortality_at_50g_choices": [5.5, 6.83, 8.0],
    "cirrhosis_rr_mortality_at_100g_choices": [12.0, 16.38, 20.0],

    # Cardiovascular (AF)
    "af_rr_per_drink_day_choices": [1.03, 1.06, 1.08],

    # Optional IHD “protection” kept separable (default False)
    "include_ihd_protection_choices": [False, True],
    "ihd_protective_rr_nadir_choices": [0.85, 0.95, 1.0],
    "binge_negates_ihd_protection_choices": [True, False],

    # AUD Markov model
    "aud_onset_base_prob_per_year_choices": [0.002, 0.005, 0.01],
    "aud_remission_prob_per_year_choices": [0.08, 0.15, 0.25],
    "aud_relapse_prob_per_year_if_abstinent_choices": [0.02, 0.05, 0.10],
    "aud_relapse_multiplier_if_risk_drinking_choices": [3, 6, 10],

    # AUD disability weights (choose one list)
    "aud_disability_weight_choices": [0.123, 0.235, 0.366],

    # Mental health comorbidity add-on (as LS points/year while in AUD)
    "aud_depression_ls_addon_choices": [0.0, 0.2, 0.5, 1.0],
    "mental_health_causal_weight_choices": [0.25, 0.5, 0.75],

    # Chronic baseline DALY rates (REQUIRED calibration knobs)
    # These represent "baseline annual DALYs per person" for each outcome in the reference population.
    # Without these, RR curves can't become utilons. Keep coarse; calibrate later using GBD/CDC/target population.
    "baseline_daly_rate_all_cancer_choices": [0.001, 0.003, 0.006],
    "baseline_daly_rate_cirrhosis_choices": [0.0003, 0.001, 0.0025],
    "baseline_daly_rate_af_choices": [0.0005, 0.0015, 0.003],
    # Optional: baseline IHD DALY rate (only used if include_ihd_protection == True)
    "baseline_daly_rate_ihd_choices": [0.001, 0.003, 0.006],
}


# =============================================================================
# 3) HELPERS: sampling, PMF over drinks_today, discounting
# =============================================================================

def pick_uniform(seq: List[object]) -> object:
    return RNG.choice(seq)


def discount_factor_continuous(r_annual: float, t_years: float) -> float:
    return math.exp(-r_annual * t_years)


def drinks_pmf(mean_drinks_per_day: float) -> List[float]:
    """
    Returns pmf[d] = P(drinks_today == d) for d=0..cap based on SCRIPT day_count_model.
    Any tail beyond cap is accumulated into cap.
    """
    cap = int(SCRIPT["max_drinks_cap"])
    mode = str(SCRIPT["day_count_model"])

    pmf = [0.0] * (cap + 1)

    if mean_drinks_per_day <= 0:
        pmf[0] = 1.0
        return pmf

    if mode == "constant":
        d = int(round(mean_drinks_per_day))
        d = min(max(d, 0), cap)
        pmf[d] = 1.0
        return pmf

    if mode == "two_point":
        p0 = float(SCRIPT["two_point_p_zero"])
        hi = int(SCRIPT["two_point_high_drinks"])
        hi = min(max(hi, 0), cap)
        if hi == 0:
            pmf[0] = 1.0
            return pmf
        # preserve mean: E[D] = (1-p0)*hi = mean -> if incompatible, clamp by pushing probability into hi
        implied_mean = (1.0 - p0) * hi
        if implied_mean <= 0:
            pmf[0] = 1.0
            return pmf
        # If user’s mean differs, adjust p0 to match mean as best as possible:
        p0_adj = 1.0 - (mean_drinks_per_day / hi)
        p0_adj = min(max(p0_adj, 0.0), 1.0)
        pmf[0] = p0_adj
        pmf[hi] = 1.0 - p0_adj
        return pmf

    if mode == "poisson":
        lam = float(mean_drinks_per_day)
        p0 = math.exp(-lam)
        pmf[0] = p0
        p = p0
        for d in range(1, cap):
            p = p * lam / d
            pmf[d] = p
        pmf[cap] = max(0.0, 1.0 - sum(pmf[:-1]))
        return pmf

    raise ValueError(f"Unknown day_count_model: {mode}")


def prob_from_pmf(pmf: List[float], predicate) -> float:
    return sum(p for d, p in enumerate(pmf) if predicate(d))


def expect_from_pmf(pmf: List[float], f) -> float:
    return sum(p * f(d) for d, p in enumerate(pmf))


# =============================================================================
# 4) POSITIVE MODULE (expected annual positive utilons)
# =============================================================================

@dataclass(frozen=True)
class PosPerson:
    p_social_day: float
    baseline_stress: float
    baseline_sociability: float
    social_setting_quality: float
    responsiveness: float
    saturation_rate: float
    ls_per_session_score: float
    w_enjoyment: float
    w_relaxation: float
    w_social: float
    w_mood: float
    max_daily_ls_uplift: float


def sample_pos_person() -> PosPerson:
    return PosPerson(
        p_social_day=float(pick_uniform(POS_MODEL["p_social_day"])),
        baseline_stress=float(pick_uniform(POS_MODEL["baseline_stress"])),
        baseline_sociability=float(pick_uniform(POS_MODEL["baseline_sociability"])),
        social_setting_quality=float(pick_uniform(POS_MODEL["social_setting_quality"])),
        responsiveness=float(pick_uniform(POS_MODEL["responsiveness"])),
        saturation_rate=float(pick_uniform(POS_MODEL["saturation_rate"])),
        ls_per_session_score=float(pick_uniform(POS_MODEL["ls_per_session_score"])),
        w_enjoyment=float(pick_uniform(POS_MODEL["w_enjoyment"])),
        w_relaxation=float(pick_uniform(POS_MODEL["w_relaxation"])),
        w_social=float(pick_uniform(POS_MODEL["w_social"])),
        w_mood=float(pick_uniform(POS_MODEL["w_mood"])),
        max_daily_ls_uplift=float(pick_uniform(POS_MODEL["max_daily_ls_uplift"])),
    )


def saturating_gain(drinks: int, saturation_rate: float) -> float:
    if drinks <= 0:
        return 0.0
    return 1.0 - math.exp(-saturation_rate * drinks)


def daily_positive_ls_uplift_det(person: PosPerson, drinks_today: int, is_social_day: bool) -> float:
    """
    Deterministic (expected) daily LS uplift; no random noise (noise mean is 0 anyway).
    Clamped to [0, max_daily_ls_uplift].
    """
    gain = saturating_gain(drinks_today, person.saturation_rate)

    enjoyment = person.w_enjoyment * gain
    relaxation = person.w_relaxation * gain * (0.5 + 0.5 * person.baseline_stress)

    if is_social_day:
        social_mult = person.social_setting_quality * (1.2 - 0.6 * person.baseline_sociability)
        social = person.w_social * gain * social_mult
    else:
        social = 0.0

    mood = person.w_mood * gain

    session_score = person.responsiveness * (enjoyment + relaxation + social + mood)
    ls = person.ls_per_session_score * session_score

    if ls < 0:
        ls = 0.0
    if ls > person.max_daily_ls_uplift:
        ls = person.max_daily_ls_uplift
    return ls


def expected_daily_positive_ls(person: PosPerson, pmf: List[float]) -> float:
    pS = person.p_social_day

    def ls_for_d(d: int) -> float:
        ls_non = daily_positive_ls_uplift_det(person, d, False)
        ls_soc = daily_positive_ls_uplift_det(person, d, True)
        return (1.0 - pS) * ls_non + pS * ls_soc

    return expect_from_pmf(pmf, ls_for_d)


# =============================================================================
# 5) NEGATIVE MODULE (expected annual negative utilons)
# =============================================================================

@dataclass(frozen=True)
class NegParams:
    # global
    grams_per_drink: int
    qaly_to_wellby: float
    discount_rate: float

    # binge definitions
    binge_threshold: int
    high_intensity_multiplier: int

    # general observational-causality attenuation
    causal_weight: float

    # acute injury/violence
    rr10_traffic: float
    rr10_nontraffic: float
    rr_per_drink_intentional: float
    p0_injury_per_drinking_day: float
    p0_violence_per_binge_day: float
    daly_nonfatal_injury: float
    injury_case_fatality: float
    daly_fatal_injury: float
    traffic_externality_multiplier: float

    # poisoning
    p_poison_per_hi_day: float
    poison_case_fatality: float
    poison_daly_nonfatal: float

    # hangover
    p_hangover_given_binge: float
    hangover_ls_loss_per_day: float
    hangover_duration_days: int

    # chronic smoothing half-lives
    half_life_chronic: float
    half_life_cancer: float
    half_life_cirrhosis: float

    # cancer
    rr10_all_cancer: float
    cancer_causal_weight: float
    baseline_daly_all_cancer: float

    # cirrhosis (piecewise anchors)
    rr_cirr_25: float
    rr_cirr_50: float
    rr_cirr_100: float
    baseline_daly_cirrhosis: float

    # cardiovascular AF
    rr_af_per_drink: float
    baseline_daly_af: float

    # optional IHD protection (kept separable)
    include_ihd_protection: bool
    ihd_rr_nadir: float
    binge_negates_ihd: bool
    baseline_daly_ihd: float

    # AUD Markov
    aud_onset_base: float
    aud_remission: float
    aud_relapse_base: float
    aud_relapse_mult_if_risk: float
    aud_disability_weight: float
    aud_depression_ls_addon: float
    mental_health_causal_weight: float


def sample_neg_params() -> NegParams:
    return NegParams(
        grams_per_drink=int(pick_uniform(NEG_MODEL["grams_ethanol_per_standard_drink_choices"])),
        qaly_to_wellby=float(pick_uniform(NEG_MODEL["qaly_to_wellby_factor_choices"])),
        discount_rate=float(pick_uniform(NEG_MODEL["discount_rate_choices"])),

        binge_threshold=int(pick_uniform(NEG_MODEL["binge_threshold_drinks_choices"])),
        high_intensity_multiplier=int(pick_uniform(NEG_MODEL["high_intensity_multiplier_choices"])),

        causal_weight=float(pick_uniform(NEG_MODEL["causal_weight_choices"])),

        rr10_traffic=float(pick_uniform(NEG_MODEL["traffic_injury_rr_per_10g_choices"])),
        rr10_nontraffic=float(pick_uniform(NEG_MODEL["nontraffic_injury_rr_per_10g_choices"])),
        rr_per_drink_intentional=float(pick_uniform(NEG_MODEL["intentional_injury_rr_per_drink_choices"])),
        p0_injury_per_drinking_day=float(pick_uniform(NEG_MODEL["injury_baseline_prob_per_drinking_day_choices"])),
        p0_violence_per_binge_day=float(pick_uniform(NEG_MODEL["violence_baseline_prob_per_binge_day_choices"])),
        daly_nonfatal_injury=float(pick_uniform(NEG_MODEL["injury_daly_per_nonfatal_event_choices"])),
        injury_case_fatality=float(pick_uniform(NEG_MODEL["injury_case_fatality_choices"])),
        daly_fatal_injury=float(pick_uniform(NEG_MODEL["injury_daly_per_fatal_event_choices"])),
        traffic_externality_multiplier=float(pick_uniform(NEG_MODEL["traffic_injury_externality_multiplier_choices"])),

        p_poison_per_hi_day=float(pick_uniform(NEG_MODEL["poisoning_prob_per_high_intensity_day_choices"])),
        poison_case_fatality=float(pick_uniform(NEG_MODEL["poisoning_case_fatality_choices"])),
        poison_daly_nonfatal=float(pick_uniform(NEG_MODEL["poisoning_daly_nonfatal_choices"])),

        p_hangover_given_binge=float(pick_uniform(NEG_MODEL["hangover_prob_given_binge_choices"])),
        hangover_ls_loss_per_day=float(pick_uniform(NEG_MODEL["hangover_ls_loss_per_day_choices"])),
        hangover_duration_days=int(pick_uniform(NEG_MODEL["hangover_duration_days_choices"])),

        half_life_chronic=float(pick_uniform(NEG_MODEL["latency_half_life_years_choices"])),
        half_life_cancer=float(pick_uniform(NEG_MODEL["cancer_latency_half_life_years_choices"])),
        half_life_cirrhosis=float(pick_uniform(NEG_MODEL["cirrhosis_latency_half_life_years_choices"])),

        rr10_all_cancer=float(pick_uniform(NEG_MODEL["all_cancer_rr_per_10g_day_choices"])),
        cancer_causal_weight=float(pick_uniform(NEG_MODEL["cancer_causal_weight_choices"])),
        baseline_daly_all_cancer=float(pick_uniform(NEG_MODEL["baseline_daly_rate_all_cancer_choices"])),

        rr_cirr_25=float(pick_uniform(NEG_MODEL["cirrhosis_rr_mortality_at_25g_choices"])),
        rr_cirr_50=float(pick_uniform(NEG_MODEL["cirrhosis_rr_mortality_at_50g_choices"])),
        rr_cirr_100=float(pick_uniform(NEG_MODEL["cirrhosis_rr_mortality_at_100g_choices"])),
        baseline_daly_cirrhosis=float(pick_uniform(NEG_MODEL["baseline_daly_rate_cirrhosis_choices"])),

        rr_af_per_drink=float(pick_uniform(NEG_MODEL["af_rr_per_drink_day_choices"])),
        baseline_daly_af=float(pick_uniform(NEG_MODEL["baseline_daly_rate_af_choices"])),

        include_ihd_protection=bool(pick_uniform(NEG_MODEL["include_ihd_protection_choices"])),
        ihd_rr_nadir=float(pick_uniform(NEG_MODEL["ihd_protective_rr_nadir_choices"])),
        binge_negates_ihd=bool(pick_uniform(NEG_MODEL["binge_negates_ihd_protection_choices"])),
        baseline_daly_ihd=float(pick_uniform(NEG_MODEL["baseline_daly_rate_ihd_choices"])),

        aud_onset_base=float(pick_uniform(NEG_MODEL["aud_onset_base_prob_per_year_choices"])),
        aud_remission=float(pick_uniform(NEG_MODEL["aud_remission_prob_per_year_choices"])),
        aud_relapse_base=float(pick_uniform(NEG_MODEL["aud_relapse_prob_per_year_if_abstinent_choices"])),
        aud_relapse_mult_if_risk=float(pick_uniform(NEG_MODEL["aud_relapse_multiplier_if_risk_drinking_choices"])),
        aud_disability_weight=float(pick_uniform(NEG_MODEL["aud_disability_weight_choices"])),
        aud_depression_ls_addon=float(pick_uniform(NEG_MODEL["aud_depression_ls_addon_choices"])),
        mental_health_causal_weight=float(pick_uniform(NEG_MODEL["mental_health_causal_weight_choices"])),
    )


def piecewise_log_rr(g_per_day: float, rr25: float, rr50: float, rr100: float) -> float:
    """
    Piecewise log-linear interpolation of RR vs grams/day anchored at 25, 50, 100g.
    For g<25: interpolate from (0, RR=1) to (25, rr25) in log space.
    For 25-50: rr25->rr50
    For 50-100: rr50->rr100
    For >100: extrapolate slope from 50-100.
    """
    if g_per_day <= 0:
        return 1.0

    def lerp_log(x, x0, x1, y0, y1):
        # log-linear interpolation
        if x1 == x0:
            return y1
        t = (x - x0) / (x1 - x0)
        return math.exp(math.log(y0) * (1 - t) + math.log(y1) * t)

    if g_per_day < 25:
        return lerp_log(g_per_day, 0.0, 25.0, 1.0, rr25)
    if g_per_day < 50:
        return lerp_log(g_per_day, 25.0, 50.0, rr25, rr50)
    if g_per_day < 100:
        return lerp_log(g_per_day, 50.0, 100.0, rr50, rr100)

    # extrapolate beyond 100 using slope from 50->100 in log space
    slope = (math.log(rr100) - math.log(rr50)) / (100.0 - 50.0)
    return math.exp(math.log(rr100) + slope * (g_per_day - 100.0))


def rr_from_rr10(rr10: float, grams_per_day: float) -> float:
    """RR(g) = rr10^(g/10)."""
    return rr10 ** (grams_per_day / 10.0)


def annual_negative_utilons_expected(pmf: List[float], neg: NegParams, ema_g: float, ema_cancer: float, ema_cirr: float
                                   ) -> Tuple[float, float, float, float, float]:
    """
    Compute expected annual negative utilons for the year, given:
      - pmf over drinks_today
      - current EMA exposures in grams/day for general chronic, cancer, cirrhosis

    Returns:
      (utilons_total, utilons_acute, utilons_hangover, utilons_chronic, utilons_ihd_protection_term)
    where the IHD term is returned separately (can be netted or not).
    """
    dpy = int(SCRIPT["days_per_year"])

    # --- derived probabilities from pmf ---
    binge_thresh = neg.binge_threshold
    hi_thresh = neg.high_intensity_multiplier * binge_thresh
    risk_thresh = binge_thresh  # risk drinking proxy: same threshold family (4/5)

    p_drinking_day = 1.0 - pmf[0]
    p_binge_day = sum(p for d, p in enumerate(pmf) if d >= binge_thresh)
    p_hi_day    = sum(p for d, p in enumerate(pmf) if d >= hi_thresh)
    p_risk_day  = sum(p for d, p in enumerate(pmf) if d >= risk_thresh)

    # expected grams/day on drinking days for RR functions (needs grams_today per draw)
    def grams_today(d: int) -> float:
        return d * neg.grams_per_drink

    # --- Acute injuries (traffic + non-traffic) ---
    # Expected RR conditional on drinks_today>0 (RR is only applied on drinking days)
    def rr_traffic_d(d: int) -> float:
        if d <= 0:
            return 0.0
        return rr_from_rr10(neg.rr10_traffic, grams_today(d))

    def rr_nontraffic_d(d: int) -> float:
        if d <= 0:
            return 0.0
        return rr_from_rr10(neg.rr10_nontraffic, grams_today(d))

    # We scale baseline p0 per drinking day by expected RR.
    # Note: This makes p0 a "calibration knob": baseline conditional on being a drinking day at low dose.
    exp_rr_traffic = expect_from_pmf(pmf, rr_traffic_d)
    exp_rr_nontraffic = expect_from_pmf(pmf, rr_nontraffic_d)

    # expected events/year
    traffic_events = dpy * neg.p0_injury_per_drinking_day * exp_rr_traffic
    nontraffic_events = dpy * neg.p0_injury_per_drinking_day * exp_rr_nontraffic

    # severity DALYs per event (mixture of fatal/nonfatal)
    daly_per_injury_event = (
        (1.0 - neg.injury_case_fatality) * neg.daly_nonfatal_injury
        + neg.injury_case_fatality * neg.daly_fatal_injury
    )

    # traffic externalities: multiply self-harm by (1 + externality_multiplier)
    traffic_dalys = traffic_events * daly_per_injury_event * (1.0 + neg.traffic_externality_multiplier)
    nontraffic_dalys = nontraffic_events * daly_per_injury_event

    # Violence: per-binge-day baseline scaled by per-drink RR anchor.
    # Use expected RR among binge days as a function of drinks (per-drink multiplicative).
    def rr_violence_d(d: int) -> float:
        if d < binge_thresh:
            return 0.0
        # RR per drink relative to 1 drink; we just apply rr^d as a steep toy approximation.
        return (neg.rr_per_drink_intentional ** d)

    exp_rr_violence = expect_from_pmf(pmf, rr_violence_d)
    violence_events = dpy * neg.p0_violence_per_binge_day * exp_rr_violence
    violence_dalys = violence_events * daly_per_injury_event  # reuse injury DALY as proxy

    # Poisoning: step on HI days
    poisoning_events = dpy * p_hi_day * neg.p_poison_per_hi_day
    daly_per_poison_event = (
        (1.0 - neg.poison_case_fatality) * neg.poison_daly_nonfatal
        + neg.poison_case_fatality * neg.daly_fatal_injury  # reuse "fatal DALY" as crude proxy
    )
    poisoning_dalys = poisoning_events * daly_per_poison_event

    acute_dalys = traffic_dalys + nontraffic_dalys + violence_dalys + poisoning_dalys
    acute_utilons = acute_dalys * neg.qaly_to_wellby * neg.causal_weight

    # --- Hangover (direct wellbeing LS loss) ---
    # expected hangover days/year: binge_days * p(hangover|binge)
    hangover_days = dpy * p_binge_day * neg.p_hangover_given_binge * neg.hangover_duration_days
    hangover_utilons = (hangover_days / dpy) * neg.hangover_ls_loss_per_day  # LS-days -> utilon-years

    # --- Chronic health burden proxies (DALY-rate style) ---
    # We use EMA grams/day (lagged exposure), map to RR, multiply baseline DALY rate.
    # (These baseline rates are intentionally coarse calibration knobs.)

    # Cancer aggregate
    rr_cancer = rr_from_rr10(neg.rr10_all_cancer, ema_cancer)
    cancer_dalys = neg.baseline_daly_all_cancer * max(0.0, rr_cancer - 1.0)
    cancer_utilons = cancer_dalys * neg.qaly_to_wellby * neg.cancer_causal_weight

    # Cirrhosis
    rr_cirr = piecewise_log_rr(ema_cirr, neg.rr_cirr_25, neg.rr_cirr_50, neg.rr_cirr_100)
    cirr_dalys = neg.baseline_daly_cirrhosis * max(0.0, rr_cirr - 1.0)
    cirr_utilons = cirr_dalys * neg.qaly_to_wellby * neg.causal_weight

    # AF (approx RR per drink/day)
    # Convert EMA grams/day to "drinks/day equivalent"
    drinks_equiv = ema_g / max(1e-9, neg.grams_per_drink)
    rr_af = neg.rr_af_per_drink ** drinks_equiv
    af_dalys = neg.baseline_daly_af * max(0.0, rr_af - 1.0)
    af_utilons = af_dalys * neg.qaly_to_wellby * neg.causal_weight

    chronic_utilons = cancer_utilons + cirr_utilons + af_utilons

    # Optional IHD protection (kept separable; not netted by default)
    ihd_term = 0.0
    if neg.include_ihd_protection:
        if neg.binge_negates_ihd and (p_binge_day > 0.0):
            ihd_rr = 1.0  # no protection
        else:
            ihd_rr = neg.ihd_rr_nadir
        # RR < 1 gives negative DALYs (a "benefit"); keep separate.
        ihd_dalys = neg.baseline_daly_ihd * (ihd_rr - 1.0)
        ihd_term = ihd_dalys * neg.qaly_to_wellby * neg.causal_weight  # likely negative number

    total_utilons = acute_utilons + hangover_utilons + chronic_utilons
    return total_utilons, acute_utilons, hangover_utilons, chronic_utilons, ihd_term


# =============================================================================
# 6) AUD Markov model (annual timestep; uses p_risk_day from PMF)
# =============================================================================

def aud_or_multiplier_from_risk_days_per_year(risk_days: float, days_per_year: int) -> float:
    """
    Map risk-drinking days/year into the NESARC-style frequency categories (coarse).
    Using the DR report categories as an ordinal ladder:
      never, <1_month, 1_3_month, 1_2_week, 3_4_week, daily
    and ORs:
      [1.0, 1.35, 2.10, 2.69, 5.27, 7.23]
    """
    # Convert to per-month / per-week intuition
    if risk_days <= 0.0:
        return 1.0

    per_month = risk_days / 12.0
    per_week = risk_days / 52.0

    ors = [1.0, 1.35, 2.10, 2.69, 5.27, 7.23]

    if per_month < 1.0:
        return ors[1]  # <1/month
    if per_month <= 3.0:
        return ors[2]  # 1-3/month
    if per_week <= 2.0:
        return ors[3]  # 1-2/week
    if per_week <= 4.0:
        return ors[4]  # 3-4/week
    return ors[5]      # daily/near-daily


def simulate_aud_lifetime_utilons(pmf: List[float], neg: NegParams) -> float:
    """
    Annual Markov model:
      states: 0=NoAUD, 1=AUD, 2=Remission
    Returns discounted lifetime utilon loss from AUD state burden + depression addon.
    """
    years = int(SCRIPT["years"])
    dpy = int(SCRIPT["days_per_year"])
    r = float(SCRIPT["discount_rate_annual"])  # use script discount for overall timeline

    # derive risk drinking days/year from pmf (risk threshold == binge threshold proxy)
    risk_thresh = neg.binge_threshold
    p_risk_day = prob_from_pmf(pmf, lambda d: d >= risk_thresh)
    risk_days = dpy * p_risk_day
    or_mult = aud_or_multiplier_from_risk_days_per_year(risk_days, dpy)

    state = 0
    total = 0.0

    for y in range(years):
        t = y + 0.5
        disc = discount_factor_continuous(r, t)

        # state-dependent LS loss per year (convert to utilons/year directly)
        if state == 1:  # AUD
            # Pathway A: disability -> WELLBY
            ls_loss = neg.aud_disability_weight * neg.qaly_to_wellby
            # Optional mental health add-on as direct LS points/year
            ls_loss += neg.aud_depression_ls_addon * neg.mental_health_causal_weight
            total += disc * ls_loss

        # transitions
        u = RNG.random()
        if state == 0:  # NoAUD -> AUD
            p_onset = neg.aud_onset_base * or_mult
            if u < p_onset:
                state = 1
        elif state == 1:  # AUD -> Remission
            if u < neg.aud_remission:
                state = 2
        else:  # Remission -> AUD (relapse)
            # if any risk drinking in year, apply multiplier
            relapse = neg.aud_relapse_base * (neg.aud_relapse_mult_if_risk if risk_days > 0 else 1.0)
            if u < relapse:
                state = 1

    # apply causal weight to the AUD channel (treat as partially causal if desired)
    return total * neg.causal_weight


# =============================================================================
# 7) ONE-RUN SIMULATION: lifetime positive, negative, net
# =============================================================================

def simulate_one_person() -> Dict[str, float]:
    years = int(SCRIPT["years"])
    dpy = int(SCRIPT["days_per_year"])
    r = float(SCRIPT["discount_rate_annual"])
    mean_drinks = float(SCRIPT["drinks_per_day"])

    pmf = drinks_pmf(mean_drinks)

    # sample person parameters
    pos_person = sample_pos_person()
    neg = sample_neg_params()

    # Positive: expected daily LS uplift -> utilons/year, then discounted sum
    daily_pos_ls = expected_daily_positive_ls(pos_person, pmf)

    pos_total = 0.0
    neg_total = 0.0
    neg_acute = 0.0
    neg_hang = 0.0
    neg_chronic = 0.0
    neg_aud = 0.0
    ihd_term_total = 0.0

    # Precompute constant mean grams/day for EMA inputs
    gbar = mean_drinks * neg.grams_per_drink

    # Initialize EMAs (grams/day)
    ema_g = 0.0
    ema_cancer = 0.0
    ema_cirr = 0.0

    # smoothing factors from half-lives
    def alpha_from_half_life(H: float) -> float:
        # ema_y = alpha*ema_{y-1} + (1-alpha)*gbar
        if H <= 0:
            return 0.0
        return math.exp(-math.log(2.0) / H)

    a_g = alpha_from_half_life(neg.half_life_chronic)
    a_ca = alpha_from_half_life(neg.half_life_cancer)
    a_ci = alpha_from_half_life(neg.half_life_cirrhosis)

    # AUD channel is lifetime-stateful; compute once and include (already discounted)
    aud_loss = simulate_aud_lifetime_utilons(pmf, neg)
    neg_aud = aud_loss

    for y in range(years):
        t = y + 0.5
        disc = discount_factor_continuous(r, t)

        # POS utilons/year: daily LS summed over year -> utilons = daily_ls (because LS-day * 365 / 365)
        pos_total += disc * daily_pos_ls

        # Update EMAs (constant exposure, smoothed)
        ema_g = a_g * ema_g + (1.0 - a_g) * gbar
        ema_cancer = a_ca * ema_cancer + (1.0 - a_ca) * gbar
        ema_cirr = a_ci * ema_cirr + (1.0 - a_ci) * gbar

        # NEG annual expected (exclude AUD here; added separately)
        neg_year, acute_u, hang_u, chronic_u, ihd_term = annual_negative_utilons_expected(
            pmf, neg, ema_g, ema_cancer, ema_cirr
        )

        neg_total += disc * neg_year
        neg_acute += disc * acute_u
        neg_hang += disc * hang_u
        neg_chronic += disc * chronic_u
        ihd_term_total += disc * ihd_term

    # Add AUD loss (already discounted; apply same timeline)
    neg_total += neg_aud

    # Keep IHD “protection” separable: you can decide whether to net it later.
    # For now: DO NOT net it into negatives by default.
    net = pos_total - neg_total

    return {
        "positive_utilons": pos_total,
        "negative_utilons": neg_total,
        "net_utilons": net,
        "neg_acute": neg_acute,
        "neg_hangover": neg_hang,
        "neg_chronic": neg_chronic,
        "neg_aud": neg_aud,
        "ihd_protection_term_separate": ihd_term_total,
    }


# =============================================================================
# 8) STATS + PLOTTING
# =============================================================================

def mean(xs: List[float]) -> float:
    return sum(xs) / len(xs) if xs else float("nan")


def percentile(xs_sorted: List[float], p: float) -> float:
    if not xs_sorted:
        return float("nan")
    if p <= 0:
        return xs_sorted[0]
    if p >= 100:
        return xs_sorted[-1]
    n = len(xs_sorted)
    idx = (p / 100.0) * (n - 1)
    lo = int(math.floor(idx))
    hi = int(math.ceil(idx))
    if lo == hi:
        return xs_sorted[lo]
    w = idx - lo
    return xs_sorted[lo] * (1 - w) + xs_sorted[hi] * w


def summarize(label: str, xs: List[float]) -> None:
    xs_sorted = sorted(xs)
    print(f"\n--- {label} ---")
    print(f"Mean: {mean(xs):.4f}")
    for q in SCRIPT["quantiles"]:
        print(f"  p{q:02d}: {percentile(xs_sorted, float(q)):.4f}")

# --- add below percentile(...) (or anywhere above main) ---
def median(xs: List[float]) -> float:
    xs_sorted = sorted(xs)
    return percentile(xs_sorted, 50.0)


def run_once_batch(include_keys: Iterable[str] | None = None) -> Dict[str, array]:
    """
    Run the current SCRIPT config for SCRIPT['num_runs'] and return selected distributions.

    Memory note:
    - Uses compact typed arrays instead of Python float lists.
    - Callers can request only the keys they need, which is especially useful for large sweeps.
    """
    num_runs = int(SCRIPT["num_runs"])
    all_keys = (
        "positive_utilons",
        "negative_utilons",
        "net_utilons",
        "neg_acute",
        "neg_hangover",
        "neg_chronic",
        "neg_aud",
        "ihd_protection_term_separate",
    )
    selected_keys = tuple(include_keys) if include_keys is not None else all_keys
    results = {
        key: array("d") for key in selected_keys
    }

    for _ in range(num_runs):
        out = simulate_one_person()
        for k in selected_keys:
            results[k].append(out[k])

    return results

# --- replace main() with this version (keeps original behavior unless you pass --sweep) ---
def main() -> None:
    parser = argparse.ArgumentParser(description="Monte Carlo alcohol welfare simulation")
    parser.add_argument("--drinks-per-day", type=float, default=None,
                        help="Override SCRIPT['drinks_per_day']")
    parser.add_argument("--runs", type=int, default=None,
                        help="Override SCRIPT['num_runs']")
    parser.add_argument("--seed", type=int, default=None,
                        help="Override SCRIPT['seed']")

    # Sweep mode
    parser.add_argument("--sweep", action="store_true",
                        help="Sweep drinks/day and histogram the median net utilons across sweep points")
    parser.add_argument("--sweep-min", type=float, default=0.0, help="Min drinks/day for sweep")
    parser.add_argument("--sweep-max", type=float, default=8.0, help="Max drinks/day for sweep")
    parser.add_argument("--sweep-step", type=float, default=0.25, help="Step size for sweep")
    parser.add_argument("--runs-per-point", type=int, default=None,
                        help="Runs per sweep point (defaults to --runs or SCRIPT['num_runs'])")
    parser.add_argument("--plot-sweep-hist", action="store_true",
                        help="In --sweep mode, show histogram of medians after sweep completes")

    args = parser.parse_args()

    # Apply basic overrides
    if args.drinks_per_day is not None:
        SCRIPT["drinks_per_day"] = float(args.drinks_per_day)
    if args.runs is not None:
        SCRIPT["num_runs"] = int(args.runs)
    if args.seed is not None:
        SCRIPT["seed"] = int(args.seed)

    # ----------------------------
    # Sweep mode
    # ----------------------------
    if args.sweep:
        runs_per_point = int(args.runs_per_point) if args.runs_per_point is not None else int(SCRIPT["num_runs"])
        base_seed = int(SCRIPT["seed"]) if SCRIPT["seed"] is not None else 12345

        # build grid (inclusive of max with float tolerance)
        dmin, dmax, step = float(args.sweep_min), float(args.sweep_max), float(args.sweep_step)
        grid = []
        x = dmin
        while x <= dmax + 1e-12:
            grid.append(round(x, 10))
            x += step

        medians = []
        pairs = []  # (drinks, median_net)
        print("=== Sweep: median(net utilons) by drinks/day ===", flush=True)
        print(
            f"Grid points: {len(grid)} | Runs/point: {runs_per_point:,} | day_count_model={SCRIPT['day_count_model']}",
            flush=True,
        )

        # run sweep
        for i, d in enumerate(grid):
            SCRIPT["drinks_per_day"] = float(d)
            SCRIPT["num_runs"] = runs_per_point

            # reseed per point for reproducibility
            reseed(base_seed + i)

            # Sweep only needs net utilons for median, so avoid storing 7 unused channels.
            results = run_once_batch(include_keys=("net_utilons",))
            m = median(results["net_utilons"])
            medians.append(m)
            pairs.append((d, m))
            print(f"  drinks/day={d:>5.2f}  median_net={m:>10.4f}", flush=True)

        # find best by median
        best_d, best_m = max(pairs, key=lambda t: t[1])
        print(f"\nBest (by median net utilons): drinks/day={best_d:.2f}  median_net={best_m:.4f}", flush=True)

        # histogram of medians across drinks/day values
        if args.plot_sweep_hist:
            make_histogram(
                medians,
                bins=int(SCRIPT["hist_bins"]),
                title="Histogram of median(net utilons) across drinks/day sweep points",
                xlabel="Median net utilons (per drinks/day value)",
                ylabel="Frequency",
            )
        return

    # ----------------------------
    # Original single-point mode
    # ----------------------------
    reseed(int(SCRIPT["seed"]) if SCRIPT["seed"] is not None else None)

    results = run_once_batch()

    print("=== Lifetime Utilon Simulation (Positive + Negative) ===")
    print(f"Runs: {int(SCRIPT['num_runs']):,}")
    print(f"Seed: {SCRIPT['seed']}")
    print(f"Horizon: {SCRIPT['years']} years")
    print(f"Discount rate (script): {SCRIPT['discount_rate_annual']:.3%} (continuous exp(-r*t))")
    print(f"Exposure: drinks_per_day = {SCRIPT['drinks_per_day']} using day_count_model={SCRIPT['day_count_model']}")

    summarize("Positive utilons (discounted lifetime)", results["positive_utilons"])
    summarize("Negative utilons (discounted lifetime)", results["negative_utilons"])
    summarize("Net utilons = Positive - Negative (discounted lifetime)", results["net_utilons"])

    summarize("Negative breakdown: acute", results["neg_acute"])
    summarize("Negative breakdown: hangover", results["neg_hangover"])
    summarize("Negative breakdown: chronic health proxies", results["neg_chronic"])
    summarize("Negative breakdown: AUD Markov", results["neg_aud"])
    summarize("IHD protection term (separate; not netted by default)", results["ihd_protection_term_separate"])

    make_histogram(
        results["net_utilons"],
        bins=int(SCRIPT["hist_bins"]),
        title=str(SCRIPT["hist_title"]),
        xlabel="Discounted lifetime utilons (net)",
        ylabel="Frequency",
    )


if __name__ == "__main__":
    main()
