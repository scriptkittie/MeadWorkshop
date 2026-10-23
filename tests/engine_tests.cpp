// Mead Workshop — engine tests (no Qt needed).
// Build & run:  cmake -S . -B build -DMEAD_TESTS_ONLY=ON && cmake --build build && ./build/engine_tests
// Every check is either a worked example printed in Piatz / Schramm / Katz, a chemistry fact,
// or parity with the browser engine (which was itself verified against the books).
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../src/engine/mead_engine.hpp"

static int passes = 0, fails = 0;
static void ok(bool cond, const std::string& msg) {
  if (cond) ++passes;
  else { ++fails; std::printf("FAIL: %s\n", msg.c_str()); }
}
static void near(double got, double want, double tol, const std::string& msg) {
  char b[64]; std::snprintf(b, sizeof b, " (got %.6f, want %.6f)", got, want);
  ok(std::fabs(got - want) <= tol, msg + b);
}

#include "parity_cases.inc"

int main() {
  using namespace mead;
  auto honeyOnly = [](double gal, double lb) { Recipe r; r.volumeGal = gal; r.honeys = {{"h", lb}}; return formulate(r).effectiveOG; };

  // Piatz ch.9 honey chart
  near(honeyOnly(1, 0.5), 1.020, 5e-4, "Piatz chart 0.5 lb/gal");
  near(honeyOnly(1, 1), 1.040, 5e-4, "Piatz chart 1 lb/gal");
  near(honeyOnly(1, 2), 1.080, 5e-4, "Piatz chart 2 lb/gal");
  near(honeyOnly(1, 3), 1.120, 5e-4, "Piatz chart 3 lb/gal");
  near(honeyOnly(1, 4), 1.160, 5e-4, "Piatz chart 4 lb/gal");
  // Schramm ch.5
  near(honeyOnly(5, 1), 1.008, 5e-4, "Schramm: 1 lb raises 5 gal ~8 points");
  near(honeyOnly(5, 11.75), 1.094, 5e-4, "Schramm: 1 gal honey (11.75 lb) in 5 gal ≈ 1.094");
  { const double og = honeyOnly(5, 15); ok(og >= 1.112 && og <= 1.128, "Schramm first mead 1.112–1.128"); }

  // Piatz ch.3 raspberry example
  { Recipe r; r.volumeGal = 6.544; r.fruits = {{"r", 15, 85.75, 4.42, Phase::Primary}};
    const auto res = formulate(r);
    near(res.volumes.fruitWater, 1.544, 0.001, "Piatz GW raspberry");
    near(res.fruitSugarLb, 0.66, 0.005, "Piatz raspberry sugar"); }

  // Piatz ch.9 worked example: NS 3.76, FS 33.12, BOG 1.144
  { Recipe r; r.volumeGal = 5; r.honeys = {{"h", 1}}; r.fruits = {{"x", 12, 86, 6, Phase::Primary}};
    const auto sol = solveHoney(r, 1.115);
    r.honeys = sol.honeys;
    const auto res = formulate(r);
    near(res.volumes.preFruit, 3.76, 0.005, "Piatz NS");
    near(res.points.fruit, 33.12, 0.01, "Piatz FS");
    near(res.mixingOG, 1.144, 5e-4, "Piatz BOG");
    near(res.effectiveOG, 1.115, 1e-4, "Piatz TOG"); }

  // Piatz ch.9 Targeting Final Gravity table
  const double table[][2] = {{5, 1.038}, {6, 1.046}, {7, 1.053}, {8, 1.061}, {9, 1.069}, {10, 1.076}, {11, 1.084}, {12, 1.092},
                             {13, 1.099}, {14, 1.107}, {15, 1.115}, {16, 1.122}, {17, 1.130}, {18, 1.137}, {19, 1.145}, {20, 1.153}};
  for (auto& t : table) near(1 + tolerancePoints(t[0]) / 1000, t[1], 1e-5, "Piatz table " + std::to_string((int)t[0]) + "%");
  near(ogForToleranceFinish(1.015, 12), 1.107, 1e-4, "Piatz: FG 1.015 with 12% yeast → OG 1.107");
  near(predictFG(1.080, 14).central, 1.000, 1e-4, "Below yeast limit ferments dry");
  near(abv(1.115, 1.005), 14.41, 0.01, "Piatz ABV example");
  near(tolerancePoints(11), 84, 1.01, "Piatz 11% ≈ 83–84 points");

  // Piatz ch.5 SNA
  { const auto n = piatzSNA(5, 1.115, 1.010);
    ok(n.fermaidK == 4 && n.dap == 8 && n.dose == 2 && n.goFerm == 5 && n.doses.size() == 4, "Piatz SNA 5 gal");
    near(n.breakSG, 1.0835, 1e-4, "Piatz 30% formula");
    const auto n9 = piatzSNA(9, 1.080, 1.000);
    ok(n9.packets == 2 && n9.goFerm == 10, "Piatz 9 gal: 2 packets, 10 g Go-Ferm"); }

  near(campdenTablets(50, 5), 5, 1e-9, "Schramm Campden rule");
  near(katzRatio(1).honeyLb, 2.4, 1e-3, "Katz 1:4 honey");

  // Backsweeten lands exactly on target
  for (auto c : {std::array<double, 3>{5, 1.000, 1.010}, {5, 0.995, 1.010}, {3, 0.998, 1.025}, {6, 1.002, 1.040}}) {
    const auto b = backsweeten(c[0], c[1], c[2]);
    near(1 + (pts(c[1]) * c[0] + b.lb * 40) / (c[0] + b.lb / 12) / 1000, c[2], 1e-9, "backsweeten exact");
  }

  // Chemistry
  near(2 * (32.06 + 2 * 15.999) / (2 * 39.098 + 2 * 32.06 + 5 * 15.999), 0.576, 5e-4, "K-meta 57.6% SO2");
  near(campdenPPMPerTabletPerGal(0.44), 67.0, 0.1, "0.44 g Campden ≈ 67 ppm/gal");
  near(dapNitrogenPPM(1), 1000 * 2 * 14.007 / 132.06, 0.5, "DAP 21.2% N");
  near(freeSO2ForPH(3.5), 44, 1e-9, "OSU pH 3.5 → 44 ppm");
  near(brixToSG(25), 1.1057, 2e-4, "Brix 25 → SG");
  { const double sg = brixToSG(10); near((sg - 1) * 1000 / (sg * 8.3454 * 0.10), 46.1, 0.2, "sucrose ≈ 46 pts/lb/gal"); }
  ok(std::fabs(abvBalling(1.080, 1.000) - abv(1.080, 1.000)) < 0.4, "ABV formulas agree at standard strength");
  ok(abvBalling(1.130, 1.000) > abv(1.130, 1.000), "linear ABV reads low at sack strength");
  near(tempCorrect(1.000, 80, 59) - 1, 0.00252, 2e-4, "hydrometer correction at 80°F");

  // Adjust-a-finished-mead math
  near(*blendVolume(5, 1.010, 0.998, 1.005), 25.0 / 7.0, 1e-9, "Pearson square blend");
  ok(!blendVolume(5, 1.010, 1.012, 1.005).has_value(), "impossible blend is rejected");
  { const double x = fruitToAdd(5, 0, 8, 88.87); near(x * 5 / (5 + x * 0.8887 / 8.33), 8.0, 1e-9, "fruit to add reaches target per 5 gal"); }
  near(dilutionForFruit(5, 13, 8), 3.125, 1e-12, "dilution for fruit");

  // Journal analysis
  { std::vector<Reading> rs = {{"2026-10-22", 1.120}, {"2026-10-26", 1.080}, {"2026-11-02", 1.040}};
    const auto a = analyzeReadings(1.150, rs, 1.003);
    ok(a && a->last.sg == 1.040 && !a->stable, "readings: latest + not stable");
    std::vector<Reading> st = {{"2026-11-01", 1.010}, {"2026-11-03", 1.010}, {"2026-11-05", 1.0105}};
    const auto b = analyzeReadings(1.120, st, 1.000);
    ok(b && b->stable && b->stuck, "stable well above target → stuck"); }
  near(*impliedHoneyPPG(1.115, 5, 0, 14), 41.0714, 1e-3, "implied honey ppg");

  // Parity with the browser engine (500 random recipes)
  double worst = 0;
  for (const auto& c : PARITY) {
    const auto res = formulate(c.recipe, c.settings);
    const double d[] = {res.mixingOG - c.mix, res.effectiveOG - c.eff, res.primaryOG - c.prim, res.fg.central - c.fg,
                        res.fg.low - c.fgLow, res.fg.high - c.fgHigh, res.fg.abv - c.abv, res.volumes.waterToAdd - c.water,
                        res.volumes.preFruit - c.pre, res.volumes.final - c.final, solveHoney(c.recipe, c.solveTarget, c.settings).lb - c.solveLb};
    for (double x : d) worst = std::max(worst, std::fabs(x));
  }
  ok(worst < 1e-9, "parity with browser engine on 500 recipes (worst diff " + std::to_string(worst) + ")");
  for (auto& b : BACKSWEETEN_CASES) near(backsweeten(b[0], b[1], b[2]).lb, b[3], 1e-12, "backsweeten parity");
  for (auto& t : TOLPTS_CASES) near(tolerancePoints(t[0]), t[1], 1e-12, "tolerance points parity");

  std::printf("%d passed, %d failed\n", passes, fails);
  return fails ? 1 : 0;
}
