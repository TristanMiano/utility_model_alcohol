# Calibrating absolute per-day alcohol-harm probabilities by drinks

## Endpoint definitions

All probabilities are **per person per 24-hour day**, conditional on consuming **d standard drinks during that day**.

**Standard drink parameterization (ethanol grams per “drink”).** I report results for both:
- **US-style standard drink = 14 g ethanol** (0.6 fl oz pure alcohol). citeturn32search0turn32search3  
- **AU-style standard drink = 10 g ethanol.** citeturn32search1  

Because dose–response sources often use other standard-drink definitions, all functions are internally modeled as a function of **grams ethanol** and then mapped back to “number of drinks” under each convention (10 g vs 14 g).

### Acute nonfatal injury: `p_injury(d)`
**Primary measurable definition (used in tables):**  
**Emergency department (ED) “injury visit”** as defined in the **2019 National Hospital Ambulatory Medical Care Survey (NHAMCS)** ED summary tables: an ED visit with any-listed reason-for-visit and diagnosis codes related to **injury and poisoning**; **excluding adverse effects and complication codes**. citeturn13view0  

This definition is intentionally **ED-attended** (not “any injury”), because it is directly countable and anchors absolute incidence.

### Interpersonal violence: `p_violence(d)`
**Primary measurable definition (used in tables):**  
**Victimization severe enough to generate an ED visit for assault injury** (i.e., the patient presents with assault-related injury). The baseline incidence is taken from an **NHAMCS-based NCHS Data Brief** defining assault-related ED visits using **ICD-10-CM external cause codes X92–Y09** (excluding sexual assault, per the report). citeturn16view0turn15view0  

**Interpretation:** this is **violent injury victimization requiring ED care**, not perpetration. It is a deliberately “hard” endpoint.

**Alternative violence endpoints (not used for the main table):** police-recorded violent incidents (e.g., UCR/NIBRS) or survey self-reports of perpetration/victimization. These can be substituted if your utilon model wants “any violence involvement,” but they require different baselines and dose–response link functions than the ED-based one.

### Ethanol poisoning requiring emergency medical attention: `p_poisoning(d)`
**Primary measurable definition (used in tables):**  
**ED visit for ethanol poisoning/toxic effect of ethanol** identified by **ICD-10-CM T51.0x** (toxic effect of ethanol) in **any diagnosis field**, using nationally representative ED discharge data (HCUP Nationwide Emergency Department Sample, NEDS) as implemented in a recent national trends analysis. citeturn22view0turn21view0  

This endpoint is a closer match to “acute ethanol poisoning requiring emergency medical attention” than broader “alcohol intoxication” diagnosis families (e.g., F10.*) because it targets medically coded toxicity. citeturn22view0  

**Important overlap note (for simulation design):** under these definitions, **assault ED visits** and **ethanol-poisoning ED visits** are both **subsets of “injury visits”** (injury ED visits are not mutually exclusive with the other endpoints). This is typical in real administrative data; if your simulator requires mutually exclusive categories, you can construct a derived non-overlapping injury endpoint (e.g., `p_injury_nonassault(d) ≈ p_injury(d) − p_violence(d)`), but that is an approximation.

## Sources and evidence base

### Baseline absolute daily incidence (US adult population)
- **Injury ED visits (baseline for `p_injury(0)`).** NHAMCS 2019 ED summary tables report ~40.988 million annual injury ED visits (all ages) and provide age-specific rates; the “injury visit” definition and rates are documented in the same tables. citeturn13view0  
- **Assault injury ED visits (baseline for `p_violence(0)`).** NCHS Data Brief (NHAMCS 2019–2021) reports assault ED visit rates (overall and by age/sex), defining assault using ICD-10-CM X92–Y09. citeturn16view0turn15view0  
- **Ethanol poisoning ED visits (for poisoning calibration).** National estimates of ED visits involving **alcohol poisoning** (T51.0x) in 2020 and associated uncertainty are reported in a nationally representative HCUP-based analysis. citeturn22view0  

