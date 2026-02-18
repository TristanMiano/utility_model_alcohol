# Negative-Effect Utilon Model for Alcohol Consumption

This report specifies a **negative utilon module** that consumes the same exposure stream as your positive model—i.e., daily draws of `drinks_today` generated from an average `drinks_per_day` (plus optional day-to-day variability and binge sensitivity)—and returns **discounted lifetime negative utilons per person**.

A “utilon” here is a **WELLBY-like unit**: **1 utilon = 1 life-satisfaction point (0–10) sustained for 1 person for 1 year** (i.e., a 1-point change for 1 year). This aligns with the WELLBY definition used in policy appraisal work. citeturn2view0turn1search7

## Accounting framework and discounting

### Core output

Let \(t\) be time in years since model start (or since age at baseline). The negative module returns:

\[
U_{\text{neg}} \;=\; \int_0^{T} e^{-r t}\; \Delta LS_{\text{neg}}(t)\; dt
\]

where \(\Delta LS_{\text{neg}}(t)\) is the **life-satisfaction loss** at time \(t\) from all negative-effect channels combined (in LS points), and \(r\) is the continuous discount rate.

In a **daily** implementation (with \(\Delta t = 1/365\) years), you can approximate:

\[
U_{\text{neg}} \approx \sum_{\text{days }k} e^{-r t_k} \cdot \Delta LS_{\text{neg}}(k)\cdot \frac{1}{365}
\]

In an **annual** implementation:

\[
U_{\text{neg}} \approx \sum_{\text{years }y} e^{-r y}\cdot \Delta LS_{\text{neg,year}}(y)
\]

### Recommended discounting defaults

A practically compatible default in applied welfare and health cost-effectiveness work is **\(r = 0.03\)**, with sensitivity to lower and higher values. For WELLBY-style appraisal, UK supplementary Green Book wellbeing guidance explicitly discusses discounting future wellbeing and points to a “health” discount schedule starting at **1.5%** for the first 30 years (then declining), which motivates including **0.015** in sensitivity. citeturn2view0

**Recommended discrete choices**
- `discount_rate_choices = [0.0, 0.015, 0.03, 0.05]` citeturn2view0

## Exposure stream alignment and binge mapping

### Converting “drinks” to ethanol

A U.S. “standard drink” is **14 grams of pure alcohol** (0.6 fl oz). citeturn11search8  
Many epidemiologic meta-analyses (especially outside the U.S.) define a standard drink as **10 g**; breast-cancer meta-analyses commonly report results per “standard drink” using ~10 g. citeturn4search1

**Recommended discrete choices**
- `grams_ethanol_per_standard_drink_choices = [10, 14]` citeturn11search8turn4search1

### Daily count model options (must match your simulator)

You already have `drinks_per_day` as the mean. The negative module assumes you can generate a daily integer `drinks_today` with one of these options:
- Constant: `drinks_today = drinks_per_day`
- Poisson: `drinks_today ~ Poisson(drinks_per_day)`
- Two-point mixture: `drinks_today = 0` with prob \(p_0\), else `drinks_today = d_hi` chosen so the mean is preserved.

**Recommended discrete choices**
- `day_count_model_choices = ["constant", "poisson", "two_point"]`
- `two_point_p_zero_choices = [0.0, 0.25, 0.5, 0.75]`
- `two_point_high_drinks_choices = [4, 6, 8, 10, 12]`

### Defining binge and high-intensity drinking

NIAAA defines **binge drinking** as a pattern that brings BAC to **0.08 g/dL**, typically **≥5 drinks for men** or **≥4 drinks for women** in about **2 hours**. citeturn11search8  
NIAAA defines **high-intensity drinking** as **≥2×** the binge thresholds. citeturn11search8

**Recommended discrete choices**
- `binge_threshold_drinks_choices = [4, 5]` citeturn11search8
- `high_intensity_multiplier_choices = [2, 3]` (2× is definition-faithful; 3× adds a “very high” sensitivity tier). citeturn11search8

### A simple mapping from drinks and binge frequency to risk

The module uses two exposure summaries:
- **Event-level exposure** on a day: `g_today = drinks_today * grams_per_standard_drink`
- **Chronic exposure** for year \(y\): `gbar_y = mean(g_today)` (or your running average)

It also uses:
- `binge_today = 1[drinks_today >= binge_threshold]`
- `high_intensity_today = 1[drinks_today >= high_intensity_multiplier * binge_threshold]`
- `binge_days_per_year` either as an input or derived from the day-count distribution.

This structure is consistent with evidence that **many major alcohol harms concentrate in heavy episodic use**, even though chronic harms also rise with average consumption. citeturn20search2turn11search1

## Channel design and evidence-anchored dose-response

This module is **additive by channel** and is designed to avoid double counting by clearly separating:
- **Health burden** (DALY/QALY-like losses, including disability and premature mortality captured as lost life-years)  
from
- **Non-health wellbeing impacts** (e.g., hangover next-day impairment, social conflict, labor-market disruption).

When evidence is mainly observational, the channel includes a **causal-weight knob**.

### Dose-response building blocks used throughout

**Log-linear RR per ethanol dose (simple to implement)**  
For a channel with “RR per 10g ethanol” equal to \(RR_{10}\):

\[
RR(g) = RR_{10}^{\,g/10}
\]

This is particularly usable for acute injury meta-analyses reported per 10g. citeturn3search4

**Risk drinking (binge-like) frequency categories (AUD and some social harms)**  
For some channels, the most evidence-anchored structure is a multiplier by **frequency of 5+/4+ drinking days**, used in NESARC prospective analyses. citeturn19search1

