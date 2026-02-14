#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

struct ScriptConfig {
    int num_runs = 100;
    int seed = 12345;
    int years = 60;
    int days_per_year = 365;
    double drinks_per_day = 1.5;
    std::string day_count_model = "poisson";
    std::string mode = "expected";
    double two_point_p_zero = 0.5;
    int two_point_high_drinks = 6;
    int max_drinks_cap = 12;
    double discount_rate_annual = 0.03;
    int hist_bins = 70;
    std::vector<int> quantiles{1, 5, 10, 25, 50, 75, 90, 95, 99};
};

static ScriptConfig SCRIPT;
static std::mt19937 RNG;

void reseed(int seed) { RNG.seed(seed); }

double discount_factor_continuous(double r_annual, double t_years) {
    return std::exp(-r_annual * t_years);
}

template <typename T>
T pick_uniform(const std::vector<T>& v) {
    std::uniform_int_distribution<size_t> dist(0, v.size() - 1);
    return v[dist(RNG)];
}

struct PosModel {
    std::vector<double> p_social_day{0.1, 0.2, 0.35, 0.5};
    std::vector<double> baseline_stress{0.2, 0.4, 0.6, 0.8};
    std::vector<double> baseline_sociability{0.2, 0.4, 0.6, 0.8};
    std::vector<double> social_setting_quality{0.3, 0.5, 0.7, 0.9};
    std::vector<double> responsiveness{0.6, 0.8, 1.0, 1.2, 1.4};
    std::vector<double> saturation_rate{0.4, 0.7, 1.0, 1.3};
    std::vector<double> ls_per_session_score{0.15, 0.25, 0.35, 0.50};
    std::vector<double> w_enjoyment{0.8, 1.0, 1.2, 1.4};
    std::vector<double> w_relaxation{0.6, 0.8, 1.0, 1.2};
    std::vector<double> w_social{0.5, 0.8, 1.1, 1.4};
    std::vector<double> w_mood{0.3, 0.5, 0.7, 0.9};
    std::vector<double> max_daily_ls_uplift{1.0, 1.5, 2.0};
};

struct NegModel {
    std::vector<double> discount_rate_choices{0.0, 0.015, 0.03, 0.05};
    std::vector<int> grams_ethanol_per_standard_drink_choices{10, 14};
    std::vector<double> qaly_to_wellby_factor_choices{5, 6, 7, 8};
    std::vector<double> causal_weight_choices{0.25, 0.5, 0.75, 1.0};
    std::vector<int> binge_threshold_drinks_choices{4, 5};
    std::vector<int> high_intensity_multiplier_choices{2, 3};
    std::vector<double> latency_half_life_years_choices{2, 5, 10};
    std::vector<double> cancer_latency_half_life_years_choices{5, 10, 15};
    std::vector<double> cirrhosis_latency_half_life_years_choices{3, 5, 10};
    std::vector<double> traffic_injury_rr_per_10g_choices{1.18, 1.24, 1.30};
    std::vector<double> nontraffic_injury_rr_per_10g_choices{1.26, 1.30, 1.34};
    std::vector<double> intentional_injury_rr_per_drink_choices{1.25, 1.38, 1.50};
    std::vector<double> injury_baseline_prob_per_drinking_day_choices{1e-4, 2.5e-4, 5e-4, 1e-3};
    std::vector<double> violence_baseline_prob_per_binge_day_choices{5e-6, 1e-5, 2e-5, 5e-5};
    std::vector<double> injury_daly_per_nonfatal_event_choices{0.005, 0.02, 0.05};
    std::vector<double> injury_case_fatality_choices{0.002, 0.005, 0.01};
    std::vector<double> injury_daly_per_fatal_event_choices{20, 30, 40};
    std::vector<double> traffic_injury_externality_multiplier_choices{0.5, 1.0, 1.5};
    std::vector<double> poisoning_prob_per_high_intensity_day_choices{1e-6, 3e-6, 1e-5, 3e-5};
    std::vector<double> poisoning_case_fatality_choices{0.005, 0.01, 0.02};
    std::vector<double> poisoning_daly_nonfatal_choices{0.01, 0.05, 0.2};
    std::vector<double> hangover_prob_given_binge_choices{0.3, 0.5, 0.7, 0.9};
    std::vector<double> hangover_ls_loss_per_day_choices{0.05, 0.1, 0.2, 0.4};
    std::vector<int> hangover_duration_days_choices{1, 2};
    std::vector<double> breast_cancer_rr_per_10g_day_choices{1.05, 1.07, 1.10};
    std::vector<double> all_cancer_rr_per_10g_day_choices{1.02, 1.04, 1.06};
    std::vector<double> cancer_causal_weight_choices{0.75, 1.0};
    std::vector<double> cirrhosis_rr_mortality_at_25g_choices{2.0, 2.65, 3.2};
    std::vector<double> cirrhosis_rr_mortality_at_50g_choices{5.5, 6.83, 8.0};
    std::vector<double> cirrhosis_rr_mortality_at_100g_choices{12.0, 16.38, 20.0};
    std::vector<double> af_rr_per_drink_day_choices{1.03, 1.06, 1.08};
    std::vector<bool> include_ihd_protection_choices{false, true};
    std::vector<double> ihd_protective_rr_nadir_choices{0.85, 0.95, 1.0};
    std::vector<bool> binge_negates_ihd_protection_choices{true, false};
    std::vector<double> aud_onset_base_prob_per_year_choices{0.002, 0.005, 0.01};
    std::vector<double> aud_remission_prob_per_year_choices{0.08, 0.15, 0.25};
    std::vector<double> aud_relapse_prob_per_year_if_abstinent_choices{0.02, 0.05, 0.10};
    std::vector<double> aud_relapse_multiplier_if_risk_drinking_choices{3, 6, 10};
    std::vector<double> aud_disability_weight_choices{0.123, 0.235, 0.366};
    std::vector<double> aud_depression_ls_addon_choices{0.0, 0.2, 0.5, 1.0};
    std::vector<double> mental_health_causal_weight_choices{0.25, 0.5, 0.75};
    std::vector<double> baseline_daly_rate_all_cancer_choices{0.001, 0.003, 0.006};
    std::vector<double> baseline_daly_rate_cirrhosis_choices{0.0003, 0.001, 0.0025};
    std::vector<double> baseline_daly_rate_af_choices{0.0005, 0.0015, 0.003};
    std::vector<double> baseline_daly_rate_ihd_choices{0.001, 0.003, 0.006};
};

static PosModel POS_MODEL;
static NegModel NEG_MODEL;

int sample_drinks_today(double mean_drinks_per_day) {
    int cap = SCRIPT.max_drinks_cap;
    if (mean_drinks_per_day <= 0.0) return 0;
    if (SCRIPT.day_count_model == "constant") {
        return std::clamp(static_cast<int>(std::llround(mean_drinks_per_day)), 0, cap);
    }
    if (SCRIPT.day_count_model == "two_point") {
        int hi = std::clamp(SCRIPT.two_point_high_drinks, 0, cap);
        if (hi == 0) return 0;
        double p0_adj = 1.0 - (mean_drinks_per_day / hi);
        p0_adj = std::clamp(p0_adj, 0.0, 1.0);
        std::bernoulli_distribution draw_hi(1.0 - p0_adj);
        return draw_hi(RNG) ? hi : 0;
    }
    if (SCRIPT.day_count_model == "poisson") {
        std::poisson_distribution<int> draw(std::max(0.0, mean_drinks_per_day));
        return std::clamp(draw(RNG), 0, cap);
    }
    throw std::runtime_error("Unknown day_count_model");
}