### Dose–response relative risk for injury and violence
For injury and violence, I use an **event-level ED case-crossover** dose–response study (18 countries, 37 EDs, n=13,119 injured drinkers) that models the odds of injury as a function of **number of standard drinks consumed in the 6 hours before the event**, compared with the same 6-hour window the prior week. citeturn18view0turn19view0turn20view0  

Key features we rely on:
- **Dose–response ORs are available at integer drink counts** (fractional polynomial estimates), including cause-specific curves (traffic, violence, fall, other). citeturn19view0turn20view0  
- The study’s “standard drink” equals **16 mL (12.8 g) ethanol**, enabling grams-based conversion to 10 g and 14 g “drinks.” citeturn19view0  

### Empirical anchor for poisoning scale
There is no widely used, high-quality **event-level** dose–response curve mapping “drinks consumed that day” → “ED visit coded T51.0x” in the same clean way as injury case-crossover studies. Therefore, poisoning requires a different strategy:
- I anchor the overall scale to the **national count of ED poisoning visits (T51.0x)** and to the **national number of binge-drinking episodes** for contextual plausibility. citeturn22view0turn24view0  
- For the high-dose tail, I constrain plausibility using clinical BAC-to-severity framing: BAC ~0.30–0.40 is commonly described as a range where alcohol poisoning is likely and loss of consciousness occurs. citeturn33search0  
- A literature example links **~15 drinks in ~6 hours** to BAC >0.3 for a typical 160-lb man, supporting the idea that ~15 “US drinks” is in a severe-risk region. citeturn33search15  

## Computation and conversion math

### Notation
- Let **d** = number of standard drinks consumed in the day (either **10 g** or **14 g**).  
- Let **g(d)** = total ethanol grams that day = `d * g_per_drink`.  
- Let **x(d)** = “Cherpitel-standard drinks” = `g(d) / 12.8` because the dose–response study uses 12.8 g per drink. citeturn19view0  
- Let **OR(x)** = odds ratio for the endpoint at consumption level x (from the ED case-crossover dose–response tables). citeturn19view0turn20view0  

### Baseline daily incidence `p0`
For endpoints with authoritative annual incidence rates:
- Convert annual per-person event rate **r** into a daily baseline probability:
\[
p_0 \approx \frac{r}{365}
\]
When events are rare (true here), the difference between modeling as Poisson rate vs probability is negligible.

### RR/OR to absolute daily probability
For injury and violence, the ED case-crossover study provides **ORs**. Because daily probabilities are small, OR is a close approximation to a daily risk multiplier. To remain conservative and prevent impossible probabilities, I use the hazard-multiplier form:
\[
p(d) = 1 - (1 - p_0)^{OR(x(d))}
\]
For small \(p_0\), this is essentially \(p(d) \approx p_0 \cdot OR(x(d))\).

### Interpolating ORs for non-integer `x(d)`
Because converting from 10 g or 14 g drinks yields non-integer \(x(d)\), I interpolate **on the log scale** between adjacent OR knots:
\[
\log OR(x) \approx (1-t)\log OR(x_0) + t\log OR(x_1), \quad t = \frac{x-x_0}{x_1-x_0}
\]
This preserves multiplicative structure and avoids artifacts.

### Worked example (injury, 4 US drinks)
Inputs:
- Baseline adult injury ED visit probability per day (derived below): \(p_0 \approx 0.0003388\). citeturn13view0  
- 4 US drinks → grams \(g = 4 \times 14 = 56\) g, so \(x = 56/12.8 = 4.375\). citeturn19view0turn20view0  
- Interpolated injury OR at \(x \approx 4.375\) is about 6.06 (from Table 2 fractional polynomial knots at 4 and 5 drinks). citeturn19view0  

Compute:
\[
p_{\text{injury}}(4) = 1 - (1 - 0.0003388)^{6.06} \approx 0.00205
\]
This matches the table below.