### Channel table

Each channel below specifies: endpoint, private/externality, acute/chronic, simplified dose-response, latency, primary sources, and parameters (with discrete menus later).

#### Acute injury and accidents (traffic and non-traffic)

- **Endpoint/mechanism**: Alcohol intoxication impairs coordination/judgment and increases injury risk; motor vehicle crash risk rises steeply with alcohol dose. citeturn11search8turn3search4  
- **Private vs externality**: Both; substantial harm to non-drinkers in traffic crashes. In U.S. fatal crashes involving alcohol-impaired drivers (2022), **~59%** of those killed are the impaired drivers themselves and **~40%** are others (passengers/other road users/pedestrians). citeturn20search1  
- **Acute vs chronic**: Acute, event-driven.  
- **Dose-response**: Use Taylor et al. meta-analysis dose-response:  
  - Motor-vehicle injury OR increases by **1.24 per +10g ethanol**, reaching extremely high ORs at very large doses (e.g., ~120g). citeturn3search4  
  - Non-motor-vehicle injury OR increases by **1.30 per +10g ethanol** (also very steep at high doses). citeturn3search4  
- **Latency**: Same day (or within hours).  
- **Key parameters**: baseline per-drinking-day risk; RR slope; severity per event; externality multiplier.

#### Violence and crime involvement (including interpersonal violence)

- **Endpoint/mechanism**: Alcohol intoxication increases aggression/impulsivity and is strongly linked to intentional injury and interpersonal violence. citeturn3search4turn20search2  
- **Private vs externality**: Often externality-dominant (victim harm), but can include perpetrator injury/arrest consequences.  
- **Acute vs chronic**: Acute, event-driven; chronic spillovers possible (legal/relationship sequelae).  
- **Dose-response**: Taylor et al. report the **highest per-drink increase** for intentional injury (OR about **1.38 per drink** in their synthesis). citeturn3search4  
- **Latency**: Same day/night.  
- **Key parameters**: baseline per-binge-day violence probability; victim-harm multiplier; severity; causal weight.

#### Risky sexual behavior outcomes (optional channel)

- **Endpoint/mechanism**: Alcohol use is associated with sexual risk behaviors; evidence is mixed on causal event-level condom effects, but meta-analyses support an association (especially in adolescents/young adults). citeturn12search0turn12search11turn12search15  
- **Private vs externality**: Mixed; includes partner harm, STI transmission, sexual violence risk.  
- **Acute vs chronic**: Acute behavior → chronic consequences (STIs, unintended pregnancy, trauma).  
- **Dose-response**: For adolescents/young adults, a meta-analysis reports alcohol consumption associated with higher odds of early initiation, inconsistent condom use (OR ~1.23), and multiple partners (OR ~1.72). citeturn12search11  
- **Latency**: Immediate behavior; outcomes may lag months/years.  
- **Key parameters**: causal weight (low/uncertain), mapping from binge days to “risky-sex opportunity days,” STI/utilon severity.

#### Acute poisoning / overdose (ethanol poisoning)

- **Endpoint/mechanism**: Very high blood alcohol levels can suppress breathing/airway reflexes; poisoning risk is disproportionately concentrated in very high-intensity drinking. citeturn11search8  
- **Private vs externality**: Primarily private (plus family spillovers).  
- **Acute vs chronic**: Acute.  
- **Dose-response**: Step-function on “high-intensity drinking” days (e.g., ≥2× binge threshold) as a simulation-ready approximation. citeturn11search8  
- **Latency**: Same day/night.  
- **Anchoring magnitudes**: Alcohol poisoning is included among acute causes in CDC’s accounting of deaths from excessive alcohol use. citeturn11search0turn11search1  
- **Key parameters**: event probability conditional on high-intensity day; fatality probability; nonfatal severity.

#### Next-day impairment / hangover productivity loss (separate from “health”)

- **Endpoint/mechanism**: Hangover causes fatigue, nausea, cognitive impairment → reduced functioning at work and daily life. citeturn10search2  
- **Private vs externality**: Mostly private; can create workplace spillovers/errors.  
- **Acute vs chronic**: Acute (next day), but repeated.  
- **Dose-response**: Probability of hangover rises with drinks/day and especially binge/high-intensity days (use step or logistic).  
- **Evidence anchor**: A 2019 Dutch survey study estimated hangover-linked **0.2 absenteeism days** and **8.3 presenteeism days** per year (among those sampled), with **~24.9% productivity loss** on hangover workdays. citeturn10search2  
- **Key parameters**: \(P(\text{hangover}|\text{binge})\); LS loss per hangover day; mapping from productivity to LS (optional).

#### Cancer (include breast and an overall cancer aggregate)

- **Endpoint/mechanism**: Alcohol is a carcinogen; increases risk of multiple cancers, including breast and colorectal, liver, head/neck. citeturn20search2turn5search4turn4search6  
- **Private vs externality**: Private (family spillovers optional).  
- **Acute vs chronic**: Chronic.  
- **Dose-response (breast cancer)**: A 2023 systematic review/meta-analysis of prospective cohort studies reports increasing breast cancer RR with higher intake; relative to nondrinking: RR ~1.10 at 1 drink/day, ~1.18 at 2 drinks/day, and ~1.22 at 3 drinks/day (their “standard drink” commonly treated as ~10 g). citeturn4search1  
- **Overall alcohol-attributable cancer burden**: A widely cited Lancet Oncology analysis estimated a substantial global burden of alcohol-attributable cancers (including moderate drinking contributing non-trivially). citeturn4search12  
- **Latency**: Years-lag; implement via a smoothed/lagged exposure (see formulas).  
- **Key parameters**: breast-cancer RR slope; overall cancer RR slope; latency half-life; causal weight.

