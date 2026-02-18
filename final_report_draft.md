# When Is Drinking Alcohol Morally Worth It?

## Purpose and framing

This report builds a single, reproducible utility model for alcohol consumption that combines positive effects (benefits) and negative effects (harms) into a common welfare unit. The model is then evaluated via Monte Carlo simulation across scenarios, producing distributions of lifetime outcomes rather than single point estimates.

A central result is scenario-dependent: the **median net utility (p50) is negative in the baseline scenario but becomes positive when drinking and driving is removed**. See Figure 1 (baseline histograms) and Figure 2 (never drink-and-drive histograms).

---

## 1) The welfare unit and accounting identity

### 1.1 Utilons

A **utilon** is defined as:

> **1 utilon = a 1-point change on a 0–10 life satisfaction scale sustained for 1 person for 1 year.**

This is a WELLBY-style unit, enabling consistent aggregation across short-lived experiences (hours/days) and long-lived outcomes (years), including premature mortality and chronic disability when expressed as life-satisfaction-point-years.

### 1.2 Net utility

For any simulated life-course realization:

* **Positive utilons** (U_{+}) capture enjoyment, mood lift, and social benefits (modeled as life-satisfaction gains over time).
* **Negative utilons** (U_{-}) capture acute risks (injury/violence/poisoning), next-day impairment, chronic health burdens, and alcohol use disorder (AUD) state burdens (modeled as life-satisfaction losses over time).

The model reports:

[
U_{\text{net}} = U_{+} - U_{-}
]

### 1.3 Discounting

Utilities are discounted over time using a continuous discount rate (r):

[
U = \int_0^{T} e^{-rt},\Delta LS(t),dt
]

In implementation with daily or annual time steps, this becomes a discounted sum over time slices. Core sensitivity includes (r \in {0,,0.015,,0.03,,0.05}).

---

## 2) Exposure stream: how drinking is simulated

This section maps directly to `sim.cpp` implementation choices. The code uses a global script config with:

* 60-year horizon (`years = 60`),
* 365 days/year,
* baseline intake `drinks_per_day = 1.5`,
* day-count model default `poisson`, and
* simulation mode default `expected` with an alternative `daily` mode.

The report outputs shown elsewhere were mostly generated in `expected` mode for speed at large Monte Carlo sizes (20k runs), while the model selected for *mechanistic explanation* here is `daily` mode because it preserves event timing and state transitions day-by-day.

### 2.1 Standardization and daily draws

Alcohol exposure is parameterized in **grams of ethanol**, with “standard drinks” treated as a conversion convenience. Two conventions are supported:

* 10 g ethanol per drink
* 14 g ethanol per drink

Each simulation generates an exposure stream, commonly at a daily level:

* `drinks_today`: integer count of drinks for that day
* `g_today = drinks_today * grams_per_drink`

### 2.2 Day-count models (frequency and binge structure)

Daily drinks can be generated using one of:

* **Constant**: `drinks_today = drinks_per_day`
* **Poisson**: `drinks_today ~ Poisson(drinks_per_day)`
* **Two-point mixture**: `drinks_today = 0` with probability (p_0), otherwise a high value chosen to preserve the mean

This matters because many harms are highly nonlinear in dose and concentrate in **heavy episodic** days.

In `sim.cpp`, these are implemented in `sample_drinks_today(...)` and `drinks_pmf(...)`:

* `sample_drinks_today` is used when the simulator needs realized day-level trajectories (the `daily` mode path).
* `drinks_pmf` is used when the simulator needs expectations over dose without sampling every day-level benefit event (the `expected` mode path).

So both modes share the same exposure assumptions, but they differ in whether the model propagates full day-level randomness through each channel.

### 2.3 Binge and high-intensity markers

Two binary indicators are derived:

* `binge_today = 1[drinks_today >= binge_threshold]` with threshold in {4, 5}
* `high_intensity_today = 1[drinks_today >= high_intensity_multiplier * binge_threshold]` with multiplier in {2, 3}

These markers route exposure into acute-risk channels and the AUD risk process.

---

## 3) Positive utility module (benefits)