### Poisoning curve construction
Because T51.0x poisoning dose–response by drinks is not cleanly identified in the same event-level fashion, I use a grams-based exponential curve with two empirical anchors:

1) **Scale anchor near typical binge intensity.** BRFSS-based national estimates imply ~1.914 billion binge-drinking episodes in 2015 and mean binge intensity ~7.0 drinks. citeturn24view0  
A national ED discharge analysis gives ~34,585 alcohol-poisoning ED visits (T51.0x) in 2020. citeturn22view0  
A crude implied average probability of a T51.0x ED visit per binge episode is:
\[
p_{\text{ref}} \approx \frac{34{,}585}{1.914\times 10^9} \approx 1.81\times 10^{-5}
\]
(used only to set order-of-magnitude).

2) **High-dose anchor at severe BAC region.** BAC 0.30–0.40 is described as a range where alcohol poisoning is likely. citeturn33search0  
A reference example associates ~15 drinks in ~6 hours with BAC >0.3 for a typical male. citeturn33search15  
I therefore set a central assumption that at **210 g ethanol** (≈15 US drinks) the probability of a medically attended poisoning is on the order of **10⁻³ per day**, with wide uncertainty.

Model:
- For **d < 4**, set \(p_{\text{poisoning}}(d)=0\) (severe ethanol poisoning requiring ED care is extremely unlikely below that level in a general adult population).
- For **d ≥ 4**, let:
\[
p_{\text{poisoning}}(g) = p_{\text{ref}}\cdot \exp\left(\beta (g - g_{\text{ref}})\right)
\]
with \(\beta\) chosen so that \(p(210\text{ g})\) matches the selected high-dose anchor.

Uncertainty menus are explicitly generated from:
- The 95% CI on the national ED count for alcohol poisoning (thus varying \(p_{\text{ref}}\)). citeturn22view0  
- A low/central/high choice for \(p(210\text{ g})\), motivated by BAC severity framing. citeturn33search0turn33search15  

## Absolute per-day probability table for acute injury

### Baseline anchoring for adults
From NHAMCS 2019, injury ED visits are reported with age-specific rates (per 100 persons per year) and counts. citeturn13view0  
To produce a representative **adult (18+)** baseline, I:
- Use NHAMCS age-group populations implied by **visits / rate** (rates are per 100 persons). citeturn13view0  
- Approximate the 18–24 slice as **70%** of the 15–24 band (7 of 10 years), which is a small contribution to total adult population and is treated as minor uncertainty.

This yields an adult injury ED visit rate of about **0.124 visits/person-year**, i.e. baseline daily probability:
- \(p_{\text{injury}}(0) \approx 0.000339\) per day. citeturn13view0  

### Dose–response link
OR(d) is taken from the ED case-crossover dose–response table (fractional polynomial estimates) for “all injury causes,” using a 12.8 g standard drink internally. citeturn19view0  

### Table: `p_injury(d)` (central with uncertainty)
Uncertainty bands in parentheses reflect the **95% CI on the dose–response OR**, propagated through the absolute-risk conversion. citeturn19view0  

${inj_table_md}

Interpretation reminders:
- These probabilities are for **medically attended injury events (ED visits)**, not “any injury.” citeturn13view0  
- The relative-risk source is tied to drinking in the **6 hours prior to injury**; mapping from “drinks in a day” to that 6-hour window implicitly assumes the day’s drinks occur in a relatively concentrated period (a reasonable approximation for many harm-relevant episodes, but it can overstate risk if drinks are spread evenly). citeturn18view0turn19view0  

## Absolute per-day probability table for interpersonal violence

### Baseline anchoring
Assault ED visit rates (2019–2021 annual average) are reported as **~4.5 visits per 1,000 people per year overall**, with age gradients (18–24 highest). citeturn16view0turn15view0  
To obtain a representative adult baseline, I population-weight age-specific rates using the adult-age population structure implied by NHAMCS injury-visit tables (approximation; the total rate remains close to the NCHS overall). citeturn16view0turn13view0  