#### Liver disease and cirrhosis

- **Endpoint/mechanism**: Chronic alcohol exposure → liver injury; cirrhosis risk rises steeply with grams/day. citeturn4search3turn11search1  
- **Private vs externality**: Private (family spillovers optional).  
- **Acute vs chronic**: Chronic.  
- **Dose-response**: A 2023 dose-response meta-analysis reports, relative to lifetime abstention:  
  - **Morbidity** RR ~1.81 at 25 g/day; ~3.54 at 50 g/day; ~8.15 at 100 g/day  
  - **Mortality** RR ~2.65 at 25 g/day; ~6.83 at 50 g/day; ~16.38 at 100 g/day citeturn4search3  
- **Latency**: Years-lag; implement via smoothed exposure.  
- **Key parameters**: RR curve slope/shape; latency; baseline burden and severity.

#### Cardiovascular outcomes (explicitly separable protective vs harmful components)

This model treats cardiovascular effects carefully and **does not net out “protective” effects by default**.

- **Clear harms (include by default)**  
  - **Atrial fibrillation**: Prospective meta-analysis indicates **~6% higher AF risk per +1 drink/day** (RR ~1.06). citeturn5search1  
  - **Blood pressure**: Dose-response meta-analysis finds even ~12 g/day associated with higher systolic BP vs nondrinking. citeturn5search2  

- **Contested / potentially protective (keep separable)**  
  - A 2014 systematic review/meta-analysis emphasizes that apparent protective associations for IHD depend on patterning and reference group definitions; “benefit” is not observed with heavy episodic drinking, and IHD risk among “moderate average but binge” drinkers can be similar to abstainers. citeturn18search1  
  - A 2024 “burden of proof” synthesis for alcohol and IHD characterizes the evidence for a protective association as weak and sensitive to heterogeneity/bias handling, reporting a nadir below RR=1 but with wide uncertainty intervals. citeturn5search0  

- **Private vs externality**: Private.  
- **Acute vs chronic**: Chronic (with acute triggers possible).  
- **Latency**: Years-lag.

Key parameters: AF slope; BP-harm mapping; “include IHD protection” toggle; binge-negates-protection toggle.

#### Alcohol Use Disorder (AUD): disability burden and persistence/relapse

- **Endpoint/mechanism**: Dependence and harmful use states create sustained disability/wellbeing loss. WHO estimates substantial global prevalence/burden. citeturn20search2turn6search0  
- **Private vs externality**: Mostly private, but large spillovers.  
- **Acute vs chronic**: Chronic, stateful.  
- **Dose-response (onset risk via risk-drinking frequency)**: A NESARC prospective study links frequency of 5+/4+ “risk drinking days” at baseline to incidence of alcohol dependence over ~3 years; adjusted ORs increase strongly with risk-drinking frequency (e.g., OR ~1.35 for <1/month up to OR ~7.23 for daily/near-daily risk drinking vs none). citeturn19search1  
- **Persistence/relapse**: Among individuals in remission (NESARC follow-up), recurrence of AUD symptoms over 3 years was much higher for risk drinkers than abstainers (e.g., ~51% vs ~7% in one remission subgroup). citeturn6search1  
- **Severity weights (DALY/QALY pathway)**: GBD disability-weight work provides severity weights for alcohol use disorder (example values by severity are reported in the GBD 2013 disability-weight paper). citeturn8search2 A 2025 meta-analysis summarizes disability-weights for alcohol use disorder severity states with values in a comparable range. citeturn9view0  
- **Key parameters**: onset baseline; frequency multipliers; remission/relapse; disability weight; direct LS decrement (optional).

#### Mental health comorbidity (depression/anxiety) with cautious causality handling

- **Endpoint/mechanism**: Alcohol problems and depression/anxiety are comorbid; directionality is complex and heterogeneous.  
- **Evidence**: Recent Mendelian randomization work suggests depression can predict later alcohol problems, while effects from alcohol to depression may vary by measure and subgroup. citeturn13search3  
- **Implementation stance**: model as **(a)** an AUD-comorbidity multiplier on disability/wellbeing loss or **(b)** an additional LS decrement state triggered by AUD/heavy drinking, with a conservative causal-weight menu.  
- **Key parameters**: comorbidity multiplier; causal weight; duration.

#### Relationship harms (divorce, domestic conflict) where there’s evidence

- **Endpoint/mechanism**: Heavy drinking is associated with marital dissolution and conflict; effects depend on concordance and baseline marital quality.  
- **Evidence (divorce risk)**: In a Norwegian cohort of couples, heavy drinking in either spouse was associated with higher divorce risk (HR about **1.39–1.41** vs light drinking), and discordant heavy drinking showed higher risk than concordant light drinking. citeturn10search12  
- **Wellbeing conversion**: divorce can reduce life satisfaction around the event with partial adaptation, but heterogeneity is large; don’t assume uniform long-run LS loss. citeturn14search1turn17search5  
- **Key parameters**: divorce hazard multiplier; LS loss magnitude/duration; partner externality factor; causal weight.

#### Long-run earnings / employment impacts

