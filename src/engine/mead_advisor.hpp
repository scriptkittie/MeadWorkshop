// Mead Workshop — advisor (C++17, no Qt dependency).
//
// Port of js/advisor.js and the Designer's profile logic from js/app.js in the browser version:
//   • the reference data (fruits, yeasts, juices, Schramm's Table 8.1) loaded from the JSON files
//   • a recipe *design* (ids + amounts, as the user enters it) → engine input
//   • the Review notes, each with its book and chapter
//   • fruit character (Schramm Table 8.1, then Piatz's per-gallon guide)
//   • the profile bars (sweetness / strength / fruit) and the "sweeter / less sweet" options
// Texts are kept word-for-word identical to the browser version; tests/advisor_tests.cpp checks that.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "mead_engine.hpp"
#include "mead_json.hpp"

namespace mead {

// JavaScript's Number.prototype.toFixed (rounds exact halves up, as the browser does).
std::string toFixed(double x, int digits);

// ---------- Reference data ----------
struct FruitInfo {
  std::string id, name, acid, pectin, schramm, schrammMatch, src;
  double water = 85;
  std::optional<double> sugar;
  bool juice = false;
};
struct YeastInfo {
  std::string id, vendor, code, common, name, toleranceRaw, tempRaw, nitrogen, comments, form, src, schramm;
  std::optional<double> tolerance, tolMin, tolMax;
  bool nitrogenConflict = false;
  bool notesParaphrased = false;   // comments are a summary in the app's words, not the book's wording (shown without quote marks)
  double packetG = 5;
  // What the books' notes say, matched once when the database loads (rating yeasts runs on every keystroke).
  struct Notes { bool ready = false, stripsFruit = false, red = false, strong = false, fruity = false, neutral = false, malic = false, residual = false; };
  Notes notes;
};
// Fill YeastInfo::notes from the strain's comments (done by loadDatabase; call it for a YeastInfo built by hand).
void readYeastNotes(YeastInfo& y);
struct JuiceInfo {
  std::string id, name, fromFruit, note;
  std::optional<double> sg;
  double ratio = 1;
  bool acidic = false;
};
struct SchrammEntry { SchrammRow row; std::string note; };
struct SugarPreset { std::string name, src; double ppg; };
struct HoneyInfo { std::string name, group, note; };

struct Database {
  std::vector<FruitInfo> fruits;
  std::vector<YeastInfo> yeasts;
  std::vector<JuiceInfo> juices;
  std::map<std::string, SchrammEntry> schramm;
  std::vector<SugarPreset> sugars;
  std::vector<HoneyInfo> honeys;
  std::vector<std::string> adjuncts;