Resulting baseline used for `p_violence(0)`:
- \(p_{\text{violence}}(0) \approx 1.40\times 10^{-5}\) per day (≈0.51% annual probability under a Poisson approximation). citeturn16view0turn13view0  

### Dose–response link
The ED case-crossover study provides cause-specific ORs for **violence-related injury**, which aligns directly with assault-injury ED outcomes (victimization). citeturn20view0  

### Table: `p_violence(d)` (victimization severe enough to generate an assault ED visit)
Uncertainty bands in parentheses reflect the **95% CI on the violence OR curve**, propagated through the absolute-risk conversion. citeturn20view0  

${vio_table_md}

Interpretation reminders:
- This endpoint is **ED-treated assault injury** (a severe subset of all interpersonal violence). citeturn16view0  
- The OR source is again based on **6-hour pre-event drinking** (see injury section). citeturn18view0turn20view0  

## Absolute per-day probability table for ethanol poisoning requiring emergency care

### Baseline incidence reference
A nationally representative HCUP-based analysis reports **34,585 ED visits** involving alcohol poisoning (T51.0x) in 2020 (95% CI shown in the table), and provides age/sex breakdowns. citeturn22view0  
Summing adult age bands (18+) from those counts yields about **33,163 adult ED visits** for alcohol poisoning in 2020. citeturn22view0  

On an adult population denominator, this is an overall annual probability on the order of **10⁻4 per adult-year** (≈0.013%/year), i.e. extremely rare on a random day. citeturn22view0turn13view0  

### Why poisoning needs a different approach
Unlike injury and violence, the key missing ingredient is a high-quality event-level study that observes *both* (a) **number of drinks in the relevant window** and (b) a **T51.0x-coded ED visit**, enabling a clean dose–response OR by drink count. The administrative ED dataset has the outcome but not precise drinks consumed; surveys have drinks but not reliably coded T51.0x ED outcomes.

### Calibrated poisoning curve and uncertainty
I therefore provide a **calibrated grams-based curve**:
- scaled so the implied average risk per “typical binge episode” (≈7 US drinks) is consistent with national binge-episode volume and national poisoning counts, citeturn24view0turn22view0  
- and constrained so that very high doses (≈15 US drinks in a short window) are in the vicinity of BAC ranges described as “likely alcohol poisoning.” citeturn33search0turn33search15  

Uncertainty bands reflect discrete model choices (low / mid / high), described explicitly in the menu section.

### Table: `p_poisoning(d)` (ED visit with ICD-10-CM T51.0x)
${poi_table_md}

Interpretation reminders:
- These are **ED-coded ethanol poisonings (T51.0x)**, not “any intoxication.” citeturn22view0  
- The curve is intentionally **very small at low d** and increases steeply with dose.

## Monte Carlo menus and calibration sanity checks

### Sanity checks against annual risks
Using the **central curves** above and assuming independence across days, the implied probability of at least one event in a year (selected scenarios) is:

**Using 14 g drinks (US standard drink).** citeturn32search0  
${sanity_14}

**Using 10 g drinks (AU standard drink).** citeturn32search1  
${sanity_10}

Consistency cross-checks:
- The **baseline injury** annual probability implied by `p_injury(0)` is about **11.6%**, consistent with NHAMCS-scale injury ED visit incidence (≈0.124 visits per adult-year). citeturn13view0  
- The **baseline assault** annual probability implied by `p_violence(0)` is about **0.51%**, consistent with the NCHS assault ED visit rate scale (a few per 1,000 per year). citeturn16view0  
- The poisoning curve is calibrated to be broadly compatible with national poisoning ED visit counts and national binge-episode totals, while explicitly retaining wide uncertainty at high doses. citeturn22view0turn24view0turn33search0turn33search15  

### Paste-ready parameter menus

The following menus are **uniform-sampleable**. Each curve is a full vector for **d = 0..15**.