- **Endpoint/mechanism**: Problem drinking is associated with impaired work performance and weaker labor-market attachment. citeturn10search22turn10search7  
- **Wellbeing conversion**: unemployment has a large, empirically measurable life satisfaction cost; one recent analysis reports an average LS difference of about **−0.64 points** (0–10 scale) for unemployed vs employed in SOEP-type data, useful as a conversion anchor for unemployment shocks. citeturn13search1  
- **Key parameters**: problem-drinking→unemployment RR; wage penalty; income→LS slope; causal weight.

## Utilon conversion methods and how to avoid double counting

You requested two conversion pathways, both supported here.

### Pathway A: DALY/QALY → WELLBY (recommended default for health channels)

If a health channel yields DALYs lost (or QALYs lost), convert to WELLBY-like utilons via:

\[
\text{utilons} = (\text{DALYs lost}) \times k_{\text{QALY→WELLBY}}
\]

UK supplementary Green Book wellbeing guidance explicitly discusses pivoting from the monetary value of a QALY to WELLBYs and uses an illustrative mapping where **1 QALY corresponds to ~7 life-satisfaction points-years** (from LS ~8 in “no health problems” to ~1 at “as bad as death,” giving 7 points). citeturn2view0

**Recommended discrete choices**
- `qaly_to_wellby_factor_choices = [5, 6, 7, 8]` (default 7). citeturn2view0

**When to use A**
- Cancer, cirrhosis, cardiovascular disease impacts, injury disability, AUD disability (if you’re treating AUD as a health-state burden). citeturn4search3turn4search1turn5search1turn8search2

### Pathway B: Direct wellbeing impacts (LS-point impacts)

Use this when the harm is not well-captured as a health-state DALY/QALY (or you explicitly want a wellbeing layer):
- hangover/next-day impairment
- relationship conflict and social functioning
- employment/unemployment wellbeing impacts

**Key guardrail to avoid double counting**
- If you convert **lost earnings** into LS loss, don’t also model hangover “productivity loss” as *additional income loss* unless you explicitly separate them:  
  - Treat “hangover” as **direct LS loss** (fatigue, irritability, low enjoyment).  
  - Treat “earnings/employment” as **separate longer-run LS loss** from job instability/unemployment, sampled with a causal-weight knob.

## Master “Negative Utilon Model” summary table

Implement each channel as an **additive annual or daily LS-loss stream**, with optional discrete-event simulation for rare acute events.

| Channel | Acute/chronic | Required inputs | Simple dose-response (implementable) | Time profile / lag | Utilon conversion | Key parameters (sample uniformly from menus) |
|---|---|---|---|---|---|---|
| Traffic injury (incl. fatalities optional) | Acute | `drinks_today`, `binge_today`, age | \(RR = RR_{10}^{g/10}\), use \(RR_{10}=1.24\) per +10g | same day | A (DALY→WELLBY) or event severity | baseline risk per drinking day; severity DALY/event; externality multiplier; causal weight. citeturn3search4turn20search1 |
| Non-traffic injury (falls, etc.) | Acute | `drinks_today`, `binge_today` | \(RR_{10}=1.30\) per +10g | same day | A | baseline risk; severity; causal weight. citeturn3search4 |
| Violence / crime involvement | Acute | `binge_today` (or `drinks_today`) | step or \(RR\) using intentional-injury per-drink OR anchor | same day | A (injury DALY) + optional B (legal/relationship LS) | baseline violence risk per binge day; victim harm multiplier; severity; causal weight. citeturn3search4turn20search2 |
| Risky sex outcomes (optional) | Acute→lagged | `binge_days_per_year` | scale “risky-sex days” with binge frequency | months/years | A (STI DALY) or B | causal weight; risky-sex probability; STI/utilon severity. citeturn12search11turn12search0 |
| Acute ethanol poisoning | Acute | `high_intensity_today` | step: probability only on high-intensity days | same day | A | p(poisoning|HI); fatality|poisoning; severity DALY; causal weight. citeturn11search8turn11search1 |
| Hangover / next-day impairment | Acute (next day) | `drinks_today`, `binge_today` | \(P(\text{hangover})\) step/logistic; apply LS loss next day | 1 day lag | B | p(hangover|binge); LS loss per hangover day; duration. citeturn10search2 |
| Breast cancer | Chronic | `gbar_y` (or lagged EMA), sex | log-linear slope from meta-analysis | multi-year lag | A | RR slope per 10g; latency half-life; baseline burden; causal weight. citeturn4search1turn20search2 |
| Other alcohol-attributable cancers (aggregate) | Chronic | `gbar_y` | log-linear slope (conservative) | multi-year lag | A | slope; baseline burden; causal weight. citeturn4search12turn5search4 |
| Cirrhosis / liver disease | Chronic | `gbar_y` | piecewise log-linear fit to RR at 25/50/100g | multi-year lag | A | RR curve shape; latency; baseline burden; causal weight. citeturn4search3 |
| Cardiovascular harms (AF, BP-driven) | Chronic | `gbar_y` | AF: RR per drink ≈1.06; BP harm optional | multi-year lag | A | AF slope; BP mapping; baseline burden. citeturn5search1turn5search2 |
| Cardiovascular “protection” (IHD) as separate optional term | Chronic | `gbar_y`, `binge_days_per_year` | allow protective RR only if no heavy episodic drinking | multi-year lag | (Do not net by default) | include toggle; binge-negates multiplier. citeturn18search1turn5search0 |
| AUD state burden | Chronic/stateful | `binge_days_per_year` or risk-drinking days, age | onset multiplier by risk-drinking frequency categories | persistent | A or B | onset baseline; OR schedule; remission/relapse; disability weight; causal weight. citeturn19search1turn6search1turn8search2turn9view0 |
| Mental health comorbidity | Chronic/stateful | AUD state, heavy drinking | multiplier or add-on LS loss | persistent | B (or A via disability) | comorbidity multiplier; causal weight; duration. citeturn13search3 |
| Relationship harms (divorce/conflict) | Chronic/event | `binge_days_per_year`, partner status | divorce hazard multiplier from heavy drinking | years | B | divorce HR; LS loss & duration; partner externality; causal weight. citeturn10search12turn14search1 |
| Employment/earnings impacts | Chronic | `binge_days_per_year`, AUD state | unemployment RR or wage penalty | years | B | unemployment RR; unemployment LS loss; duration; causal weight. citeturn10search22turn13search1 |

