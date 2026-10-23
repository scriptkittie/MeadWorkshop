// Mead Workshop — formulation engine. See mead_engine.hpp for sources of each rule.
#include "mead_engine.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <utility>

namespace mead {

namespace {
// Piatz ch.9 "Targeting Final Gravity": ABV reached when fermented from this SG to 1.000
constexpr std::array<std::pair<double, double>, 16> PIATZ_FG_TABLE{{
    {5, 1.038}, {6, 1.046}, {7, 1.053}, {8, 1.061}, {9, 1.069}, {10, 1.076}, {11, 1.084}, {12, 1.092},
    {13, 1.099}, {14, 1.107}, {15, 1.115}, {16, 1.122}, {17, 1.130}, {18, 1.137}, {19, 1.145}, {20, 1.153}}};

// Free SO2 (mg/L) needed for 0.8 mg/L molecular SO2 — Oklahoma State Univ. Extension (wine chemistry)
constexpr std::array<std::pair<double, double>, 11> FREE_SO2_BY_PH{{
    {3.0, 14}, {3.1, 18}, {3.2, 22}, {3.3, 28}, {3.4, 35}, {3.5, 44}, {3.6, 55}, {3.7, 69}, {3.8, 87}, {3.9, 109}, {4.0, 125}}};

std::string fmt3(double v) { char b[32]; std::snprintf(b, sizeof b, "%.3f", v); return b; }

// days since epoch for YYYY-MM-DD (UTC, no timezone issues)
long daysFromDate(const std::string& d) {
  int y = 1970, m = 1, day = 1;
  std::sscanf(d.c_str(), "%d-%d-%d", &y, &m, &day);
  // Howard Hinnant's days_from_civil
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = static_cast<unsigned>((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + day - 1);
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long>(doe) - 719468;
}
}  // namespace

// ---------- Yeast tolerance <-> gravity points ----------
double tolerancePoints(double abvV) {
  const auto& t = PIATZ_FG_TABLE;
  if (abvV <= t[0].first) return pts(t[0].second) * abvV / t[0].first;
  for (size_t i = 1; i < t.size(); ++i) {
    if (abvV <= t[i].first) {
      const auto [a0, s0] = t[i - 1];
      const auto [a1, s1] = t[i];
      return pts(s0) + (pts(s1) - pts(s0)) * (abvV - a0) / (a1 - a0);
    }
  }
  const auto [a0, s0] = t[t.size() - 2];
  const auto [a1, s1] = t[t.size() - 1];
  return pts(s1) + (pts(s1) - pts(s0)) * (abvV - a1) / (a1 - a0);
}

double abvFromPoints(double p) {
  double lo = 0, hi = 30;
  for (int i = 0; i < 50; ++i) { const double m = (lo + hi) / 2; if (tolerancePoints(m) < p) lo = m; else hi = m; }
  return (lo + hi) / 2;
}

double abv(double og, double fg) { return C::ABV_FACTOR * (og - fg); }
double brixToSG(double b) { return 1 + b / (258.6 - (b / 258.2) * 227.1); }
double sgToBrix(double sg) { return ((182.4601 * sg - 775.6821) * sg + 1262.7794) * sg - 669.5622; }
double abvBalling(double og, double fg) {
  const double OE = sgToBrix(og), AE = sgToBrix(fg);
  const double RE = 0.1808 * OE + 0.8192 * AE;
  const double abw = (OE - RE) / (2.0665 - 0.010665 * OE);
  return abw * fg / 0.794;
}

// ---------- Classification ----------
std::string strengthClass(double a) {
  if (a < 3.5) return "Below hydromel";
  if (a < 7.5) return "Hydromel";
  if (a < 14) return "Standard";
  if (a <= 18) return "Sack";
  return "Above sack strength";
}

const SweetScale& sweetnessScale(const std::string& name) {
  static const SweetScale piatz{"Piatz (BJCP)", "Piatz ch.2", {
      {"dry", "Dry", 0.990, 1.010, 1.000},
      {"medium", "Semisweet", 1.010, 1.025, 1.015},
      {"sweet", "Sweet", 1.025, 1.050, 1.035}}};
  static const SweetScale schramm{"Schramm", "Schramm ch.5", {
      {"dry", "Dry", 0.990, 1.006, 1.000},
      {"medium", "Medium", 1.006, 1.015, 1.010},
      {"sweet", "Sweet", 1.012, 1.020, 1.016},
      {"dessert", "Dessert", 1.020, 1.060, 1.025}}};
  return name == "schramm" ? schramm : piatz;
}

const SweetLevel& sweetnessByKey(const std::string& key, const std::string& scaleName) {
  const auto& L = sweetnessScale(scaleName).levels;
  for (const auto& l : L) if (l.key == key) return l;
  return key == "dessert" ? L.back() : L[1];
}

std::string sweetnessClass(double fg, const std::string& scaleName) {
  const auto& L = sweetnessScale(scaleName).levels;
  if (fg < L.front().lo) return "Very dry";
  std::string hits;
  for (const auto& l : L) if (fg >= l.lo && fg <= l.hi) hits += (hits.empty() ? "" : " / ") + l.label;
  if (!hits.empty()) return hits;
  if (fg > L.back().hi) {
    std::string lab = L.back().label;
    std::transform(lab.begin(), lab.end(), lab.begin(), ::tolower);
    return "Sweeter than " + lab;
  }
  return L.front().label;
}

// ---------- Final gravity ----------
FGPrediction predictFG(double og, double tol, std::optional<double> tolMin, std::optional<double> tolMax) {
  auto at = [og](double x) { return std::max(C::DRY_FG, og - tolerancePoints(x) / 1000); };
  const double central = at(tol);
  return {central, at(tolMax.value_or(tol)), at(tolMin.value_or(tol)),
          og - tolerancePoints(tol) / 1000 <= C::DRY_FG, abv(og, central), tolerancePoints(tol)};
}
double ogForToleranceFinish(double targetFG, double tol) { return 1 + (targetFG - 1) + tolerancePoints(tol) / 1000; }
double ogForDryABV(double abvTarget) { return 1 + tolerancePoints(abvTarget) / 1000; }

// ---------- Schramm fruit table ----------
FruitIntensity fruitIntensity(const SchrammRow& row, double amount, double batchGal) {
  const double per5 = amount * (5 / batchGal);
  std::string level;
  if (per5 < row.mild[0]) level = "Below mild";
  else if (per5 <= row.mild[1]) level = "Mild";
  else if (per5 < row.medium[0]) level = "Mild–medium";
  else if (per5 <= row.medium[1]) level = "Medium";
  else if (per5 < row.strong) level = "Medium–strong";
  else level = "Strong";
  return {per5, level};
}

FruitRec fruitRecommendation(const SchrammRow& row, const std::string& level, double batchGal,
                             const std::string& sweetnessKey, bool isLemonLime) {
  // Schramm: "a little less for dry meads, and a little more for sweets" — the app reads "a little" as 10%.
  const double adj = sweetnessKey == "dry" ? 0.9 : (sweetnessKey == "sweet" || sweetnessKey == "dessert") ? 1.1 : 1.0;
  std::string eff = level;
  if (isLemonLime) eff = level == "strong" ? "medium" : "mild";
  double lo, hi;
  if (eff == "strong") { lo = hi = row.strong; }
  else if (eff == "medium") { lo = row.medium[0]; hi = row.medium[1]; }
  else { lo = row.mild[0]; hi = row.mild[1]; }
  const double s = (batchGal / 5) * adj;
  return {lo * s, hi * s, (lo + hi) / 2 * s, adj, eff};
}

// ---------- Formulation ----------
Result formulate(const Recipe& r, const Settings& s) {
  const double H = s.honeyPPG, HLG = s.honeyLbPerGal;
  const double V = std::max(0.01, r.volumeGal);
  Result out{};

  double honeyLb = 0;
  for (const auto& h : r.honeys) honeyLb += h.lb;
  const double honeyPts = honeyLb * H, honeyVol = honeyLb / HLG;

  double juicePtsP = 0, juicePtsS = 0, juiceVolP = 0, juiceVolS = 0;
  for (const auto& j : r.juices) {
    const double p = j.gal * std::max(1.0, j.ratio) * pts(j.sg);
    if (j.phase == Phase::Secondary) { juicePtsS += p; juiceVolS += j.gal; } else { juicePtsP += p; juiceVolP += j.gal; }
  }

  double sugarPtsP = 0, sugarPtsS = 0, sugarVol = 0;
  for (const auto& sg : r.sugars) {
    const double p = sg.lb * sg.ppg;
    if (sg.phase == Phase::Secondary) sugarPtsS += p; else sugarPtsP += p;
    sugarVol += sg.lb / HLG;  // approximation: dissolved sugars taken at honey's volume per pound
  }

  double fwP = 0, fwS = 0, fpP = 0, fpS = 0, fruitLb = 0, fruitSugarLb = 0;
  for (const auto& f : r.fruits) {
    fruitLb += f.lb;
    const double w = (f.lb * f.water / 100) / C::WATER_LB_PER_GAL;   // Piatz GW
    const double sl = f.lb * f.sugar.value_or(0) / 100;
    const double p = sl * C::SUGAR_PPG;                               // Piatz FS
    fruitSugarLb += sl;
    if (!f.sugar) out.unknownSugar.push_back(f.name);
    if (f.phase == Phase::Secondary) { fwS += w; fpS += p; } else { fwP += w; fpP += p; }
  }
  const double fruitWater = fwP + fwS;

  const double preFruitVol = V - (r.countFruitWater ? fruitWater : 0) - juiceVolS;   // Piatz NS
  const double finalVol = r.countFruitWater ? V : V + fruitWater;
  const double waterToAdd = preFruitVol - honeyVol - juiceVolP - sugarVol;

  const double mixPts = honeyPts + juicePtsP + sugarPtsP;
  const double totalPts = mixPts + juicePtsS + sugarPtsS + fpP + fpS;

  out.volumes = {finalVol, preFruitVol, honeyVol, juiceVolP, juiceVolS, sugarVol, fruitWater, fwP, fwS, waterToAdd};
  out.points = {honeyPts, juicePtsP + juicePtsS, sugarPtsP + sugarPtsS, fpP + fpS, totalPts, mixPts};
  out.honeyLb = honeyLb; out.fruitLb = fruitLb; out.fruitSugarLb = fruitSugarLb;
  out.honeyPPG = H; out.honeyLbPerGal = HLG;
  out.mixingOG = sgOf(mixPts / preFruitVol);
  out.effectiveOG = sgOf(totalPts / finalVol);
  out.primaryOG = sgOf((mixPts + fpP) / (preFruitVol + fwP));
  out.tolerance = r.yeast.tolerance;
  out.fg = predictFG(out.effectiveOG, r.yeast.tolerance, r.yeast.tolMin, r.yeast.tolMax);
  out.dryABV = abv(out.effectiveOG, C::DRY_FG);
  out.strength = strengthClass(out.fg.abv);
  return out;
}

SolveResult solveHoney(const Recipe& r, double targetOG, const Settings& s) {
  Recipe base = r;
  if (base.honeys.empty()) base.honeys.push_back({"Honey", 1});
  double total0 = 0;
  for (const auto& h : base.honeys) total0 += h.lb;
  std::vector<double> shares;
  for (const auto& h : base.honeys) shares.push_back(total0 > 0 ? h.lb / total0 : 1.0 / static_cast<double>(base.honeys.size()));
  for (auto& h : base.honeys) h.lb = 0;
  const Result r0 = formulate(base, s);
  const double lb = (pts(targetOG) * r0.volumes.final - r0.points.total) / s.honeyPPG;
  if (lb < 0) return {0, base.honeys, true, r0.volumes.waterToAdd};
  for (size_t i = 0; i < base.honeys.size(); ++i) base.honeys[i].lb = lb * shares[i];
  const Result r1 = formulate(base, s);
  return {lb, base.honeys, r1.volumes.waterToAdd >= -0.01, r1.volumes.waterToAdd};
}

// ---------- Nutrients ----------
SNA piatzSNA(double volumeGal, double og, double fgTarget) {
  const double scale = volumeGal / 5;
  const int packets = std::max(1, static_cast<int>(std::lround(volumeGal / 5)));
  const double breakSG = og - (og - fgTarget) * 0.3;   // Piatz: 30% point
  const double dose = 2 * scale;
  SNA n{};
  n.packets = packets; n.goFerm = 5.0 * packets; n.rehydrationOz = 4.0 * packets;
  n.goFerm5g = 6.25 * packets; n.water5gMl = 125.0 * packets;   // Lallemand: 1.25 × yeast; water 20 × Go-Ferm
  n.fermaidK = 4 * scale; n.dap = 8 * scale; n.mix = 12 * scale; n.dose = dose; n.dosed = dose * 4; n.breakSG = breakSG;
  n.doses = {{"At initial mixing of must", 0.0, std::nullopt, dose},
             {"After 48 hours", 48.0, std::nullopt, dose},
             {"After 96 hours", 96.0, std::nullopt, dose},
             {"When gravity has dropped 30% of the way — about " + fmt3(breakSG), std::nullopt, breakSG, dose}};
  return n;
}

// ---------- Post-fermentation ----------
Backsweeten backsweeten(double volumeGal, double currentFG, double targetFG, double honeyPPG, double honeyLbPerGal) {
  const double Pc = pts(currentFG), Pt = pts(targetFG);
  const double dPts = std::max(0.0, Pt - Pc);
  // Final points = (Pc·V + lb·H) / (V + lb/HLG) = Pt  →  lb = (Pt − Pc)·V / (H − Pt/HLG)
  const double lb = (dPts * volumeGal) / (honeyPPG - Pt / honeyLbPerGal);
  return {lb, dPts, lb / honeyLbPerGal, lb / honeyLbPerGal * 16};
}

std::vector<BenchStep> benchTrial(double sampleMl, const std::vector<double>& steps, double honeyPPG) {
  const double gPerLPerPt = (C::LB_G / C::GAL_L) / honeyPPG;
  std::vector<BenchStep> out;
  for (double d : steps) { const double g = d * gPerLPerPt * sampleMl / 1000; out.push_back({d, g, 2 * g}); }
  return out;
}

double campdenTablets(double ppm, double volumeGal) { return volumeGal * ppm / C::PPM_PER_CAMPDEN_GAL; }
double kmetaGrams(double ppm, double volumeGal) { return (ppm * volumeGal * C::GAL_L) / 1000 / 0.576; }
double campdenPPMPerTabletPerGal(double tabletG) { return tabletG * 0.576 * 1000 / C::GAL_L; }
double dapNitrogenPPM(double gPerL) { return gPerL * 212; }

double freeSO2ForPH(double ph) {
  const auto& t = FREE_SO2_BY_PH;
  if (ph <= t.front().first) return t.front().second;
  if (ph >= t.back().first) return t.back().second;
  for (size_t i = 1; i < t.size(); ++i)
    if (ph <= t[i].first) {
      const auto [a, x] = t[i - 1];
      const auto [b, y] = t[i];
      return x + (y - x) * (ph - a) / (b - a);
    }
  return t.back().second;
}

std::optional<double> blendVolume(double volA, double valA, double valB, double target) {
  if ((valA - target) * (valB - target) >= 0 || valB == target) return std::nullopt;
  return volA * (valA - target) / (target - valB);
}

double fruitToAdd(double volGal, double lbAlready, double targetPer5, double waterPct) {
  const double w = waterPct / 100 / C::WATER_LB_PER_GAL;
  const double x = (targetPer5 * volGal / 5 - lbAlready) / (1 - targetPer5 * w / 5);
  return std::max(0.0, x);
}

double dilutionForFruit(double volGal, double lbFruit, double targetPer5) {
  return std::max(0.0, 5 * lbFruit / targetPer5 - volGal);
}

// ---------- Measurement tools ----------
double tempCorrect(double sg, double tempF, double calF) {
  auto f = [](double t) {
    return 1.00130346 - 0.000134722124 * t + 0.00000204052596 * t * t - 0.00000000232820948 * t * t * t;
  };
  return sg * (f(tempF) / f(calF));
}

double refractoFG(double origBrix, double finalBrix, double wcf) {
  const double ri = origBrix / wcf, rf = finalBrix / wcf;
  return 1 - 0.0044993 * ri + 0.011774 * rf + 0.00027581 * ri * ri - 0.0012717 * rf * rf -
         0.0000072800 * ri * ri * ri + 0.000063293 * rf * rf * rf;
}

KatzResult katzRatio(double totalGal, double honeyParts, double waterParts, double honeyPPG, double honeyLbPerGal) {
  const double honeyGal = totalGal * honeyParts / (honeyParts + waterParts);
  const double lb = honeyGal * honeyLbPerGal;
  return {honeyGal, totalGal - honeyGal, lb, sgOf(lb * honeyPPG / totalGal)};
}

// ---------- Journal ----------
std::optional<ReadingAnalysis> analyzeReadings(double og, std::vector<Reading> rs, std::optional<double> fgTarget) {
  rs.erase(std::remove_if(rs.begin(), rs.end(), [](const Reading& r) { return !(r.sg > 0); }), rs.end());
  if (rs.empty() || !(og > 0)) return std::nullopt;
  std::stable_sort(rs.begin(), rs.end(), [](const Reading& a, const Reading& b) { return daysFromDate(a.date) < daysFromDate(b.date); });
  ReadingAnalysis a{};
  a.last = rs.back();
  a.abv = abv(og, a.last.sg);
  if (fgTarget) {
    a.progress = std::clamp((og - a.last.sg) / (og - *fgTarget) * 100, 0.0, 150.0);
    a.breakSG = og - (og - *fgTarget) * 0.3;
    a.passedBreak = a.last.sg <= *a.breakSG;
  }
  a.stable = false;
  if (rs.size() >= 3) {
    const auto& r0 = rs[rs.size() - 3];
    const auto& r2 = rs.back();
    double mx = r0.sg, mn = r0.sg;
    for (size_t i = rs.size() - 3; i < rs.size(); ++i) { mx = std::max(mx, rs[i].sg); mn = std::min(mn, rs[i].sg); }
    const long span = daysFromDate(r2.date) - daysFromDate(r0.date);
    a.stable = (mx - mn) <= 0.001 + 1e-12 && span >= 3;
  }
  a.stuck = a.stable && fgTarget && a.last.sg > *fgTarget + 0.008;
  return a;
}

std::optional<double> impliedHoneyPPG(double measuredOG, double preFruitVol, double otherMixPts, double honeyLb) {
  if (!(measuredOG > 1) || !(honeyLb > 0)) return std::nullopt;
  return (pts(measuredOG) * preFruitVol - otherMixPts) / honeyLb;
}

}  // namespace mead
