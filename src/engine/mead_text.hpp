// Mead Workshop — the words on the screens (C++17, no Qt dependency).
//
// Every sentence, label, list and date the app shows is built here from the engine's numbers, so the
// tests can check the wording against the plan or recipe it describes (tests/consistency_tests.cpp).
// The Qt layer (src/app/designer.cpp) only copies these strings into QVariant maps.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "mead_advisor.hpp"

namespace mead::text {

// ---------- number formats used everywhere on screen ----------
std::string fixed(double v, int digits);   // toFixed
std::string wt(double lb);                 // "12.25 lb"
std::string vol(double gal);               // "5.00 gal"
std::string grams(double g);               // "2.5 g" below 10 g, "12 g" from 10 g
std::string num(double v);                 // shortest form, like QString::number: 7.2, 40, 14
std::string lower(std::string s);

// ---------- dates (proleptic Gregorian, no time zones) ----------
struct Date { int y = 0, m = 0, d = 0; };
std::optional<Date> parseISO(const std::string& iso);     // "2026-10-20"; empty if not a real date
Date addDays(const Date& date, int days);
std::string isoOf(const Date& date);
std::string shortLabel(const Date& date);                 // "Tue, Oct 20"
int daysBetween(const Date& a, const Date& b);            // b − a

// ---------- Plan tab ----------
struct Row { std::string name, amount, when; };
struct PlanText {
  std::vector<std::string> summary;   // "In short"
  std::vector<Row> honeys, fruits, extras;
  std::string yeastLine;              // "71B · 2 packets (5 g each…) · rehydrate with …"
  std::string strategy;               // "Ferment dry, then stabilize and backsweeten"
  std::string backsweeten;            // empty unless planned
  std::string styleWords;             // "Melomel · standard strength · medium"
  std::string waterHint;              // the Water row's note: mix to X gal, hydrometer should read Y
  std::string problem;                // empty when the plan can be made; otherwise why not (and the summary is just this)
  std::string bottled;                // "about 3.6–3.9 gal" and why (the app's estimate)
  double bottledLo = 0, bottledHi = 0;
  std::vector<std::string> more;      // "Want more mead?": what to add for +½, +1, +1½ gal
};
// goal: the goal as planned (blank fruit rows removed). r: formulate(toEngine(fp.plan.design).recipe).
PlanText planText(const FittedPlan& fp, const Goal& goal, const Result& r, const Database& db,
                  const std::string& scaleName, double fermenterGal, const Settings& s = Settings{});

// What you'll likely bottle (the app's rough estimate, not from the books): the batch minus juice the fruit pulp
// keeps (10–20% of the fruit's water) and sediment left at two rackings (0.15–0.25 gal per 5 gal each).
struct Bottled { double lo, hi; };
Bottled bottledEstimate(const Design& d, const Result& r);

// "Want more mead?" One option per size step: a bigger batch with the same fruit (more honey and water),
// and, for fruit you use all of, the fruit scaled up too so it tastes as strong.
struct MoreOption {
  double addGal = 0, volumeGal = 0;
  bool moreFruit = false;            // true: fruit scaled up to keep its strength
  Design design;                     // the bigger plan
  double brewDayGal = 0, fullPct = 0;
  bool withinFill = true;            // within the fill limit you chose (or no fermenter given)
  std::string text;
};
std::vector<MoreOption> moreMead(const FittedPlan& fp, const Goal& goal, const Database& db, const Settings& s,
                                 const std::string& scaleName, double fermenterGal);

// The Plan's Yeast drop-down.
std::string suggestChoiceLabel(const YeastInfo* pick);    // "Suggest one for me (★ Lalvin 71B)"
std::string yeastChoiceLabel(const YeastFit& f);          // "★  Lalvin 71B – Narbonne (14%) · Suggested"

// The Plan's "Yeast options" card: the chosen yeast and the best flavour match always, plus up to four others, best first.
std::vector<YeastFit> yeastOptionsShown(const Goal& goal, const Database& db, const YeastInfo* chosen);

// The planned design as it is copied into the Designer ("Use this plan").
Design designFromPlan(const FittedPlan& fp, const std::string& brewDateISO);

// ---------- Designer tab ----------
std::string yeastAmountLine(const SNA& sna);              // "How much: 2 packets …"
std::string yeastInfo(const YeastInfo& y);
struct NutrientText { std::string goFerm, mix, caveat, schramm, chemistry; std::vector<Row> doses; };
NutrientText nutrientText(const SNA& sna);
std::string fruitBarLabel(const Database& db, const std::string& fruitId, double lb, double batchGal);

// Review notes the Designer adds on top of review(): fermenter too full (first) and style label mismatch (last).
// tooThick: the honey and water before fruit would be heavier than any must in the books (MAX_MIXING_OG).
struct ExtraNotes { std::optional<Note> overfull, tooThick, styleMismatch; };
ExtraNotes designerNotes(const Design& d, const Result& r, const Database& db, double fermenterGal, double fillFraction = 2.0 / 3.0);

// The published original, under an adapted recipe.
std::string publishedFigures(const json::Value& recipe);  // "OG 1.120 (before fruit), effective 1.104, FG 1.010."
std::string publishedNotes(const json::Value& recipe);

// ---------- Directions tab ----------
struct ShoppingItem { std::string what, amount, note; };
constexpr size_t kShortAmount = 20;                   // longer "amounts" are instructions: shown with the timing instead
std::string whenWords(const std::string& when);       // "secondary" → "around day 21, when you move it to the carboy"
std::vector<ShoppingItem> shoppingList(const Design& d, const Result& r, const EngineInput& eng);

struct TimelineEntry {
  std::string when, title, detail, src, kind;
  std::optional<int> day;           // days after brew day; empty = at a gravity
  std::optional<std::string> date;  // ISO date when the brew date is valid
};
// The schedule after brew day, with the recipe's added spices/zest/juice, in date order. Uses d.brewDate.
std::vector<TimelineEntry> timeline(const Design& d, const Result& r, const EngineInput& eng);

// ---------- The math ----------
struct MathLine { std::string k, v; };
std::vector<MathLine> mathLines(const Result& r);

// ---------- Solvers with their messages (the little notice at the bottom of the window) ----------
struct Solved { std::optional<Design> design; std::string message; };
Solved solveOG(const Design& d, const Database& db, const std::string& ogText, const Settings& s);
Solved solveABV(const Design& d, const Database& db, const std::string& abvText, const Settings& s, const std::string& scaleName);
Solved applyOption(const Design& d, const Database& db, double og, double fg, const Settings& s, const std::string& scaleName);
// A typed number: blank → empty; accepts "1.5" and "1,5"; ignores the system locale.
std::optional<double> parseNumber(const std::string& t);

// ---------- My recipes (files) ----------
// A file name (without ".json") for a recipe name that works on Windows and Linux: no \ / : * ? " < > |,
// no control characters, no trailing dots or spaces, not a reserved Windows name (CON, NUL, COM1…),
// at most 80 characters. Blank → "Untitled recipe".
std::string recipeFileStem(const std::string& name);
// The stem itself if it's free, otherwise "stem (2)", "stem (3)"… (taken() compares case-insensitively itself).
std::string uniqueStem(const std::string& stem, const std::function<bool(const std::string&)>& taken);

}  // namespace mead::text