std::vector<double> drinks_pmf(double mean_drinks_per_day) {
    int cap = SCRIPT.max_drinks_cap;
    std::vector<double> pmf(cap + 1, 0.0);
    if (mean_drinks_per_day <= 0.0) {
        pmf[0] = 1.0;
        return pmf;
    }
    if (SCRIPT.day_count_model == "constant") {
        int d = std::clamp(static_cast<int>(std::llround(mean_drinks_per_day)), 0, cap);
        pmf[d] = 1.0;
        return pmf;
    }
    if (SCRIPT.day_count_model == "two_point") {
        int hi = std::clamp(SCRIPT.two_point_high_drinks, 0, cap);
        if (hi == 0) { pmf[0] = 1.0; return pmf; }
        double p0_adj = 1.0 - (mean_drinks_per_day / hi);
        p0_adj = std::clamp(p0_adj, 0.0, 1.0);
        pmf[0] = p0_adj;
        pmf[hi] = 1.0 - p0_adj;
        return pmf;
    }
    if (SCRIPT.day_count_model == "poisson") {
        double lam = mean_drinks_per_day;
        double p = std::exp(-lam);
        pmf[0] = p;
        for (int d = 1; d < cap; ++d) {
            p = p * lam / d;
            pmf[d] = p;
        }
        double partial = std::accumulate(pmf.begin(), pmf.end() - 1, 0.0);
        pmf[cap] = std::max(0.0, 1.0 - partial);
        return pmf;
    }
    throw std::runtime_error("Unknown day_count_model");
}

void validate_pmf(const std::vector<double>& pmf, const std::string& where) {
    if (pmf.empty()) throw std::runtime_error("[" + where + "] PMF empty");
    for (double p : pmf) {
        if (!std::isfinite(p)) throw std::runtime_error("[" + where + "] PMF non-finite");
        if (p < 0.0) throw std::runtime_error("[" + where + "] PMF negative");
    }
    double total = std::accumulate(pmf.begin(), pmf.end(), 0.0);
    if (std::abs(total - 1.0) > 1e-9) {
        std::ostringstream oss;
        oss << "[" << where << "] PMF sum != 1.0 (" << total << ")";
        throw std::runtime_error(oss.str());
    }
}

template <typename Fn>
double expect_from_pmf(const std::vector<double>& pmf, Fn f) {
    double total = 0.0;
    for (size_t i = 0; i < pmf.size(); ++i) {
        if (pmf[i] == 0.0) continue;
        double v = f(static_cast<int>(i));
        if (!std::isfinite(v)) throw std::runtime_error("expect_from_pmf got non-finite value");
        total += pmf[i] * v;
    }
    return total;
}

template <typename Pred>
double prob_from_pmf(const std::vector<double>& pmf, Pred pred) {
    double s = 0.0;
    for (size_t i = 0; i < pmf.size(); ++i) if (pred(static_cast<int>(i))) s += pmf[i];
    return s;
}

struct PosPerson {
    double p_social_day, baseline_stress, baseline_sociability, social_setting_quality;
    double responsiveness, saturation_rate, ls_per_session_score;
    double w_enjoyment, w_relaxation, w_social, w_mood, max_daily_ls_uplift;
};

PosPerson sample_pos_person() {
    return {
        pick_uniform(POS_MODEL.p_social_day),
        pick_uniform(POS_MODEL.baseline_stress),
        pick_uniform(POS_MODEL.baseline_sociability),
        pick_uniform(POS_MODEL.social_setting_quality),
        pick_uniform(POS_MODEL.responsiveness),
        pick_uniform(POS_MODEL.saturation_rate),
        pick_uniform(POS_MODEL.ls_per_session_score),
        pick_uniform(POS_MODEL.w_enjoyment),
        pick_uniform(POS_MODEL.w_relaxation),
        pick_uniform(POS_MODEL.w_social),
        pick_uniform(POS_MODEL.w_mood),
        pick_uniform(POS_MODEL.max_daily_ls_uplift),
    };
}

double daily_positive_ls_uplift_det(const PosPerson& p, int d, bool social) {
    double gain = d <= 0 ? 0.0 : (1.0 - std::exp(-p.saturation_rate * d));
    double enjoyment = p.w_enjoyment * gain;
    double relaxation = p.w_relaxation * gain * (0.5 + 0.5 * p.baseline_stress);
    double social_term = 0.0;
    if (social) {
        double social_mult = p.social_setting_quality * (1.2 - 0.6 * p.baseline_sociability);
        social_term = p.w_social * gain * social_mult;
    }
    double mood = p.w_mood * gain;
    double ls = p.ls_per_session_score * p.responsiveness * (enjoyment + relaxation + social_term + mood);
    return std::clamp(ls, 0.0, p.max_daily_ls_uplift);
}

double expected_daily_positive_ls(const PosPerson& p, const std::vector<double>& pmf) {
    return expect_from_pmf(pmf, [&](int d) {
        double non = daily_positive_ls_uplift_det(p, d, false);
        double soc = daily_positive_ls_uplift_det(p, d, true);
        return (1.0 - p.p_social_day) * non + p.p_social_day * soc;
    });
}

struct NegParams {
    int grams_per_drink, binge_threshold, high_intensity_multiplier, hangover_duration_days;
    double qaly_to_wellby, discount_rate, causal_weight;
    double rr10_traffic, rr10_nontraffic, rr_per_drink_intentional;
    double p0_injury_per_drinking_day, p0_violence_per_binge_day;
    double daly_nonfatal_injury, injury_case_fatality, daly_fatal_injury, traffic_externality_multiplier;
    double p_poison_per_hi_day, poison_case_fatality, poison_daly_nonfatal;
    double p_hangover_given_binge, hangover_ls_loss_per_day;
    double half_life_chronic, half_life_cancer, half_life_cirrhosis;
    double rr10_all_cancer, cancer_causal_weight, baseline_daly_all_cancer;
    double rr_cirr_25, rr_cirr_50, rr_cirr_100, baseline_daly_cirrhosis;
    double rr_af_per_drink, baseline_daly_af;
    bool include_ihd_protection, binge_negates_ihd;
    double ihd_rr_nadir, baseline_daly_ihd;
    double aud_onset_base, aud_remission, aud_relapse_base, aud_relapse_mult_if_risk;
    double aud_disability_weight, aud_depression_ls_addon, mental_health_causal_weight;
};

