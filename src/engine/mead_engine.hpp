// Mead Workshop — formulation engine (C++17, no Qt dependency)
//
// Uses the calculation methods printed in the books (Piatz, Schramm):
//   • Gravity is counted in "points" (SG − 1) × 1000, additive by weight/volume.
//   • Honey: 40 points per pound per gallon — Piatz ch.9 chart; Schramm ch.5 (8 points per lb in 5 gal).
//   • Fruit: water (gal) = lb × water% ÷ 8.33; sugar points = lb × sugar% × 46 — Piatz ch.3, ch.9.
//   • Before-fruit size NS = BS − fruit water — Piatz ch.9.
//   • Yeast limit from Piatz's "Targeting Final Gravity" table; OG = 1 + (FG−1) + (YT−1) — Piatz ch.9.
//   • ABV = 131 × (OG − FG) — Piatz ch.9.
// Internal units: US gallons, pounds, specific gravity, °F.
//
// This is a line-for-line port of js/engine.js from the browser version; tests/engine_tests.cpp
// checks it against the books' worked examples and against the browser engine's outputs.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace mead {

namespace C {
constexpr double LB_G = 453.59237;
constexpr double GAL_L = 3.785411784;
constexpr double WATER_LB_PER_GAL = 8.33;   // Piatz
constexpr double SUGAR_PPG = 46;            // Piatz: FS = FSP × FW × 46
constexpr double HONEY_PPG = 40;            // Piatz chart; Schramm
constexpr double HONEY_LB_PER_GAL = 12;     // Piatz ch.5 (Schramm: 11.75)
constexpr double DRY_FG = 1.000;            // Piatz: dry finish around or below 1.000
constexpr double ABV_FACTOR = 131;          // Piatz: ABV = 131 × (OG − FG)
constexpr double PPM_PER_CAMPDEN_GAL = 50;  // Schramm ch.4
}  // namespace C

enum class Phase { Primary, Secondary };

// ---------- Inputs ----------
struct HoneyIn { std::string name; double lb = 0; };
struct FruitIn {
  std::string name;
  double lb = 0;
  double water = 85;               // % by weight
  std::optional<double> sugar;     // % by weight; empty = unknown (counted as 0, flagged)
  Phase phase = Phase::Primary;
};
struct JuiceIn { std::string name; double gal = 0; double sg = 1.0; double ratio = 1; Phase phase = Phase::Primary; };
struct SugarIn { std::string name; double lb = 0; double ppg = C::SUGAR_PPG; Phase phase = Phase::Primary; };
struct YeastIn { double tolerance = 14; std::optional<double> tolMin, tolMax; };

struct Recipe {
  double volumeGal = 5;
  bool countFruitWater = true;     // Piatz: batch volume includes fruit water
  std::vector<HoneyIn> honeys;
  std::vector<FruitIn> fruits;
  std::vector<JuiceIn> juices;
  std::vector<SugarIn> sugars;
  YeastIn yeast;
};

struct Settings { double honeyPPG = C::HONEY_PPG; double honeyLbPerGal = C::HONEY_LB_PER_GAL; };

// ---------- Outputs ----------
struct FGPrediction { double central, low, high; bool reachesDry; double abv; double ytPoints; };

struct Volumes {
  double final, preFruit, honey, juicePrimary, juiceSecondary, sugar;
  double fruitWater, fruitWaterPrimary, fruitWaterSecondary, waterToAdd;
};
struct Points { double honey, juice, sugar, fruit, total, mix; };

struct Result {
  Volumes volumes;
  Points points;
  double honeyLb, fruitLb, fruitSugarLb;
  std::vector<std::string> unknownSugar;
  double honeyPPG, honeyLbPerGal;
  double mixingOG, primaryOG, effectiveOG;
  double dryABV;
  FGPrediction fg;
  double tolerance;
  std::string strength;
};

struct SolveResult { double lb; std::vector<HoneyIn> honeys; bool reachable; double waterToAdd; };

// ---------- Basic conversions ----------
inline double pts(double sg) { return (sg - 1) * 1000; }
inline double sgOf(double p) { return 1 + p / 1000; }