Interpretation of “curve choices”:
- **Injury & violence:** `[low, mid, high]` correspond to using the **lower CI / point estimate / upper CI** of the dose–response ORs (then converted to absolute risk). citeturn19view0turn20view0  
- **Poisoning:** `[low, mid, high]` correspond to:
  - low: poisoning ED count near its lower CI and a conservative high-dose anchor  
  - mid: point-count scale and mid high-dose anchor  
  - high: poisoning ED count near its upper CI and a less conservative high-dose anchor citeturn22view0turn33search0turn33search15  

```python
D_VALUES = list(range(0, 16))  # 0..15 drinks

p_injury_curve_choices_10g = [
    [0.000338760228021, 0.000584435049331, 0.000845525247125, 0.00108867709967, 0.00132360968369, 0.00153420591164, 0.00170824097098, 0.00187561906089, 0.00202439772378, 0.00217305131073, 0.00230974821665, 0.00244475932166, 0.00256910239283, 0.00269095320142, 0.00281111076236, 0.0031085361364],
    [0.000338760228021, 0.000648798020682, 0.00098638863559, 0.0013021274647, 0.00160031152312, 0.00189087266045, 0.00215626737232, 0.00238999346438, 0.00260339026539, 0.00281534036073, 0.00299890804461, 0.0031829560057, 0.00334301577124, 0.00349702175584, 0.00364577906373, 0.00367910930837],
    [0.000338760228021, 0.000675070686902, 0.00106799551229, 0.0014579771342, 0.00181038762189, 0.00215425598591, 0.00244912500956, 0.00273981648174, 0.00302377457797, 0.00328876125783, 0.00349851956058, 0.00370836005207, 0.00390843962929, 0.00410103939799, 0.00428841862416, 0.00437036198164],
]

p_injury_curve_choices_14g = [
    [0.000338760228021, 0.000728992983007, 0.00111467323858, 0.00146825606032, 0.00178944714242, 0.0020397300406, 0.00228625997669, 0.00250739941655, 0.00270121479339, 0.00289219627016, 0.00303249928776, 0.00317076038549, 0.00325087654338, 0.003330369457, 0.00340873598301, 0.00349702175584],
    [0.000338760228021, 0.00080955766615, 0.00124690261426, 0.00165363826939, 0.002049841708, 0.00239751552869, 0.00273130025075, 0.00301612707471, 0.00322046501738, 0.00342308188461, 0.00357347666843, 0.00372223459266, 0.00381021872227, 0.00389769914054, 0.00398414089838, 0.00426033378742],
    [0.000338760228021, 0.000851011558232, 0.00139376010918, 0.00187367522819, 0.00233257130329, 0.00275993116062, 0.00313596036963, 0.00349787113997, 0.00375327975242, 0.00400723590975, 0.00422547763981, 0.00444172418735, 0.00459507239034, 0.00474708501305, 0.00489722101311, 0.00520830147957],
]

p_violence_curve_choices_10g = [
    [1.40164095782e-05, 3.32176297862e-05, 5.61484022119e-05, 7.6295066246e-05, 9.32510946536e-05, 0.000110538997664, 0.000125214052709, 0.000139285448888, 0.000152074395013, 0.00016391225137, 0.000174154291894, 0.000184067073534, 0.000192793281704, 0.000201248628302, 0.0002095069296, 0.00023935786384],
    [1.40164095782e-05, 3.67279106084e-05, 6.57073156552e-05, 9.42667019794e-05, 0.000120159854708, 0.000143450145837, 0.000163687919604, 0.000183480715458, 0.000200437200006, 0.000217657917251, 0.000233214977845, 0.000248546346366, 0.000261827077494, 0.000275356124521, 0.00028838393479, 0.00029248196451],
    [1.40164095782e-05, 4.08230494343e-05, 7.95042429483e-05, 0.000118486693969, 0.000155432715029, 0.000205454960232, 0.000244142717043, 0.000286179557541, 0.000324750584257, 0.00036367525794, 0.000401639023126, 0.000440299443418, 0.000476345827654, 0.000512369033851, 0.000548418712396, 0.0000659314501876],
]

p_violence_curve_choices_14g = [
    [1.40164095782e-05, 4.41118331411e-05, 7.1428131788e-05, 9.56786039644e-05, 0.000116621817705, 0.000127636951636, 0.000138701941802, 0.000148362902824, 0.00015670558044, 0.000164878578015, 0.000171401986251, 0.000177795885858, 0.000186438319968, 0.000194775620876, 0.000202863259131, 0.00023935786384],
    [1.40164095782e-05, 5.15547676272e-05, 8.8543312984e-05, 0.000123861456344, 0.000155069937794, 0.000177075311138, 0.000198395409556, 0.000216142306209, 0.000231906343898, 0.000247609377514, 0.00026173014704, 0.000275356124521, 0.000291368688256, 0.000306689674401, 0.000321507797467, 0.000350268112133],
    [1.40164095782e-05, 5.85638907209e-05, 0.000111151868552, 0.000161182892171, 0.000206493617836, 0.000257323761051, 0.000282084049749, 0.000313156919206, 0.000343420843936, 0.000373999053338, 0.000402249826925, 0.000440299443418, 0.000465173551815, 0.000489237227842, 0.000512377279683, 0.000512358641224],
]

p_poisoning_curve_choices_10g = [
    [0, 0, 0, 0, 1.41035073382e-06, 1.95654265856e-06, 2.71488329362e-06, 3.76755950556e-06, 5.2287990514e-06, 7.25701873955e-06, 1.00722122829e-05, 1.39780259842e-05, 1.93970013648e-05, 2.69156854867e-05, 3.73531095881e-05, 6.57554002135e-05],
    [0, 0, 0, 0, 2.26050973515e-06, 3.23475431181e-06, 4.62916371762e-06, 6.62385989525e-06, 9.47845727201e-06, 1.35636921568e-05, 1.94095691028e-05, 2.77735472448e-05, 3.9741905711e-05, 5.68703030952e-05, 8.13837319058e-05, 0.000120009390884],
    [0, 0, 0, 0, 2.42550838434e-06, 3.96794615865e-06, 6.49136746997e-06, 1.06288992968e-05, 1.74094264569e-05, 2.85316441388e-05, 4.67684078689e-05, 7.66869198181e-05, 0.000125449412088, 0.000205383323923, 0.000336207328951, 0.000231399077358],
]

p_poisoning_curve_choices_14g = [
    [0, 0, 0, 0, 2.71290010278e-06, 4.46301269302e-06, 7.34313953916e-06, 1.20797324709e-05, 1.98676331813e-05, 3.26808056996e-05, 5.37601258075e-05, 8.84381606514e-05, 0.000145476770383, 0.000239119551225, 0.000393033129405, 0.0005],
    [0, 0, 0, 0, 4.01071915027e-06, 6.62385989525e-06, 1.09401687726e-05, 1.80671192178e-05, 2.98390035272e-05, 4.9281754959e-05, 8.13978032088e-05, 0.000134368781256, 0.000221822141259, 0.000366200066575, 0.000604709293878, 0.001],
    [0, 0, 0, 0, 4.12445653891e-06, 8.07754779106e-06, 1.58133712188e-05, 3.09569723683e-05, 6.06029044259e-05, 0.00011856857104, 0.000231930908776, 0.000453783060487, 0.000887965893526, 0.00173713247564, 0.00339842996608, 0.003],
]
```