NegParams sample_neg_params() {
    return {
        pick_uniform(NEG_MODEL.grams_ethanol_per_standard_drink_choices),
        pick_uniform(NEG_MODEL.binge_threshold_drinks_choices),
        pick_uniform(NEG_MODEL.high_intensity_multiplier_choices),
        pick_uniform(NEG_MODEL.hangover_duration_days_choices),
        pick_uniform(NEG_MODEL.qaly_to_wellby_factor_choices),
        pick_uniform(NEG_MODEL.discount_rate_choices),
        pick_uniform(NEG_MODEL.causal_weight_choices),
        pick_uniform(NEG_MODEL.traffic_injury_rr_per_10g_choices),
        pick_uniform(NEG_MODEL.nontraffic_injury_rr_per_10g_choices),
        pick_uniform(NEG_MODEL.intentional_injury_rr_per_drink_choices),
        pick_uniform(NEG_MODEL.injury_baseline_prob_per_drinking_day_choices),
        pick_uniform(NEG_MODEL.violence_baseline_prob_per_binge_day_choices),
        pick_uniform(NEG_MODEL.injury_daly_per_nonfatal_event_choices),
        pick_uniform(NEG_MODEL.injury_case_fatality_choices),
        pick_uniform(NEG_MODEL.injury_daly_per_fatal_event_choices),
        pick_uniform(NEG_MODEL.traffic_injury_externality_multiplier_choices),
        pick_uniform(NEG_MODEL.poisoning_prob_per_high_intensity_day_choices),
        pick_uniform(NEG_MODEL.poisoning_case_fatality_choices),
        pick_uniform(NEG_MODEL.poisoning_daly_nonfatal_choices),
        pick_uniform(NEG_MODEL.hangover_prob_given_binge_choices),
        pick_uniform(NEG_MODEL.hangover_ls_loss_per_day_choices),
        pick_uniform(NEG_MODEL.latency_half_life_years_choices),
        pick_uniform(NEG_MODEL.cancer_latency_half_life_years_choices),
        pick_uniform(NEG_MODEL.cirrhosis_latency_half_life_years_choices),
        pick_uniform(NEG_MODEL.all_cancer_rr_per_10g_day_choices),
        pick_uniform(NEG_MODEL.cancer_causal_weight_choices),
        pick_uniform(NEG_MODEL.baseline_daly_rate_all_cancer_choices),
        pick_uniform(NEG_MODEL.cirrhosis_rr_mortality_at_25g_choices),
        pick_uniform(NEG_MODEL.cirrhosis_rr_mortality_at_50g_choices),
        pick_uniform(NEG_MODEL.cirrhosis_rr_mortality_at_100g_choices),
        pick_uniform(NEG_MODEL.baseline_daly_rate_cirrhosis_choices),
        pick_uniform(NEG_MODEL.af_rr_per_drink_day_choices),
        pick_uniform(NEG_MODEL.baseline_daly_rate_af_choices),
        pick_uniform(NEG_MODEL.include_ihd_protection_choices),
        pick_uniform(NEG_MODEL.binge_negates_ihd_protection_choices),
        pick_uniform(NEG_MODEL.ihd_protective_rr_nadir_choices),
        pick_uniform(NEG_MODEL.baseline_daly_rate_ihd_choices),
        pick_uniform(NEG_MODEL.aud_onset_base_prob_per_year_choices),
        pick_uniform(NEG_MODEL.aud_remission_prob_per_year_choices),
        pick_uniform(NEG_MODEL.aud_relapse_prob_per_year_if_abstinent_choices),
        pick_uniform(NEG_MODEL.aud_relapse_multiplier_if_risk_drinking_choices),
        pick_uniform(NEG_MODEL.aud_disability_weight_choices),
        pick_uniform(NEG_MODEL.aud_depression_ls_addon_choices),
        pick_uniform(NEG_MODEL.mental_health_causal_weight_choices),
    };
}

double piecewise_log_rr(double g, double rr25, double rr50, double rr100) {
    if (g <= 0.0) return 1.0;
    auto lerp = [](double x, double x0, double x1, double y0, double y1) {
        double t = (x - x0) / (x1 - x0);
        return std::exp(std::log(y0) * (1.0 - t) + std::log(y1) * t);
    };
    if (g < 25.0) return lerp(g, 0.0, 25.0, 1.0, rr25);
    if (g < 50.0) return lerp(g, 25.0, 50.0, rr25, rr50);
    if (g < 100.0) return lerp(g, 50.0, 100.0, rr50, rr100);
    double slope = (std::log(rr100) - std::log(rr50)) / 50.0;
    return std::exp(std::log(rr100) + slope * (g - 100.0));
}

double rr_from_rr10(double rr10, double grams_per_day) {
    return std::pow(rr10, grams_per_day / 10.0);
}

struct AnnualNegBreakdown {
    double total = 0.0;
    double acute = 0.0;
    double hang = 0.0;
    double chronic = 0.0;
    double ihd = 0.0;
    double acute_traffic = 0.0;
    double acute_nontraffic = 0.0;
    double acute_violence = 0.0;
    double acute_poison = 0.0;
    double chronic_cancer = 0.0;
    double chronic_cirrhosis = 0.0;
    double chronic_af = 0.0;
};