  const FruitInfo* fruit(const std::string& id) const;
  const YeastInfo* yeast(const std::string& id) const;
  const YeastInfo* yeastByCode(const std::string& code) const;
  const JuiceInfo* juiceInfo(const std::string& id) const;
  const SchrammEntry* schrammRow(const std::string& key) const;
};

// Build the database from the four JSON files (fruits.json, yeasts.json, schramm.json, reference.json).
Database loadDatabase(const std::string& fruitsJson, const std::string& yeastsJson,
                      const std::string& schrammJson, const std::string& referenceJson);

// Juice SG estimated from a sugar % (a gallon ≈ 8.33 lb; sugar × 46 points). Rounded to 3 decimals.
double juiceSGFromSugar(double sugarPct);
struct JuiceDefault { std::optional<double> sg; bool estimated; };
JuiceDefault juiceDefaultSG(const Database& db, const JuiceInfo& j);

// ---------- A recipe as the user designs it ----------
struct DesignFruit {
  std::string fruitId, name, note;
  double lb = 0;
  std::optional<double> water, sugar;   // empty = use the fruit table
  Phase phase = Phase::Primary;
};
struct DesignJuice {
  std::string juiceId, name;
  double gal = 0, sg = 1, ratio = 1;
  Phase phase = Phase::Primary;
  bool sgEstimated = false;
};
struct DesignSugar { std::string name; double lb = 0, ppg = C::SUGAR_PPG; Phase phase = Phase::Primary; };
struct DesignAdjunct { std::string name, amount, when; };

struct PlannedBacksweeten { double toFG, lb; };

struct Design {
  std::string name = "New mead", style = "Traditional", kind = "original", parentId, notes, brewDate;
  double volumeGal = 5;
  bool countFruitWater = true;
  std::string sweetness = "medium";       // key on the active sweetness scale
  std::optional<double> targetFG;         // set by the sweetness slider
  std::vector<HoneyIn> honeys;
  std::vector<DesignFruit> fruits;
  std::vector<DesignJuice> juices;
  std::vector<DesignSugar> sugars;
  std::vector<DesignAdjunct> adjuncts;
  std::string yeastId;
  std::optional<double> yeastTolerance;   // override
  bool wild = false, noGravityModel = false;
  std::optional<PlannedBacksweeten> backsweeten;   // "B · ferment dry, then sweeten" added to the plan
};

// Same shape as the browser version's saved designs (so its exports can be imported).
Design designFromJson(const json::Value& v);
json::Value designToJson(const Design& d);

// A published recipe (an item of recipes.json) as a design, exactly as printed. kind = "published".
Design designFromPublished(const json::Value& recipe, const Database& db);
// A published recipe with its ingredients. The built-in list has the books' recipes by name and chapter only
// (an index); the full recipes come from a "book pack" file the app loads if it's there.
bool recipeHasIngredients(const json::Value& recipe);
// Sweetness key the browser version infers from a published FG.
std::string sweetFromFG(std::optional<double> fg);

// How far an adapted design has moved from its published original.
struct Faithfulness { double scale; std::vector<std::string> changes; std::string level; };
Faithfulness faithfulness(const Design& design, const Design& parent);

// ---------- Design → engine ----------
struct EngineInput {
  Recipe recipe;
  std::vector<const FruitInfo*> fruitBase;   // parallel to recipe.fruits (may be null)
  std::vector<const JuiceInfo*> juiceBase;   // parallel to recipe.juices (may be null)
  const YeastInfo* yeast = nullptr;
  std::string yeastName;
  bool yeastAssumed = false;
  std::string sweetness;
};
EngineInput toEngine(const Design& d, const Database& db);

// ---------- Review ----------
struct Note { std::string level, text, src; };   // level: "warn" | "info" | "good"
std::vector<Note> review(const Design& d, const Result& res, const EngineInput& eng, const std::string& scaleName);

// ---------- Fruit character ----------
struct FruitCharacter { std::vector<std::string> names; std::string level, basis, note, src; };
std::vector<FruitCharacter> fruitCharacter(const EngineInput& eng, const Result& res, const Database& db,
                                           const std::string& sweetness);

// ---------- Profile bars ----------
struct Zone { double from, to; std::string label; int shade; };
std::vector<Zone> sweetnessZones(const std::string& scaleName);    // 0.990 – 1.050
std::vector<Zone> strengthZones();                                // 0 – 20 % ABV
struct FruitScale { std::vector<Zone> zones; double max; std::string src; bool lemonLime; };
FruitScale fruitScale(const Database& db, const std::string& fruitId, double batchGal);
std::string fruitLevelFor(const Database& db, const std::string& fruitId, double lb, double batchGal);
std::string sweetKeyFor(double fg, const std::string& scaleName);
double targetFGFor(const Design& d, const std::string& scaleName);   // slider value, else middle of the chosen level

// "How do I get to this sweetness?" — the options under the sweetness slider.
struct SweetOption {
  std::string kind;     // "ok" | "lessHoney" | "strongerYeast" | "A" | "B" | "balance"
  std::string title, text, src;
  std::optional<double> applyOG;          // re-solve honey to this effective OG
  std::optional<double> backsweetenLb;    // or plan this much honey after stabilising
  std::optional<double> honeyLb;          // honey the option needs (total)
  bool reachable = true;
};
std::vector<SweetOption> sweetOptions(const Design& d, const EngineInput& eng, const Result& res,
                                      double targetFG, const Settings& s);

// ---------- Directions: brew-day steps and the timeline after it ----------
struct ScheduleItem {
  std::optional<double> day;      // days after brew day; empty = "when the gravity reaches sg"
  std::string title, detail, src, kind;
  std::optional<double> sg;
  bool brewDay = false;           // covered by the brew-day steps (the Directions tab hides it from the timeline)
};
// Port of the browser version's schedule (timings from the books). nutrients may be null (wild ferment).
std::vector<ScheduleItem> schedule(const Design& d, const Result& res, const EngineInput& eng, const SNA* nutrients);

struct Step { std::string title, detail, src; };
struct Directions { std::vector<Step> prep, brewDay; };
// Piatz's basic process (ch.5) written out with this recipe's amounts.
Directions directions(const Design& d, const Result& res, const EngineInput& eng);

// ---------- Styles (Piatz ch.2, following the BJCP categories) ----------
struct StyleInfo { std::string key, name, plain; };
const std::vector<StyleInfo>& styles();
const StyleInfo* styleByName(const std::string& nameOrKey);
struct StyleMatch { const StyleInfo* style; std::string because; };
StyleMatch detectStyle(const Design& d, const Database& db);   // from the ingredients
// "Fruit and spice mead · sack strength (14–18%) · sweet" with what each part means
struct ProfileWords { std::string style, strength, strengthPlain, sweetness, sweetnessPlain; };
ProfileWords describeProfile(const Design& d, const Result& r, const Database& db, const std::string& scaleName);

// ---------- Goal planner ("I have these; make me a mead") ----------
struct GoalFruit {
  std::string fruitId;
  std::optional<double> haveLb;     // what you have on hand
  std::string intensity = "medium"; // mild | medium | strong | all (use everything you have)
  std::string phase = "auto";       // auto | primary | secondary
};
struct Goal {
  std::string name = "Planned mead", honeyName = "Wildflower", sweetness = "medium";
  double volumeGal = 5, abv = 12;
  std::string yeastId;              // empty = suggest one
  std::string strategy = "auto";    // auto | tolerance | dry
  std::vector<GoalFruit> fruits;
  // When "When" is left on Recommend, non-acidic fruit goes in on brew day ("piatz") or after 2–4 weeks ("schramm").
  // Only planForEquipment() uses this; plan() keeps the browser version's behaviour.
  std::string fruitTiming = "piatz";
  // How full the primary may be on brew day: "two-thirds" (Piatz's limit, the default) or "three-quarters"
  // (your choice: more mead from the same bucket, but stir gently and watch for foam). planForEquipment() only.
  std::string fill = "two-thirds";
  // Spices, herbs, flowers, oak: each at a level (see mead_spices.hpp). planForEquipment() only.
  struct Spice { std::string spiceId; std::string level = "noticeable"; };   // subtle | noticeable | strong
  std::vector<Spice> spices;
};
double fillFraction(const Goal& goal);           // 2/3 or 3/4
std::string fillWords(const Goal& goal);         // "two-thirds" or "three-quarters"
struct Reason { std::string text, src; };
struct Plan {
  Design design;
  std::vector<Reason> why;
  std::string strategy;
  double targetOG = 1;
  const YeastInfo* yeast = nullptr;
  bool reachable = true;
};
Plan plan(const Goal& goal, const Database& db, const Settings& s, const std::string& scaleName);

// Lemons and limes in the kitchen (neither book gives amounts): what a pound yields, and a starting dose.
// Yields: about 2 tbsp juice and 2 tsp zest per Persian lime, ½–¾ cup juice per pound; about 2 tbsp juice and
// 1 tbsp zest per lemon, 4–5 lemons per pound. Starting dose: about one fruit's worth per gallon (homebrewers'
// rule of thumb), then add more to taste after a bench trial.
struct CitrusYield {
  double juiceTbspLo = 0, juiceTbspHi = 0, zestTbspLo = 0, zestTbspHi = 0;   // from the pounds you have (0 if unknown)
  double startJuiceTbsp = 0, startZestTbsp = 0;                               // for this batch
};
CitrusYield citrusYield(const std::string& fruitName, double lb, double batchGal);
// Kitchen measure: "2 tsp", "3 tbsp", "½ cup", "1¼ cups" (rounded to the nearest ½ tbsp or ¼ cup).
std::string kitchenMeasure(double tbsp);
// A range in one unit: "8–14 tbsp" or "1½–2¼ cups".
std::string kitchenRange(double loTbsp, double hiTbsp);

// The thickest honey-and-water must the books mix before fruit goes in: Piatz's Strawberry–Banana–Ginger, 1.254 (ch.10).
// A plan that would need more than this can't be made as asked (honey itself is only about 1.42).
constexpr double MAX_MIXING_OG = 1.254;

// What sits in the primary fermenter on brew day: the honey must plus any fruit added at the start
// (fruit's volume estimated as its weight ÷ 8.33 lb/gal — an approximation, fruit is close to water's density).
double primaryContentsGal(const Design& d, const Result& r, const Database& db);

// The planner, fitted to your equipment:
//  • fruit timing: Piatz adds fruit on brew day, Schramm after primary slows (acidic fruit always later);
//  • if letting the yeast stop sweet would start the fermentation at 1.140 or more (Schramm: some yeasts struggle),
//    and you left the strategy on "Choose for me", it ferments dry and you sweeten afterwards instead;
//  • with a fermenter size, the batch shrinks until the primary is at most two-thirds full (Piatz ch.9).
struct FittedPlan {
  Plan plan;
  std::vector<Reason> fit;     // what was changed and why (shown first)
  double primaryGal = 0, laterGal = 0, fermenterGal = 0, fillLimitGal = 0;
  bool fits = true;
};
FittedPlan planForEquipment(const Goal& goal, const Database& db, const Settings& s, const std::string& scaleName, double fermenterGal);

// How well each yeast suits a goal, judged on two things separately:
//   strength — can it reach your target ABV (Piatz's tolerance table)?
//   flavour  — what the books' notes say it does, and how that suits your fruit
// mark: "★" suggested · "✓" good fit · "*" works, with a catch · "✗" not suitable · "?" no tolerance figure
struct YeastFit {
  const YeastInfo* yeast;
  std::string mark, label;           // overall
  std::string strengthMark, strength; // e.g. "✓", "Rated 18%, reaches 16%"
  std::string flavourMark, flavour;   // e.g. "✓", "Fruit-forward: …"
  std::string extra;                  // nutrients, quirks
  bool bestFlavour = false;           // the best flavour match, whatever its strength
  int rank = 1;
  double score = 0;
};
const YeastInfo* suggestYeast(const Goal& goal, const Database& db);   // the planner's pick when you leave it on "Suggest"
std::vector<YeastFit> rateYeasts(const Goal& goal, const Database& db); // best first

// Change one fruit's weight; optionally re-solve the honey so the effective OG stays put.
Design withFruitAmount(const Design& d, const Database& db, size_t index, double lb, bool holdOG, const Settings& s);
// Plan option B: note the honey to add after stabilising, and aim the sweetness there.
Design withBacksweetening(const Design& d, double toFG, double lb, const std::string& scaleName);
// Re-solve all honeys (keeping their proportions) for an effective OG; rounded to 0.01 lb.
std::optional<Design> withHoneyForOG(const Design& d, const Database& db, double og, const Settings& s);

}  // namespace mead