double tolerancePoints(double abv);           // Piatz table, interpolated
double abvFromPoints(double points);          // inverse of the table
double abv(double og, double fg);             // Piatz 131 × ΔSG
double abvBalling(double og, double fg);      // chemistry cross-check (not in the books)
double brixToSG(double brix);
double sgToBrix(double sg);

// ---------- Classification ----------
std::string strengthClass(double abv);

struct SweetLevel { std::string key, label; double lo, hi, target; };
struct SweetScale { std::string name, src; std::vector<SweetLevel> levels; };
const SweetScale& sweetnessScale(const std::string& name);   // "piatz" (default) or "schramm"
const SweetLevel& sweetnessByKey(const std::string& key, const std::string& scaleName);
std::string sweetnessClass(double fg, const std::string& scaleName);

// ---------- Final gravity ----------
FGPrediction predictFG(double og, double tol, std::optional<double> tolMin = {}, std::optional<double> tolMax = {});
double ogForToleranceFinish(double targetFG, double tol);
double ogForDryABV(double abvTarget);

// ---------- Schramm fruit table ----------
struct SchrammRow { std::string name, unit; double mild[2]; double medium[2]; double strong; };
struct FruitIntensity { double per5; std::string level; };
FruitIntensity fruitIntensity(const SchrammRow& row, double amount, double batchGal);
struct FruitRec { double lo, hi, mid, adj; std::string effLevel; };
FruitRec fruitRecommendation(const SchrammRow& row, const std::string& level, double batchGal,
                             const std::string& sweetnessKey, bool isLemonLime);

// ---------- Formulation ----------
Result formulate(const Recipe& r, const Settings& s = {});
SolveResult solveHoney(const Recipe& r, double targetOG, const Settings& s = {});

// ---------- Nutrients (Piatz ch.4–5) ----------
struct Dose { std::string when; std::optional<double> hours; std::optional<double> sg; double grams; };
struct SNA {
  int packets; double goFerm, rehydrationOz, goFerm5g, water5gMl;
  double fermaidK, dap, mix, dose, dosed, breakSG;
  std::vector<Dose> doses;
};
SNA piatzSNA(double volumeGal, double og, double fgTarget);

// ---------- Post-fermentation ----------
struct Backsweeten { double lb, dPts, addedVolGal, cups; };
Backsweeten backsweeten(double volumeGal, double currentFG, double targetFG,
                        double honeyPPG = C::HONEY_PPG, double honeyLbPerGal = C::HONEY_LB_PER_GAL);
struct BenchStep { double dPts, honeyG, syrupG; };
std::vector<BenchStep> benchTrial(double sampleMl, const std::vector<double>& steps, double honeyPPG = C::HONEY_PPG);
double campdenTablets(double ppm, double volumeGal);            // Schramm's rule
double kmetaGrams(double ppm, double volumeGal);                // chemistry
double campdenPPMPerTabletPerGal(double tabletG = 0.44);        // chemistry
double freeSO2ForPH(double ph);                                 // OSU Extension table
double dapNitrogenPPM(double gPerL);                            // chemistry

std::optional<double> blendVolume(double volA, double valA, double valB, double target);  // Pearson square
double fruitToAdd(double volGal, double lbAlready, double targetPer5, double waterPct);
double dilutionForFruit(double volGal, double lbFruit, double targetPer5);

// ---------- Measurement tools (not from the books) ----------
double tempCorrect(double sg, double tempF, double calF = 60);
double refractoFG(double origBrix, double finalBrix, double wcf = 1.0);
struct KatzResult { double honeyGal, waterGal, honeyLb, og; };
KatzResult katzRatio(double totalGal, double honeyParts = 1, double waterParts = 4,
                     double honeyPPG = C::HONEY_PPG, double honeyLbPerGal = C::HONEY_LB_PER_GAL);

// ---------- Journal ----------
struct Reading { std::string date; double sg; };   // date as YYYY-MM-DD
struct ReadingAnalysis {
  Reading last; double abv; std::optional<double> progress, breakSG; std::optional<bool> passedBreak;
  bool stable, stuck;
};
std::optional<ReadingAnalysis> analyzeReadings(double og, std::vector<Reading> readings, std::optional<double> fgTarget);
std::optional<double> impliedHoneyPPG(double measuredOG, double preFruitVol, double otherMixPts, double honeyLb);

}  // namespace mead