## Discrete parameter menus

Below are **paste-ready discrete menus**. All should be sampled **uniformly** in Monte Carlo. (If you later want non-uniform priors, keep the menus and add weights externally.)

### Global accounting and conversion

- `discount_rate_choices = [0.0, 0.015, 0.03, 0.05]` citeturn2view0  
- `grams_ethanol_per_standard_drink_choices = [10, 14]` citeturn11search8turn4search1  
- `qaly_to_wellby_factor_choices = [5, 6, 7, 8]` citeturn2view0  
- `causal_weight_choices = [0.25, 0.5, 0.75, 1.0]` (general-purpose sensitivity for observational channels)

### Exposure and binge settings

- `day_count_model_choices = ["constant", "poisson", "two_point"]`  
- `binge_threshold_drinks_choices = [4, 5]` citeturn11search8  
- `high_intensity_multiplier_choices = [2, 3]` citeturn11search8  
- `two_point_p_zero_choices = [0.0, 0.25, 0.5, 0.75]`  
- `two_point_high_drinks_choices = [4, 6, 8, 10, 12]`  
- `latency_half_life_years_choices = [2, 5, 10]` (chronic channels EMA smoothing)

### Acute injury and violence

(Use \(RR(g)=RR_{10}^{g/10}\) with \(g\) in grams ethanol/day.)

- `traffic_injury_rr_per_10g_choices = [1.18, 1.24, 1.30]` (center 1.24) citeturn3search4  
- `nontraffic_injury_rr_per_10g_choices = [1.26, 1.30, 1.34]` (center 1.30) citeturn3search4  
- `intentional_injury_rr_per_drink_choices = [1.25, 1.38, 1.50]` (center 1.38) citeturn3search4  

Baseline event-rate knobs (because RR needs an absolute baseline):
- `injury_baseline_prob_per_drinking_day_choices = [1e-4, 5e-4, 2e-3]` (calibrate to your population later)
- `violence_baseline_prob_per_binge_day_choices = [2e-5, 1e-4, 5e-4]` (calibrate to setting)

Severity knobs (DALY per event; keep deliberately coarse to avoid false precision):
- `injury_daly_per_nonfatal_event_choices = [0.005, 0.02, 0.05]`
- `injury_daly_per_fatal_event_choices = [20, 30, 40]` (only if you are *not* already modeling mortality endogenously)
- `injury_case_fatality_choices = [0.002, 0.005, 0.01]`

Externality multiplier knobs:
- `traffic_fatality_other_victim_share_choices = [0.40, 0.52]` (U.S. 2022 ≈0.40; WHO global road-crash “someone else’s drinking” share implies large non-drinker victim share) citeturn20search1turn20search2  
- `traffic_injury_externality_multiplier_choices = [0.5, 1.0, 1.5]` (expected other-harm per self-harm, coarse)

### Acute poisoning

- `poisoning_prob_per_high_intensity_day_choices = [1e-6, 1e-5, 1e-4]` citeturn11search8turn11search1  
- `poisoning_case_fatality_choices = [0.005, 0.01, 0.02]`  
- `poisoning_daly_nonfatal_choices = [0.01, 0.05, 0.2]`

### Hangover / next-day impairment (direct wellbeing)

- `hangover_prob_given_binge_choices = [0.3, 0.5, 0.7, 0.9]`  
- `hangover_ls_loss_per_day_choices = [0.05, 0.1, 0.2, 0.4]`  
- `hangover_duration_days_choices = [1, 2]`  
- `hangover_applies_to_next_day_choices = [True]`  
Evidence anchors for plausibility: hangover is associated with sizeable presenteeism/absenteeism and reduced work performance. citeturn10search2

### Cancer

Breast cancer (per +10g/day ethanol; map grams/day accordingly):
- `breast_cancer_rr_per_10g_day_choices = [1.05, 1.07, 1.10]` citeturn4search1turn4search0  
- `cancer_latency_half_life_years_choices = [5, 10, 15]` (cancer-specific lag)  
- `cancer_causal_weight_choices = [0.75, 1.0]` (carcinogenicity is strongly supported) citeturn20search2turn5search4  

Overall alcohol-attributable cancer aggregate (conservative slope):
- `all_cancer_rr_per_10g_day_choices = [1.02, 1.04, 1.06]` citeturn4search12turn5search4

### Liver cirrhosis

Use piecewise log-linear anchored to the 2023 dose-response meta-analysis:
- `cirrhosis_rr_mortality_at_25g_choices = [2.0, 2.65, 3.2]` citeturn4search3  
- `cirrhosis_rr_mortality_at_50g_choices = [5.5, 6.83, 8.0]` citeturn4search3  
- `cirrhosis_rr_mortality_at_100g_choices = [12.0, 16.38, 20.0]` citeturn4search3  
- `cirrhosis_latency_half_life_years_choices = [3, 5, 10]`

### Cardiovascular