## Bibliography (sources cited in the report)

* Cairns, C., & Kang, K. (2022). *National Hospital Ambulatory Medical Care Survey: 2019 Emergency Department Summary Tables* (NHAMCS). National Center for Health Statistics (CDC). ([CDC Stacks][1])

* Davis, D., & Santo, L. (2023). *Emergency department visit rates for assault: United States, 2019–2021* (NCHS Data Brief No. 481). National Center for Health Statistics (CDC). ([CDC][2])

* Cherpitel, C. J., Ye, Y., Bond, J., Borges, G., & Monteiro, M. (2015). Relative risk of injury from acute alcohol consumption: Modeling the dose-response relationship in emergency department data from 18 countries. *Addiction, 110*(2), 279–288. ([PubMed][3])

* Conrad, S. W., Greene, C. R., Dal Pan, G., Callahan, C. L., Meyer, T. E., & Radin, R. G. (2026). United States healthcare encounters for poisoning involving cannabis relative to other substances. *The American Journal of Drug and Alcohol Abuse.* (Author manuscript in PMC). ([PMC][4])

* National Institute on Alcohol Abuse and Alcoholism (NIAAA). (n.d.). *What is a standard drink?* ([NIAAA][5])

* Centers for Disease Control and Prevention (CDC). (2024). *About standard drink sizes.* ([CDC][6])