AnnualNegBreakdown annual_negative_utilons_expected(
    const std::vector<double>& pmf,
    const NegParams& n,
    double ema_g,
    double ema_cancer,
    double ema_cirr,
    double p_binge_realized = -1.0,
    double p_hi_realized = -1.0) {
    const int dpy = SCRIPT.days_per_year;
    int binge = n.binge_threshold;
    int hi = n.high_intensity_multiplier * binge;
    double p_binge = p_binge_realized >= 0.0 ? p_binge_realized : prob_from_pmf(pmf, [&](int d){ return d >= binge;});
    double p_hi = p_hi_realized >= 0.0 ? p_hi_realized : prob_from_pmf(pmf, [&](int d){ return d >= hi;});

    auto grams_today = [&](int d){ return d * n.grams_per_drink; };
    auto rr_traffic_d = [&](int d){ return d <= 0 ? 0.0 : rr_from_rr10(n.rr10_traffic, grams_today(d)); };
    auto rr_non_d = [&](int d){ return d <= 0 ? 0.0 : rr_from_rr10(n.rr10_nontraffic, grams_today(d)); };

    double exp_rr_traffic = expect_from_pmf(pmf, rr_traffic_d);
    double exp_rr_non = expect_from_pmf(pmf, rr_non_d);

    double traffic_events = dpy * n.p0_injury_per_drinking_day * exp_rr_traffic;
    double nontraffic_events = dpy * n.p0_injury_per_drinking_day * exp_rr_non;
    double daly_injury = (1.0 - n.injury_case_fatality) * n.daly_nonfatal_injury + n.injury_case_fatality * n.daly_fatal_injury;
    double traffic_dalys = traffic_events * daly_injury * (1.0 + n.traffic_externality_multiplier);
    double nontraffic_dalys = nontraffic_events * daly_injury;

    auto rr_violence_d = [&](int d){ return d < binge ? 0.0 : std::pow(n.rr_per_drink_intentional, d); };
    double exp_rr_violence = expect_from_pmf(pmf, rr_violence_d);
    double violence_events = dpy * n.p0_violence_per_binge_day * exp_rr_violence;
    double violence_dalys = violence_events * daly_injury;

    double poisoning_events = dpy * p_hi * n.p_poison_per_hi_day;
    double daly_poison = (1.0 - n.poison_case_fatality) * n.poison_daly_nonfatal + n.poison_case_fatality * n.daly_fatal_injury;
    double poisoning_dalys = poisoning_events * daly_poison;

    double acute_traffic_utilons = traffic_dalys * n.qaly_to_wellby * n.causal_weight;
    double acute_nontraffic_utilons = nontraffic_dalys * n.qaly_to_wellby * n.causal_weight;
    double acute_violence_utilons = violence_dalys * n.qaly_to_wellby * n.causal_weight;
    double acute_poison_utilons = poisoning_dalys * n.qaly_to_wellby * n.causal_weight;
    double acute_utilons = acute_traffic_utilons + acute_nontraffic_utilons + acute_violence_utilons + acute_poison_utilons;
    double hang_days = dpy * p_binge * n.p_hangover_given_binge * n.hangover_duration_days;
    double hang_utilons = (hang_days / dpy) * n.hangover_ls_loss_per_day;

    double rr_cancer = rr_from_rr10(n.rr10_all_cancer, ema_cancer);
    double cancer_utilons = n.baseline_daly_all_cancer * std::max(0.0, rr_cancer - 1.0) * n.qaly_to_wellby * n.cancer_causal_weight;
    double rr_cirr = piecewise_log_rr(ema_cirr, n.rr_cirr_25, n.rr_cirr_50, n.rr_cirr_100);
    double cirr_utilons = n.baseline_daly_cirrhosis * std::max(0.0, rr_cirr - 1.0) * n.qaly_to_wellby * n.causal_weight;
    double drinks_equiv = ema_g / std::max(1e-9, static_cast<double>(n.grams_per_drink));
    double rr_af = std::pow(n.rr_af_per_drink, drinks_equiv);
    double af_utilons = n.baseline_daly_af * std::max(0.0, rr_af - 1.0) * n.qaly_to_wellby * n.causal_weight;
    double chronic_utilons = cancer_utilons + cirr_utilons + af_utilons;

    double ihd_term = 0.0;
    if (n.include_ihd_protection) {
        double ihd_rr = (n.binge_negates_ihd && p_binge > 0.0) ? 1.0 : n.ihd_rr_nadir;
        ihd_term = n.baseline_daly_ihd * (ihd_rr - 1.0) * n.qaly_to_wellby * n.causal_weight;
    }
    return {
        acute_utilons + hang_utilons + chronic_utilons,
        acute_utilons,
        hang_utilons,
        chronic_utilons,
        ihd_term,
        acute_traffic_utilons,
        acute_nontraffic_utilons,
        acute_violence_utilons,
        acute_poison_utilons,
        cancer_utilons,
        cirr_utilons,
        af_utilons,
    };
}

double aud_or_multiplier_from_risk_days_per_year(double risk_days) {
    if (risk_days <= 0.0) return 1.0;
    double per_month = risk_days / 12.0;
    double per_week = risk_days / 52.0;
    if (per_month < 1.0) return 1.35;
    if (per_month <= 3.0) return 2.10;
    if (per_week <= 2.0) return 2.69;
    if (per_week <= 4.0) return 5.27;
    return 7.23;
}

double simulate_aud_lifetime_utilons(const std::vector<double>& pmf, const NegParams& n) {
    double p_risk_day = prob_from_pmf(pmf, [&](int d){ return d >= n.binge_threshold;});
    double risk_days = SCRIPT.days_per_year * p_risk_day;
    double or_mult = aud_or_multiplier_from_risk_days_per_year(risk_days);

    int state = 0;
    double total = 0.0;
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    for (int y = 0; y < SCRIPT.years; ++y) {
        double disc = discount_factor_continuous(SCRIPT.discount_rate_annual, y + 0.5);
        if (state == 1) {
            double ls_loss = n.aud_disability_weight * n.qaly_to_wellby + n.aud_depression_ls_addon * n.mental_health_causal_weight;
            total += disc * ls_loss;
        }
        double u = u01(RNG);
        if (state == 0) {
            if (u < n.aud_onset_base * or_mult) state = 1;
        } else if (state == 1) {
            if (u < n.aud_remission) state = 2;
        } else {
            double relapse = n.aud_relapse_base * (risk_days > 0.0 ? n.aud_relapse_mult_if_risk : 1.0);
            if (u < relapse) state = 1;
        }
    }
    return total * n.causal_weight;
}

struct DailyState {
    int drinks_today = 0;
    bool traffic_event = false;
    bool nontraffic_event = false;
    bool violence_event = false;
    bool poison_event = false;
    int acute_event_count = 0;
    bool aud_active = false;
    bool alive = true;
    double pos_ls = 0.0;
    double acute_utilons = 0.0;
    double hang_utilons = 0.0;
    double chronic_utilons = 0.0;
    double ihd_term = 0.0;
};

struct LifeState {
    bool alive = true;
    // AUD state coding: 0 = never AUD, 1 = active AUD, 2 = remission.
    int aud_state = 0;
    int hangover_days_remaining = 0;
    double ema_g = 0.0;
    double ema_ca = 0.0;
    double ema_ci = 0.0;
};

struct DailyEventResult {
    bool traffic_event = false;
    bool nontraffic_event = false;
    bool violence_event = false;
    bool poison_event = false;
    int acute_event_count = 0;
    double acute_utilons = 0.0;
    double acute_traffic_utilons = 0.0;
    double acute_nontraffic_utilons = 0.0;
    double acute_violence_utilons = 0.0;
    double acute_poison_utilons = 0.0;
    double hang_utilons = 0.0;
    bool fatal_event = false;
};