Atrial fibrillation slope:
- `af_rr_per_drink_day_choices = [1.03, 1.06, 1.08]` citeturn5search1  

IHD (protective) as separable component:
- `include_ihd_protection_choices = [False, True]`
- `ihd_protective_rr_nadir_choices = [0.85, 0.95, 1.0]` (capture weak/uncertain protection) citeturn5search0turn18search1  
- `binge_negates_ihd_protection_choices = [True, False]` (default True) citeturn18search1turn18search4  

### AUD state model

Prevalence anchors:
- `aud_past_year_prevalence_anchor_choices = [0.08, 0.103, 0.13]` (U.S. adults past-year AUD ≈10.3% in 2024 NSDUH, plus sensitivity) citeturn6search0  

Onset multipliers by risk-drinking frequency (5+/4+ days):
- `risk_drinking_frequency_categories_choices = ["never", "<1_month", "1_3_month", "1_2_week", "3_4_week", "daily"]` citeturn19search1  
- `aud_incidence_or_by_risk_freq_choices = [1.0, 1.35, 2.10, 2.69, 5.27, 7.23]` citeturn19search1  

Baseline annual onset probability (calibration knob; you’ll calibrate to prevalence by age/sex if available):
- `aud_onset_base_prob_per_year_choices = [0.002, 0.005, 0.01]`

Remission/relapse (simplified):
- `aud_remission_prob_per_year_choices = [0.08, 0.15, 0.25]`  
- `aud_relapse_prob_per_year_if_abstinent_choices = [0.02, 0.05, 0.10]`  
- `aud_relapse_multiplier_if_risk_drinking_choices = [3, 6, 10]` (consistent with large relapse risk differences by drinking status) citeturn6search1  

AUD disability weights (choose one severity mapping approach):
- `aud_disability_weight_choices_gbd2013 = [0.123, 0.235, 0.366]` (very mild / mild / severe examples) citeturn8search2  
- `aud_disability_weight_choices_meta2025 = [0.167, 0.256, 0.366]` (mild / moderate / severe summary) citeturn9view0  

### Mental health comorbidity

- `aud_depression_ls_addon_choices = [0.0, 0.2, 0.5, 1.0]`  
- `mental_health_causal_weight_choices = [0.25, 0.5, 0.75]` (directionality is mixed in MR evidence) citeturn13search3turn13search15

### Relationship and labor-market

Divorce risk multiplier (heavy drinking):
- `divorce_hr_heavy_drinking_choices = [1.2, 1.4, 1.6]` citeturn10search12  
Relationship wellbeing loss (high-uncertainty, heterogeneous):
- `relationship_ls_loss_per_year_choices = [0.0, 0.2, 0.5, 1.0]` citeturn14search1turn17search5  
- `relationship_harm_duration_years_choices = [1, 3, 5]`

Employment/unemployment conversion anchors:
- `unemployment_ls_loss_per_year_choices = [0.4, 0.64, 0.9]` citeturn13search1  
- `problem_drinking_unemployment_rr_choices = [1.0, 1.2, 1.4]` citeturn10search7turn10search22  
- `labor_causal_weight_choices = [0.25, 0.5, 0.75]`

## Minimal implementable formulas

Below are “first-pass, codeable” formulas that match your exposure stream. (They are intentionally compact; the parameter menus above carry the uncertainty.)

### Exposure preprocessing

- `g_today = drinks_today * grams_ethanol_per_standard_drink`
- `binge_today = (drinks_today >= binge_threshold)`
- `high_intensity_today = (drinks_today >= high_intensity_multiplier * binge_threshold)`

For chronic lagging (EMA with half-life \(H\) years):
- `ema_y = ema_{y-1} * exp(-ln(2)/H) + gbar_y * (1 - exp(-ln(2)/H))`

### Acute event probability per day (injury/violence/poisoning)

For a channel with RR per 10g \(RR_{10}\):
- `RR = RR_10 ** (g_today / 10.0)`
- `p_event_today = p0_event_per_drinking_day * RR` if `drinks_today>0` else `0`

Poisoning (step):
- `p_poisoning_today = p_poisoning_per_high_intensity_day if high_intensity_today else 0`

Expected daily LS loss (direct wellbeing, e.g., hangover next day):
- If hangover is realized for tomorrow: `ls_next_day -= hangover_ls_loss_per_day`

### Chronic burden per year (DALY-rate style)

For each chronic health outcome \(i\):
- `RR_i = exp(beta_i * (ema_y / 10.0))`  (beta per 10g)
- `daly_loss_i_y = baseline_daly_rate_i(age, sex) * (RR_i - 1)`  
- `utilon_loss_i_y = daly_loss_i_y * qaly_to_wellby_factor * causal_weight_i`

If you don’t have `baseline_daly_rate_i(age, sex)`, a simulation-ready calibration is:
- Provide `baseline_daly_rate_i` as a parameter, or  
- Calibrate it from population data (e.g., GBD by age/sex/country via entity["organization","Institute for Health Metrics and Evaluation","gbd at uw seattle"] datasets).

### AUD simplified Markov model (annual timestep)

States: `S ∈ {NoAUD, AUD, Remission}`

Compute `risk_drinking_days_per_year` from your daily draws as:
- `risk_day = 1[drinks_today >= risk_threshold]` with `risk_threshold` = 5 (or 4) drinks/day. citeturn19search1turn11search8  

Map frequency to category and multiplier `OR_mult` using the NESARC schedule. citeturn19search1