### 3.1 What is counted

The positive module expresses short-run and social effects as increments to life satisfaction over specified durations, then aggregates across episodes and time.

Typical benefit channels include:

* **Hedonic pleasure / in-the-moment mood elevation** (minutes–hours)
* **Social bonding and affiliation** (hours–days, potentially with short spillovers)
* **Sociability / reduced inhibition** (hours)
* **Expectancy-driven confidence effects** (hours)

### 3.2 Attribution structure (context vs expectancy vs ethanol)

When evidence supports it, benefits are decomposed conceptually into:

* **Context effect**: being in the social situation
* **Expectancy effect**: believing alcohol was consumed
* **Pharmacological effect**: ethanol beyond expectancy

The strictest “ethanol-attributable” benefits correspond to the pharmacological component (alcohol vs placebo). Broader “occasion-attributable” benefits include context dependence and substitution assumptions.

### 3.3 Generic conversion template

For a benefit channel (k):

[
U_{k} = N \times \Delta LS_{k} \times D_{k} \times A_{k}
]

where:

* (N): number of affected persons (often 1, but social spillovers can expand this)
* (\Delta LS_{k}): life satisfaction increment (0–10 scale)
* (D_{k}): duration in years (e.g., hours / 8760, days / 365)
* (A_{k}): attribution weight in ([0,1]) capturing substitution and causal tightness

All positive-channel parameters are sampled in Monte Carlo rather than treated as fixed.

---

## 4) Negative utility module (harms)

The negative module consumes the same exposure stream and produces discounted lifetime losses. Harms are organized to reduce double counting by separating acute events, next-day impairment, chronic health proxies, and AUD state burden.

### 4.1 Acute injury and accident risks

Acute injury probability rises steeply with ethanol dose. A common implementation uses a log-linear relative-risk form:

[
RR(g) = RR_{10}^{g/10}
]

Daily event probability (rare-event approximation with a conservative bound):

[
p(d) = 1 - (1-p_0)^{RR(g(d))}
]

Acute endpoints include:

* traffic injury (and associated externalities)
* non-traffic injury
* violence-related injury (victimization severe enough to require ED care)
* acute ethanol poisoning (modeled as concentrated in high-intensity days)

**Externalities:** traffic harm includes harm to non-drinkers (other road users/passengers), represented via an externality multiplier and/or victim-share parameters.

### 4.2 Next-day impairment (hangover)

Hangover is treated as a **direct life satisfaction loss** applied on the following day with probability tied to binge/high-intensity days:

* `P(hangover | binge)` sampled from a menu
* `LS loss per hangover day` sampled from a menu
* duration commonly 1–2 days

This channel is intentionally kept distinct from long-run earnings effects to avoid counting “productivity loss” twice.

### 4.3 Chronic health proxies (lagged exposure)

Chronic risks depend on longer-run exposure. A smoothed exposure measure can be used with half-life (H) years:

[
EMA_y = EMA_{y-1},e^{-\ln(2)/H} + \bar{g}_y,(1-e^{-\ln(2)/H})
]

Chronic burden is then modeled as a baseline DALY/QALY-rate multiplied by excess risk, then mapped into utilons using a QALY→WELLBY factor (sampled):

[
U_{\text{health}} = \text{DALYs lost} \times k_{\text{QALY}\rightarrow\text{WELLBY}}
]

### 4.4 AUD Markov process (stateful harm)

AUD is modeled as an annual Markov process with states:

* NoAUD
* AUD
* Remission

Transitions depend on risk-drinking frequency derived from the daily exposure stream (risk-day counts), with remission/relapse dynamics and disability weights sampled in Monte Carlo. This channel drives a heavy right tail in negative outcomes in some scenarios.

---

## 5) Monte Carlo design: what is randomized and why

### 5.0 How `sim.cpp` is organized (execution flow)

At a high level, each run does:

1. Sample one positive-utility person profile (`PosPerson`) from discrete menus.
2. Sample one negative-parameter draw (`NegParams`) from discrete menus.
3. Branch on `--mode`:
   * `daily`: call `simulate_life_rollout(...)`.
   * `expected`: call the hybrid annualized path inside `simulate_one_person()`.