DailyEventResult simulate_daily_events(int drinks_today, const NegParams& neg, LifeState& state, double aud_event_risk_multiplier) {
    DailyEventResult out;
    if (!state.alive) return out;

    int grams_today = drinks_today * neg.grams_per_drink;
    bool is_drinking = drinks_today > 0;
    bool is_binge = drinks_today >= neg.binge_threshold;
    bool is_hi = drinks_today >= neg.high_intensity_multiplier * neg.binge_threshold;

    double p_traffic = 0.0;
    double p_nontraffic = 0.0;
    if (is_drinking) {
        p_traffic = std::clamp(neg.p0_injury_per_drinking_day * rr_from_rr10(neg.rr10_traffic, grams_today) * aud_event_risk_multiplier, 0.0, 1.0);
        p_nontraffic = std::clamp(neg.p0_injury_per_drinking_day * rr_from_rr10(neg.rr10_nontraffic, grams_today) * aud_event_risk_multiplier, 0.0, 1.0);
    }
    std::bernoulli_distribution traffic_draw(p_traffic);
    std::bernoulli_distribution nontraffic_draw(p_nontraffic);
    out.traffic_event = traffic_draw(RNG);
    out.nontraffic_event = nontraffic_draw(RNG);

    double p_violence = is_binge
        ? std::clamp(neg.p0_violence_per_binge_day * std::pow(neg.rr_per_drink_intentional, drinks_today) * aud_event_risk_multiplier, 0.0, 1.0)
        : 0.0;
    std::bernoulli_distribution violence_draw(p_violence);
    out.violence_event = violence_draw(RNG);

    double p_poison = is_hi ? std::clamp(neg.p_poison_per_hi_day * aud_event_risk_multiplier, 0.0, 1.0) : 0.0;
    std::bernoulli_distribution poison_draw(p_poison);
    out.poison_event = poison_draw(RNG);

    out.acute_event_count = static_cast<int>(out.traffic_event) + static_cast<int>(out.nontraffic_event) +
        static_cast<int>(out.violence_event) + static_cast<int>(out.poison_event);

    double daly_injury = (1.0 - neg.injury_case_fatality) * neg.daly_nonfatal_injury + neg.injury_case_fatality * neg.daly_fatal_injury;
    if (out.traffic_event) out.acute_traffic_utilons += daly_injury * (1.0 + neg.traffic_externality_multiplier) * neg.qaly_to_wellby * neg.causal_weight;
    if (out.nontraffic_event) out.acute_nontraffic_utilons += daly_injury * neg.qaly_to_wellby * neg.causal_weight;
    if (out.violence_event) out.acute_violence_utilons += daly_injury * neg.qaly_to_wellby * neg.causal_weight;
    if (out.poison_event) {
        double daly_poison = (1.0 - neg.poison_case_fatality) * neg.poison_daly_nonfatal + neg.poison_case_fatality * neg.daly_fatal_injury;
        out.acute_poison_utilons += daly_poison * neg.qaly_to_wellby * neg.causal_weight;
    }
    out.acute_utilons = out.acute_traffic_utilons + out.acute_nontraffic_utilons + out.acute_violence_utilons + out.acute_poison_utilons;

    if (is_binge) {
        std::bernoulli_distribution hang_draw(std::clamp(neg.p_hangover_given_binge, 0.0, 1.0));
        if (hang_draw(RNG)) {
            state.hangover_days_remaining = std::max(state.hangover_days_remaining, neg.hangover_duration_days);
        }
    }
    if (state.hangover_days_remaining > 0) {
        out.hang_utilons = neg.hangover_ls_loss_per_day / SCRIPT.days_per_year;
        --state.hangover_days_remaining;
    }

    double p_die = 0.0;
    if (out.traffic_event || out.nontraffic_event || out.violence_event) p_die = std::max(p_die, neg.injury_case_fatality);
    if (out.poison_event) p_die = std::max(p_die, neg.poison_case_fatality);
    std::bernoulli_distribution death_draw(std::clamp(p_die, 0.0, 1.0));
    out.fatal_event = death_draw(RNG);
    state.alive = !out.fatal_event;

    return out;
}

struct SimOut {
    double pos, neg, net, acute, hang, chronic, aud, ihd;
    double acute_traffic, acute_nontraffic, acute_violence, acute_poison;
    double chronic_cancer, chronic_cirrhosis, chronic_af;
};

SimOut simulate_life_rollout(const PosPerson& pos_person, const NegParams& neg) {
    int total_days = SCRIPT.years * SCRIPT.days_per_year;
    std::vector<DailyState> days;
    days.reserve(total_days);

    auto alpha_from_half_life = [](double H){ return H <= 0 ? 0.0 : std::exp(-std::log(2.0)/H); };
    double a_g = alpha_from_half_life(neg.half_life_chronic);
    double a_ca = alpha_from_half_life(neg.half_life_cancer);
    double a_ci = alpha_from_half_life(neg.half_life_cirrhosis);

    LifeState life_state;

    double pos_total=0.0, neg_total=0.0, neg_acute=0.0, neg_hang=0.0, neg_chronic=0.0, ihd_total=0.0, neg_aud=0.0;
    double neg_acute_traffic=0.0, neg_acute_nontraffic=0.0, neg_acute_violence=0.0, neg_acute_poison=0.0;
    double neg_chronic_cancer=0.0, neg_chronic_cirrhosis=0.0, neg_chronic_af=0.0;
    std::uniform_real_distribution<double> u01(0.0, 1.0);

    for (int day = 0; day < total_days; ++day) {
        if (!life_state.alive) break;

        DailyState st;
        // Assumption: active AUD increases next-day drinking intensity, while remission has partial persistence.
        // These multipliers are intentionally conservative placeholders pending direct calibration data.
        double aud_drink_multiplier = 1.0;
        if (life_state.aud_state == 1) aud_drink_multiplier = 1.35;
        else if (life_state.aud_state == 2) aud_drink_multiplier = 0.90;
        st.drinks_today = sample_drinks_today(SCRIPT.drinks_per_day * aud_drink_multiplier);
        double t_years = (day + 0.5) / static_cast<double>(SCRIPT.days_per_year);
        double disc = discount_factor_continuous(SCRIPT.discount_rate_annual, t_years);

        std::bernoulli_distribution social_draw(pos_person.p_social_day);
        bool social_today = social_draw(RNG);
        st.pos_ls = daily_positive_ls_uplift_det(pos_person, st.drinks_today, social_today);
        pos_total += disc * (st.pos_ls / SCRIPT.days_per_year);

        int grams_today = st.drinks_today * neg.grams_per_drink;
        life_state.ema_g = a_g * life_state.ema_g + (1.0 - a_g) * grams_today;
        life_state.ema_ca = a_ca * life_state.ema_ca + (1.0 - a_ca) * grams_today;
        life_state.ema_ci = a_ci * life_state.ema_ci + (1.0 - a_ci) * grams_today;

        bool is_binge = st.drinks_today >= neg.binge_threshold;

        // Assumption: active AUD elevates acute event risk above dose-only effects; remission retains a smaller excess risk.
        double aud_event_risk_multiplier = 1.0;
        if (life_state.aud_state == 1) aud_event_risk_multiplier = 1.25;
        else if (life_state.aud_state == 2) aud_event_risk_multiplier = 1.08;
        DailyEventResult day_events = simulate_daily_events(st.drinks_today, neg, life_state, aud_event_risk_multiplier);
        st.traffic_event = day_events.traffic_event;
        st.nontraffic_event = day_events.nontraffic_event;
        st.violence_event = day_events.violence_event;
        st.poison_event = day_events.poison_event;
        st.acute_event_count = day_events.acute_event_count;
        st.acute_utilons = day_events.acute_utilons;
        st.hang_utilons = day_events.hang_utilons;
        neg_acute_traffic += disc * day_events.acute_traffic_utilons;
        neg_acute_nontraffic += disc * day_events.acute_nontraffic_utilons;
        neg_acute_violence += disc * day_events.acute_violence_utilons;
        neg_acute_poison += disc * day_events.acute_poison_utilons;

        double rr_cancer = rr_from_rr10(neg.rr10_all_cancer, life_state.ema_ca);
        double cancer_utilons_year = neg.baseline_daly_all_cancer * std::max(0.0, rr_cancer - 1.0) * neg.qaly_to_wellby * neg.cancer_causal_weight;
        double rr_cirr = piecewise_log_rr(life_state.ema_ci, neg.rr_cirr_25, neg.rr_cirr_50, neg.rr_cirr_100);
        double cirr_utilons_year = neg.baseline_daly_cirrhosis * std::max(0.0, rr_cirr - 1.0) * neg.qaly_to_wellby * neg.causal_weight;
        double drinks_equiv = life_state.ema_g / std::max(1e-9, static_cast<double>(neg.grams_per_drink));
        double rr_af = std::pow(neg.rr_af_per_drink, drinks_equiv);
        double af_utilons_year = neg.baseline_daly_af * std::max(0.0, rr_af - 1.0) * neg.qaly_to_wellby * neg.causal_weight;
        neg_chronic_cancer += disc * (cancer_utilons_year / SCRIPT.days_per_year);
        neg_chronic_cirrhosis += disc * (cirr_utilons_year / SCRIPT.days_per_year);
        neg_chronic_af += disc * (af_utilons_year / SCRIPT.days_per_year);
        st.chronic_utilons = (cancer_utilons_year + cirr_utilons_year + af_utilons_year) / SCRIPT.days_per_year;

        if (neg.include_ihd_protection) {
            double ihd_rr = (neg.binge_negates_ihd && is_binge) ? 1.0 : neg.ihd_rr_nadir;
            st.ihd_term = (neg.baseline_daly_ihd * (ihd_rr - 1.0) * neg.qaly_to_wellby * neg.causal_weight) / SCRIPT.days_per_year;
        }

        int day_of_month = day % 30;
        if (day_of_month == 0 && day > 0) {
            double risk_days = 0.0;
            double drinks_recent = 0.0;
            int start = std::max(0, day - 30);
            for (int k = start; k < day; ++k) {
                drinks_recent += days[k].drinks_today;
                if (days[k].drinks_today >= neg.binge_threshold) risk_days += 1.0;
            }

            double annualized_risk_days = risk_days * (365.0 / 30.0);
            double or_mult = aud_or_multiplier_from_risk_days_per_year(annualized_risk_days);
            double onset_month = std::clamp((neg.aud_onset_base * or_mult) / 12.0, 0.0, 1.0);
            double remission_month = std::clamp(neg.aud_remission / 12.0, 0.0, 1.0);
            bool recent_risk_drinking = risk_days > 0.0 || drinks_recent > 0.0;
            double relapse_month = std::clamp((neg.aud_relapse_base * (recent_risk_drinking ? neg.aud_relapse_mult_if_risk : 1.0)) / 12.0, 0.0, 1.0);

            double u = u01(RNG);
            if (life_state.aud_state == 0) {
                if (u < onset_month) life_state.aud_state = 1;
            } else if (life_state.aud_state == 1) {
                if (u < remission_month) life_state.aud_state = 2;
            } else {
                if (u < relapse_month) life_state.aud_state = 1;
            }
        }
        st.aud_active = (life_state.aud_state == 1);
        if (st.aud_active) {
            double aud_day = (neg.aud_disability_weight * neg.qaly_to_wellby + neg.aud_depression_ls_addon * neg.mental_health_causal_weight) / SCRIPT.days_per_year;
            neg_aud += disc * aud_day * neg.causal_weight;
        }

        pos_total += 0.0; // keep symmetry with daily accounting in local state
        neg_acute += disc * st.acute_utilons;
        neg_hang += disc * st.hang_utilons;
        neg_chronic += disc * st.chronic_utilons;
        ihd_total += disc * st.ihd_term;

        neg_total += disc * (st.acute_utilons + st.hang_utilons + st.chronic_utilons);

        st.alive = life_state.alive;
        days.push_back(st);
        if (!life_state.alive) break;
    }

    neg_total += neg_aud;
    return {
        pos_total, neg_total, pos_total-neg_total, neg_acute, neg_hang, neg_chronic, neg_aud, ihd_total,
        neg_acute_traffic, neg_acute_nontraffic, neg_acute_violence, neg_acute_poison,
        neg_chronic_cancer, neg_chronic_cirrhosis, neg_chronic_af,
    };
}

