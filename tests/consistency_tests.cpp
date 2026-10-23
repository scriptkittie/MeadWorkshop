// Consistency audit: plan thousands of different meads and check every plan against the books' rules.
// Each rule below names where it comes from. Prints a summary per rule and the first few failures.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <regex>
#include <sstream>
#include <string>

#include "mead_advisor.hpp"
#include "mead_text.hpp"
#include "mead_words.hpp"
#include "mead_spices.hpp"

#ifndef MEAD_SOURCE_DIR
#define MEAD_SOURCE_DIR "."
#endif

using namespace mead;

static std::string slurp(const std::string& rel) {
  std::ifstream f(std::string(MEAD_SOURCE_DIR) + "/" + rel, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

struct Rule { int checked = 0, failed = 0; std::vector<std::string> examples; };
static std::map<std::string, Rule> rules;
static void rule(const std::string& name, bool ok, const std::string& detail) {
  Rule& r = rules[name];
  ++r.checked;
  if (!ok) { ++r.failed; if (r.examples.size() < 4) r.examples.push_back(detail); }
}
static bool near(double a, double b, double t) { return std::fabs(a - b) <= t; }
static std::string sg3(double v) { return toFixed(v, 3); }
static std::string jsNum(double v) { char b[32]; std::snprintf(b, sizeof b, "%g", v); return b; }


static bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }
static bool starts(const std::string& s, const std::string& p) { return s.rfind(p, 0) == 0; }
// numbers written in a line, in order ("1.105", "12.25", "40")
static std::vector<double> numbersIn(const std::string& s) {
  std::vector<double> out;
  static const std::regex re("[0-9]+(\\.[0-9]+)?");
  for (auto it = std::sregex_iterator(s.begin(), s.end(), re); it != std::sregex_iterator(); ++it) out.push_back(std::stod(it->str()));
  return out;
}

// Every sentence shown with tappable words must read exactly the same once the links are removed,
// link only to real word-list entries, and never nest links.
static void checkLinks(const std::string& text, const std::string& tag) {
  const std::string l = mead::words::linkTerms(text);
  bool keysOk = true, nested = false;
  int depth = 0;
  for (size_t i = 0; i < l.size(); ++i) {
    if (l.compare(i, 3, "<a ") == 0) {
      if (++depth > 1) nested = true;
      const size_t k = l.find("term:", i), q = l.find('"', k + 5);
      if (k == std::string::npos || q == std::string::npos || !mead::words::find(l.substr(k + 5, q - k - 5))) keysOk = false;
    } else if (l.compare(i, 4, "</a>") == 0) --depth;
  }
  rule("Words: tappable terms leave every sentence unchanged", mead::words::stripLinks(l) == text, tag + " " + text.substr(0, 80));
  rule("Words: every tappable term is in the word list, none nested", keysOk && !nested && depth == 0, tag + " " + l.substr(0, 120));
}

// ---- The words on the Designer and Directions tabs, checked against the recipe they describe ----
static void auditDesignText(const Database& db, const Design& d, const Result& r, const EngineInput& eng, const std::string& scale,
                            double fermenter, const std::string& tag) {
  namespace T = mead::text;
  const SNA sna = piatzSNA(r.volumes.final, r.effectiveOG, r.fg.central);
  // Shopping list
  const auto shop = T::shoppingList(d, r, eng);
  auto find = [&](const std::string& what, int nth = 0) -> const T::ShoppingItem* {
    for (const auto& it : shop) if (it.what == what && nth-- == 0) return &it;
    return nullptr;
  };
  for (const auto& h : d.honeys) {
    if (h.lb <= 0) continue;
    const auto* it = find(h.name + " honey");
    rule("Screen: shopping list has every honey at the recipe's weight", it && it->amount == T::wt(h.lb), tag + " " + h.name);
  }
  for (size_t i = 0; i < d.fruits.size(); ++i) {
    int nth = 0;   // the same fruit can be on two rows (e.g. some on brew day, some later)
    for (size_t k = 0; k < i; ++k) if (eng.recipe.fruits[k].name == eng.recipe.fruits[i].name) ++nth;
    const auto* it = find(eng.recipe.fruits[i].name, nth);
    rule("Screen: shopping list has every fruit at the recipe's weight", it && it->amount == T::wt(d.fruits[i].lb), tag + " " + eng.recipe.fruits[i].name);
    if (it) rule("Screen: shopping list says when each fruit goes in, matching the recipe",
                 has(it->note, "later") == (d.fruits[i].phase == Phase::Secondary), tag + " " + it->what + ": " + it->note);
  }
  for (const auto& a : d.adjuncts) {
    if (a.name.empty()) continue;
    const auto* it = find(a.name);
    rule("Screen: shopping list has every extra (zest, juice, spice), with its amount or instruction",
         it && (a.amount.size() <= T::kShortAmount ? it->amount == a.amount : it->amount.empty() && has(it->note, a.amount)), tag + " " + a.name);
    if (it) rule("Screen: shopping list says when in plain words, never 'secondary'/'aging'/'bottling'",
                 it->note != "secondary" && it->note != "aging" && it->note != "bottling" && it->note != "primary" && it->amount.size() <= T::kShortAmount, tag + " " + it->note);
  }
  {
    const auto* w = find("Water (dechlorinated)");
    rule("Screen: shopping list water = the recipe's water to add", w && w->amount == T::vol(std::max(0.0, r.volumes.waterToAdd)), tag);
  }
  if (!d.wild && eng.yeast) {
    const auto* y = find(eng.yeast->name);
    rule("Screen: shopping list yeast packets = Piatz's one per 5 gal", y && numbersIn(y->amount).size() == 1 && numbersIn(y->amount)[0] == sna.packets, tag);
    const auto* gf = find("Go-Ferm");
    const auto* fk = find("Fermaid K");
    const auto* dap = find("DAP (diammonium phosphate)");
    rule("Screen: shopping list nutrients = the nutrient card's amounts", gf && fk && dap && gf->amount == T::grams(sna.goFerm5g) &&
         fk->amount == T::grams(sna.fermaidK) && dap->amount == T::grams(sna.dap), tag);
    // Yeast card and nutrient card use the same numbers
    const std::string amt = T::yeastAmountLine(sna);
    rule("Screen: yeast amount line matches the packets and Go-Ferm", starts(amt, "How much: " + std::to_string(sna.packets) + " packet" + (sna.packets > 1 ? "s " : " ")) &&
         has(amt, T::grams(sna.goFerm5g) + " Go-Ferm"), tag + " " + amt);
    const T::NutrientText nt = T::nutrientText(sna);
    rule("Screen: nutrient mix line adds up", nt.mix == "Fermaid K " + T::grams(sna.fermaidK) + " + DAP " + T::grams(sna.dap) + " = " + T::grams(sna.mix) &&
         near(sna.fermaidK + sna.dap, sna.mix, 1e-9), tag + " " + nt.mix);
    rule("Screen: one dose line per nutrient dose", nt.doses.size() == sna.doses.size(), tag);
  }
  rule("Screen: shopping list has stabilizers exactly when the mead will be sweetened (Schramm ch.5)",
       (find("Potassium sorbate + metabisulfite") != nullptr) == (d.backsweeten.has_value() || d.sweetness != "dry"), tag);

  for (const auto& nn : review(d, r, eng, scale)) checkLinks(nn.text, tag);
  for (const auto& it : shop) checkLinks(it.what + " " + it.amount + " " + it.note, tag);
  // Timeline with dates
  const auto tl = T::timeline(d, r, eng);
  for (const auto& e : tl) checkLinks(e.title + "\n" + e.detail, tag);
  const auto brew = T::parseISO(d.brewDate);
  double last = -1;
  bool sorted = true;
  for (const auto& e : tl) {
    const double key = e.day ? *e.day : 4.5;
    if (key < last - 1e-9) sorted = false;
    last = key;
    if (e.day) {
      const std::string head = *e.day == 0 ? "Brew day" : "Day " + std::to_string(*e.day);
      rule("Screen: timeline 'Day N' label matches its day", starts(e.when, head), tag + " " + e.when);
      if (brew) {
        const T::Date on = T::addDays(*brew, *e.day);
        rule("Screen: timeline date = brew date + day", e.date && *e.date == T::isoOf(on) && T::daysBetween(*brew, *T::parseISO(*e.date)) == *e.day &&
             has(e.when, T::shortLabel(on)), tag + " " + e.when);
      } else {
        rule("Screen: no dates shown without a valid brew date", !e.date && !has(e.when, "·"), tag + " " + e.when);
      }
    } else {
      rule("Screen: gravity steps show their gravity", e.when.size() == 8 && starts(e.when, "At 1."), tag + " " + e.when);
    }
  }
  if (!d.wild) rule("Screen: timeline is in date order", sorted, tag);
  {
    std::map<std::string, int> titles;
    for (const auto& e : tl) ++titles[e.title + "@" + e.when];
    bool dup = false;
    for (const auto& kv : titles) if (kv.second > 1) dup = true;
    rule("Screen: no step appears twice in the timeline", !dup, tag);
  }
  if (!d.wild) {
    const bool laterFruit = std::any_of(d.fruits.begin(), d.fruits.end(), [](const DesignFruit& f) { return f.phase == Phase::Secondary; }) ||
                            std::any_of(d.juices.begin(), d.juices.end(), [](const DesignJuice& j) { return j.phase == Phase::Secondary; });
    for (const auto& a : d.adjuncts) {
      if (a.name.empty() || starts(a.name, "Honey for backsweetening")) continue;
      int want = a.when == "secondary" ? 21 : a.when == "aging" ? (laterFruit ? 56 : 42) : a.when == "bottling" ? (laterFruit ? 63 : 49) : -1;
      if (want < 0) continue;
      int found = 0;
      for (const auto& e : tl) if (e.title == "Add: " + a.name && e.day && *e.day == want) ++found;
      rule("Screen: every extra appears once in the timeline on its day (rack ~day 21; after fermentation)", found == 1, tag + " " + a.name + " " + a.when);
    }
  }

  // The math: each line's shown result is what its shown inputs give
  const auto ml = T::mathLines(r);
  auto line = [&](const std::string& k) { for (const auto& l : ml) if (l.k == k) return l.v; return std::string(); };
  {
    const auto mix = numbersIn(line("Mixing OG"));   // 1, points, gal, 1000, OG
    // the shown inputs are rounded (points to 1, gallons to 0.01), so allow for that
    auto slack = [](double P, double G) { return (0.5 / G + std::fabs(P) * 0.005 / (G * (G - 0.005))) / 1000 + 0.0006; };
    rule("Screen: math 'Mixing OG' line shows the result's OG and adds up", mix.size() == 5 && toFixed(mix[4], 3) == sg3(r.mixingOG) &&
         (mix[2] <= 0.01 || near(1 + mix[1] / mix[2] / 1000, mix[4], slack(mix[1], mix[2]))), tag + " " + line("Mixing OG"));
    const auto eff = numbersIn(line("Effective OG"));
    rule("Screen: math 'Effective OG' line shows the result's OG and adds up", eff.size() == 5 && toFixed(eff[4], 3) == sg3(r.effectiveOG) &&
         (eff[2] <= 0.01 || near(1 + eff[1] / eff[2] / 1000, eff[4], slack(eff[1], eff[2]))), tag + " " + line("Effective OG"));
    const auto ab = numbersIn(line("ABV"));         // 131, OG, FG, ABV, dry ABV
    rule("Screen: math 'ABV' line = 131 × (OG − FG) with the shown numbers (Piatz ch.9)", ab.size() == 5 && toFixed(ab[3], 1) == toFixed(r.fg.abv, 1) &&
         near(131 * (ab[1] - ab[2]), ab[3], 0.131 + 0.051), tag + " " + line("ABV"));
    const auto hon = numbersIn(line("Honey"));      // lb, ppg, points, gal, lb/gal
    rule("Screen: math 'Honey' line = lb × points per pound", hon.size() == 5 && near(hon[0] * hon[1], hon[2], 0.5 + hon[1] * 0.005), tag + " " + line("Honey"));
  }

  // Review extras
  const T::ExtraNotes ex = T::designerNotes(d, r, db, fermenter);
  if (fermenter > 0 && !d.wild)
    rule("Screen: fermenter-too-full note shown exactly when brew day passes two-thirds (Piatz ch.9)",
         ex.overfull.has_value() == (primaryContentsGal(d, r, db) > fermenter * 2 / 3 + 1e-9), tag);
  {
    const StyleInfo* named = styleByName(d.style);
    const StyleMatch sm = detectStyle(d, db);
    rule("Screen: style-label note shown exactly when the label and ingredients disagree (Piatz ch.2)",
         ex.styleMismatch.has_value() == (named && named->key != sm.style->key), tag);
  }
  if (!d.wild && r.volumes.waterToAdd >= -0.02)
    rule("Screen: 'can't be mixed' warning exactly when the must before fruit passes 1.254 (Piatz ch.10)", ex.tooThick.has_value() == (r.mixingOG > MAX_MIXING_OG + 1e-9),
         tag + " " + sg3(r.mixingOG));
  for (size_t i = 0; i < d.fruits.size(); ++i)
    rule("Screen: fruit bar label starts with the fruit's weight",
         starts(T::fruitBarLabel(db, d.fruits[i].fruitId, d.fruits[i].lb, r.volumes.final), T::wt(d.fruits[i].lb) + " · "), tag);
}

// ---- The words on the Plan tab, checked against the plan ----
static void auditPlanText(const Database& db, const Goal& g, const FittedPlan& fp, const Result& r, const std::string& scale,
                          double fermenter, const std::string& tag, const Settings& st) {
  namespace T = mead::text;
  const Plan& p = fp.plan;
  const Design& d = p.design;
  const T::PlanText t = T::planText(fp, g, r, db, scale, fermenter, st);
  const auto& S = t.summary;
  rule("In short: a plan that can be made has no problem line", t.problem.empty(), tag);
  for (const auto& l : S) checkLinks(l, tag);
  for (const auto& w : fp.fit) checkLinks(w.text, tag);
  for (const auto& w : p.why) checkLinks(w.text, tag);
  checkLinks(t.waterHint, tag);
  checkLinks(t.yeastLine, tag);
  {
    static const std::regex negative("(^|[^0-9A-Za-z])-[0-9]|hydrometer 0\\.");
    std::string bad;
    for (const auto& l : S) if (std::regex_search(l, negative)) bad = l;
    if (std::regex_search(t.waterHint, negative)) bad = t.waterHint;
    rule("In short: no negative amounts or gravities below 1 on the Plan", bad.empty(), tag + " " + bad);
  }
  rule("In short: first line names the batch and fermenter", !S.empty() &&
       S[0] == "Make " + T::vol(d.volumeGal) + (fermenter > 0 ? " in your " + T::num(fermenter) + "-gal fermenter" : "") + ".", tag + " " + (S.empty() ? "" : S[0]));
  double honey = 0;
  for (const auto& h : d.honeys) honey += h.lb;
  std::vector<const DesignFruit*> early, later;
  for (const auto& f : d.fruits) (f.phase == Phase::Primary ? early : later).push_back(&f);
  if (S.size() > 1) {
    const std::string& b = S[1];
    rule("In short: brew-day line = the recipe's honey, volume and hydrometer reading",
         starts(b, "Brew day: pour about " + T::vol(std::max(0.0, r.volumes.waterToAdd)) + " of water into the bucket and stir in " + T::wt(honey) +
                   " of honey. That's all the water: with the honey in, the level comes up to about " + T::vol(r.volumes.preFruit) + " (the honey takes up " +
                   T::vol(r.volumes.honey) + "). Check that the hydrometer reads " + sg3(r.mixingOG)) &&
         near(r.volumes.waterToAdd + r.volumes.honey, r.volumes.preFruit, 0.006 + r.volumes.juicePrimary + r.volumes.sugar), tag + " " + b);
    rule("In short: brew-day line adds fruit exactly when fruit goes in on brew day", has(b, ". Then add ") == !early.empty(), tag + " " + b);
    for (const auto* f : early)
      rule("In short: every brew-day fruit is named with its weight", has(b, T::wt(f->lb) + " " + T::lower(f->name)), tag + " " + f->name);
  }
  const std::string* rack = nullptr;
  for (const auto& l : S) if (starts(l, "Around day 21")) rack = &l;
  const bool zest = std::any_of(d.adjuncts.begin(), d.adjuncts.end(), [](const DesignAdjunct& a) { return a.when == "secondary"; });
  rule("In short: a 'day 21' line exactly when something goes in at racking", (rack != nullptr) == (!later.empty() || zest), tag);
  for (const auto* f : later)
    rule("In short: every later fruit is named with its weight in the day-21 line", rack && has(*rack, T::wt(f->lb) + " " + T::lower(f->name)), tag + " " + f->name);
  for (const auto* f : early)
    rule("In short: a brew-day fruit is never also listed for day 21", !rack || !has(*rack, T::wt(f->lb) + " " + T::lower(f->name)), tag + " " + f->name);
  // the finish line
  int finishLines = 0;
  for (const auto& l : S) {
    const bool tolLine = starts(l, "The yeast should stop on its own"), dryLine = starts(l, "It ferments dry"), weakLine = starts(l, "The yeast should stop around");
    if (!tolLine && !dryLine && !weakLine) continue;
    ++finishLines;
    rule("In short: 'stops on its own' exactly for the yeast-stops-sweet strategy", tolLine == (p.strategy == "tolerance"), tag + " " + l);
    if (p.strategy != "tolerance")
      rule("In short: 'ferments dry' only when the prediction is dry (Piatz ch.9)", dryLine == (r.fg.reachesDry || r.fg.central <= 1.0015), tag + " " + l);
    rule("In short: alcohol in the summary = the prediction", has(l, toFixed(r.fg.abv, 1) + "% alcohol"), tag + " " + l);
    if (!dryLine) rule("In short: finishing gravity in the summary = the prediction", has(l, "around " + sg3(r.fg.central)), tag + " " + l);
  }
  rule("In short: exactly one line says how it finishes", finishLines == 1, tag);
  {
    const std::string* bs = nullptr;
    for (const auto& l : S) if (starts(l, "When it's finished and clear")) bs = &l;
    rule("In short: backsweetening line exactly when it's planned (Schramm ch.5)", (bs != nullptr) == d.backsweeten.has_value(), tag);
    if (bs && d.backsweeten) rule("In short: backsweetening honey and gravity = the plan", has(*bs, T::wt(d.backsweeten->lb)) && has(*bs, sg3(d.backsweeten->toFG)), tag + " " + *bs);
    rule("Plan: backsweeten line on the Plan card exactly when planned", t.backsweeten.empty() != d.backsweeten.has_value(), tag);
  }
  {
    const std::vector<YeastFit> rated = rateYeasts(g, db);
    int flavourLines = 0;
    for (const auto& l : S) {
      if (!starts(l, "Yeast: ")) continue;
      ++flavourLines;
      rule("In short: the yeast line names the plan's yeast", p.yeast && starts(l, "Yeast: " + p.yeast->name + " (★)"), tag + " " + l);
      bool named = false;
      for (const auto& y : rated) if (y.bestFlavour && y.yeast != p.yeast && has(l, y.yeast->name + " is the best flavour match")) {
        named = true;
        rule("In short: the best-flavour yeast's limit is its rated tolerance", y.yeast->tolerance && has(l, "stops around " + T::num(*y.yeast->tolerance) + "%"), tag + " " + l);
      }
      rule("In short: the best-flavour line names the ★ list's best flavour match", named, tag + " " + l);
    }
    rule("In short: at most one best-flavour line", flavourLines <= 1, tag);
    // Yeast list labels
    for (const auto& y : rated) {
      const std::string lab = T::yeastChoiceLabel(y);
      rule("Screen: each yeast's label starts with its mark and names it", starts(lab, y.mark + "  " + y.yeast->vendor + " " + y.yeast->code) && has(lab, "· " + y.label), tag + " " + lab);
    }
    const YeastInfo* pick = suggestYeast(g, db);
    rule("Screen: 'Suggest one for me' names the ★ yeast", T::suggestChoiceLabel(pick) == (pick ? "Suggest one for me (★ " + pick->vendor + " " + pick->code + ")" : "Suggest one for me") &&
         (!pick || rated.empty() || rated[0].yeast == pick), tag);
    const auto shown = T::yeastOptionsShown(g, db, p.yeast);
    bool hasChosen = p.yeast == nullptr, hasBest = true;
    for (const auto& y : rated) if (y.bestFlavour) { hasBest = false; for (const auto& s2 : shown) if (s2.yeast == y.yeast) hasBest = true; }
    for (const auto& s2 : shown) if (s2.yeast == p.yeast) hasChosen = true;
    rule("Screen: Yeast options always show the chosen yeast and the best flavour match", hasChosen && hasBest, tag);
  }
  // Recipe rows
  rule("Plan: one honey row per honey, at the plan's weight", t.honeys.size() == d.honeys.size() &&
       std::equal(d.honeys.begin(), d.honeys.end(), t.honeys.begin(), [](const HoneyIn& h, const T::Row& row) { return row.amount == T::wt(h.lb); }), tag);
  bool fruitRowsOk = t.fruits.size() == d.fruits.size();
  for (size_t i = 0; fruitRowsOk && i < d.fruits.size(); ++i)
    fruitRowsOk = t.fruits[i].name == d.fruits[i].name && t.fruits[i].amount == T::wt(d.fruits[i].lb) &&
                  starts(t.fruits[i].when, "later") == (d.fruits[i].phase == Phase::Secondary);
  rule("Plan: fruit rows = the plan's fruit, weight and timing", fruitRowsOk, tag);
  bool extrasOk = t.extras.size() == d.adjuncts.size();
  for (size_t i = 0; extrasOk && i < d.adjuncts.size(); ++i)
    extrasOk = has(t.extras[i].when, d.adjuncts[i].amount) && has(t.extras[i].when, "carboy") == (d.adjuncts[i].when == "secondary");
  rule("Plan: extras rows = the plan's zest/juice amounts and timing", extrasOk, tag);
  if (p.yeast) {
    const SNA sna = piatzSNA(r.volumes.final, r.effectiveOG, r.fg.central);
    rule("Plan: yeast line = the plan's yeast and Piatz's packet count", starts(t.yeastLine, p.yeast->name + " · " + std::to_string(sna.packets) + " packet"), tag + " " + t.yeastLine);
  }
  rule("Plan: strategy label matches the plan", p.strategy == "tolerance" ? t.strategy == "Let the yeast stop sweet at its limit"
                                                : has(t.strategy, "backsweeten") == d.backsweeten.has_value(), tag + " " + t.strategy);
  // Brew-day fruit: freeze/crush advice, and the honey-water warning when it alone is 1.140+
  if (!early.empty()) {
    rule("In short: brew-day fruit comes with 'freeze and thaw, or crush'", S.size() > 1 && has(S[1], "freeze and thaw, or crush"), tag);
    if (r.mixingOG >= 1.14 && r.primaryOG < 1.14) {
      bool said = false;
      for (const auto& f : fp.fit) if (has(f.text, "The honey water alone reads about " + sg3(r.mixingOG))) said = true;
      rule("Plan warns when the honey water alone is 1.140+ and the start relies on the fruit's juice (Schramm ch.5)", said, tag);
    }
  }
  // What you'll bottle
  rule("Bottling estimate is below the batch and above zero", t.bottledLo > 0 && t.bottledLo <= t.bottledHi && t.bottledHi < r.volumes.final, tag);
  rule("In short: says how much you'll likely bottle", std::any_of(S.begin(), S.end(), [&](const std::string& l) {
         return starts(l, "Expect to bottle about " + toFixed(t.bottledLo, 1) + "–" + toFixed(t.bottledHi, 1) + " gal"); }), tag);
  // Want more mead? Each option is the size it says, as strong, and its bucket numbers are right.
  {
    const auto opts = T::moreMead(fp, g, db, st, scale, fermenter);
    for (const auto& o : opts) {
      const Result ro = formulate(toEngine(o.design, db).recipe, st);
      const Result rb = formulate(toEngine(d, db).recipe, st);
      rule("Want more: the bigger batch is the size it says", near(o.design.volumeGal, d.volumeGal + o.addGal, 1e-9) && starts(o.text, "+" + T::num(o.addGal) + " gal (a " + T::num(o.volumeGal) + "-gal batch)"), tag + " " + o.text);
      rule("Want more: same strength as the plan (same starting sugar)", near(ro.effectiveOG, rb.effectiveOG, 0.0025), tag + " " + sg3(ro.effectiveOG) + " vs " + sg3(rb.effectiveOG));
      double h = 0;
      for (const auto& hh : o.design.honeys) h += hh.lb;
      rule("Want more: honey in the text = the bigger plan's honey", has(o.text, ": " + T::wt(h) + " honey"), tag + " " + o.text);
      if (fermenter > 0)
        rule("Want more: bucket fullness is right", near(o.brewDayGal, primaryContentsGal(o.design, ro, db), 1e-9) &&
             o.withinFill == (o.brewDayGal <= fermenter * fillFraction(g) + 1e-9) && has(o.text, o.withinFill ? "% full)." : "over the " + fillWords(g)), tag + " " + o.text);
      bool fruitOk = true;
      for (size_t i = 0; i < o.design.fruits.size() && i < d.fruits.size(); ++i)
        if (o.moreFruit ? o.design.fruits[i].lb < d.fruits[i].lb - 1e-9 : (o.design.fruits[i].fruitId == d.fruits[i].fruitId && o.design.fruits[i].lb > d.fruits[i].lb + 1e-9 &&
                                                                         std::any_of(g.fruits.begin(), g.fruits.end(), [&](const GoalFruit& gf) { return gf.fruitId == d.fruits[i].fruitId && gf.intensity == "all"; })))
          fruitOk = false;
      rule("Want more: 'same fruit' keeps what you have; 'more fruit' never uses less", fruitOk, tag + " " + o.text);
    }
  }
  // "Use this plan" copies everything and records the reasons
  {
    const Design used = T::designFromPlan(fp, "2026-10-20");
    size_t numbered = 0;
    for (size_t pos = 0; (pos = used.notes.find("\n", pos)) != std::string::npos; ++pos) ++numbered;
    rule("Use plan: the Designer's notes list every reason", numbered == fp.fit.size() + p.why.size(), tag);
    rule("Use plan: backsweetening honey copied to the recipe", !d.backsweeten ||
         (used.adjuncts.size() == d.adjuncts.size() + 1 && used.adjuncts.back().amount == toFixed(d.backsweeten->lb, 2) + " lb"), tag);
  }
}

int main(int argc, char** argv) {
  const unsigned seedArg = argc > 1 ? static_cast<unsigned>(std::stoul(argv[1])) : 12345u;
  const Database db = loadDatabase(slurp("data/fruits.json"), slurp("data/yeasts.json"), slurp("data/schramm.json"), slurp("data/reference.json"));
  const std::regex acidic("lemon|lime|currant|cranberr|rhubarb|gooseberr|grapefruit|chokeberr|cherries, sour|tart cherr", std::regex::icase);
  const std::regex lemonLime("lemon|lime", std::regex::icase);

  // Fruit sets: none, single fruits of every group, mixes, and fruit + citrus.
  const std::vector<std::vector<std::string>> fruitSets = {
      {}, {"pears"}, {"apples-whole"}, {"blueberries"}, {"raspberries"}, {"strawberries"}, {"cherries-sweet"}, {"cherries-sour"},
      {"peaches"}, {"apricots"}, {"mangos"}, {"prickly-pears"}, {"bananas"}, {"blackberries"}, {"pears", "limes-whole"},
      {"blueberries", "cherries-sweet"}, {"peaches", "mangos"}, {"strawberries", "bananas"}, {"oranges-whole"}, {"plums"}, {"cranberries"}};
  const std::vector<double> abvs = {6, 10, 12, 13, 14, 15, 16, 18};
  const std::vector<std::string> sweets = {"dry", "medium", "sweet", "dessert"};
  const std::vector<std::string> intensities = {"all", "mild", "medium", "strong"};
  const std::vector<std::string> yeasts = {"", "lallemand-71b-1122", "lallemand-ec-1118", "lallemand-icv-d47"};
  const std::vector<double> fermenters = {0, 7.2, 6.5, 12};
  const std::vector<double> volumes = {1, 3, 5, 6};
  const std::vector<std::string> timings = {"piatz", "schramm"};
  const std::vector<std::string> scales = {"piatz", "schramm"};

  int plans = 0, unreachable = 0;
  unsigned seed = seedArg;
  auto rnd = [&]() { seed = seed * 1103515245u + 12345u; return (seed >> 8) % 1000000 / 1000000.0; };

  for (size_t fi = 0; fi < fruitSets.size(); ++fi)
    for (double abvT : abvs)
      for (const auto& sweet : sweets)
        for (int k = 0; k < (argc > 1 ? 6 : 4); ++k) {   // random combinations of the other settings
          Goal g;
          g.abv = abvT;
          g.sweetness = sweet;
          g.volumeGal = volumes[static_cast<size_t>(rnd() * static_cast<double>(volumes.size()))];
          g.yeastId = yeasts[static_cast<size_t>(rnd() * static_cast<double>(yeasts.size()))];
          g.fruitTiming = timings[static_cast<size_t>(rnd() * static_cast<double>(timings.size()))];
          g.fill = rnd() < 0.3 ? "three-quarters" : "two-thirds";
          for (int k = static_cast<int>(rnd() * 3); k > 0; --k) {   // 0–2 spices at random levels
            const auto& sp = spices()[static_cast<size_t>(rnd() * static_cast<double>(spices().size()))];
            g.spices.push_back({sp.id, std::vector<std::string>{"subtle", "noticeable", "strong"}[static_cast<size_t>(rnd() * 3)]});
          }
          const std::string scale = scales[static_cast<size_t>(rnd() * static_cast<double>(scales.size()))];
          if (scale == "piatz" && sweet == "dessert") continue;   // Piatz's scale has no dessert level
          const double fermenter = fermenters[static_cast<size_t>(rnd() * static_cast<double>(fermenters.size()))];
          bool skip = false;
          for (const auto& id : fruitSets[fi]) {
            if (!db.fruit(id)) { skip = true; break; }
            GoalFruit gf;
            gf.fruitId = id;
            gf.intensity = intensities[static_cast<size_t>(rnd() * static_cast<double>(intensities.size()))];
            if (gf.intensity == "all" || rnd() < 0.5) gf.haveLb = std::round((2 + rnd() * 25) * 4) / 4;
            g.fruits.push_back(gf);
          }
          if (skip) continue;
          Settings s;
          s.honeyPPG = rnd() < 0.7 ? 40 : 42;

          const FittedPlan fp = planForEquipment(g, db, s, scale, fermenter);
          ++plans;
          const Plan& p = fp.plan;
          const Design& d = p.design;
          const EngineInput eng = toEngine(d, db);
          const Result r = formulate(eng.recipe, s);
          char tagBuf[256];
          std::snprintf(tagBuf, sizeof tagBuf, "[%s | %.0f%% %s | %s | %.1f gal | ferm %.1f | yeast %s | %s timing | %s scale]",
                        fruitSets[fi].empty() ? "traditional" : fruitSets[fi][0].c_str(), abvT, sweet.c_str(), d.style.c_str(), d.volumeGal, fermenter,
                        p.yeast ? p.yeast->code.c_str() : "-", g.fruitTiming.c_str(), scale.c_str());
          const std::string tag = tagBuf;
          if (!p.reachable || !fp.fits) {
            ++unreachable;
            bool explained = false;
            for (const auto& f : fp.fit) if (f.text.find("won't fit") != std::string::npos || f.text.find("fruit alone") != std::string::npos || f.text.find("can't be mixed") != std::string::npos) explained = true;
            for (const auto& w : p.why) if (w.text.find("can't be reached") != std::string::npos) explained = true;
            rule("A plan that can't be made says why", explained, tag);
            {
              const Result rx = formulate(toEngine(d, db).recipe, s);
              const auto tx = mead::text::planText(fp, g, rx, db, scale, fermenter, s);
              rule("In short: a plan that can't be made gives the reason and no brewing steps",
                   tx.summary.size() == 1 && starts(tx.summary[0], "This plan can't be made as asked. ") && tx.summary[0].size() > 40 && !tx.problem.empty(), tag);
            }
            continue;
          }
          const double tol = eng.recipe.yeast.tolerance;
          const SweetLevel& sw = sweetnessByKey(g.sweetness, scale);

          // --- Gravity and alcohol (Piatz ch.9) ---
          // Your own yeast caps the target at its limit (the plan says so); a suggested yeast is picked to reach it.
          const bool capped = !g.yeastId.empty() && g.strategy != "tolerance" && tol > 0 && g.abv > tol + 0.5;
          const double aimABV = capped ? tol : g.abv;
          if (capped) {
            bool said = false;
            for (const auto& f : fp.fit) if (f.text.find("so the plan aims for " + jsNum(tol) + "%") != std::string::npos) said = true;
            rule("Your yeast's limit caps the target, and the plan says so (Piatz ch.4 table)", said && r.effectiveOG <= std::max(ogForDryABV(tol), ogForToleranceFinish(sw.target, tol)) + 0.0016, tag + " OG " + sg3(r.effectiveOG));
          }
          const double wantOG = p.strategy == "tolerance" ? ogForToleranceFinish(sw.target, tol) : ogForDryABV(aimABV);
          rule("Honey solved to the planned starting gravity (Piatz ch.9; honey rounded to ¼ lb)", near(r.effectiveOG, wantOG, 0.0016),
               tag + " OG " + toFixed(r.effectiveOG, 4) + " vs " + toFixed(wantOG, 4));
          rule("ABV = 131 × (OG − FG) (Piatz ch.9)", near(r.fg.abv, 131 * (r.effectiveOG - r.fg.central), 1e-9), tag);
          rule("FG = max(1.000, OG − (YT − 1)) from the yeast's limit (Piatz ch.9)",
               near(r.fg.central, std::max(1.0, r.effectiveOG - tolerancePoints(tol) / 1000), 1e-9), tag);
          if (p.strategy == "dry") {
            if (tol >= aimABV - 0.5)
              rule("'Ferment dry' plans actually ferment dry when the yeast can reach the target (Piatz ch.9)", r.fg.reachesDry || r.fg.central <= 1.0015, tag + " FG " + sg3(r.fg.central));
            else {
              bool flagged = false;
              for (const auto& w : p.why) if (w.text.find("below your") != std::string::npos && w.text.find("may stop sweet") != std::string::npos) flagged = true;
              rule("A yeast too weak for a dry target is flagged in the plan (Piatz ch.4 table)", flagged, tag);
            }
            // The yeast may stop above 1.000 (at its limit); sweetening starts from there.
            const double stopsAt = std::max(1.0, r.fg.central);
            rule("Backsweetening planned exactly when the target isn't dry and the yeast stops below it (Schramm ch.5)",
                 d.backsweeten.has_value() == (g.sweetness != "dry" && stopsAt < sw.target - 0.0005), tag + " stops " + sg3(stopsAt) + " target " + sg3(sw.target));
            if (d.backsweeten) {
              const double V = d.volumeGal, lb = d.backsweeten->lb;
              const double after = (V * (stopsAt - 1) * 1000 + lb * s.honeyPPG) / (V + lb / s.honeyLbPerGal);
              rule("Backsweetening honey, added where the yeast stops, lands on the target gravity", near(1 + after / 1000, d.backsweeten->toFG, 0.0005),
                   tag + " gives " + toFixed(1 + after / 1000, 4) + " for " + toFixed(d.backsweeten->toFG, 4) + " from " + sg3(stopsAt));
            }
          } else {
            rule("'Yeast stops sweet' plans finish in the chosen sweetness range (Piatz ch.9)",
                 r.fg.central >= sw.lo - 0.002 && r.fg.central <= sw.hi + 0.002, tag + " FG " + sg3(r.fg.central));
            rule("'Yeast stops sweet' is only used when the yeast is near the target ABV (Piatz ch.9; Schramm ch.5)", std::fabs(tol - aimABV) <= 1.0 + 1e-9 || g.strategy == "tolerance",
                 tag + " tol " + jsNum(tol));
          }
          // --- Starting gravity warning matches what the yeast actually starts in (Schramm ch.5) ---
          {
            const bool fruitIn = std::any_of(d.fruits.begin(), d.fruits.end(), [](const DesignFruit& f) { return f.phase == Phase::Primary; });
            const double start = fruitIn ? r.primaryOG : r.mixingOG;
            bool warned = false;
            for (const auto& n : review(d, r, eng, scale)) if (n.text.rfind("Starting gravity in the fermenter", 0) == 0) warned = true;
            rule("1.140 warning shown exactly when fermentation starts at 1.140 or more (Schramm ch.5)", warned == (start >= 1.14), tag + " start " + sg3(start));
            if ((g.strategy == "auto" || g.strategy.empty()) && p.strategy == "tolerance" && start >= 1.14) {
              // Allowed only when fermenting dry wouldn't start any lower, and then the plan must say so.
              Goal g2 = g;
              g2.strategy = "dry";
              const FittedPlan dryFp = planForEquipment(g2, db, s, scale, fermenter);
              const Result rd = formulate(toEngine(dryFp.plan.design, db).recipe, s);
              const bool dryIn = std::any_of(dryFp.plan.design.fruits.begin(), dryFp.plan.design.fruits.end(), [](const DesignFruit& f) { return f.phase == Phase::Primary; });
              const double dryStart = dryIn ? rd.primaryOG : rd.mixingOG;
              bool said = false;
              for (const auto& f : fp.fit) if (f.text.find("above the 1.140") != std::string::npos) said = true;
              rule("Auto strategy keeps 'yeast stops sweet' at 1.140+ only when fermenting dry starts no lower, and warns (Schramm ch.5)",
                   said && !(dryFp.plan.reachable && dryStart < start - 0.0005), tag + " start " + sg3(start) + " dry " + sg3(dryStart));
            }
          }
          // --- Equipment (Piatz ch.9) ---
          if (fermenter > 0)
            rule("Brew day fills the fermenter at most the limit you chose (two-thirds is Piatz's, ch.9)", fp.primaryGal <= fermenter * fillFraction(g) + 1e-6,
                 tag + " " + toFixed(fp.primaryGal, 2) + " gal in " + jsNum(fermenter));
          // --- Fruit (Schramm ch.5, ch.8) ---
          for (const auto& f : d.fruits) {
            rule("Lemons and limes become zest and juice, never whole fruit (Schramm ch.5; Piatz ch.3)", !std::regex_search(f.name, lemonLime), tag + " " + f.name);
            if (std::regex_search(f.name, acidic))
              rule("Acidic fruit goes in after fermentation slows (Schramm ch.5)", f.phase == Phase::Secondary, tag + " " + f.name);
            for (const auto& gf : g.fruits)
              if (gf.fruitId == f.fruitId && gf.haveLb)
                rule("Never plans more fruit than you have", f.lb <= *gf.haveLb + 1e-9, tag + " " + f.name + " " + jsNum(f.lb) + " > " + jsNum(*gf.haveLb));
          }
          // --- Yeast ---
          if (g.yeastId.empty() && p.yeast) {
            const auto rated = rateYeasts(g, db);
            const YeastInfo* star = nullptr;
            for (const auto& yr : rated) if (yr.mark == "★") { star = yr.yeast; break; }
            rule("Suggested yeast is the ★ in the Yeast list", star == nullptr || p.yeast == star, tag + " plan " + p.yeast->code + " vs ★ " + (star ? star->code : "-"));
            rule("Suggested yeast can reach the target ABV (Piatz ch.4 table)", *p.yeast->tolerance >= g.abv - 0.5, tag);
            rule("Suggested yeast is a dry yeast (the rehydration steps are for dry packets)", p.yeast->form == "dry", tag);
          }
          if (!g.yeastId.empty()) rule("Your own yeast choice is kept", p.yeast && p.yeast->id == g.yeastId, tag);
          {
            // Messages only name the plan's yeast or the best-flavour alternative.
            std::string text;
            for (const auto& f : fp.fit) text += f.text + "\n";
            for (const auto& w : p.why) text += w.text + "\n";
            const auto rated = rateYeasts(g, db);
            std::string alt;
            for (const auto& yr : rated) if (yr.bestFlavour) alt = yr.yeast->code;
            bool stray = false;
            for (const auto& y : db.yeasts) {
              if (p.yeast && y.code == p.yeast->code) continue;
              if (y.code == alt) continue;
              if (y.code.size() >= 4 && text.find(y.code) != std::string::npos) { stray = true; break; }
            }
            rule("Plan text never names a yeast it isn't using or offering", !stray, tag);
          }
          // --- Nutrients and rehydration (Piatz ch.4–5; Lallemand) ---
          {
            const SNA n = piatzSNA(r.volumes.final, r.effectiveOG, r.fg.central);
            const double per5 = r.volumes.final / 5;
            rule("Nutrients scale Piatz's 4 g Fermaid K + 8 g DAP per 5 gal (Piatz ch.5)", near(n.fermaidK, 4 * per5, 1e-9) && near(n.dap, 8 * per5, 1e-9), tag);
            rule("One yeast packet per 5 gal, at least one (Piatz ch.5)", n.packets == std::max(1, static_cast<int>(std::lround(r.volumes.final / 5))), tag);
            rule("Go-Ferm 1.25 × yeast weight, water 20 × Go-Ferm (Lallemand)", near(n.goFerm5g, 6.25 * n.packets, 1e-9) && near(n.water5gMl, 125.0 * n.packets, 1e-9), tag);
          }
          // --- Classification (Piatz ch.2) ---
          {
            const double a = r.fg.abv;
            const std::string want = a < 3.5 ? "Below hydromel" : a < 7.5 ? "Hydromel" : a < 14 ? "Standard" : a <= 18 ? "Sack" : "Above sack strength";
            rule("Strength label matches Piatz's ranges (hydromel 3.5–7.5, standard 7.5–14, sack 14–18)", r.strength == want, tag + " " + r.strength + " at " + toFixed(a, 2));
            const StyleMatch sm = detectStyle(d, db);
            const bool hasFruit = !d.fruits.empty();
            const bool hasSpice = std::any_of(d.adjuncts.begin(), d.adjuncts.end(), [](const DesignAdjunct& x) {
              const SpiceInfo* sp = spiceForName(x.name);
              return x.name.find("zest") != std::string::npos || (sp && sp->id != "oak");
            });
            const std::string sk = sm.style->key;
            rule("Style follows the ingredients (Piatz ch.2)",
                 !hasFruit && !hasSpice ? sk == "traditional" : hasFruit && hasSpice ? sk == "fruitspice" : hasFruit ? (sk == "melomel" || sk == "cyser" || sk == "pyment" || sk == "berry" || sk == "stone") : sk == "metheglin",
                 tag + " → " + sm.style->name);
          }
          // --- Directions and timeline agree with the recipe ---
          {
            const Directions dir = directions(d, r, eng);
            std::string all;
            for (const auto& st : dir.brewDay) all += st.detail + "\n";
            rule("Brew-day steps use this recipe's target reading and volume", all.find("until it reads " + sg3(r.mixingOG) + ". That takes about " + toFixed(std::max(0.0, r.volumes.waterToAdd), 2) + " gal of water in all, and with the honey the bucket then holds about " + toFixed(r.volumes.preFruit, 2) + " gal") != std::string::npos, tag);
            const SNA n = piatzSNA(r.volumes.final, r.effectiveOG, r.fg.central);
            const auto sched = schedule(d, r, eng, &n);
            const bool later = std::any_of(d.fruits.begin(), d.fruits.end(), [](const DesignFruit& f) { return f.phase == Phase::Secondary; });
            bool hasLater = false, hasStab = false;
            for (const auto& x : sched) {
              if (x.title.rfind("Add secondary ingredients", 0) == 0) hasLater = true;
              if (x.kind == "stabilize") hasStab = true;
            }
            rule("Timeline has the 'add later fruit' step exactly when fruit goes in later (Schramm ch.8)", hasLater == later, tag);
            rule("Timeline has the stabilize step whenever the mead will be sweetened or isn't dry (Schramm ch.5; Piatz ch.8)", hasStab == (d.backsweeten.has_value() || d.sweetness != "dry"), tag);
          }
          rule("A plan that can be made never needs a must thicker than the books' heaviest (1.254, Piatz ch.10)", r.mixingOG <= MAX_MIXING_OG + 1e-9,
               tag + " mixing " + sg3(r.mixingOG));
          // --- Spices: the amount for the level you chose, at the right time, explained as a starting point ---
          std::vector<std::string> spiceSeenT;
          for (const auto& gs : g.spices) {
            const SpiceInfo* sp = spiceById(gs.spiceId);
            if (std::find(spiceSeenT.begin(), spiceSeenT.end(), sp->id) != spiceSeenT.end()) continue;   // listed twice: the first level wins
            spiceSeenT.push_back(sp->id);
            const DesignAdjunct* a = nullptr;
            int copies = 0;
            for (const auto& x : d.adjuncts) if (x.name == sp->name) { if (!a) a = &x; ++copies; }
            rule("Spices: a spice listed twice goes in once", copies <= 1, tag + " " + sp->id);
            rule("Spices: each chosen spice is in the recipe", a != nullptr, tag + " " + sp->id);
            if (!a) continue;
            rule("Spices: amount = the book/web figure for that level, scaled to the batch",
                 a->amount == spiceMeasure(*sp, spiceAmount(*sp, gs.level, d.volumeGal)) + " to start", tag + " " + sp->id + " " + gs.level + ": " + a->amount);
            rule("Spices: added at the time the books give", a->when == sp->when, tag + " " + sp->id);
            bool explained = false;
            for (const auto& f : fp.fit)
              if (f.text.rfind(sp->name + " (" + gs.level + ")", 0) == 0 && has(f.text, "starting point") && has(f.text, spiceLevel(*sp, gs.level).src)) explained = true;
            rule("Spices: the plan says where the figure comes from and that it's a starting point", explained, tag + " " + sp->id);
          }
          if (!g.spices.empty()) {
            bool realSpice = false;
            for (const auto& gs : g.spices) if (gs.spiceId != "oak") realSpice = true;
            const std::string sk = detectStyle(d, db).style->key;
            if (realSpice) rule("Spices: a spiced plan is a metheglin, or fruit and spice with fruit (Piatz ch.2)",
                                d.fruits.empty() ? (sk == "metheglin" || sk == "experimental") : (sk == "fruitspice" || sk == "experimental"), tag + " → " + sk);
          }
          // --- Lemon and lime zest/juice come with a real starting amount, never more than you'll have ---
          for (const auto& a : d.adjuncts) {
            const bool zest = a.name.find(" zest") != std::string::npos, juice = a.name.find(" juice") != std::string::npos;
            if ((!zest && !juice) || !std::regex_search(a.name, lemonLime)) continue;
            static const std::regex kitchen("^[0-9]*(¼|½|¾)? ?(tsp|tbsp|cups?) to start$|^[0-9]+(¼|½|¾)? (tsp|tbsp|cups?) to start$|^(¼|½|¾) cup to start$");
            rule("Citrus: zest and juice have a kitchen amount to start with", std::regex_search(a.amount, kitchen), tag + " " + a.name + ": " + a.amount);
            std::string an = a.name;
            for (auto& ch : an) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            for (const auto& gf : g.fruits) {
              const FruitInfo* fi = db.fruit(gf.fruitId);
              if (!fi || !gf.haveLb || *gf.haveLb <= 0 || !std::regex_search(fi->name, lemonLime)) continue;
              std::string fn = fi->name.substr(0, 4);
              for (auto& ch : fn) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
              if (an.find(fn) != 0) continue;
              const CitrusYield cy = citrusYield(fi->name, *gf.haveLb, d.volumeGal);
              rule("Citrus: the starting amount is no more than your fruit gives", zest ? cy.startZestTbsp <= cy.zestTbspLo + 1e-9 : cy.startJuiceTbsp <= cy.juiceTbspLo + 1e-9, tag + " " + a.name);
            }
          }
          // --- The words on screen agree with the plan ---
          auditPlanText(db, g, fp, r, scale, fermenter, tag, s);
          {
            Design used = mead::text::designFromPlan(fp, rnd() < 0.8 ? mead::text::isoOf(mead::text::addDays({2026, 1, 1}, static_cast<int>(rnd() * 1500))) : std::string("not a date"));
            const EngineInput eu = toEngine(used, db);
            auditDesignText(db, used, formulate(eu.recipe, s), eu, scale, fermenter, tag);
          }
        }

  // ---- Phase 2: random hand-made Designer recipes ----
  int designs = 0;
  for (int n = 0; n < (argc > 1 ? 1500 : 600); ++n) {
    Design d;
    d.volumeGal = std::round((1 + rnd() * 9) * 4) / 4;
    d.sweetness = sweets[static_cast<size_t>(rnd() * 3)];
    d.honeys = {{"Wildflower", std::round(rnd() * 25 * 4) / 4}};
    const int nf = static_cast<int>(rnd() * 3);
    for (int i = 0; i < nf; ++i) {
      const auto& set = fruitSets[1 + static_cast<size_t>(rnd() * static_cast<double>(fruitSets.size() - 1))];
      const FruitInfo* f = db.fruit(set[0]);
      if (!f) continue;
      DesignFruit df;
      df.fruitId = f->id; df.name = f->name; df.lb = std::round(rnd() * 20 * 4) / 4;
      df.phase = rnd() < 0.5 ? Phase::Primary : Phase::Secondary;
      d.fruits.push_back(df);
    }
    const auto& y = db.yeasts[static_cast<size_t>(rnd() * static_cast<double>(db.yeasts.size()))];
    d.yeastId = y.id;
    Settings s;
    const std::string scale = rnd() < 0.5 ? "piatz" : "schramm";
    const EngineInput eng = toEngine(d, db);
    const Result r = formulate(eng.recipe, s);
    ++designs;
    const std::string tag = "[design " + std::to_string(n) + ": " + toFixed(d.volumeGal, 2) + " gal, " + toFixed(d.honeys[0].lb, 2) + " lb honey, " +
                            std::to_string(d.fruits.size()) + " fruit, " + y.code + "]";
    if (r.volumes.waterToAdd < -0.02) {   // over-full recipes: the review must say so
      bool said = false;
      for (const auto& nn : review(d, r, eng, scale)) if (nn.text.find("take up more than the batch volume") != std::string::npos) said = true;
      rule("Designer: an over-full recipe is flagged", said, tag);
      continue;
    }
    // Review says "ferments dry" exactly when it will (and the target isn't dry)
    {
      bool saysDry = false;
      for (const auto& nn : review(d, r, eng, scale)) if (nn.text.find("should ferment this dry") != std::string::npos) saysDry = true;
      if (!d.wild) rule("Designer: 'should ferment this dry' note matches the prediction (Piatz ch.9)", saysDry == (r.fg.reachesDry && d.sweetness != "dry"), tag);
    }
    // Fruit bar label and fruit-character text agree on the level
    for (size_t i = 0; i < d.fruits.size(); ++i) {
      const std::string bar = fruitLevelFor(db, d.fruits[i].fruitId, d.fruits[i].lb, r.volumes.final);
      for (const auto& fc : fruitCharacter(eng, r, db, d.sweetness)) {
        if (fc.names.size() != 1 || fc.names[0] != eng.recipe.fruits[i].name || fc.src != "Schramm ch.8") continue;
        std::string lvl = fc.level.substr(0, fc.level.find(" ("));
        std::string a = bar, b = lvl;
        for (auto& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (auto& c : b) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        rule("Designer: fruit bar label and fruit-character level agree (Schramm Table 8.1)", a == b, tag + " bar '" + bar + "' vs text '" + lvl + "'");
      }
    }
    // Sweetness option A, applied, lands on the target
    if (!d.honeys.empty() && d.honeys[0].lb > 0) {
      const double t = 1.005 + rnd() * 0.04;
      for (const auto& o : sweetOptions(d, eng, r, t, s)) {
        if (o.kind != "A" || !o.applyOG) continue;
        const auto applied = withHoneyForOG(d, db, *o.applyOG, s);
        if (!applied) continue;
        const Result ra = formulate(toEngine(*applied, db).recipe, s);
        if (ra.volumes.waterToAdd < -0.02) continue;
        rule("Designer: option A applied lands on the target FG (Piatz ch.9)", near(ra.fg.central, std::max(1.0, t), 0.0012),
             tag + " target " + toFixed(t, 3) + " got " + sg3(ra.fg.central));
      }
    }
    // Fruit slider with 'keep the OG' keeps it
    if (!d.fruits.empty() && d.honeys[0].lb > 1) {
      const double newLb = std::round(rnd() * 15 * 4) / 4;
      const Design d2 = withFruitAmount(d, db, 0, newLb, true, s);
      const Result r2 = formulate(toEngine(d2, db).recipe, s);
      if (d2.honeys[0].lb > 0.01 && r2.volumes.waterToAdd >= -0.02)
        rule("Designer: fruit slider keeps the effective OG when asked", near(r2.effectiveOG, r.effectiveOG, 0.0008), tag + " " + sg3(r.effectiveOG) + " → " + sg3(r2.effectiveOG));
    }
    // The words on screen agree with this recipe
    d.brewDate = rnd() < 0.8 ? mead::text::isoOf(mead::text::addDays({2024, 2, 20}, static_cast<int>(rnd() * 1200))) : std::string();
    if (rnd() < 0.4) d.style = styles()[static_cast<size_t>(rnd() * static_cast<double>(styles().size()))].name;
    auditDesignText(db, d, r, eng, scale, rnd() < 0.5 ? 7.2 : 0, tag);
    // Solvers say what they did
    {
      const double og = 1.06 + rnd() * 0.08;
      const auto sv = mead::text::solveOG(d, db, toFixed(og, 3), s);
      if (sv.design) {
        double lb = 0;
        for (const auto& h : sv.design->honeys) lb += h.lb;
        const Result rs = formulate(toEngine(*sv.design, db).recipe, s);
        rule("Screen: 'Honey set to …' message = the honey the solver chose", sv.message == "Honey set to " + mead::text::wt(lb) + " for OG " + toFixed(og, 3), tag + " " + sv.message);
        rule("Screen: solved honey reaches the OG it announces", near(rs.effectiveOG, og, 0.0012), tag + " " + sg3(rs.effectiveOG) + " vs " + toFixed(og, 3));
      }
      const auto bad = mead::text::solveOG(d, db, "abc", s);
      rule("Screen: a non-number OG is refused with a hint", !bad.design && bad.message == "Enter a target OG like 1.115", tag);
    }
    // Save → load keeps everything
    rule("Designer: save and load give the same recipe", json::stringify(designToJson(designFromJson(json::parse(json::stringify(designToJson(d)))))) ==
                                                            json::stringify(designToJson(d)), tag);
  }

  // ---- Phase 3: common-sense trends in the planner ----
  for (size_t fi = 0; fi < fruitSets.size(); ++fi) {
    for (const auto& sweet : {std::string("dry"), std::string("sweet")}) {
      double lastHoney = -1, lastOG = 0;
      for (double abvT : {8.0, 10.0, 12.0, 14.0, 16.0, 18.0}) {
        Goal g;
        g.abv = abvT; g.sweetness = sweet; g.volumeGal = 5; g.yeastId = "lallemand-ec-1118"; g.strategy = "dry";
        for (const auto& id : fruitSets[fi]) if (db.fruit(id)) g.fruits.push_back({id, 10.0, "all", "auto"});
        Settings s;
        const FittedPlan fp = planForEquipment(g, db, s, "piatz", 0);
        if (!fp.plan.reachable || !fp.fits) continue;
        double honey = 0;
        for (const auto& h : fp.plan.design.honeys) honey += h.lb;
        const Result r = formulate(toEngine(fp.plan.design, db).recipe, s);
        const std::string tag = "[" + (fruitSets[fi].empty() ? std::string("traditional") : fruitSets[fi][0]) + " " + jsNum(abvT) + "% " + sweet + "]";
        if (lastHoney >= 0) {
          rule("Trend: a higher ABV target never needs less honey", honey >= lastHoney - 1e-9, tag + " " + jsNum(honey) + " < " + jsNum(lastHoney));
          rule("Trend: a higher ABV target gives a higher starting gravity", r.effectiveOG > lastOG, tag);
        }
        lastHoney = honey;
        lastOG = r.effectiveOG;
      }
    }
    // Twice the batch → about twice the honey (traditional and fruit scaled together)
    {
      Goal a, b;
      a.abv = b.abv = 12; a.sweetness = b.sweetness = "dry"; a.yeastId = b.yeastId = "lallemand-ec-1118";
      a.volumeGal = 5; b.volumeGal = 10;
      for (const auto& id : fruitSets[fi]) if (db.fruit(id)) { a.fruits.push_back({id, 6.0, "all", "auto"}); b.fruits.push_back({id, 12.0, "all", "auto"}); }
      Settings s;
      const Plan pa = planForEquipment(a, db, s, "piatz", 0).plan, pb = planForEquipment(b, db, s, "piatz", 0).plan;
      if (pa.reachable && pb.reachable && pa.design.honeys[0].lb > 1)
        rule("Trend: doubling the batch (and fruit) doubles the honey", near(pb.design.honeys[0].lb, 2 * pa.design.honeys[0].lb, 0.3),
             std::string("[") + (fruitSets[fi].empty() ? "traditional" : fruitSets[fi][0]) + "] " + jsNum(pa.design.honeys[0].lb) + " → " + jsNum(pb.design.honeys[0].lb));
    }
  }
  // Batch shrinking finds the biggest batch that fits (0.1 gal steps)
  for (size_t fi = 1; fi < fruitSets.size(); ++fi) {
    Goal g;
    g.abv = 13; g.sweetness = "medium"; g.volumeGal = 6; g.fruitTiming = "piatz";
    for (const auto& id : fruitSets[fi]) if (db.fruit(id)) g.fruits.push_back({id, 15.0, "all", "auto"});
    Settings s;
    const FittedPlan fp = planForEquipment(g, db, s, "piatz", 7.2);
    if (!fp.fits || fp.plan.design.volumeGal >= 6 - 1e-9) continue;
    Goal bigger = g;
    bigger.yeastId = fp.plan.yeast ? fp.plan.yeast->id : "";
    bigger.strategy = fp.plan.strategy;
    bigger.volumeGal = fp.plan.design.volumeGal + 0.1;
    const FittedPlan fb = planForEquipment(bigger, db, s, "piatz", 0);
    rule("Bucket fit: 0.1 gal more would overfill (the plan uses the biggest batch that fits)", fb.primaryGal > 7.2 * 2 / 3 - 1e-6,
         "[" + fruitSets[fi][0] + "] " + jsNum(fp.plan.design.volumeGal) + " gal plan; +0.1 gal gives " + toFixed(fb.primaryGal, 2));
  }

  // ---- Book recipes: engine vs printed gravities (Piatz's and Schramm's own numbers) ----
  const json::Value recipes = json::parse(slurp("data/recipes.json"));
  int compared = 0, within = 0;
  double sumAbs40 = 0, sumAbs42 = 0;
  std::vector<std::string> far;
  for (const auto& rec : recipes.items()) {
    if (!rec["og"].isNumber() || rec["noGravityModel"].flag() || rec["wild"].flag()) continue;
    const Design d = designFromPublished(rec, db);
    Settings s40, s42;
    s42.honeyPPG = 42;
    const Result a = formulate(toEngine(d, db).recipe, s40), b = formulate(toEngine(d, db).recipe, s42);
    const bool pre = rec["ogBasis"].str() == "pre-fruit";
    const double printed = rec["og"].num();
    const double e40 = pre ? a.mixingOG : a.effectiveOG, e42 = pre ? b.mixingOG : b.effectiveOG;
    ++compared;
    sumAbs40 += std::fabs(e40 - printed);
    sumAbs42 += std::fabs(e42 - printed);
    if (std::fabs(e42 - printed) <= 0.006 || std::fabs(e40 - printed) <= 0.006) ++within;
    else if (far.size() < 8) far.push_back(rec["name"].str() + ": printed " + toFixed(printed, 3) + ", app " + toFixed(e40, 3) + " (40) / " + toFixed(e42, 3) + " (42)");
  }

  // ---- Report ----
  int totalChecks = 0, totalFailed = 0;
  std::printf("Consistency audit (seed %u): %d plans (%d skipped as not reachable in the volume or bucket), %d hand-made designs\n\n", seedArg, plans, unreachable, designs);
  for (const auto& [name, r] : rules) {
    totalChecks += r.checked;
    totalFailed += r.failed;
    std::printf("%s %-100s %6d checked, %d failed\n", r.failed ? "FAIL" : " ok ", name.c_str(), r.checked, r.failed);
    for (const auto& e : r.examples) std::printf("        e.g. %s\n", e.c_str());
  }
  if (compared > 0)
    std::printf("\nBook recipes: %d compared; %d within 0.006 of the printed OG; average difference %.4f at 40 points/lb/gal, %.4f at 42.\n",
                compared, within, sumAbs40 / compared, sumAbs42 / compared);
  else
    std::printf("\nBook recipes: not compared (the built-in list has them by name only).\n");
  for (const auto& f : far) std::printf("   further off: %s\n", f.c_str());
  std::printf("\n%d checks, %d failed\n", totalChecks, totalFailed);
  return totalFailed ? 1 : 0;
}