Transitions:
- `p_onset = aud_onset_base_prob_per_year * OR_mult`
- `p_remit = aud_remission_prob_per_year`
- `p_relapse = aud_relapse_prob_per_year_if_abstinent * (aud_relapse_multiplier_if_risk_drinking if risk_drinking_days_per_year>0 else 1)`

State-dependent utilon losses:
- Pathway A: `ls_loss_year = aud_disability_weight * qaly_to_wellby_factor` citeturn8search2turn9view0  
- Optional add-on Pathway B: `ls_loss_year += aud_depression_ls_addon * mental_health_causal_weight` citeturn13search3

### Externalities accounting (traffic example)

If you simulate a traffic fatality/injury event attributable to the drinker:
- `harm_total = harm_to_self * (1 + traffic_externality_multiplier)`
where `traffic_externality_multiplier` is sampled from your externality menu and can be anchored using U.S. shares (about 40% of alcohol-impaired crash fatalities are non-driver victims). citeturn20search1turn20search2

## Uncertainty guidance by channel

This section identifies the **dominant uncertainty drivers** and indicates which menus matter most.

### Acute injury and traffic harms
Top uncertainty drivers:
- **Baseline “exposure-conditional” event rates** (how often your simulated person engages in high-risk contexts when drinking).  
- **Externality multiplier** (share of victim harm borne by others). citeturn20search1turn20search2  
- **Severity per event** (DALY/event, proportion fatal).

Use:
- `injury_baseline_prob_per_drinking_day_choices`
- `traffic_injury_externality_multiplier_choices`
- `injury_daly_per_*_choices`, `injury_case_fatality_choices`

### Cancer
Top uncertainty drivers:
- **RR slope at low doses** (still debated in public discourse but meta-analyses support risk rising even at low intake for breast cancer). citeturn4search1turn20search2  
- **Latency mapping** (how to map current drinking to long-run hazard).  
- **QALY→WELLBY conversion factor**. citeturn2view0  

Use:
- `breast_cancer_rr_per_10g_day_choices`
- `cancer_latency_half_life_years_choices`
- `qaly_to_wellby_factor_choices`

### Cirrhosis
Top uncertainty drivers:
- **RR curve convexity at high doses** (strong nonlinearity). citeturn4search3  
- **Latency / cumulative exposure**.  
- **Baseline liver-disease burden in your target population**.

Use:
- `cirrhosis_rr_*_choices`
- `cirrhosis_latency_half_life_years_choices`

### Cardiovascular
Top uncertainty drivers:
- **Whether to include IHD “protection” at all**, and under what drinking patterns (binge can remove apparent benefit). citeturn18search1turn5search0  
- **Which CVD endpoints dominate** (AF and BP are clearer harms). citeturn5search1turn5search2  
- **Netting policy** (this negative module should keep protection separable unless you explicitly choose to net at a higher layer).

Use:
- `include_ihd_protection_choices`
- `binge_negates_ihd_protection_choices`
- `af_rr_per_drink_day_choices`

### AUD
Top uncertainty drivers:
- **Mapping from your binge/risk-day counts to AUD onset** (population heterogeneity). citeturn19search1  
- **Remission/relapse dynamics** (strong dependence on post-remission drinking pattern). citeturn6search1  
- **Disability weight / wellbeing decrement** choice. citeturn8search2turn9view0  

Use:
- `aud_onset_base_prob_per_year_choices`
- `aud_incidence_or_by_risk_freq_choices`
- `aud_remission_prob_per_year_choices`, `aud_relapse_multiplier_if_risk_drinking_choices`
- `aud_disability_weight_choices_*`

### Hangover and productivity/wellbeing
Top uncertainty drivers:
- **Probability of hangover given binge day** (heterogeneous).  
- **LS loss per impaired day** (direct wellbeing mapping is not standardized).  
- **Whether to additionally map productivity loss into earnings** (double-counting hazard).

Use:
- `hangover_prob_given_binge_choices`
- `hangover_ls_loss_per_day_choices`

### Relationship and labor-market
Top uncertainty drivers:
- **Causality vs selection** (observational confounding is a major issue).  
- **Magnitude/duration of LS change** (highly heterogeneous).  
- **Externalities to partner/family**.

Use:
- `causal_weight_choices`
- `relationship_ls_loss_per_year_choices`, `relationship_harm_duration_years_choices`
- `problem_drinking_unemployment_rr_choices`, `unemployment_ls_loss_per_year_choices`


# Bibliography (sources used in the negative utilon alcohol report)

## Wellbeing units (WELLBY) and discounting / appraisal
- UK HM Treasury (Supplementary Green Book guidance). *Wellbeing guidance for appraisal: supplementary Green Book guidance* (PDF). https://assets.publishing.service.gov.uk/media/60fa9169d3bf7f0448719daf/Wellbeing_guidance_for_appraisal_-_supplementary_Green_Book_guidance.pdf
- Frijters, P. et al. *WELLBYs, cost-benefit analyses and the Easterlin Discount* (LSE eprints PDF). https://eprints.lse.ac.uk/114605/1/Frijters_PR3.pdf