SimOut simulate_one_person() {
    PosPerson pos_person = sample_pos_person();
    NegParams neg = sample_neg_params();

    if (SCRIPT.mode == "daily") {
        return simulate_life_rollout(pos_person, neg);
    }

    auto pmf = drinks_pmf(SCRIPT.drinks_per_day);
    validate_pmf(pmf, "simulate_one_person");

    double daily_pos_ls = expected_daily_positive_ls(pos_person, pmf);
    double pos_total=0, neg_total=0, neg_acute=0, neg_hang=0, neg_chronic=0, ihd_total=0;
    double neg_acute_traffic=0.0, neg_acute_nontraffic=0.0, neg_acute_violence=0.0, neg_acute_poison=0.0;
    double neg_chronic_cancer=0.0, neg_chronic_cirrhosis=0.0, neg_chronic_af=0.0;
    double ema_g=0, ema_ca=0, ema_ci=0;

    auto alpha_from_half_life_days = [](double H_years) {
        if (H_years <= 0.0) return 0.0;
        return std::exp(-std::log(2.0) / (H_years * SCRIPT.days_per_year));
    };
    double a_g = alpha_from_half_life_days(neg.half_life_chronic);
    double a_ca = alpha_from_half_life_days(neg.half_life_cancer);
    double a_ci = alpha_from_half_life_days(neg.half_life_cirrhosis);

    double neg_aud = simulate_aud_lifetime_utilons(pmf, neg);

    for (int y = 0; y < SCRIPT.years; ++y) {
        double disc = discount_factor_continuous(SCRIPT.discount_rate_annual, y + 0.5);
        pos_total += disc * daily_pos_ls;

        int binge_days = 0;
        int hi_days = 0;
        double ema_g_sum = 0.0;
        double ema_ca_sum = 0.0;
        double ema_ci_sum = 0.0;
        int hi_threshold = neg.high_intensity_multiplier * neg.binge_threshold;
        for (int d = 0; d < SCRIPT.days_per_year; ++d) {
            int drinks_today = sample_drinks_today(SCRIPT.drinks_per_day);
            int grams_today = drinks_today * neg.grams_per_drink;
            ema_g = a_g * ema_g + (1.0 - a_g) * grams_today;
            ema_ca = a_ca * ema_ca + (1.0 - a_ca) * grams_today;
            ema_ci = a_ci * ema_ci + (1.0 - a_ci) * grams_today;
            ema_g_sum += ema_g;
            ema_ca_sum += ema_ca;
            ema_ci_sum += ema_ci;
            if (drinks_today >= neg.binge_threshold) ++binge_days;
            if (drinks_today >= hi_threshold) ++hi_days;
        }

        double p_binge_year = binge_days / static_cast<double>(SCRIPT.days_per_year);
        double p_hi_year = hi_days / static_cast<double>(SCRIPT.days_per_year);
        double ema_g_year = ema_g_sum / static_cast<double>(SCRIPT.days_per_year);
        double ema_ca_year = ema_ca_sum / static_cast<double>(SCRIPT.days_per_year);
        double ema_ci_year = ema_ci_sum / static_cast<double>(SCRIPT.days_per_year);

        AnnualNegBreakdown year_breakdown = annual_negative_utilons_expected(
            pmf, neg, ema_g_year, ema_ca_year, ema_ci_year, p_binge_year, p_hi_year);
        neg_total += disc * year_breakdown.total;
        neg_acute += disc * year_breakdown.acute;
        neg_hang += disc * year_breakdown.hang;
        neg_chronic += disc * year_breakdown.chronic;
        ihd_total += disc * year_breakdown.ihd;
        neg_acute_traffic += disc * year_breakdown.acute_traffic;
        neg_acute_nontraffic += disc * year_breakdown.acute_nontraffic;
        neg_acute_violence += disc * year_breakdown.acute_violence;
        neg_acute_poison += disc * year_breakdown.acute_poison;
        neg_chronic_cancer += disc * year_breakdown.chronic_cancer;
        neg_chronic_cirrhosis += disc * year_breakdown.chronic_cirrhosis;
        neg_chronic_af += disc * year_breakdown.chronic_af;
    }
    neg_total += neg_aud;
    return {
        pos_total, neg_total, pos_total-neg_total, neg_acute, neg_hang, neg_chronic, neg_aud, ihd_total,
        neg_acute_traffic, neg_acute_nontraffic, neg_acute_violence, neg_acute_poison,
        neg_chronic_cancer, neg_chronic_cirrhosis, neg_chronic_af,
    };
}