* Australian Government Department of Health, Disability and Ageing. (2024). *Standard drinks guide* (10g definition). ([Department of Health and Ageing][7])

* National Health and Medical Research Council (NHMRC, Australia). (n.d.). *Alcohol — What is a standard drink?* (10g definition). ([NHMRC][8])

* Cleveland Clinic. (2022). *Blood alcohol content (BAC): What it is & levels.* ([Cleveland Clinic][9])

* Kanny, D., Naimi, T. S., Liu, Y., & Brewer, R. D. (2020). Trends in total binge drinks per adult who reported binge drinking — United States, 2011–2017. *MMWR*, 69(2), 30–34. ([CDC][10])

* Bohm, M. K., Liu, Y., Esser, M. B., Mesnick, J. B., Lu, H., Pan, Y., & Greenlund, K. J. (2021). Binge drinking among adults, by select characteristics and state — United States, 2018. *MMWR*, 70(41), 1441–1446. ([CDC][11])

[1]: https://stacks.cdc.gov/view/cdc/115748?utm_source=chatgpt.com "National Hospital Ambulatory Medical Care Survey: 2019 Emergency Department Summary Tables"
[2]: https://www.cdc.gov/nchs/products/databriefs/db481.htm?utm_source=chatgpt.com "Products - Data Briefs - Number 481 - October 2023"
[3]: https://pubmed.ncbi.nlm.nih.gov/25355374/?utm_source=chatgpt.com "Relative risk of injury from acute alcohol consumption: modeling the dose-response relationship in emergency department data from 18 countries - PubMed"
[4]: https://pmc.ncbi.nlm.nih.gov/articles/PMC12889077/?utm_source=chatgpt.com "United States Healthcare Encounters for Poisoning Involving Cannabis Relative to Other Substances - PMC"
[5]: https://www.niaaa.nih.gov/alcohols-effects-health/what-standard-drink?utm_source=chatgpt.com "What Is A Standard Drink? | National Institute on Alcohol Abuse and Alcoholism (NIAAA)"
[6]: https://www.cdc.gov/alcohol/standard-drink-sizes/index.html?utm_source=chatgpt.com "About Standard Drink Sizes | Alcohol Use | CDC"
[7]: https://www.health.gov.au/topics/alcohol/about-alcohol/standard-drinks-guide?utm_source=chatgpt.com "Standard drinks guide | Australian Government Department of Health, Disability and Ageing"
[8]: https://www.nhmrc.gov.au/health-advice/alcohol?utm_source=chatgpt.com "Alcohol | NHMRC"
[9]: https://my.clevelandclinic.org/health/diagnostics/22689-blood-alcohol-content-bac?utm_source=chatgpt.com "Blood Alcohol Content (BAC): What It Is & Levels"
[10]: https://www.cdc.gov/mmwr/volumes/69/wr/mm6902a2.htm?utm_source=chatgpt.com "Trends in Total Binge Drinks per Adult Who Reported Binge Drinking — United States, 2011–2017 | MMWR"
[11]: https://www.cdc.gov/mmwr/volumes/70/wr/mm7041a2.htm?utm_source=chatgpt.com "Binge Drinking Among Adults, by Select Characteristics and State — United States, 2018 | MMWR"