4. Return a `SimOut` tuple containing total positive, total negative, net utility, and component breakdowns.
5. Aggregate over `--runs` and print percentile summaries plus decile contribution tables.

This makes the Monte Carlo outer loop identical across modes; only the within-person utility accounting engine changes.

### 5.1 Parameter menus and uniform sampling

Instead of single best-guess parameters, the model uses **discrete parameter menus** and samples **uniformly** across:

* discount rate
* grams per standard drink
* binge thresholds and high-intensity multipliers
* acute risk curves and baseline event rates
* hangover probability and severity
* chronic latency half-lives and risk slopes
* QALY→WELLBY conversion factors
* causal-weight knobs for observational channels
* AUD onset/remission/relapse and disability weights

This yields distributions over (U_{+}), (U_{-}), and (U_{\text{net}}), enabling percentile reporting and sensitivity comparisons across scenario variants.

### 5.2 Scenarios (interventions on the exposure–harm map)

The scenario set modifies the simulated world in targeted ways:

* **Baseline**: full model as specified
* **Never drink and drive**: removes (or sharply reduces) the driving-related acute harm channel while leaving other components intact
* **No binge**: removes binge/high-intensity structure (or forces it absent), reducing acute tails and AUD risk triggers
* **Abstinence**: eliminates both benefits and harms from alcohol exposure

### 5.3 Which model was chosen and why (`daily` focus)

For this write-up, the chosen structural model is **`--mode daily`**, because it is the most behaviorally explicit representation of utility over time:

* It simulates each day in sequence over the life horizon.
* It discounts each day at continuous-time factor `exp(-r t)` using the day midpoint.
* It realizes whether that day is social, whether acute events occur, whether hangover carries to the next day, and whether AUD state changes at month boundaries.
* It updates lagged chronic exposure states (EMA) every day before computing chronic disutility.

In utility terms, `daily` mode is closest to the conceptual object

[
U_{\text{net}} = \sum_{d=1}^{D} e^{-r t_d}\left(\Delta LS_{+,d} - \Delta LS_{-,d}\right),
]

with path-dependent state variables (AUD state, hangover days remaining, alive/dead status, exposure EMAs).

By contrast, `expected` mode is a computational shortcut that:

* uses expected daily positive utility from the drink-count PMF,
* computes annual expected acute/hangover/chronic terms from PMF-based probabilities and yearly realized binge/high-intensity frequencies,
* simulates AUD at annual transitions rather than monthly/day-linked state updates.

So `expected` remains useful for large sweeps and sensitivity grids, while `daily` is preferred when we want the utility narrative to explicitly reflect within-life timing and dependence across events.

### 5.4 Daily-mode mechanics in utility accounting

Inside `simulate_life_rollout(...)`, daily accounting proceeds as:

1. **Drinking draw** (possibly AUD-modified intensity): sample `drinks_today`.
2. **Positive utility**: compute day-level life-satisfaction uplift via a saturating function of drinks, plus social-context modifier; add discounted `\Delta LS / 365`.
3. **Acute harms**: draw Bernoulli events for traffic injury, non-traffic injury, violence (binge-linked), and poisoning (high-intensity-linked); convert event DALY burden to utilons.
4. **Hangover utility loss**: on binge-triggered days, stochastically start/extend hangover spell; apply next-day LS loss while spell lasts.
5. **Chronic harms**: update chronic EMA exposure states and compute daily equivalent chronic utility loss.
6. **AUD burden and transitions**: apply daily AUD disability equivalent when active; update AUD state monthly using risk-day-dependent onset/remission/relapse hazards.
7. **Mortality stop rule**: if a fatal acute event occurs, terminate future utility flow.

The resulting net utility is therefore not just “expected dose effect,” but a discounted path integral over realized stochastic life histories.

---

## 6) Interpreting the histogram figures

Figure 1 (baseline histograms) and Figure 2 (never drink-and-drive histograms) visualize the joint outcome distributions across key components. The headline comparison is the **net utility shift**:

* Baseline: median net utility is **negative**
* Never drink and drive: median net utility becomes **positive**

The shift is driven primarily by a large reduction in the acute harm distribution (notably the high-variance tail), while the positive distribution remains similar.

---

## 7) Results

### 7.1 Headline results (median / p50)

* **Positive utilons (p50)** are stable across baseline vs never drink-and-drive (≈10.4 utilons).
* **Negative utilons (p50)** drop sharply when drinking and driving is removed (≈11.9 → 7.6).
* **Net utilons (p50)** flip sign:

  * baseline **−1.41**
  * never drink-and-drive **+2.29**

### 7.2 Scenario summary table (distribution highlights)

All values are utilons; net is (U_{+} - U_{-}). Percentiles are taken from Monte Carlo outputs.

| Scenario              | Positive p50 | Negative p50 | **Net p50** | Net p05 | Net p95 |
| --------------------- | -----------: | -----------: | ----------: | ------: | ------: |
| Baseline              |        10.43 |        11.91 |   **−1.41** |  −52.70 |   16.03 |
| Never drink and drive |        10.45 |         7.60 |   **+2.29** |  −29.53 |   18.09 |
| No binge              |        10.37 |         9.79 |   **+0.37** |  −48.19 |   17.13 |
| Abstinence            |         0.00 |         0.00 |    **0.00** |   −8.04 |    0.00 |

Interpretation notes:

* The lower tail (e.g., net p05) is strongly negative in non-abstinence scenarios, reflecting rare-but-severe harm realizations.
* The median improves materially when driving-related harms are removed, indicating that a concentrated risk channel dominates the sign at p50 in the baseline.

### 7.3 Where the negative mass comes from (baseline vs never drink-and-drive)

Component medians (p50) for negative utilons:

| Negative component (p50) |  Baseline | Never drink and drive |
| ------------------------ | --------: | --------------------: |
| Acute harms              |      8.18 |                  4.68 |
| Hangover                 |      0.11 |                  0.11 |
| Chronic health proxies   |      0.10 |                  0.10 |
| AUD Markov               |      0.00 |                  0.00 |
| **Total negative**       | **11.91** |              **7.60** |

The acute channel explains most of the median reduction under never drink-and-drive, while hangover and chronic proxies are largely unchanged at p50. AUD is typically zero at the median but contributes to the upper tail (p75+), consistent with a heavy-tailed state-burden process.

### 7.4 Seed robustness (sanity check)

Across multiple baseline seeds, the net p50 remains close to the baseline median (roughly −1.7 to −1.1), suggesting the sign flip under never drink-and-drive is not a random-seed artifact but a structural effect of removing a high-impact harm pathway.

---

## 8) Practical moral criterion implied by the model outputs

Under this framework, alcohol use is “morally worth it” (net-positive expected welfare in the median sense) when the exposure pattern avoids high-impact harm pathways and concentrates any benefits without triggering heavy episodic risk.

The scenario results identify two dominant levers:

1. **Eliminating drinking and driving** is the largest single improvement observed, flipping median net utility from negative to positive (Figures 1–2; Table 7.2).
2. **Eliminating binge structure** improves net outcomes and reduces tail risk, but does not match the median shift produced by removing driving-related harms (Table 7.2).

Abstinence is a neutral baseline in this utilon accounting (0 net by construction) and provides a reference point for interpreting whether any modeled drinking pattern clears a “net-positive” bar under chosen ethical aggregation rules.

---

## 9) Figures to insert

* **Figure 1:** Baseline histograms (positive / negative / net and component distributions)
* **Figure 2:** Never drink-and-drive histograms (same layout)

These figures support the headline distributional shift: **net p50 changes from negative to positive** when drinking and driving is removed.

---

## Appendix A) `sim.cpp` configuration parameter table

The table below inventories the configurable parameters used by `sim.cpp`, with defaults and/or sampled ranges plus an interpretation column. The final citation mapping is left as requested.