void print_event_share_summary_table(const std::vector<SimOut>& runs) {
    if (runs.empty()) return;

    struct RankedRun { double net = 0.0; std::array<double, 9> shares{}; };
    std::vector<RankedRun> ranked;
    ranked.reserve(runs.size());

    for (const auto& r : runs) {
        std::array<double, 9> comps{
            r.acute_traffic,
            r.acute_nontraffic,
            r.acute_violence,
            r.acute_poison,
            r.hang,
            r.chronic_cancer,
            r.chronic_cirrhosis,
            r.chronic_af,
            r.aud,
        };
        double denom = std::accumulate(comps.begin(), comps.end(), 0.0);
        RankedRun rr;
        rr.net = r.net;
        if (denom > 0.0) {
            for (size_t i = 0; i < comps.size(); ++i) rr.shares[i] = 100.0 * comps[i] / denom;
        }
        ranked.push_back(rr);
    }

    std::sort(ranked.begin(), ranked.end(), [](const RankedRun& a, const RankedRun& b) { return a.net < b.net; });

    std::vector<std::string> labels{
        "acute_traffic", "acute_nontraffic", "acute_violence", "acute_poison",
        "hangover", "chronic_cancer", "chronic_cirrhosis", "chronic_af", "aud",
    };

    std::cout << "\n=== Event contribution summary by net-utilon decile ===\n";
    std::cout << "(Rows are sorted by run net utilons; cells show mean % contribution to total negative utility.)\n\n";
    std::cout << std::left << std::setw(8) << "Decile" << std::setw(10) << "n";
    for (const auto& lab : labels) std::cout << std::setw(19) << lab;
    std::cout << "\n";

    for (int d = 0; d < 10; ++d) {
        size_t start = (d * ranked.size()) / 10;
        size_t end = ((d + 1) * ranked.size()) / 10;
        if (end <= start) continue;
        std::array<double, 9> avg{};
        for (size_t i = start; i < end; ++i) {
            for (size_t j = 0; j < avg.size(); ++j) avg[j] += ranked[i].shares[j];
        }
        double n = static_cast<double>(end - start);
        for (double& v : avg) v /= n;

        std::ostringstream dec_label;
        dec_label << "D" << (d + 1);
        std::cout << std::left << std::setw(8) << dec_label.str() << std::setw(10) << static_cast<int>(n);
        for (double v : avg) {
            std::ostringstream cell;
            cell << std::fixed << std::setprecision(1) << v << "%";
            std::cout << std::setw(19) << cell.str();
        }
        std::cout << "\n";
    }
}

struct HistogramBin {
    double left = 0.0;
    double right = 0.0;
    int count = 0;
};

std::vector<HistogramBin> build_histogram(const std::vector<double>& xs, int bins) {
    std::vector<HistogramBin> out;
    if (xs.empty()) return out;
    int n_bins = std::max(1, bins);
    double min_v = *std::min_element(xs.begin(), xs.end());
    double max_v = *std::max_element(xs.begin(), xs.end());
    out.resize(n_bins);

    if (min_v == max_v) {
        out[0] = {min_v, max_v, static_cast<int>(xs.size())};
        for (int i = 1; i < n_bins; ++i) out[i] = {min_v, max_v, 0};
        return out;
    }

    double width = (max_v - min_v) / n_bins;
    for (int i = 0; i < n_bins; ++i) {
        double left = min_v + i * width;
        double right = (i == n_bins - 1) ? max_v : left + width;
        out[i] = {left, right, 0};
    }

    for (double x : xs) {
        int idx = static_cast<int>((x - min_v) / width);
        if (idx < 0) idx = 0;
        if (idx >= n_bins) idx = n_bins - 1;
        out[idx].count += 1;
    }
    return out;
}

void print_histogram_data(const std::string& label, const std::vector<double>& xs, int bins) {
    auto hist = build_histogram(xs, bins);
    std::cout << "\n--- Histogram data: " << label << " ---\n";
    std::cout << "bin,left,right,count\n";
    for (size_t i = 0; i < hist.size(); ++i) {
        std::cout << i << ","
                  << std::fixed << std::setprecision(6) << hist[i].left << ","
                  << std::fixed << std::setprecision(6) << hist[i].right << ","
                  << hist[i].count << "\n";
    }
}

void write_histogram_csv(
    const std::string& out_path,
    const std::vector<std::pair<std::string, const std::vector<double>*>>& series,
    int bins
) {
    std::ofstream out(out_path);
    if (!out) throw std::runtime_error("Failed to open histogram output file: " + out_path);
    out << "metric,bin,left,right,count\n";
    for (const auto& entry : series) {
        auto hist = build_histogram(*entry.second, bins);
        for (size_t i = 0; i < hist.size(); ++i) {
            out << entry.first << "," << i << ","
                << std::fixed << std::setprecision(10) << hist[i].left << ","
                << std::fixed << std::setprecision(10) << hist[i].right << ","
                << hist[i].count << "\n";
        }
    }
}

double mean(const std::vector<double>& xs){ return xs.empty()?std::numeric_limits<double>::quiet_NaN():std::accumulate(xs.begin(), xs.end(), 0.0)/xs.size(); }

double percentile(std::vector<double> xs, double p) {
    if (xs.empty()) return std::numeric_limits<double>::quiet_NaN();
    std::sort(xs.begin(), xs.end());
    if (p <= 0) return xs.front();
    if (p >= 100) return xs.back();
    double idx = (p / 100.0) * (xs.size() - 1);
    size_t lo = static_cast<size_t>(std::floor(idx));
    size_t hi = static_cast<size_t>(std::ceil(idx));
    if (lo == hi) return xs[lo];
    double w = idx - lo;
    return xs[lo] * (1 - w) + xs[hi] * w;
}

