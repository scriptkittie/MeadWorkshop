// Advisor tests: the C++ port must say exactly what the browser version says.
//   • 346 designs (all 43 published recipes on both sweetness scales + 260 random ones):
//     every Review note, every fruit-character row and the headline numbers must match.
//   • Profile bars, sweetness options, edits and saving checked against the books' rules.
#include <algorithm>
#include <cmath>
#include <regex>
#include <map>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "mead_advisor.hpp"
#include "mead_text.hpp"
#include "mead_words.hpp"
#include "mead_spices.hpp"

#ifndef MEAD_SOURCE_DIR
#define MEAD_SOURCE_DIR "."
#endif

static int passed = 0, failed = 0;
static void check(bool ok, const std::string& what) {
  if (ok) ++passed;
  else { ++failed; std::printf("FAIL: %s\n", what.c_str()); }
}
static bool near(double a, double b, double tol = 1e-9) { return std::fabs(a - b) <= tol; }
static std::string slurp(const std::string& rel) {
  std::ifstream f(std::string(MEAD_SOURCE_DIR) + "/" + rel, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
static std::string join(const std::vector<std::string>& v, const char* sep) {
  std::string s;
  for (size_t i = 0; i < v.size(); ++i) s += (i ? sep : "") + v[i];
  return s;
}

int main() {
  using namespace mead;
  const Database db = loadDatabase(slurp("data/fruits.json"), slurp("data/yeasts.json"), slurp("data/schramm.json"),
                                   slurp("data/reference.json"));
  check(db.fruits.size() == 48, "48 fruits loaded");
  check(db.yeasts.size() == 45, "45 yeasts loaded");
  check(db.juices.size() == 16, "16 juices loaded");
  check(db.schramm.count("blueberries") == 1, "Schramm table loaded");

  // The browser version's answers (word-for-word parity), checked only when tests/advisor_cases.json is present.
  const std::string casesText = slurp("tests/advisor_cases.json");
  const bool haveCases = !casesText.empty();
  const json::Value data = haveCases ? json::parse(casesText) : json::Value();
  if (!haveCases) std::printf("Browser-version comparison skipped: tests/advisor_cases.json isn't present.\n");

  // ---------- JavaScript-compatible number formatting ----------
  for (const auto& c : data["toFixedCases"].items()) {
    const double x = c.items()[0].num();
    const int d = static_cast<int>(c.items()[1].num());
    const std::string want = c.items()[2].str();
    check(toFixed(x, d) == want, "toFixed(" + std::to_string(x) + ", " + std::to_string(d) + ") = " + toFixed(x, d) + ", browser " + want);
  }
  for (const auto& c : data["juiceSG"].items())
    check(near(juiceSGFromSugar(c.items()[0].num()), c.items()[1].num()), "juice SG from sugar " + std::to_string(c.items()[0].num()));

  // ---------- Parity with the browser advisor ----------
  int caseNo = 0, notesCompared = 0, fruitCompared = 0, scheduleCompared = 0, caseFailures = 0;
  for (const auto& c : data["cases"].items()) {
    ++caseNo;
    const Design d = designFromJson(c["design"]);
    const std::string scale = c["scale"].str();
    Settings s;
    s.honeyPPG = c["settings"]["honeyPPG"].num();
    s.honeyLbPerGal = c["settings"]["honeyLbPerGal"].num();
    const EngineInput eng = toEngine(d, db);
    const Result res = formulate(eng.recipe, s);
    const auto& ex = c["expect"];
    const std::string tag = "case " + std::to_string(caseNo) + " (" + c["design"]["name"].str() + ", " + scale + ")";
    const int before = failed;

    check(near(res.effectiveOG, ex["effectiveOG"].num()), tag + ": effective OG");
    check(near(res.mixingOG, ex["mixingOG"].num()), tag + ": mixing OG");
    check(near(res.fg.central, ex["fg"].num()), tag + ": FG");
    check(near(res.fg.abv, ex["abv"].num()), tag + ": ABV");
    check(near(eng.recipe.yeast.tolerance, ex["tolerance"].num()), tag + ": yeast tolerance");
    check(eng.yeastName == ex["yeastName"].str(), tag + ": yeast name");
    check(eng.yeastAssumed == ex["assumed"].flag(), tag + ": yeast assumed");

    const auto notes = review(d, res, eng, scale);
    const auto& want = ex["notes"].items();
    check(notes.size() == want.size(), tag + ": " + std::to_string(notes.size()) + " notes, browser " + std::to_string(want.size()));
    for (size_t i = 0; i < std::min(notes.size(), want.size()); ++i) {
      ++notesCompared;
      const auto& w = want[i].items();
      const bool same = notes[i].level == w[0].str() && notes[i].text == w[1].str() && notes[i].src == w[2].str();
      check(same, tag + ": note " + std::to_string(i + 1) + "\n   C++:     [" + notes[i].level + "] " + notes[i].text + " {" + notes[i].src +
                      "}\n   browser: [" + w[0].str() + "] " + w[1].str() + " {" + w[2].str() + "}");
    }

    const auto fc = fruitCharacter(eng, res, db, d.sweetness);
    const auto& wantF = ex["fruitChar"].items();
    check(fc.size() == wantF.size(), tag + ": fruit character rows");
    for (size_t i = 0; i < std::min(fc.size(), wantF.size()); ++i) {
      ++fruitCompared;
      const auto& w = wantF[i].items();
      const bool same = join(fc[i].names, "|") == w[0].str() && fc[i].level == w[1].str() && fc[i].basis == w[2].str() &&
                        fc[i].note == w[3].str() && fc[i].src == w[4].str();
      check(same, tag + ": fruit row " + std::to_string(i + 1) + "\n   C++:     " + fc[i].level + " / " + fc[i].basis +
                      "\n   browser: " + w[1].str() + " / " + w[2].str());
    }
    // Schedule (timeline) and brew-day directions
    {
      const SNA sna = piatzSNA(res.volumes.final, res.effectiveOG, res.fg.central);
      const auto sched = schedule(d, res, eng, d.wild ? nullptr : &sna);
      const auto& ws = ex["schedule"].items();
      check(sched.size() == ws.size(), tag + ": " + std::to_string(sched.size()) + " timeline items, browser " + std::to_string(ws.size()));
      for (size_t i = 0; i < std::min(sched.size(), ws.size()); ++i) {
        ++scheduleCompared;
        const auto& w = ws[i].items();
        const bool dayOk = w[0].isNull() ? !sched[i].day : (sched[i].day && near(*sched[i].day, w[0].num()));
        const bool sgOk = w[5].isNull() ? !sched[i].sg : (sched[i].sg && near(*sched[i].sg, w[5].num()));
        check(dayOk && sgOk && sched[i].title == w[1].str() && sched[i].detail == w[2].str() && sched[i].src == w[3].str() && sched[i].kind == w[4].str(),
              tag + ": timeline " + std::to_string(i + 1) + "\n   C++:     " + sched[i].title + " / " + sched[i].detail + "\n   browser: " + w[1].str() + " / " + w[2].str());
      }
      const Directions dir = directions(d, res, eng);
      check(d.wild ? dir.brewDay.empty() : (dir.prep.size() == 4 && dir.brewDay.size() >= 7), tag + ": brew-day directions present");
    }
    if (failed > before) ++caseFailures;
  }
  std::printf("Parity: %d designs, %d review notes, %d fruit rows and %d timeline steps compared with the browser version (%d designs differ).\n",
              caseNo, notesCompared, fruitCompared, scheduleCompared, caseFailures);

  // ---------- Published recipes and adapted copies ----------
  {
    int n = 0;
    for (const auto& p : data["published"].items()) {
      ++n;
      const std::string tag = "published " + p["recipe"]["name"].str();
      const Design mine = designFromPublished(p["recipe"], db);
      const Design want = designFromJson(p["design"]);
      Design cmp = mine;   // the native app also marks juice SGs that were estimated; the browser didn't store that
      for (auto& j : cmp.juices) j.sgEstimated = false;
      check(json::stringify(designToJson(cmp)) == json::stringify(designToJson(want)), tag + ": loads exactly as the browser version\n   C++:     " +
            json::stringify(designToJson(cmp)) + "\n   browser: " + json::stringify(designToJson(want)));
      const Faithfulness f = faithfulness(designFromJson(p["adapted"]), mine);
      const auto& wf = p["faith"];
      std::vector<std::string> wantChanges;
      for (const auto& c : wf["changes"].items()) wantChanges.push_back(c.str());
      check(near(f.scale, wf["scale"].num()) && f.level == wf["level"].str() && f.changes == wantChanges,
            tag + ": faithfulness\n   C++:     " + f.level + " / " + join(f.changes, "; ") + "\n   browser: " + wf["level"].str() + " / " +
                join(wantChanges, "; "));
    }
    if (haveCases) check(n == 43, "all 43 published recipes checked");
  }

  // ---------- Goal planner ----------
  {
    int n = 0, differ = 0;
    for (const auto& c : data["plans"].items()) {
      ++n;
      const auto& g = c["goal"];
      Goal goal;
      goal.name = g["name"].str(); goal.honeyName = g["honeyName"].str(); goal.sweetness = g["sweetness"].str();
      goal.volumeGal = g["volumeGal"].num(5); goal.abv = g["abv"].num(12);
      goal.yeastId = g["yeastId"].str(); goal.strategy = g["strategy"].str();
      for (const auto& f : g["fruits"].items()) {
        GoalFruit gf;
        gf.fruitId = f["fruitId"].str();
        if (f["haveLb"].isNumber()) gf.haveLb = f["haveLb"].num();
        gf.intensity = f["intensity"].str(); gf.phase = f["phase"].str();
        goal.fruits.push_back(gf);
      }
      Settings s;
      s.honeyPPG = c["settings"]["honeyPPG"].num(); s.honeyLbPerGal = c["settings"]["honeyLbPerGal"].num();
      const std::string scale = c["settings"]["sweetnessScale"].str();
      const Plan p = plan(goal, db, s, scale);
      const std::string tag = "plan " + std::to_string(n) + " (" + goal.name + ")";
      const int before = failed;
      check(p.strategy == c["strategy"].str(), tag + ": strategy " + p.strategy + ", browser " + c["strategy"].str());
      check(near(p.targetOG, c["targetOG"].num()), tag + ": target OG");
      check((p.yeast ? p.yeast->id : "") == c["yeastId"].str(), tag + ": yeast");
      const std::string mine = json::stringify(designToJson(p.design)), want = json::stringify(designToJson(designFromJson(c["design"])));
      check(mine == want, tag + ": recipe\n   C++:     " + mine + "\n   browser: " + want);
      const auto& w = c["why"].items();
      check(p.why.size() == w.size(), tag + ": " + std::to_string(p.why.size()) + " reasons, browser " + std::to_string(w.size()));
      for (size_t i = 0; i < std::min(p.why.size(), w.size()); ++i)
        check(p.why[i].text == w[i].items()[0].str() && p.why[i].src == w[i].items()[1].str(),
              tag + ": reason " + std::to_string(i + 1) + "\n   C++:     " + p.why[i].text + " {" + p.why[i].src + "}\n   browser: " + w[i].items()[0].str() + " {" + w[i].items()[1].str() + "}");
      if (failed > before) ++differ;
    }
    std::printf("Planner: %d goals compared with the browser version (%d differ).\n", n, differ);

    // "Use all I have": 20 lb pears in 6 gal stays 20 lb, and the honey is solved around it.
    Goal g;
    g.volumeGal = 6; g.abv = 13; g.yeastId = "lallemand-71b-1122";
    g.fruits = {{"pears", 20.0, "all", "auto"}};
    Settings s;
    const Plan p = plan(g, db, s, "piatz");
    check(p.design.fruits.size() == 1 && near(p.design.fruits[0].lb, 20), "use all I have keeps 20 lb of pears");
    check(p.reachable && near(formulate(toEngine(p.design, db).recipe, s).effectiveOG, p.targetOG, 0.0012), "honey solved around the pears for the planned OG");
    check(!p.why.empty() && p.why[0].text.find("using all 20 lb") != std::string::npos, "the reason says it uses all 20 lb");
  }

  // ---------- Directions with real numbers (Piatz's Orange Blossom, 14 lb in 5 gal) ----------
  {
    Design d;
    d.volumeGal = 5;
    d.honeys = {{"Orange blossom", 14}};
    d.yeastId = "lallemand-71b-1122";
    Settings s;
    const EngineInput eng = toEngine(d, db);
    const Result r = formulate(eng.recipe, s);
    const Directions dir = directions(d, r, eng);
    std::string all;
    for (const auto& st : dir.prep) all += st.title + ": " + st.detail + "\n";
    for (const auto& st : dir.brewDay) all += st.title + ": " + st.detail + "\n";
    check(all.find("14.00 lb orange blossom honey") != std::string::npos, "directions: weigh in 14 lb of honey");
    check(all.find("until it reads 1.112.") != std::string::npos && all.find("the bucket then holds about 5.00 gal (the honey takes up the difference: it is not more water)") != std::string::npos, "directions: water up to 1.112; 5 gal is the level, not the water");
    check(all.find("4.0 g Fermaid K and 8.0 g DAP") != std::string::npos, "directions: Piatz's nutrient amounts for 5 gal");
    check(all.find("6.3 g Go-Ferm") != std::string::npos && all.find("125 mL") != std::string::npos, "directions: Go-Ferm for one 5 g packet");
    check(all.find("Add the fruit") == std::string::npos, "directions: no fruit step without fruit");
  }

  // ---------- Fitting a plan to the equipment: 20 lb pears + 3 lb limes, sweet, 13%, a 7.2-gal bucket ----------
  {
    Goal g;
    g.name = "Pear mead"; g.volumeGal = 6; g.abv = 13; g.sweetness = "sweet";
    g.fruits = {{"pears", 20.0, "all", "auto"}, {"limes-raw", 3.0, "medium", "auto"}};
    if (!db.fruit("limes-raw")) g.fruits[1].fruitId = "limes-whole";
    Settings s;
    const FittedPlan fp = planForEquipment(g, db, s, "piatz", 7.2);
    const Design& d = fp.plan.design;
    const Result r = formulate(toEngine(d, db).recipe, s);
    std::printf("Pear & lime in a 7.2-gal bucket: %s gal, honey %s lb, start %s (honey must %s), FG %s, ABV %s, primary %s gal, later %s gal\n",
                toFixed(d.volumeGal, 2).c_str(), toFixed(d.honeys[0].lb, 2).c_str(), toFixed(r.effectiveOG, 3).c_str(), toFixed(r.mixingOG, 3).c_str(),
                toFixed(r.fg.central, 3).c_str(), toFixed(r.fg.abv, 1).c_str(), toFixed(fp.primaryGal, 2).c_str(), toFixed(fp.laterGal, 2).c_str());
    for (const auto& f : fp.fit) std::printf("   - %s\n", f.text.c_str());
    bool pearsPrimary = false, limesLater = false;
    for (const auto& f : d.fruits) {
      if (f.fruitId == "pears") pearsPrimary = f.phase == Phase::Primary;
      else limesLater = f.phase == Phase::Secondary;
    }
    check(pearsPrimary, "pears go in on brew day (Piatz's way, the default)");
    check(!limesLater && d.fruits.size() == 1, "limes are not fermented as whole fruit");
    int zest = 0, juice = 0;
    for (const auto& a : d.adjuncts) {
      if (a.name.find("Lime zest") == 0 && a.when == "secondary") ++zest;
      if (a.name.find("Lime juice") == 0 && a.when == "aging") ++juice;
    }
    check(zest == 1 && juice == 1, "limes become zest (at racking) and juice (after fermentation)");
    check(fp.primaryGal <= 7.2 * 2 / 3 + 1e-9, "primary at most two-thirds of the 7.2-gal bucket");
    check(r.primaryOG < 1.14, "fermentation starts below 1.140 (dry, then sweeten)");
    check(fp.plan.strategy == "dry" && d.backsweeten.has_value(), "switches to ferment dry + backsweeten");
    check(near(d.fruits[0].lb, 20), "still uses all 20 lb of pears");

    Goal gs = g;
    gs.fruitTiming = "schramm";
    const FittedPlan fs = planForEquipment(gs, db, s, "piatz", 7.2);
    std::printf("   Schramm timing: %s gal, honey %s lb, brew-day %s gal, later %s gal\n", toFixed(fs.plan.design.volumeGal, 2).c_str(),
                toFixed(fs.plan.design.honeys[0].lb, 2).c_str(), toFixed(fs.primaryGal, 2).c_str(), toFixed(fs.laterGal, 2).c_str());
    for (const auto& f : fs.fit) std::printf("   - %s\n", f.text.c_str());
    check(fs.plan.design.fruits[0].phase == Phase::Secondary, "Schramm timing adds pears later");
  }

  // ---------- Yeast ratings: strength and flavour judged separately ----------
  {
    Goal g;
    g.abv = 16; g.sweetness = "sweet";
    g.fruits = {{"pears", 20.0, "all", "auto"}};
    const auto rated = rateYeasts(g, db);
    auto find = [&](const std::vector<YeastFit>& rs, const char* code) -> const YeastFit* { for (const auto& r : rs) if (r.yeast->code == code) return &r; return nullptr; };
    check(rated.front().mark == "★" && rated.front().yeast->code == "ICV-K1 (V 1116)", "16% pear: K1 suggested (★): strong enough and fruit-forward");
    const YeastFit* y71 = find(rated, "71B-1122");
    check(y71 && y71->mark == "*" && y71->strengthMark == "*" && y71->flavourMark == "✓" && y71->bestFlavour,
          "16% pear: 71B is the best flavour match but stops ~14% (*)");
    check(y71 && y71->label.find("best flavour match") != std::string::npos, "…and its label says so");
    check(find(rated, "RC 212")->mark == "*" && find(rated, "RC 212")->flavour.find("red-wine") != std::string::npos, "16% pear: RC 212 flagged as a red-wine strain");
    check(find(rated, "WLP770")->mark == "*", "16% pear: Burgundy (red) strain no longer a plain good fit");
    check(find(rated, "CL23")->mark == "*" && find(rated, "CL23")->flavour.find("removes fruity") != std::string::npos, "CL23 flagged: removes fruity character");
    check(find(rated, "4021")->mark == "✓" && find(rated, "4021")->flavour.find("Neutral") == 0, "Pasteur Champagne (crisp, dry) rated neutral");
    std::printf("   16%% pear, top five:\n");
    for (size_t i = 0; i < 5 && i < rated.size(); ++i)
      std::printf("     %s %s · %s | %s %s | %s %s\n", rated[i].mark.c_str(), rated[i].yeast->name.c_str(), rated[i].label.c_str(), rated[i].strengthMark.c_str(),
                  rated[i].strength.c_str(), rated[i].flavourMark.c_str(), rated[i].flavour.substr(0, 60).c_str());

    g.sweetness = "dry";
    check(find(rateYeasts(g, db), "71B-1122")->mark == "✗", "dry 16%: 71B not suitable");
    g.abv = 13; g.sweetness = "sweet";
    check(rateYeasts(g, db).front().yeast->code == "71B-1122", "13% pear: 71B suggested");
    {
      Goal pg; pg.abv = 16; pg.sweetness = "sweet"; pg.volumeGal = 5;
      pg.fruits = {{"pears", 20.0, "all", "auto"}, {"limes-whole", 0.0, "medium", "auto"}};
      Settings st;
      const FittedPlan fp = planForEquipment(pg, db, st, "piatz", 7.2);
      std::string all;
      for (const auto& f : fp.fit) all += f.text + "\n";
      for (const auto& w : fp.plan.why) all += w.text + "\n";
      check(fp.plan.yeast && fp.plan.yeast->code == "ICV-K1 (V 1116)", "plan uses the ★ yeast (K1)");
      check(all.find("RC 212") == std::string::npos, "plan never mentions a yeast it didn't suggest (RC 212)");
      check(all.find("Yeast: Lallemand ICV-K1 (V 1116) (★)") != std::string::npos && all.find("71B") != std::string::npos,
            "plan explains its ★ yeast and offers 71B");
      pg.yeastId = "lallemand-71b-1122";
      const FittedPlan mine = planForEquipment(pg, db, st, "piatz", 7.2);
      bool explained = false;
      for (const auto& f : mine.fit) if (f.text.find("Yeast: Lallemand 71B-1122 (Narbonne) (your choice") == 0) explained = true;
      check(mine.plan.yeast && mine.plan.yeast->code == "71B-1122" && explained, "your own yeast choice is kept and explained");
      for (const auto& f : mine.fit) if (f.text.rfind("Yeast:", 0) == 0) std::printf("   %s\n", f.text.c_str());
    }
    Goal cherry;
    cherry.abv = 16; cherry.fruits = {{"cherries-sweet", 15.0, "all", "auto"}};
    const YeastInfo* cy = suggestYeast(cherry, db);
    check(cy && cy->code == "RC 212", std::string("16% cherry: RC 212 suggested (dark fruit), got ") + (cy ? cy->code : "none"));
  }

  // ---------- Styles from ingredients (Piatz ch.2) ----------
  {
    auto styleOf = [&](Design d) { return detectStyle(d, db).style->key; };
    Design t; t.honeys = {{"Clover", 12}};
    check(styleOf(t) == "traditional", "honey only: traditional");
    Design pear = t; pear.fruits = {{"pears", "Pears", "", 20, std::nullopt, std::nullopt, Phase::Primary}};
    check(styleOf(pear) == "melomel", "pears: melomel");
    Design pz = pear; pz.adjuncts = {{"Lime zest from 3 lb limes (no white pith)", "half", "secondary"}};
    check(styleOf(pz) == "fruitspice", "pears + lime zest: fruit and spice");
    Design cy = t; cy.juices = {{"apple-juice", "Apple juice / cider", 3, 1.05, 1, Phase::Primary, false}};
    check(styleOf(cy) == "cyser", "apple juice: cyser");
    Design bb = t; bb.fruits = {{"blueberries", "Blueberries", "", 8, std::nullopt, std::nullopt, Phase::Secondary}};
    check(styleOf(bb) == "berry", "blueberries: berry mead");
    Design ch = t; ch.fruits = {{"cherries-sweet", "Cherries, sweet", "", 8, std::nullopt, std::nullopt, Phase::Secondary}};
    check(styleOf(ch) == "stone", "cherries: stone fruit mead");
    Design mix = bb; mix.fruits.push_back(ch.fruits[0]);
    check(styleOf(mix) == "melomel", "berries + cherries: melomel");
    Design sp = t; sp.adjuncts = {{"Cinnamon stick", "1", "secondary"}, {"Pectinase", "1 tsp", "primary"}};
    check(styleOf(sp) == "metheglin", "cinnamon: metheglin (pectinase ignored)");
    Design br = t; br.sugars = {{"Malt extract", 6, 40, Phase::Primary}};
    check(styleOf(br) == "braggot", "malt: braggot");
    Design ex = t; ex.sugars = {{"Agave nectar", 2, 40, Phase::Primary}};
    check(styleOf(ex) == "experimental", "agave: experimental");
    check(styleByName("Fruit & spice") && styleByName("Fruit & spice")->key == "fruitspice", "old style names still recognised");
    check(!styleByName("Sack"), "sack is a strength, not a style");
    int matched = 0;
    for (const auto& p : data["published"].items()) {
      const Design d = designFromPublished(p["recipe"], db);
      const StyleInfo* named = styleByName(d.style);
      if (named && named->key == detectStyle(d, db).style->key) ++matched;
    }
    if (haveCases) std::printf("   Published recipes whose printed style matches the detected one: %d of 43\n", matched);
  }

  // ---------- Profile bars ----------
  {
    const auto z = sweetnessZones("piatz");
    check(z.size() == 3 && near(z[0].from, 0.990) && near(z[0].to, 1.010) && z[1].label == "Semisweet" && near(z[2].to, 1.050),
          "Piatz sweetness zones 0.990 / 1.010 / 1.025 / 1.050");
    const auto zs = sweetnessZones("schramm");
    check(zs.size() == 4 && near(zs[1].from, 1.006) && near(zs[2].from, 1.012) && near(zs[3].to, 1.050), "Schramm sweetness zones");
    check(strengthZones()[2].label == "Standard" && near(strengthZones()[3].from, 14), "strength zones (Piatz ch.2)");
    check(sweetKeyFor(1.000, "piatz") == "dry" && sweetKeyFor(1.030, "piatz") == "sweet" && sweetKeyFor(1.08, "piatz") == "sweet" &&
              sweetKeyFor(0.98, "piatz") == "dry",
          "sweetness key from FG");
    check(sweetKeyFor(1.013, "schramm") == "medium", "Schramm overlap 1.012–1.015 picks the first level");

    // Blueberries, Schramm Table 8.1: mild 5–7, medium 7–10, strong 11+ lb per 5 gal.
    const FruitScale bb = fruitScale(db, "blueberries", 5);
    check(bb.zones.size() == 6 && near(bb.zones[1].from, 5) && near(bb.zones[3].to, 10) && near(bb.zones[5].from, 11) && near(bb.max, 16.5),
          "blueberry bar follows Schramm Table 8.1");
    check(fruitLevelFor(db, "blueberries", 3, 5) == "Below mild", "3 lb blueberries / 5 gal = below mild");
    check(fruitLevelFor(db, "blueberries", 6, 5) == "Mild", "6 lb = mild");
    check(fruitLevelFor(db, "blueberries", 10.5, 5) == "Medium–strong", "10.5 lb = between medium and strong");
    check(fruitLevelFor(db, "blueberries", 20, 5) == "Strong", "20 lb = strong");
    check(fruitLevelFor(db, "blueberries", 12, 10) == "Mild", "12 lb in 10 gal = 6 per 5 gal = mild");
    // Bananas: not in Schramm's table → Piatz 1–2 lb/gal light, 4–5+ strong.
    const FruitScale ban = fruitScale(db, "bananas", 5);
    check(ban.zones.size() == 4 && near(ban.zones[1].from, 5) && near(ban.zones[3].from, 20) && ban.src.find("Piatz") == 0,
          "fruit without a Schramm row uses Piatz's per-gallon guide");
    check(fruitLevelFor(db, "bananas", 15, 5) == "Between light and strong", "3 lb/gal bananas = between light and strong");
    check(fruitScale(db, "limes-whole", 5).lemonLime, "limes flagged (Schramm: use less)");
  }

  // ---------- Sweetness options ----------
  {
    Design d;
    d.volumeGal = 5;
    d.honeys = {{"Orange blossom", 12}};
    d.yeastId = "lallemand-71b-1122";
    Settings s;
    const EngineInput eng = toEngine(d, db);
    const Result r = formulate(eng.recipe, s);
    check(r.fg.reachesDry, "12 lb in 5 gal (OG 1.096) with 71B (14%) ferments dry");

    auto opts = sweetOptions(d, eng, r, 1.015, s);
    check(opts.size() == 2 && opts[0].kind == "A" && opts[1].kind == "B", "sweet target gives options A and B");
    const double ogA = ogForToleranceFinish(1.015, eng.recipe.yeast.tolerance);
    check(opts[0].applyOG && near(*opts[0].applyOG, std::stod(toFixed(ogA, 4))), "A applies Piatz's OG = 1 + (FG−1) + (YT−1)");
    const auto applied = withHoneyForOG(d, db, *opts[0].applyOG, s);
    check(applied && near(formulate(toEngine(*applied, db).recipe, s).effectiveOG, ogA, 0.0005), "applying A lands on that OG");
    check(applied && near(formulate(toEngine(*applied, db).recipe, s).fg.central, 1.015, 0.0006), "…and the yeast stops at 1.015");
    const Backsweeten bs = backsweeten(r.volumes.final, 1.000, 1.015, s.honeyPPG, s.honeyLbPerGal);
    check(opts[1].backsweetenLb && near(*opts[1].backsweetenLb, std::stod(toFixed(bs.lb, 3))), "B uses the backsweetening formula");

    const Design withB = withBacksweetening(d, 1.015, *opts[1].backsweetenLb, "piatz");
    check(withB.backsweeten && withB.adjuncts.size() == 1 && withB.adjuncts[0].when == "bottling" && withB.sweetness == "medium",
          "B adds a backsweetening line to the recipe");
    const Design twice = withBacksweetening(withB, 1.030, 3, "piatz");
    check(twice.adjuncts.size() == 1 && twice.sweetness == "sweet", "re-planning B replaces the earlier line");

    opts = sweetOptions(d, eng, r, 1.000, s);
    check(opts.size() == 1 && opts[0].kind == "ok", "dry target with a dry-fermenting recipe: already fine");

    Design big = d;
    big.honeys[0].lb = 22;
    const EngineInput e2 = toEngine(big, db);
    const Result r2 = formulate(e2.recipe, s);
    opts = sweetOptions(big, e2, r2, 1.000, s);
    check(opts.size() == 2 && opts[0].kind == "lessHoney" && opts[1].kind == "strongerYeast", "dry target with too much honey");
    const auto less = withHoneyForOG(big, db, *opts[0].applyOG, s);
    check(less && formulate(toEngine(*less, db).recipe, s).fg.reachesDry, "less honey makes it ferment dry");
    opts = sweetOptions(big, e2, r2, 1.005, s);
    check(!opts.empty() && opts.back().kind == "balance", "aiming drier than predicted mentions acidity and tannin");
  }

  // ---------- Fruit slider keeps the OG ----------
  {
    Design d;
    d.volumeGal = 5;
    d.honeys = {{"Wildflower", 12}, {"Clover", 3}};
    d.fruits = {{"blueberries", "Blueberries", "", 8, std::nullopt, std::nullopt, Phase::Secondary}};
    d.yeastId = "lallemand-71b-1122";
    Settings s;
    const double og0 = formulate(toEngine(d, db).recipe, s).effectiveOG;
    const Design d2 = withFruitAmount(d, db, 0, 14, true, s);
    check(near(d2.fruits[0].lb, 14), "fruit amount changed");
    check(near(formulate(toEngine(d2, db).recipe, s).effectiveOG, og0, 0.0003), "honey re-solved: effective OG held");
    check(near(d2.honeys[0].lb / d2.honeys[1].lb, 4.0, 0.01), "honey proportions kept (12:3)");
    check(d2.honeys[0].lb < 12, "more fruit sugar → less honey");
    const Design d3 = withFruitAmount(d, db, 0, 14, false, s);
    check(near(d3.honeys[0].lb, 12) && formulate(toEngine(d3, db).recipe, s).effectiveOG > og0, "without hold, OG rises");
  }

  // ---------- Saving and loading a design ----------
  {
    Design d;
    d.name = "Blåbär \"test\" — mead";
    d.volumeGal = 3.5;
    d.honeys = {{"Tupelo", 9.25}};
    d.fruits = {{"blueberries", "Blueberries", "frozen", 6.5, 84.0, std::nullopt, Phase::Secondary}};
    d.juices = {{"pomegranate-juice", "Pomegranate juice", 0.5, 1.072, 1, Phase::Primary, false}};
    d.sugars = {{"Raisins", 0.5, 27, Phase::Primary}};
    d.adjuncts = {{"Vanilla bean", "1", "aging"}};
    d.yeastId = "lallemand-icv-d47";
    d.yeastTolerance = 13.5;
    d.targetFG = 1.012;
    d.backsweeten = PlannedBacksweeten{1.012, 1.25};
    const std::string text = json::stringify(designToJson(d));
    const Design back = designFromJson(json::parse(text));
    check(back.name == d.name && near(back.volumeGal, 3.5) && near(back.honeys[0].lb, 9.25), "design survives save/load (basics)");
    check(back.fruits.size() == 1 && back.fruits[0].note == "frozen" && back.fruits[0].water && near(*back.fruits[0].water, 84) &&
              !back.fruits[0].sugar && back.fruits[0].phase == Phase::Secondary,
          "fruit survives save/load (blank sugar stays blank)");
    check(back.juices.size() == 1 && near(back.juices[0].sg, 1.072), "juice survives save/load");
    check(back.sugars.size() == 1 && near(back.sugars[0].ppg, 27), "sugar survives save/load");
    check(back.yeastTolerance && near(*back.yeastTolerance, 13.5) && back.targetFG && near(*back.targetFG, 1.012), "overrides survive");
    check(back.backsweeten && near(back.backsweeten->lb, 1.25), "planned backsweetening survives");
    check(json::stringify(designToJson(back)) == text, "save → load → save gives identical text");
  }

  // ---------- JSON reader ----------
  {
    const auto v = json::parse(R"({"a":[1,2.5e1,-3],"s":"é🍯\n","t":true,"n":null,"o":{}})");
    check(v["a"].items().size() == 3 && near(v["a"].items()[1].num(), 25), "JSON numbers");
    check(v["s"].str() == "\xC3\xA9\xF0\x9F\x8D\xAF\n", "JSON unicode escapes (é and the honey-pot emoji)");
    check(v["t"].flag() && v["n"].isNull() && v["o"].isObject() && v["missing"].isNull(), "JSON literals");
    bool threw = false;
    try { json::parse("{\"a\":}"); } catch (const std::exception&) { threw = true; }
    check(threw, "bad JSON is rejected");
    bool deep = false;
    try { json::parse(std::string(100000, '[')); } catch (const std::exception&) { deep = true; }
    check(deep, "a corrupt file nested 100,000 deep is refused instead of crashing");
    bool fine = true;
    try { json::parse(std::string(150, '[') + std::string(150, ']')); } catch (const std::exception&) { fine = false; }
    check(fine, "ordinary nesting still reads");
  }

  // ---------- Screen words: number formats and dates ----------
  {
    namespace T = mead::text;
    check(T::num(7.2) == "7.2" && T::num(40) == "40" && T::num(13.5) == "13.5" && T::num(125) == "125", "numbers shown like Qt's QString::number");
    check(T::grams(2.5) == "2.5 g" && T::grams(12.4) == "12 g" && T::grams(9.96) == "10 g", "gram formats");
    check(T::wt(12.25) == "12.25 lb" && T::vol(5) == "5.00 gal", "weight and volume formats");
    const auto d = T::parseISO("2026-10-20");
    check(d && T::shortLabel(*d) == "Tue, Oct 20", "20 Oct 2026 is a Tuesday");
    check(d && T::isoOf(T::addDays(*d, 21)) == "2026-11-10" && T::shortLabel(T::addDays(*d, 21)) == "Tue, Nov 10", "day 21 after 20 Oct 2026");
    check(d && T::isoOf(T::addDays(*d, 73)) == "2027-01-01" && T::shortLabel(T::addDays(*d, 73)) == "Fri, Jan 1", "across the new year");
    check(T::isoOf(T::addDays(*T::parseISO("2028-02-28"), 1)) == "2028-02-29" && T::isoOf(T::addDays(*T::parseISO("2027-02-28"), 1)) == "2027-03-01", "leap years");
    check(T::isoOf(T::addDays(*T::parseISO("2100-02-28"), 1)) == "2100-03-01" && T::isoOf(T::addDays(*T::parseISO("2000-02-28"), 1)) == "2000-02-29", "century leap rule");
    check(!T::parseISO("2026-02-30") && !T::parseISO("2026-13-01") && !T::parseISO("20261020") && !T::parseISO(""), "bad dates are refused");
    check(T::daysBetween(*T::parseISO("2026-01-01"), *T::parseISO("2027-01-01")) == 365, "days in a year");
    check(T::parseNumber(" 1,115 ") && near(*T::parseNumber(" 1,115 "), 1.115) && !T::parseNumber("1.1x") && !T::parseNumber(""), "typed numbers");
  }

  // ---------- My recipes: file names that work on Windows and Linux ----------
  {
    namespace T = mead::text;
    check(T::recipeFileStem("Pear mead") == "Pear mead", "a plain name is kept");
    check(T::recipeFileStem("Lime/ginger: v2?") == "Lime ginger v2", "characters Windows forbids become spaces");
    check(T::recipeFileStem("  Sack   mead  ") == "Sack mead", "extra spaces are trimmed");
    check(T::recipeFileStem("Batch 3...") == "Batch 3", "no trailing dots");
    check(T::recipeFileStem("") == "Untitled recipe" && T::recipeFileStem("???") == "Untitled recipe", "a blank name still saves");
    check(T::recipeFileStem("con") == "Recipe con" && T::recipeFileStem("NUL.txt") == "Recipe NUL.txt" && T::recipeFileStem("Console") == "Console",
          "reserved Windows names are avoided, similar names are not");
    check(T::recipeFileStem("Mel\xC3\xB3mel de pera") == "Mel\xC3\xB3mel de pera", "accented letters are kept");
    const std::string longName = std::string(79, 'a') + "\xC3\xA9xyz";
    const std::string cut = T::recipeFileStem(longName);
    check(cut.size() <= 80 && cut == std::string(79, 'a'), "long names are cut at 80 without splitting a letter");
    const std::vector<std::string> have = {"Pear mead", "Pear mead (2)"};
    auto taken = [&](const std::string& x) { return std::find(have.begin(), have.end(), x) != have.end(); };
    check(T::uniqueStem("Pear mead", taken) == "Pear mead (3)" && T::uniqueStem("Lime mead", taken) == "Lime mead", "a second recipe with the same name gets (2), (3)…");
  }

  // ---------- Lemons and limes: kitchen amounts ----------
  {
    check(kitchenMeasure(0.67) == "2 tsp" && kitchenMeasure(3) == "3 tbsp" && kitchenMeasure(2.5) == "2½ tbsp" && kitchenMeasure(8) == "½ cup" &&
          kitchenMeasure(12) == "¾ cup" && kitchenMeasure(16) == "1 cup" && kitchenMeasure(20) == "1¼ cups" && kitchenMeasure(36) == "2¼ cups", "kitchen measures");
    check(kitchenRange(8, 14) == "8–14 tbsp" && kitchenRange(24, 36) == "1½–2¼ cups", "kitchen ranges");
    const CitrusYield y = citrusYield("Limes", 3, 4.4);
    check(near(y.juiceTbspLo, 24) && near(y.juiceTbspHi, 36), "3 lb limes: 1½–2¼ cups of juice");
    check(near(y.zestTbspLo, 8) && near(y.zestTbspHi, 14), "3 lb limes: 8–14 tbsp of zest");
    check(near(y.startJuiceTbsp, 8.8) && near(y.startZestTbsp, 4.4 * 2 / 3), "start: one lime's juice and zest per gallon");
    const CitrusYield little = citrusYield("Limes", 0.25, 5);
    check(little.startJuiceTbsp <= little.juiceTbspLo + 1e-9 && little.startZestTbsp <= little.zestTbspLo + 1e-9, "never start with more than you'll have");
  }

  // ---------- Spices, herbs, flowers, oak ----------
  {
    check(spices().size() >= 20, "spice list covers the common spices, herbs, flowers and oak");
    for (const auto& s : spices()) {
      check(s.subtle.per5 > 0 && s.subtle.per5 < s.noticeable.per5 && s.noticeable.per5 < s.strong.per5, "levels rise subtle < noticeable < strong: " + s.id);
      for (const SpiceLevel* l : {&s.subtle, &s.noticeable, &s.strong}) {
        const bool book = l->src.rfind("Piatz", 0) == 0 || l->src.rfind("Schramm", 0) == 0;
        const bool web = l->src.rfind("web:", 0) == 0, app = l->src.rfind("app:", 0) == 0;
        check(book || web || app, "every figure says where it comes from: " + s.id + " " + l->src);
        if (web) check(!s.web.empty(), "web figures name their pages: " + s.id);
        if (book) check(std::regex_search(l->src, std::regex("ch\\.[0-9]+")), "book figures give the chapter: " + s.id);
      }
      check(s.when == "primary" || s.when == "secondary" || s.when == "aging", "each has a time to add it: " + s.id);
      check(spiceForName(s.name) == &s, "the recipe name maps back to its entry: " + s.name);
      check(spiceAmount(s, "noticeable", 10) > spiceAmount(s, "noticeable", 5) * 1.99, "amounts scale with the batch: " + s.id);
      check(spiceGuide(s, 4.4).find("starting points") != std::string::npos, "the Designer guide says these are starting points: " + s.id);
    }
    check(spiceById("ginger")->noticeable.per5 == 7.5 && spiceById("ginger")->strong.per5 == 16, "ginger: Schramm's 1–2 oz per gallon and up to 1 lb in 5 gal");
    check(spiceById("cloves")->subtle.per5 == 1 && spiceById("cloves")->noticeable.per5 == 2, "cloves: Schramm's one or two in 5 gal");
    check(spiceById("nutmeg")->strong.per5 == 4, "nutmeg: Schramm's up to 4 oz per 5 gal");
    check(spiceById("chiles")->noticeable.per5 == 2, "chiles: Piatz's two dried peppers");
    check(spiceMeasure(*spiceById("ginger"), 7.5) == "7½ oz" && spiceMeasure(*spiceById("ginger"), 16) == "1 lb" && spiceMeasure(*spiceById("ginger"), 0.2) == "6 g",
          "spice measures in oz, lb and grams");
    check(spiceMeasure(*spiceById("cinnamon"), 1) == "1 stick" && spiceMeasure(*spiceById("cinnamon"), 2.4) == "2 sticks" &&
          spiceMeasure(*spiceById("allspice"), 10) == "10 berries" && spiceMeasure(*spiceById("orange"), 6) == "6 oranges' zest", "counted spices");
    check(spiceForName("Dried peppers for tincture") == spiceById("chiles") && spiceForName("Black pepper") == spiceById("pepper") &&
          spiceForName("Vodka soak") == nullptr && spiceForName("Oak chips (optional)") == spiceById("oak") && spiceForName("Pectinase") == nullptr,
          "names from the books' recipes map to the right spice");
  }

  // ---------- Word list ----------
  {
    namespace W = mead::words;
    std::map<std::string, int> keys;
    for (const auto& e : W::glossary()) {
      ++keys[e.key];
      check(!e.term.empty() && !e.plain.empty() && e.what.size() > 30 && !e.match.empty(), "word list entry complete: " + e.key);
      check(std::find(W::groups().begin(), W::groups().end(), e.group) != W::groups().end(), "word list group known: " + e.key);
      for (const auto& w : e.match) {
        const std::string linked = W::linkTerms("Check the " + w + " today.");
        check(linked.find("href=\"term:" + e.key + "\"") != std::string::npos, "'" + w + "' links to " + e.key + ": " + linked);
      }
    }
    bool unique = true;
    for (const auto& kv : keys) if (kv.second != 1) unique = false;
    check(unique, "word list keys are unique");
    check(W::glossary().size() >= 50, "word list covers the terms used on screen");
    const std::string a = W::linkTerms("Effective OG is 1.105; OG means original gravity.");
    check(a.find("term:effOG") != std::string::npos && a.find(">Effective OG</a>") != std::string::npos, "'Effective OG' is one term, not OG");
    check(a.find("term:og") != std::string::npos, "plain OG still links");
    size_t ogLinks = 0;
    for (size_t p0 = 0; (p0 = a.find("term:og\"", p0)) != std::string::npos; ++p0) ++ogLinks;
    check(ogLinks == 1, "each term links once per sentence");
    check(W::linkTerms("you must stir the dog's bowl; Og said hi").find("<a") == std::string::npos, "no links inside other words or for 'you must'");
    const std::string tricky = "1 < 2 & 3 > 2\nABV & FG";
    check(W::stripLinks(W::linkTerms(tricky)) == tricky, "links round-trip: escapes and line breaks");
    check(W::linkTerms(tricky).find("<br>") != std::string::npos && W::linkTerms(tricky).find("&lt;") != std::string::npos, "escaped for Qt StyledText");
  }

  std::printf("%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