| Parameter (sim.cpp name / CLI override) | Value or range in current model | What it means | Citation |
| --- | --- | --- | --- |
| `SCRIPT.num_runs` / `--runs` | Default: `100` | Number of Monte Carlo person-runs per scenario call. | N/A |
| `SCRIPT.seed` / `--seed` | Default: `12345` | RNG seed for reproducibility. | N/A |
| `SCRIPT.years` | `60` | Simulation horizon in years. | N/A |
| `SCRIPT.days_per_year` | `365` | Time discretization used for daily accounting and LS-year conversion. | N/A |
| `SCRIPT.drinks_per_day` / `--drinks-per-day` | Default: `1.5` (sweepable via `--sweep-min/max/step`) | Baseline mean daily exposure before AUD multipliers in daily mode. | N/A |
| `SCRIPT.day_count_model` | Default: `poisson` (supported: `constant`, `poisson`, `two_point`) | Rule used to convert mean drinks/day into daily drink counts. | N/A |
| `SCRIPT.mode` / `--mode` | Default: `expected` (allowed: `expected`, `daily`) | Chooses annualized-expected vs pathwise daily simulation engine. | N/A |
| `SCRIPT.two_point_p_zero` | `0.5` (currently not used directly; probability is adjusted from mean and high point) | Placeholder two-point mixture control in script config. | N/A |
| `SCRIPT.two_point_high_drinks` | Default: `6` | High-drink mass point used in two-point day-count model. | N/A |
| `SCRIPT.max_drinks_cap` | `12` | Hard cap on drinks/day in PMF and daily sampling. | N/A |
| `SCRIPT.discount_rate_annual` | `0.03` | Time discount rate for utility flow (`exp(-r t)`). | N/A |
| `SCRIPT.hist_bins` / `--print-hist-data`, `--hist-data-out` context | `70` | Number of bins when exporting/printing histogram summaries. | N/A |
| `SCRIPT.quantiles` | `{1,5,10,25,50,75,90,95,99}` | Percentiles reported in summary output. | N/A |
| `POS_MODEL.p_social_day` / `--p-social-day` | `{0.1, 0.2, 0.35, 0.5}` | Probability a drinking day is social; affects social benefit term. | needs source |
| `POS_MODEL.baseline_stress` / `--baseline-stress` | `{0.2, 0.4, 0.6, 0.8}` | Baseline stress index; scales relaxation benefit component. | needs source |
| `POS_MODEL.baseline_sociability` / `--baseline-sociability` | `{0.2, 0.4, 0.6, 0.8}` | Baseline sociability index; moderates marginal social gain. | needs source |
| `POS_MODEL.social_setting_quality` / `--social-setting-quality` | `{0.3, 0.5, 0.7, 0.9}` | Quality multiplier for social context contribution. | needs source |
| `POS_MODEL.responsiveness` / `--responsiveness` | `{0.6, 0.8, 1.0, 1.2, 1.4}` | Person-specific responsiveness to positive drinking effects. | needs source |
| `POS_MODEL.saturation_rate` / `--saturation-rate` | `{0.4, 0.7, 1.0, 1.3}` | Controls diminishing returns in positive gain vs drink count. | needs source |
| `POS_MODEL.ls_per_session_score` / `--ls-per-session-score` | `{0.15, 0.25, 0.35, 0.50}` | Base LS scaling for one drinking session’s positive effect. | needs source |
| `POS_MODEL.w_enjoyment` / `--w-enjoyment` | `{0.8, 1.0, 1.2, 1.4}` | Weight on hedonic enjoyment in positive utility. | needs source |
| `POS_MODEL.w_relaxation` / `--w-relaxation` | `{0.6, 0.8, 1.0, 1.2}` | Weight on relaxation channel in positive utility. | needs source |
| `POS_MODEL.w_social` / `--w-social` | `{0.5, 0.8, 1.1, 1.4}` | Weight on social channel in positive utility. | needs source |
| `POS_MODEL.w_mood` / `--w-mood` | `{0.3, 0.5, 0.7, 0.9}` | Weight on mood channel in positive utility. | needs source |
| `POS_MODEL.max_daily_ls_uplift` / `--max-daily-ls-uplift` | `{1.0, 1.5, 2.0}` | Ceiling on day-level LS benefit from drinking. | needs source |
| `NEG_MODEL.discount_rate_choices` / `--discount-rate-choices` | `{0.0, 0.015, 0.03, 0.05}` | Drawn discount-rate menu for negative-side parameterization. | needs source |
| `NEG_MODEL.grams_ethanol_per_standard_drink_choices` / `--grams-ethanol-per-standard-drink-choices` | `{10, 14}` | Ethanol grams per standard drink conversion. | NIAAA (14 g standard drink); Australian standard drink definition (10 g ethanol) |
| `NEG_MODEL.qaly_to_wellby_factor_choices` / `--qaly-to-wellby-factor-choices` | `{5, 6, 7, 8}` | QALY/DALY to WELLBY(utilon) conversion multiplier. | needs source |
| `NEG_MODEL.causal_weight_choices` / `--causal-weight-choices` | `{0.25, 0.5, 0.75, 1.0}` | Global causal-attribution weight for harm channels. | needs source |
| `NEG_MODEL.binge_threshold_drinks_choices` / `--binge-threshold-drinks-choices` | `{4, 5}` | Drinks threshold defining binge day. | NIAAA binge-drinking threshold definitions (4/5+ drinks) |
| `NEG_MODEL.high_intensity_multiplier_choices` / `--high-intensity-multiplier-choices` | `{2, 3}` | Multiplier on binge threshold for high-intensity day definition. | NIAAA high-intensity drinking framing (2x/3x binge threshold) |
| `NEG_MODEL.latency_half_life_years_choices` / `--latency-half-life-years-choices` | `{2, 5, 10}` | Half-life for chronic exposure EMA memory (general). | needs source |
| `NEG_MODEL.cancer_latency_half_life_years_choices` / `--cancer-latency-half-life-years-choices` | `{5, 10, 15}` | Half-life for cancer EMA memory. | needs source |
| `NEG_MODEL.cirrhosis_latency_half_life_years_choices` / `--cirrhosis-latency-half-life-years-choices` | `{3, 5, 10}` | Half-life for cirrhosis EMA memory. | needs source |
| `NEG_MODEL.traffic_injury_rr_per_10g_choices` / `--traffic-injury-rr-per-10g-choices` | `{1.18, 1.24, 1.30}` | Relative-risk multiplier per 10g ethanol for traffic injury. | Cherpitel et al. multinational ED case-crossover dose-response (injury odds vs drinks in prior 6h) |
| `NEG_MODEL.nontraffic_injury_rr_per_10g_choices` / `--nontraffic-injury-rr-per-10g-choices` | `{1.26, 1.30, 1.34}` | Relative-risk multiplier per 10g ethanol for non-traffic injury. | Cherpitel et al. multinational ED case-crossover dose-response (injury odds vs drinks in prior 6h) |
| `NEG_MODEL.intentional_injury_rr_per_drink_choices` / `--intentional-injury-rr-per-drink-choices` | `{1.25, 1.38, 1.50}` | Per-drink RR slope for violence/intentional injury channel. | Cherpitel et al. multinational ED case-crossover dose-response (cause-specific intentional injury ORs) |
| `NEG_MODEL.injury_baseline_prob_per_drinking_day_choices` / `--injury-baseline-prob-per-drinking-day-choices` | `{1e-4, 2.5e-4, 5e-4, 1e-3}` | Baseline daily acute injury probability on drinking days. | NHAMCS 2019 ED injury-visit rates (baseline absolute incidence) |
| `NEG_MODEL.violence_baseline_prob_per_binge_day_choices` / `--violence-baseline-prob-per-binge-day-choices` | `{5e-6, 1e-5, 2e-5, 5e-5}` | Baseline violence-event probability conditional on binge day. | NCHS Data Brief (NHAMCS 2019-2021) assault-related ED visit rates, ICD-10-CM X92-Y09 |
| `NEG_MODEL.injury_daly_per_nonfatal_event_choices` / `--injury-daly-per-nonfatal-event-choices` | `{0.005, 0.02, 0.05}` | DALY burden of a nonfatal acute injury event. | needs source |
| `NEG_MODEL.injury_case_fatality_choices` / `--injury-case-fatality-choices` | `{0.002, 0.005, 0.01}` | Fatality probability for acute injury events. | needs source |
| `NEG_MODEL.injury_daly_per_fatal_event_choices` / `--injury-daly-per-fatal-event-choices` | `{20, 30, 40}` | DALY burden assigned to fatal acute event. | needs source |
| `NEG_MODEL.traffic_injury_externality_multiplier_choices` / `--traffic-injury-externality-multiplier-choices` | `{0.5, 1.0, 1.5}` | Extra harm factor for non-drinker externality in traffic injuries. | NHTSA alcohol-impaired crash fatality victim shares (non-driver externalities) |
| `NEG_MODEL.poisoning_prob_per_high_intensity_day_choices` / `--poisoning-prob-per-high-intensity-day-choices` | `{1e-6, 3e-6, 1e-5, 3e-5}` | Acute poisoning probability on high-intensity days. | HCUP NEDS T51.0x ethanol-poisoning ED estimates, scaled with BRFSS binge-episode anchors |
| `NEG_MODEL.poisoning_case_fatality_choices` / `--poisoning-case-fatality-choices` | `{0.005, 0.01, 0.02}` | Fatality probability for poisoning events. | needs source |
| `NEG_MODEL.poisoning_daly_nonfatal_choices` / `--poisoning-daly-nonfatal-choices` | `{0.01, 0.05, 0.2}` | DALY burden for nonfatal poisoning event. | needs source |
| `NEG_MODEL.hangover_prob_given_binge_choices` / `--hangover-prob-given-binge-choices` | `{0.3, 0.5, 0.7, 0.9}` | Probability of hangover after binge day. | needs source |
| `NEG_MODEL.hangover_ls_loss_per_day_choices` / `--hangover-ls-loss-per-day-choices` | `{0.05, 0.1, 0.2, 0.4}` | Life-satisfaction loss applied per hangover day. | needs source |
| `NEG_MODEL.hangover_duration_days_choices` / `--hangover-duration-days-choices` | `{1, 2}` | Hangover spell length in days. | needs source |
| `NEG_MODEL.breast_cancer_rr_per_10g_day_choices` / `--breast-cancer-rr-per-10g-day-choices` | `{1.05, 1.07, 1.10}` | Breast-cancer RR menu (currently present in config menu; not directly consumed in current equations). | Alcohol-breast cancer dose-response meta-analysis (exact study not specified in current docs) |
| `NEG_MODEL.all_cancer_rr_per_10g_day_choices` / `--all-cancer-rr-per-10g-day-choices` | `{1.02, 1.04, 1.06}` | RR per 10g/day for all-cancer chronic burden. | Alcohol-all-cancer dose-response meta-analysis (exact study not specified in current docs) |
| `NEG_MODEL.cancer_causal_weight_choices` / `--cancer-causal-weight-choices` | `{0.75, 1.0}` | Cancer-specific causal attribution weight. | needs source |
| `NEG_MODEL.cirrhosis_rr_mortality_at_25g_choices` / `--cirrhosis-rr-mortality-at-25g-choices` | `{2.0, 2.65, 3.2}` | RR anchor for cirrhosis mortality at 25g/day. | 2023 dose-response meta-analysis for alcohol and cirrhosis mortality |
| `NEG_MODEL.cirrhosis_rr_mortality_at_50g_choices` / `--cirrhosis-rr-mortality-at-50g-choices` | `{5.5, 6.83, 8.0}` | RR anchor for cirrhosis mortality at 50g/day. | 2023 dose-response meta-analysis for alcohol and cirrhosis mortality |
| `NEG_MODEL.cirrhosis_rr_mortality_at_100g_choices` / `--cirrhosis-rr-mortality-at-100g-choices` | `{12.0, 16.38, 20.0}` | RR anchor for cirrhosis mortality at 100g/day. | 2023 dose-response meta-analysis for alcohol and cirrhosis mortality |
| `NEG_MODEL.af_rr_per_drink_day_choices` / `--af-rr-per-drink-day-choices` | `{1.03, 1.06, 1.08}` | Atrial-fibrillation RR slope per drink/day equivalent. | Atrial fibrillation alcohol dose-response meta-analyses (per-drink RR) |
| `NEG_MODEL.include_ihd_protection_choices` / `--include-ihd-protection-choices` | `{false, true}` | Toggle for including modeled IHD protective term. | IHD alcohol-protection literature is mixed (observational vs MR); modeled as sensitivity toggle |
| `NEG_MODEL.ihd_protective_rr_nadir_choices` / `--ihd-protective-rr-nadir-choices` | `{0.85, 0.95, 1.0}` | IHD RR nadir when protection is included. | IHD alcohol-protection literature is mixed (observational vs MR); modeled as sensitivity range |
| `NEG_MODEL.binge_negates_ihd_protection_choices` / `--binge-negates-ihd-protection-choices` | `{true, false}` | Whether binge days cancel IHD protection effect. | Evidence that heavy episodic/binge drinking attenuates putative IHD protection |
| `NEG_MODEL.aud_onset_base_prob_per_year_choices` / `--aud-onset-base-prob-per-year-choices` | `{0.002, 0.005, 0.01}` | Baseline annual onset probability for AUD state model. | needs source |
| `NEG_MODEL.aud_remission_prob_per_year_choices` / `--aud-remission-prob-per-year-choices` | `{0.08, 0.15, 0.25}` | Annual probability of AUD remission. | needs source |
| `NEG_MODEL.aud_relapse_prob_per_year_if_abstinent_choices` / `--aud-relapse-prob-per-year-if-abstinent-choices` | `{0.02, 0.05, 0.10}` | Annual relapse probability when abstinent/remitted. | needs source |
| `NEG_MODEL.aud_relapse_multiplier_if_risk_drinking_choices` / `--aud-relapse-multiplier-if-risk-drinking-choices` | `{3, 6, 10}` | Relapse-risk multiplier when risk drinking is present. | NESARC-based AUD risk heterogeneity framing (exact relapse multiplier source not specified) |
| `NEG_MODEL.aud_disability_weight_choices` / `--aud-disability-weight-choices` | `{0.123, 0.235, 0.366}` | Disability weight applied to active AUD state. | GBD alcohol-use-disorder disability weight sets (GBD 2013 + newer synthesis) |
| `NEG_MODEL.aud_depression_ls_addon_choices` / `--aud-depression-ls-addon-choices` | `{0.0, 0.2, 0.5, 1.0}` | Additional LS decrement proxy linked to AUD/depressive burden. | needs source |
| `NEG_MODEL.mental_health_causal_weight_choices` / `--mental-health-causal-weight-choices` | `{0.25, 0.5, 0.75}` | Causal weight for mental-health-related AUD addon term. | needs source |
| `NEG_MODEL.baseline_daly_rate_all_cancer_choices` / `--baseline-daly-rate-all-cancer-choices` | `{0.001, 0.003, 0.006}` | Baseline all-cancer DALY rate before alcohol-attributable excess risk. | IHME Global Burden of Disease baseline DALY rates (age/sex/population calibration) |
| `NEG_MODEL.baseline_daly_rate_cirrhosis_choices` / `--baseline-daly-rate-cirrhosis-choices` | `{0.0003, 0.001, 0.0025}` | Baseline cirrhosis DALY rate before alcohol-attributable excess risk. | IHME Global Burden of Disease baseline DALY rates (age/sex/population calibration) |
| `NEG_MODEL.baseline_daly_rate_af_choices` / `--baseline-daly-rate-af-choices` | `{0.0005, 0.0015, 0.003}` | Baseline AF DALY rate before alcohol-attributable excess risk. | IHME Global Burden of Disease baseline DALY rates (age/sex/population calibration) |
| `NEG_MODEL.baseline_daly_rate_ihd_choices` / `--baseline-daly-rate-ihd-choices` | `{0.001, 0.003, 0.006}` | Baseline IHD DALY rate used for protective/harm adjustment term. | IHME Global Burden of Disease baseline DALY rates (age/sex/population calibration) |