void summarize(const std::string& label, const std::vector<double>& xs) {
    std::cout << "\n--- " << label << " ---\n";
    std::cout << "Mean: " << std::fixed << std::setprecision(4) << mean(xs) << "\n";
    for (int q : SCRIPT.quantiles) {
        std::cout << "  p" << std::setw(2) << std::setfill('0') << q << std::setfill(' ') << ": "
                  << std::fixed << std::setprecision(4) << percentile(xs, q) << "\n";
    }
}

void usage() {
    std::cout << "Usage: ./sim_cpp [--drinks-per-day X] [--runs N] [--seed S] [--mode expected|daily] [--sweep] [--sweep-min X --sweep-max X --sweep-step X] [--runs-per-point N] [--print-hist-data] [--hist-data-out PATH]\n";
}

int main(int argc, char** argv) {
    bool sweep = false;
    bool print_hist_data = false;
    std::string hist_data_out;
    double sweep_min = 0.0, sweep_max = 8.0, sweep_step = 0.25;
    int runs_per_point = -1;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const std::string& flag){ if (i+1 >= argc) throw std::runtime_error("Missing value for " + flag); return std::string(argv[++i]); };
        if (a == "--drinks-per-day") SCRIPT.drinks_per_day = std::stod(need(a));
        else if (a == "--runs") SCRIPT.num_runs = std::stoi(need(a));
        else if (a == "--seed") SCRIPT.seed = std::stoi(need(a));
        else if (a == "--mode") SCRIPT.mode = need(a);
        else if (a == "--sweep") sweep = true;
        else if (a == "--sweep-min") sweep_min = std::stod(need(a));
        else if (a == "--sweep-max") sweep_max = std::stod(need(a));
        else if (a == "--sweep-step") sweep_step = std::stod(need(a));
        else if (a == "--runs-per-point") runs_per_point = std::stoi(need(a));
        else if (a == "--print-hist-data") print_hist_data = true;
        else if (a == "--hist-data-out") hist_data_out = need(a);
        else if (a == "--help") { usage(); return 0; }
        else throw std::runtime_error("Unknown argument: " + a);
    }

    if (SCRIPT.mode != "expected" && SCRIPT.mode != "daily") throw std::runtime_error("--mode must be expected or daily");

    reseed(SCRIPT.seed);

    if (sweep) {
        int rpp = runs_per_point > 0 ? runs_per_point : SCRIPT.num_runs;
        std::vector<double> medians;
        std::vector<std::pair<double, double>> pairs;
        std::vector<SimOut> sweep_runs;
        std::cout << "=== Sweep: median(net utilons) by drinks/day ===\n";
        for (int idx = 0;; ++idx) {
            double d = sweep_min + idx * sweep_step;
            if (d > sweep_max + 1e-12) break;
            SCRIPT.drinks_per_day = d;
            SCRIPT.num_runs = rpp;
            reseed(SCRIPT.seed + idx);
            std::vector<double> nets;
            nets.reserve(SCRIPT.num_runs);
            for (int r = 0; r < SCRIPT.num_runs; ++r) {
                SimOut out = simulate_one_person();
                nets.push_back(out.net);
                sweep_runs.push_back(out);
            }
            double med = percentile(nets, 50.0);
            medians.push_back(med);
            pairs.push_back({d, med});
            std::cout << "  drinks/day=" << std::setw(5) << std::fixed << std::setprecision(2) << d
                      << "  median_net=" << std::setw(10) << std::setprecision(4) << med << "\n";
        }
        auto best = *std::max_element(pairs.begin(), pairs.end(), [](auto& a, auto& b){ return a.second < b.second; });
        std::cout << "\nBest (by median net utilons): drinks/day=" << std::setprecision(2) << best.first
                  << "  median_net=" << std::setprecision(4) << best.second << "\n";

        print_event_share_summary_table(sweep_runs);
        return 0;
    }

    std::vector<double> pos, neg, net, acute, hang, chronic, aud, ihd;
    std::vector<SimOut> all_runs;
    pos.reserve(SCRIPT.num_runs); neg.reserve(SCRIPT.num_runs); net.reserve(SCRIPT.num_runs);
    acute.reserve(SCRIPT.num_runs); hang.reserve(SCRIPT.num_runs); chronic.reserve(SCRIPT.num_runs);
    aud.reserve(SCRIPT.num_runs); ihd.reserve(SCRIPT.num_runs);
    all_runs.reserve(SCRIPT.num_runs);

    for (int i = 0; i < SCRIPT.num_runs; ++i) {
        auto out = simulate_one_person();
        pos.push_back(out.pos); neg.push_back(out.neg); net.push_back(out.net);
        acute.push_back(out.acute); hang.push_back(out.hang); chronic.push_back(out.chronic);
        aud.push_back(out.aud); ihd.push_back(out.ihd);
        all_runs.push_back(out);
    }

    std::cout << "=== Lifetime Utilon Simulation (Positive + Negative) ===\n";
    std::cout << "Runs: " << SCRIPT.num_runs << "\n";
    std::cout << "Seed: " << SCRIPT.seed << "\n";
    std::cout << "Horizon: " << SCRIPT.years << " years\n";
    std::cout << "Discount rate (script): " << std::fixed << std::setprecision(3) << SCRIPT.discount_rate_annual * 100.0
              << "% (continuous exp(-r*t))\n";
    std::cout << "Exposure: drinks_per_day = " << SCRIPT.drinks_per_day << " using day_count_model=" << SCRIPT.day_count_model
              << " and mode=" << SCRIPT.mode << "\n";

    summarize("Positive utilons (discounted lifetime)", pos);
    summarize("Negative utilons (discounted lifetime)", neg);
    summarize("Net utilons = Positive - Negative (discounted lifetime)", net);
    summarize("Negative breakdown: acute", acute);
    summarize("Negative breakdown: hangover", hang);
    summarize("Negative breakdown: chronic health proxies", chronic);
    summarize("Negative breakdown: AUD Markov", aud);
    summarize("IHD protection term (separate; not netted by default)", ihd);

    print_event_share_summary_table(all_runs);

    std::vector<std::pair<std::string, const std::vector<double>*>> hist_series{
        {"positive", &pos},
        {"negative", &neg},
        {"net", &net},
        {"acute", &acute},
        {"hangover", &hang},
        {"chronic", &chronic},
        {"aud", &aud},
        {"ihd", &ihd},
    };

    if (print_hist_data) {
        for (const auto& entry : hist_series) print_histogram_data(entry.first, *entry.second, SCRIPT.hist_bins);
    }

    if (!hist_data_out.empty()) {
        write_histogram_csv(hist_data_out, hist_series, SCRIPT.hist_bins);
        std::cout << "\nHistogram data written to: " << hist_data_out << "\n";
    }

    if (!print_hist_data && hist_data_out.empty()) {
        std::cout << "\n[info] Use --print-hist-data to print histogram bins or --hist-data-out <file.csv> to export bins for plotting.\n";
    }
    return 0;
}