## Alcohol definitions, binge/high-intensity drinking, and broad burden
- NIAAA. *Understanding the Dangers of Alcohol Overdose* (standard drink, binge & high-intensity definitions). https://www.niaaa.nih.gov/publications/brochures-and-fact-sheets/understanding-dangers-of-alcohol-overdose
- WHO. *Alcohol — Fact sheet*. https://www.who.int/news-room/fact-sheets/detail/alcohol
- CDC. *Alcohol and Public Health: Alcohol-Related Disease Impact (ARDI) / facts & stats*. https://www.cdc.gov/alcohol/facts-stats/index.html
- CDC (MMWR). *Deaths from Excessive Alcohol Use — United States* (MMWR report). https://www.cdc.gov/mmwr/volumes/73/wr/mm7308a1.htm
- NIAAA. *Alcohol Use Disorder (AUD) in the United States: age groups and demographic characteristics*. https://www.niaaa.nih.gov/alcohols-effects-health/alcohol-topics/alcohol-facts-and-statistics/alcohol-use-disorder-aud-united-states-age-groups-and-demographic-characteristics

## Acute injury / accidents and impaired driving externalities
- Taylor, B. et al. (2010). *A systematic review and meta-analysis of how acute alcohol consumption and injury risk relate to alcohol dose and drinking pattern.* **Drug and Alcohol Dependence** (ScienceDirect abstract page). https://www.sciencedirect.com/science/article/abs/pii/S0376871610000712
- CDC. *Impaired Driving: Facts* (shares of deaths among drivers vs others, etc.). https://www.cdc.gov/impaired-driving/facts/index.html

## Cancer
- (HHS) Office of the Assistant Secretary for Health. *Alcohol and Cancer Risk* (PDF). https://www.hhs.gov/sites/default/files/oash-alcohol-cancer-risk.pdf
- The Lancet (2018). *Alcohol use and burden / no-safe-level style synthesis* (full text). https://www.thelancet.com/article/S0140-6736%2818%2931310-2/fulltext
- The Lancet Oncology. *Global alcohol-attributable cancer burden* (full text). https://www.thelancet.com/journals/lanonc/article/PIIS1470-2045%2821%2900279-5/fulltext
- Systematic review/meta-analysis (2023). *Alcohol intake and breast cancer risk (prospective cohorts)* (PMC full text). https://pmc.ncbi.nlm.nih.gov/articles/PMC11629438/

## Liver disease / cirrhosis dose-response
- Dose-response meta-analysis (2023). *Alcohol consumption and liver cirrhosis morbidity/mortality* (PubMed record). https://pubmed.ncbi.nlm.nih.gov/37684424/

## Cardiovascular outcomes (harms + contested protection)
- Meta-analysis (AF). *Alcohol consumption and atrial fibrillation risk* (PMC full text). https://pmc.ncbi.nlm.nih.gov/articles/PMC9561500/
- Dose-response meta-analysis (BP). *Alcohol intake and blood pressure* (PMC full text). https://pmc.ncbi.nlm.nih.gov/articles/PMC10510850/
- Systematic review/meta-analysis (2014). *Alcohol and ischemic heart disease; patterning / reference group issues* (PMC full text). https://pmc.ncbi.nlm.nih.gov/articles/PMC4203905/
- “Burden of proof” style synthesis (2024). *Alcohol and ischemic heart disease protective-claim uncertainty* (PMC full text). https://pmc.ncbi.nlm.nih.gov/articles/PMC11094064/

## Alcohol use disorder (AUD): incidence/relapse + disability weights
- NESARC prospective analysis (risk drinking frequency → dependence incidence) (PMC full text). https://pmc.ncbi.nlm.nih.gov/articles/PMC2366117/
- Remission/recurrence follow-up (NESARC-related) (PubMed record). https://pubmed.ncbi.nlm.nih.gov/18034696/
- GBD disability weights paper (GBD 2013 DWs; includes AUD DWs) (The Lancet Global Health full text). https://www.thelancet.com/journals/langlo/article/PIIS2214-109X%2815%2900069-8/fulltext
- Meta-analysis (2025). *Disability weights for alcohol use disorder severity states* (PMC full text). https://pmc.ncbi.nlm.nih.gov/articles/PMC12595804/

## Mental health comorbidity (MR evidence)
- Mendelian randomization study on depression ↔ alcohol problems (PubMed record). https://pubmed.ncbi.nlm.nih.gov/41587741/

## Hangover / productivity loss
- Study on hangover-related absenteeism/presenteeism and productivity loss (PMC full text). https://pmc.ncbi.nlm.nih.gov/articles/PMC10855845/

## Relationship harms (divorce / adaptation)
- Cohort study on spousal drinking concordance/discordance and divorce (Ovid PDF). https://www.ovid.com/journals/alcer/pdf/10.1111/acer.12029~discordant-and-concordant-alcohol-use-in-spouses-as
- Review on life events and wellbeing adaptation (PMC full text). https://pmc.ncbi.nlm.nih.gov/articles/PMC3289759/
- Classic adaptation / wellbeing dynamics paper (PubMed record). https://pubmed.ncbi.nlm.nih.gov/16313658/

## Labor-market / employment and wellbeing conversion anchor
- BMJ Open. *Alcohol use/problems and employment outcomes* (article page). https://bmjopen.bmj.com/content/9/7/e029184
- Related employment/labor-market paper (PMC full text). https://pmc.ncbi.nlm.nih.gov/articles/PMC6451700/
- Unemployment → life satisfaction effect size (Springer link). https://link.springer.com/article/10.1007/s10902-025-00941-0

## Risky sexual behavior (optional channel)
- Meta-analysis / review on alcohol and sexual risk behaviors (PMC full text). https://pmc.ncbi.nlm.nih.gov/articles/PMC10270666/
- Related systematic review (PMC full text). https://pmc.ncbi.nlm.nih.gov/articles/PMC4553138/
- International Journal of Public Health article (Frontiers / SSPH journal site). https://www.ssph-journal.org/journals/international-journal-of-public-health/articles/10.3389/ijph.2023.1605669/full

