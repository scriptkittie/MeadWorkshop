// Mead Workshop — advisor. Port of js/advisor.js and the Designer profile logic in js/app.js.
// Every text below is word-for-word the browser version's; tests/advisor_tests.cpp compares them.
#include "mead_advisor.hpp"
#include "mead_spices.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <regex>
#include <system_error>

namespace mead {

// ---------------------------------------------------------------- number formatting
std::string toFixed(double x, int digits) {
  if (std::isnan(x)) return "NaN";
  if (std::isinf(x)) return x > 0 ? "Infinity" : "-Infinity";
  if (x < 0) return "-" + toFixed(-x, digits);
  // Exact decimal expansion, then round half up at `digits` (what JavaScript's toFixed does).
  char buf[512];
  auto r = std::to_chars(buf, buf + sizeof buf, x, std::chars_format::fixed, 80);
  std::string s(buf, r.ptr);
  const size_t dot = s.find('.');
  std::string intPart = s.substr(0, dot);
  std::string frac = dot == std::string::npos ? std::string() : s.substr(dot + 1);
  frac.resize(static_cast<size_t>(digits) + 1, '0');
  const bool up = frac[static_cast<size_t>(digits)] >= '5';
  std::string kept = intPart + frac.substr(0, static_cast<size_t>(digits));
  if (up) {
    int i = static_cast<int>(kept.size()) - 1;
    while (i >= 0) {
      if (kept[static_cast<size_t>(i)] == '9') { kept[static_cast<size_t>(i)] = '0'; --i; }
      else { ++kept[static_cast<size_t>(i)]; break; }
    }
    if (i < 0) kept.insert(kept.begin(), '1');
  }
  const size_t intLen = kept.size() - static_cast<size_t>(digits);
  return digits > 0 ? kept.substr(0, intLen) + "." + kept.substr(intLen) : kept;
}

namespace {

// JavaScript's String(number): shortest form that reads back exactly.
std::string jsNum(double x) {
  if (std::isnan(x)) return "NaN";
  char buf[64];
  auto r = std::to_chars(buf, buf + sizeof buf, x);
  std::string s(buf, r.ptr);
  return s == "-0" ? "0" : s;
}
std::string lower(std::string s) {
  for (auto& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}
std::string sg3(double v) { return toFixed(v, 3); }
std::string wt(double lb) { return toFixed(lb, 2) + " lb"; }

std::regex icase(const char* pattern) { return std::regex(pattern, std::regex::ECMAScript | std::regex::icase); }
const std::regex& HIGH_ACID() { static const std::regex r = icase("lemon|lime|currant|cranberr|rhubarb|gooseberr|grapefruit|chokeberr|cherries, sour|tart cherr"); return r; }
const std::regex& LEMON_LIME() { static const std::regex r = icase("lemon|lime"); return r; }
const std::regex& PEACHLIKE() { static const std::regex r = icase("peach|nectarine|apricot|mango"); return r; }
const std::regex& APPLE_CIDER() { static const std::regex r = icase("apple|cider"); return r; }
bool matches(const std::string& s, const std::regex& re) { return std::regex_search(s, re); }

// JS num(): '' / null / not-a-number → fallback.
double numOf(const json::Value& v, double fallback) {
  if (v.isNumber()) return v.num();
  if (v.isBool()) return v.flag() ? 1 : 0;
  if (v.isString()) {
    const std::string& s = v.str();
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return v.str().empty() ? fallback : 0;   // +"  " === 0 in JS
    size_t b = s.find_last_not_of(" \t\r\n");
    double d = 0;
    auto r = std::from_chars(s.data() + a, s.data() + b + 1, d);
    if (r.ec != std::errc() || r.ptr != s.data() + b + 1) return fallback;
    return d;
  }
  return fallback;
}
// A value the user left blank ('' or null) → empty.
std::optional<double> optNum(const json::Value& v) {
  if (v.isNull() || (v.isString() && v.str().empty())) return std::nullopt;
  double d = numOf(v, std::nan(""));
  if (std::isnan(d)) return std::nullopt;
  return d;
}
Phase phaseOf(const json::Value& v) { return v.str() == "secondary" ? Phase::Secondary : Phase::Primary; }

}  // namespace

// ---------------------------------------------------------------- database
const FruitInfo* Database::fruit(const std::string& id) const {
  for (const auto& f : fruits) if (f.id == id) return &f;
  return nullptr;
}
const YeastInfo* Database::yeast(const std::string& id) const {
  for (const auto& y : yeasts) if (y.id == id) return &y;
  return nullptr;
}
const YeastInfo* Database::yeastByCode(const std::string& code) const {
  for (const auto& y : yeasts) if (y.code == code) return &y;
  return nullptr;
}
const JuiceInfo* Database::juiceInfo(const std::string& id) const {
  for (const auto& j : juices) if (j.id == id) return &j;
  return nullptr;
}
const SchrammEntry* Database::schrammRow(const std::string& key) const {
  auto it = schramm.find(key);
  return it == schramm.end() ? nullptr : &it->second;
}

Database loadDatabase(const std::string& fruitsJson, const std::string& yeastsJson,
                      const std::string& schrammJson, const std::string& referenceJson) {
  Database db;
  // Keep each parsed document alive while looping over it.
  const json::Value fruitsDoc = json::parse(fruitsJson), yeastsDoc = json::parse(yeastsJson), schrammDoc = json::parse(schrammJson);
  for (const auto& v : fruitsDoc.items()) {
    FruitInfo f;
    f.id = v["id"].str(); f.name = v["name"].str(); f.acid = v["acid"].str(); f.pectin = v["pectin"].str();
    f.schramm = v["schramm"].str(); f.schrammMatch = v["schrammMatch"].str(); f.src = v["src"].str();
    f.water = numOf(v["water"], 85);
    f.sugar = optNum(v["sugar"]);
    f.juice = v["juice"].flag();
    db.fruits.push_back(std::move(f));
  }
  for (const auto& v : yeastsDoc.items()) {
    YeastInfo y;
    y.id = v["id"].str(); y.vendor = v["vendor"].str(); y.code = v["code"].str(); y.common = v["common"].str();
    y.name = v["name"].str(); y.toleranceRaw = v["toleranceRaw"].str(); y.tempRaw = v["tempRaw"].str();
    y.nitrogen = v["nitrogen"].str(); y.comments = v["comments"].str(); y.form = v["form"].str(); y.src = v["src"].str();
    y.schramm = v["schramm"].str();
    y.tolerance = optNum(v["tolerance"]); y.tolMin = optNum(v["tolMin"]); y.tolMax = optNum(v["tolMax"]);
    y.nitrogenConflict = v["nitrogenConflict"].flag();
    y.notesParaphrased = v["notesParaphrased"].flag();
    y.packetG = numOf(v["packetG"], 5);
    readYeastNotes(y);
    db.yeasts.push_back(std::move(y));
  }
  for (const auto& [key, v] : schrammDoc.members()) {
    SchrammEntry e;
    e.row.name = v["name"].str(); e.row.unit = v["unit"].str();
    e.row.mild[0] = v["mild"].items().at(0).num(); e.row.mild[1] = v["mild"].items().at(1).num();
    e.row.medium[0] = v["medium"].items().at(0).num(); e.row.medium[1] = v["medium"].items().at(1).num();
    e.row.strong = v["strong"].num();
    e.note = v["note"].str();
    db.schramm[key] = e;
  }
  const json::Value ref = json::parse(referenceJson);
  for (const auto& v : ref["juices"].items()) {
    JuiceInfo j;
    j.id = v["id"].str(); j.name = v["name"].str(); j.fromFruit = v["fromFruit"].str(); j.note = v["note"].str();
    j.sg = optNum(v["sg"]);
    if (j.sg && *j.sg == 0) j.sg.reset();
    j.ratio = numOf(v["ratio"], 1);
    if (j.ratio == 0) j.ratio = 1;
    j.acidic = v["acidic"].flag();
    db.juices.push_back(std::move(j));
  }
  for (const auto& v : ref["sugars"].items()) db.sugars.push_back({v["name"].str(), v["src"].str(), numOf(v["ppg"], C::SUGAR_PPG)});
  for (const auto& v : ref["honeys"].items()) db.honeys.push_back({v["name"].str(), v["group"].str(), v["note"].str()});
  for (const auto& v : ref["adjuncts"].items()) db.adjuncts.push_back(v.str());
  return db;
}

double juiceSGFromSugar(double pct) {
  double sg = 1;
  for (int i = 0; i < 4; ++i) sg = 1 + pct / 100 * 8.33 * sg * 46 / 1000;
  return std::stod(toFixed(sg, 3));
}
JuiceDefault juiceDefaultSG(const Database& db, const JuiceInfo& j) {
  if (j.sg) return {j.sg, false};
  const FruitInfo* f = j.fromFruit.empty() ? nullptr : db.fruit(j.fromFruit);
  if (f && f->sugar && *f->sugar != 0) return {juiceSGFromSugar(*f->sugar), true};
  return {std::nullopt, true};
}

// ---------------------------------------------------------------- designs
Design designFromJson(const json::Value& v) {
  Design d;
  auto s = [&](const char* k, std::string& out) { if (v[k].isString()) out = v[k].str(); };
  s("name", d.name); s("style", d.style); s("kind", d.kind); s("parentId", d.parentId); s("notes", d.notes);
  s("brewDate", d.brewDate); s("sweetness", d.sweetness); s("yeastId", d.yeastId);
  d.volumeGal = numOf(v["volumeGal"], 5);
  d.countFruitWater = !(v["countFruitWater"].isBool() && !v["countFruitWater"].flag());
  d.targetFG = optNum(v["targetFG"]);
  d.yeastTolerance = optNum(v["yeastTolerance"]);
  d.wild = v["wild"].flag();
  d.noGravityModel = v["noGravityModel"].flag();
  for (const auto& h : v["honeys"].items()) d.honeys.push_back({h["name"].str(), numOf(h["lb"], 0)});
  for (const auto& f : v["fruits"].items()) {
    DesignFruit x;
    x.fruitId = f["fruitId"].isString() ? f["fruitId"].str() : f["id"].str();
    x.name = f["name"].str(); x.note = f["note"].str();
    x.lb = numOf(f["lb"], 0);
    x.water = optNum(f["water"]); x.sugar = optNum(f["sugar"]);
    x.phase = phaseOf(f["phase"]);
    d.fruits.push_back(std::move(x));
  }
  for (const auto& j : v["juices"].items()) {
    DesignJuice x;
    x.juiceId = j["juiceId"].isString() ? j["juiceId"].str() : j["id"].str();
    x.name = j["name"].str();
    x.gal = numOf(j["gal"], 0); x.sg = numOf(j["sg"], 1); x.ratio = numOf(j["ratio"], 1);
    x.phase = phaseOf(j["phase"]);
    x.sgEstimated = j["sgEst"].flag();
    d.juices.push_back(std::move(x));
  }
  for (const auto& sg : v["sugars"].items()) {
    DesignSugar x;
    x.name = sg["name"].str(); x.lb = numOf(sg["lb"], 0);
    const auto ppg = optNum(sg["ppg"]);
    x.ppg = ppg ? *ppg : numOf(sg["pct"], 100) / 100 * C::SUGAR_PPG;
    x.phase = phaseOf(sg["phase"]);
    d.sugars.push_back(std::move(x));
  }
  for (const auto& a : v["adjuncts"].items()) {
    const json::Value& amt = a["amount"];
    d.adjuncts.push_back({a["name"].str(), amt.isNumber() ? jsNum(amt.num()) : amt.str(), a["phase"].str()});
  }
  if (v["backsweeten"].isObject()) d.backsweeten = PlannedBacksweeten{numOf(v["backsweeten"]["toFG"], 1.0), numOf(v["backsweeten"]["lb"], 0)};
  return d;
}

json::Value designToJson(const Design& d) {
  using json::Value;
  auto str = [](const std::string& s) { return Value::string(s); };
  auto opt = [](const std::optional<double>& o) { return o ? Value::number(*o) : Value::string(""); };
  auto phase = [](Phase p) { return Value::string(p == Phase::Secondary ? "secondary" : "primary"); };
  Value o = Value::object();
  auto& m = o.obj();
  m["name"] = str(d.name); m["style"] = str(d.style); m["kind"] = str(d.kind);
  if (!d.parentId.empty()) m["parentId"] = str(d.parentId);
  m["notes"] = str(d.notes); m["brewDate"] = str(d.brewDate); m["sweetness"] = str(d.sweetness);
  m["volumeGal"] = Value::number(d.volumeGal);
  m["countFruitWater"] = Value::boolean(d.countFruitWater);
  m["targetFG"] = opt(d.targetFG);
  m["yeastId"] = str(d.yeastId);
  m["yeastTolerance"] = opt(d.yeastTolerance);
  if (d.wild) m["wild"] = Value::boolean(true);
  if (d.noGravityModel) m["noGravityModel"] = Value::boolean(true);
  Value honeys = Value::array();
  for (const auto& h : d.honeys) { Value x = Value::object(); x.obj()["name"] = str(h.name); x.obj()["lb"] = Value::number(h.lb); honeys.arr().push_back(x); }
  m["honeys"] = honeys;
  Value fruits = Value::array();
  for (const auto& f : d.fruits) {
    Value x = Value::object(); auto& xm = x.obj();
    xm["fruitId"] = str(f.fruitId); xm["name"] = str(f.name); xm["lb"] = Value::number(f.lb);
    xm["water"] = opt(f.water); xm["sugar"] = opt(f.sugar); xm["phase"] = phase(f.phase);
    if (!f.note.empty()) xm["note"] = str(f.note);
    fruits.arr().push_back(x);
  }
  m["fruits"] = fruits;
  Value juices = Value::array();
  for (const auto& j : d.juices) {
    Value x = Value::object(); auto& xm = x.obj();
    xm["juiceId"] = str(j.juiceId); xm["name"] = str(j.name); xm["gal"] = Value::number(j.gal);
    xm["sg"] = Value::number(j.sg); xm["ratio"] = Value::number(j.ratio); xm["phase"] = phase(j.phase);
    xm["sgEst"] = Value::boolean(j.sgEstimated);
    juices.arr().push_back(x);
  }
  m["juices"] = juices;
  Value sugars = Value::array();
  for (const auto& s : d.sugars) {
    Value x = Value::object(); auto& xm = x.obj();
    xm["name"] = str(s.name); xm["lb"] = Value::number(s.lb); xm["ppg"] = Value::number(s.ppg); xm["phase"] = phase(s.phase);
    sugars.arr().push_back(x);
  }
  m["sugars"] = sugars;
  Value adj = Value::array();
  for (const auto& a : d.adjuncts) {
    Value x = Value::object(); auto& xm = x.obj();
    xm["name"] = str(a.name); xm["amount"] = str(a.amount); xm["phase"] = str(a.when);
    adj.arr().push_back(x);
  }
  m["adjuncts"] = adj;
  if (d.backsweeten) {
    Value b = Value::object();
    b.obj()["toFG"] = Value::number(d.backsweeten->toFG); b.obj()["lb"] = Value::number(d.backsweeten->lb);
    m["backsweeten"] = b;
  }
  return o;
}

std::string sweetFromFG(std::optional<double> fg) {
  if (!fg || *fg == 0) return "medium";
  if (*fg < 1.006) return "dry";
  if (*fg < 1.013) return "medium";
  if (*fg <= 1.020) return "sweet";
  return "dessert";
}

bool recipeHasIngredients(const json::Value& r) {
  return !r["honeys"].items().empty() || !r["juices"].items().empty() || !r["sugars"].items().empty();
}

Design designFromPublished(const json::Value& r, const Database& db) {
  Design d;
  d.name = r["name"].str(); d.style = r["style"].str(); d.kind = "published"; d.parentId = r["id"].str();
  d.volumeGal = numOf(r["volumeGal"], 5);
  d.countFruitWater = !(r["countFruitWater"].isBool() && !r["countFruitWater"].flag());
  d.sweetness = sweetFromFG(optNum(r["fg"]));
  d.wild = r["wild"].flag();
  d.noGravityModel = r["noGravityModel"].flag();
  for (const auto& h : r["honeys"].items()) d.honeys.push_back({h["name"].str(), numOf(h["lb"], 0)});
  for (const auto& f : r["fruits"].items()) {
    const FruitInfo* b = db.fruit(f["id"].str());
    DesignFruit x;
    x.fruitId = f["id"].str();
    x.name = !f["name"].str().empty() ? f["name"].str() : b ? b->name : "";
    x.lb = numOf(f["lb"], 0);
    x.phase = phaseOf(f["phase"]);
    x.water = !f["water"].isNull() ? optNum(f["water"]) : b ? std::optional<double>(b->water) : std::nullopt;
    x.sugar = !f["sugar"].isNull() ? optNum(f["sugar"]) : b ? b->sugar : std::nullopt;
    x.note = f["note"].str();
    d.fruits.push_back(std::move(x));
  }
  for (const auto& j : r["juices"].items()) {
    const JuiceInfo* b = db.juiceInfo(j["id"].str());
    DesignJuice x;
    x.juiceId = j["id"].str();
    x.name = !j["name"].str().empty() ? j["name"].str() : b ? b->name : "";
    x.gal = numOf(j["gal"], 0);
    if (!j["sg"].isNull()) x.sg = numOf(j["sg"], 1);
    else if (b) { const auto def = juiceDefaultSG(db, *b); x.sg = def.sg ? *def.sg : 1; x.sgEstimated = def.estimated; }
    const double ratio = numOf(j["ratio"], 0);
    x.ratio = ratio != 0 ? ratio : b ? b->ratio : 1;
    x.phase = phaseOf(j["phase"]);
    d.juices.push_back(std::move(x));
  }
  for (const auto& sg : r["sugars"].items()) {
    DesignSugar x;
    x.name = sg["name"].str(); x.lb = numOf(sg["lb"], 0);
    const auto ppg = optNum(sg["ppg"]);
    x.ppg = ppg ? *ppg : numOf(sg["pct"], 100) / 100 * C::SUGAR_PPG;
    x.phase = phaseOf(sg["phase"]);
    d.sugars.push_back(std::move(x));
  }
  for (const auto& a : r["adjuncts"].items()) {
    const json::Value& amt = a["amount"];
    d.adjuncts.push_back({a["name"].str(), amt.isNumber() ? jsNum(amt.num()) : amt.str(), a["phase"].str()});
  }
  d.yeastId = r["yeastId"].str();
  return d;
}

Faithfulness faithfulness(const Design& design, const Design& parent) {
  struct Ing { std::string kind, name; double amount; bool numeric; std::string phase; };
  auto canon = [](const Design& d) {
    std::vector<Ing> v;
    for (const auto& h : d.honeys) v.push_back({"honey", lower(h.name), h.lb, true, ""});
    for (const auto& f : d.fruits) v.push_back({"fruit", lower(f.name.empty() ? f.fruitId : f.name), f.lb, true, f.phase == Phase::Secondary ? "secondary" : "primary"});
    for (const auto& j : d.juices) v.push_back({"juice", lower(j.name.empty() ? j.juiceId : j.name), j.gal, true, ""});
    for (const auto& s : d.sugars) v.push_back({"sugar", lower(s.name), s.lb, true, ""});
    for (const auto& a : d.adjuncts) v.push_back({"adjunct", lower(a.name), 0, false, ""});
    return v;
  };
  Faithfulness out;
  out.scale = design.volumeGal / (parent.volumeGal != 0 ? parent.volumeGal : 5);
  const auto a = canon(parent), b = canon(design);
  auto find = [](const std::vector<Ing>& list, const Ing& x) -> const Ing* {
    for (const auto& y : list) if (y.kind == x.kind && y.name == x.name) return &y;
    return nullptr;
  };
  for (const auto& x : a) {
    const Ing* y = find(b, x);
    if (!y) out.changes.push_back("Removed " + x.kind + ": " + x.name);
    else if (x.numeric && x.amount > 0 && std::fabs(y->amount / (x.amount * out.scale) - 1) > 0.03)
      out.changes.push_back(x.name + ": " + toFixed(x.amount * out.scale, 2) + " → " + jsNum(y->amount) + " (scaled original vs yours)");
    else if (!x.phase.empty() && !y->phase.empty() && x.phase != y->phase)
      out.changes.push_back(x.name + ": moved from " + x.phase + " to " + y->phase);
  }
  for (const auto& y : b) if (!find(a, y)) out.changes.push_back("Added " + y.kind + ": " + y.name);
  if (design.yeastId != parent.yeastId) out.changes.push_back("Different yeast");
  out.level = out.changes.empty() ? (std::fabs(out.scale - 1) < 0.001 ? "As published" : "Faithful (scaled only)")
              : out.changes.size() <= 2 ? "Minor adaptation" : "Substantial adaptation";
  return out;
}

EngineInput toEngine(const Design& d, const Database& db) {
  EngineInput e;
  Recipe& r = e.recipe;
  r.volumeGal = d.volumeGal;
  r.countFruitWater = d.countFruitWater;
  r.honeys = d.honeys;
  for (const auto& f : d.fruits) {
    const FruitInfo* base = db.fruit(f.fruitId);
    FruitIn x;
    x.name = !f.name.empty() ? f.name : base && !base->name.empty() ? base->name : "Fruit";
    x.lb = f.lb;
    x.phase = f.phase;
    x.water = f.water ? *f.water : base ? base->water : 85;
    x.sugar = f.sugar ? f.sugar : base ? base->sugar : std::nullopt;
    r.fruits.push_back(x);
    e.fruitBase.push_back(base);
  }
  for (const auto& j : d.juices) {
    r.juices.push_back({j.name.empty() ? "Juice" : j.name, j.gal, j.sg, j.ratio, j.phase});
    e.juiceBase.push_back(db.juiceInfo(j.juiceId));
  }
  for (const auto& s : d.sugars) r.sugars.push_back({s.name, s.lb, s.ppg, s.phase});
  e.yeast = d.yeastId.empty() ? nullptr : db.yeast(d.yeastId);
  const bool override_ = d.yeastTolerance.has_value();
  const double fromTable = e.yeast && e.yeast->tolerance && *e.yeast->tolerance != 0 ? *e.yeast->tolerance : 14;
  r.yeast.tolerance = override_ ? *d.yeastTolerance : fromTable;
  if (!override_ && e.yeast) { r.yeast.tolMin = e.yeast->tolMin; r.yeast.tolMax = e.yeast->tolMax; }
  e.yeastName = e.yeast ? e.yeast->name : "Unspecified";
  e.yeastAssumed = !e.yeast && !override_;
  e.sweetness = d.sweetness.empty() ? "medium" : d.sweetness;
  return e;
}

// ---------------------------------------------------------------- review
std::vector<Note> review(const Design& d, const Result& res, const EngineInput& eng, const std::string& scaleName) {
  std::vector<Note> out;
  auto w = [&](const char* level, std::string text, std::string src) { out.push_back({level, std::move(text), std::move(src)}); };
  const YeastInfo* y = eng.yeast;
  const SweetScale& sc = sweetnessScale(scaleName);
  const double tol = eng.recipe.yeast.tolerance;
  if (d.wild) w("info", "Wild fermentation: raw honey and unheated water, no added yeast, stirred vigorously several times a day. Bubbling builds over a few days and subsides after a week or 10 days. The slower fructose fermentation continues for months, so the gravity predictions here don't apply.", "Katz");
  else if (eng.yeastAssumed) w("info", "No yeast chosen, so predictions assume a 14% tolerance.", "");
  if (res.volumes.waterToAdd < -0.02) w("warn", "The ingredients take up more than the batch volume (" + toFixed(-res.volumes.waterToAdd, 2) + " gal over). Increase the volume or reduce the honey or juice.", "");
  // With fruit in from brew day the yeast works in must + fruit; otherwise in the honey must alone.
  const bool fruitIn = std::any_of(eng.recipe.fruits.begin(), eng.recipe.fruits.end(), [](const FruitIn& f) { return f.phase == Phase::Primary; });
  const double startOG = fruitIn ? res.primaryOG : res.mixingOG;
  if (startOG >= 1.140) w("warn", "Starting gravity in the fermenter is about " + sg3(startOG) + ". Schramm notes some strains have difficulty starting at 1.140 or higher, and recommends choosing a yeast known for high-gravity tolerance. He describes feeding honey 1–2 lb at a time as fermentation slows, though he doesn't do it himself.", "Schramm ch.5");
  else if (fruitIn && res.mixingOG >= 1.140)
    w("info", "The honey must reads about " + sg3(res.mixingOG) + " before the fruit goes in, but with the fruit in from brew day the batch starts around " + sg3(res.primaryOG) +
                  ". Piatz mixes strong pre-fruit musts like this: his Strawberry–Banana–Ginger reads about 1.254 before the fruit and about 1.137 with it. Frozen fruit gives up its juice more readily.",
      "Piatz ch.10; Schramm ch.8");
  if (d.noGravityModel) w("info", "This recipe has mashed grain, which the app doesn't model, so the gravity covers honey and listed sugars only.", "");
  if (res.mixingOG > 1.150 && !eng.recipe.fruits.empty()) w("info", "Your hydrometer may not read the pre-fruit must (" + sg3(res.mixingOG) + "). Piatz's trick: mix equal volumes of must and water, then measure. It should read about " + sg3(1 + (res.mixingOG - 1) / 2) + ".", "Piatz ch.10");
  if (res.fg.abv > 18) w("warn", "About " + toFixed(res.fg.abv, 1) + "% ABV is above sack strength (14–18%).", "Piatz ch.2");

  const SweetLevel& target = sweetnessByKey(eng.sweetness, scaleName);
  if (!d.wild) {
    const std::string tolTxt = eng.yeastName + " (≈" + jsNum(tol) + "%)";
    const std::string range = sg3(target.lo) + "–" + sg3(target.hi);
    if (res.fg.reachesDry && eng.sweetness != "dry")
      w("info", tolTxt + " should ferment this dry (around 1.000). Piatz: when the OG is below the yeast's limit, the mead ferments dry. To finish " + lower(target.label) + " (" + range + " on " + sc.name + "'s scale), either stabilize and backsweeten, or raise the OG to about " + sg3(ogForToleranceFinish(target.target, tol)) + " so the yeast stops sweet.", "Piatz ch.9");
    else if (!res.fg.reachesDry && eng.sweetness == "dry")
      w("warn", "The yeast should stop around " + sg3(res.fg.central) + ", which is not dry. For a dry finish, keep the OG below about " + sg3(ogForDryABV(tol)) + ", the yeast's limit in Piatz's table, or choose a more tolerant yeast.", "Piatz ch.9");
    else if (!res.fg.reachesDry && (res.fg.central < target.lo - 0.001 || res.fg.central > target.hi + 0.001))
      w("info", "Predicted FG " + sg3(res.fg.central) + " is outside " + sc.name + "'s " + lower(target.label) + " range (" + range + "). Piatz's formula OG = 1 + (FG − 1) + (YT − 1) gives about " + sg3(ogForToleranceFinish(target.target, tol)) + " for " + sg3(target.target) + ".", "Piatz ch.9");
    if (!res.fg.reachesDry) w("info", "You're relying on the yeast stopping at its limit. Piatz warns a mead can look sweet while the yeast has only slowed. His Sangiovese–Zinfandel–Black Currant fermented further than expected. Sorbate doesn't stop an active fermentation, so confirm it has truly stopped before sweetening or bottling.", "Piatz ch.8, ch.10");
    if (res.fg.abv < 11 && eng.sweetness != "dry" && res.fg.reachesDry) w("info", "Most mead yeasts tolerate 11–18%, so a sweet low-alcohol mead needs post-fermentation sweetening or an early stop.", "Piatz ch.8–9");
  }

  for (size_t i = 0; i < eng.recipe.fruits.size(); ++i) {
    const FruitIn& f = eng.recipe.fruits[i];
    const FruitInfo* base = eng.fruitBase[i];
    if (!f.sugar) w("info", f.name + ": the fruit table gives no sugar figure, so it's counted as 0 and the real OG will be a little higher.", "Piatz ch.3");
    if (matches(f.name, HIGH_ACID()) && f.phase == Phase::Primary) w("warn", f.name + " in primary: Schramm strongly advises against acid additions before fermentation is complete. Citrus and some berries add considerable acid, and a pH below 3.5 slows yeast (calcium carbonate raises it). Consider adding it in secondary.", "Schramm ch.5");
    if (base && base->pectin == "High") w("info", f.name + " is high in pectin. Piatz suggests adding pectinase with the fruit (follow the package amount) to avoid pectin haze.", "Piatz ch.3");
  }
  for (size_t i = 0; i < eng.recipe.juices.size(); ++i) {
    const JuiceIn& j = eng.recipe.juices[i];
    const JuiceInfo* base = eng.juiceBase[i];
    if (((base && base->acidic) || matches(j.name, HIGH_ACID())) && j.phase != Phase::Secondary) w("warn", j.name + " in primary adds acid. Schramm advises adding acid only after fermentation is complete.", "Schramm ch.5");
    if (j.sg == 0 || j.sg <= 1) w("warn", j.name + ": enter the juice's measured SG.", "");
  }
  const auto& F = eng.recipe.fruits;
  const auto& J = eng.recipe.juices;
  if (std::any_of(F.begin(), F.end(), [](const FruitIn& f) { return f.phase == Phase::Primary; }))
    w("info", "With fruit in primary, punch the cap down when you stir, because drying fruit gets warm and adds unpleasant flavors. Piatz puts the fruit in a sanitized mesh bag.", "Piatz ch.5, ch.10");
  if (std::any_of(F.begin(), F.end(), [](const FruitIn& f) { return f.phase == Phase::Secondary; }) ||
      std::any_of(J.begin(), J.end(), [](const JuiceIn& j) { return j.phase == Phase::Secondary; }))
    w("info", "Fruit added later still brings water and sugar. The effective OG above includes it (Piatz's method), and the yeast will ferment that sugar.", "Piatz ch.9");
  bool malic = false;
  for (const auto* b : eng.fruitBase) if (b && b->acid.find("Malic") != std::string::npos) malic = true;
  if (y && y->code.find("71B") != std::string::npos && malic) w("good", "71B metabolizes 20–40% of the malic acid, which Piatz notes can benefit melomels made from high-malic fruit.", "Piatz ch.4");
  if (y && y->nitrogen == "high") w("warn", y->name + " has a high nitrogen requirement. Piatz notes such strains may need more than the typical staggered dosage.", "Piatz ch.4");
  if (y && y->nitrogenConflict) w("info", "Piatz and Schramm disagree about " + y->code + "'s nitrogen needs; the app goes with the higher one.", "Piatz ch.4; Schramm ch.5");
  if (y && !y->tempRaw.empty()) w("info", "Ferment " + y->code + " within " + y->tempRaw + ".", y->src);
  return out;
}

// ---------------------------------------------------------------- fruit character
std::vector<FruitCharacter> fruitCharacter(const EngineInput& eng, const Result& res, const Database& db,
                                           const std::string& sweetness) {
  struct Row { std::string id, key, match; std::vector<std::string> names; double lb = 0; };
  std::vector<Row> rows;
  auto rowFor = [&](const std::string& id) -> Row& {
    for (auto& r : rows) if (r.id == id) return r;
    rows.push_back({});
    rows.back().id = id;
    return rows.back();
  };
  const double V = res.volumes.final;
  for (size_t i = 0; i < eng.recipe.fruits.size(); ++i) {
    const FruitIn& f = eng.recipe.fruits[i];
    const FruitInfo* base = eng.fruitBase[i];
    const std::string key = base ? base->schramm : "";
    Row& r = rowFor(key.empty() ? "other:" + f.name : key);
    if (r.names.empty()) { r.key = key; r.match = base ? base->schrammMatch : ""; }
    r.names.push_back(f.name);
    r.lb += f.lb;
  }
  for (const auto& j : eng.recipe.juices) {
    if (!matches(j.name, APPLE_CIDER())) continue;
    Row& r = rowFor("apples");
    if (r.names.empty()) { r.key = "apples"; r.match = "exact"; }
    r.names.push_back(j.name);
    r.lb += j.gal * j.ratio;
  }
  std::vector<FruitCharacter> out;
  for (const auto& r : rows) {
    FruitCharacter c;
    c.names = r.names;
    const SchrammEntry* e = r.key.empty() ? nullptr : db.schrammRow(r.key);
    if (!e) {
      const double perGal = r.lb / V;
      c.level = perGal < 1 ? "Below light" : perGal <= 2 ? "Light" : perGal < 4 ? "Between light and strong" : "Strong";
      c.basis = "Not in Schramm's table. Piatz's guide for moderate fruits: 1–2 lb/gal is light, 4–5 lb/gal or more is strong. You have " + toFixed(perGal, 1) + " lb/gal.";
      c.src = "Piatz ch.9";
      out.push_back(c);
      continue;
    }
    const SchrammRow& row = e->row;
    const FruitIntensity ev = fruitIntensity(row, r.lb, V);
    const bool isLL = std::all_of(r.names.begin(), r.names.end(), [](const std::string& n) { return matches(n, LEMON_LIME()); });
    c.level = ev.level + (isLL ? " (lemons and limes need less than this table suggests)" : "");
    std::string extra;
    if (sweetness == "dry") extra = " Schramm: use a little less for dry meads.";
    else if (sweetness == "sweet" || sweetness == "dessert") extra = " Schramm: use a little more for sweets.";
    c.basis = "Schramm Table 8.1, \"" + row.name + "\"" + (r.match == "closest" ? " (closest row)" : "") + ": mild " +
              jsNum(row.mild[0]) + "–" + jsNum(row.mild[1]) + ", medium " + jsNum(row.medium[0]) + "–" + jsNum(row.medium[1]) +
              ", strong " + jsNum(row.strong) + "+ " + row.unit + " per 5 gal of medium mead. You have " + toFixed(ev.per5, 1) + " " +
              row.unit + " per 5 gal." + extra;
    c.note = e->note;
    c.src = "Schramm ch.8";
    out.push_back(c);
  }
  return out;
}

// ---------------------------------------------------------------- profile bars
std::vector<Zone> sweetnessZones(const std::string& scaleName) {
  const auto& L = sweetnessScale(scaleName).levels;
  std::vector<Zone> z;
  for (size_t i = 0; i < L.size(); ++i)
    z.push_back({std::max(0.990, L[i].lo), i + 1 < L.size() ? L[i + 1].lo : 1.050, L[i].label, static_cast<int>(i)});
  return z;
}
std::vector<Zone> strengthZones() {
  return {{0, 3.5, "", 0}, {3.5, 7.5, "Hydromel", 1}, {7.5, 14, "Standard", 2}, {14, 18, "Sack", 3}, {18, 20, "", 4}};
}
FruitScale fruitScale(const Database& db, const std::string& fruitId, double batchGal) {
  const FruitInfo* f = db.fruit(fruitId);
  const SchrammEntry* e = f && !f->schramm.empty() ? db.schrammRow(f->schramm) : nullptr;
  if (e && e->row.unit == "lb") {
    const SchrammRow& row = e->row;
    const double k = batchGal / 5;
    FruitScale s;
    s.zones = {{0, row.mild[0] * k, "", 0}, {row.mild[0] * k, row.mild[1] * k, "Mild", 1},
               {row.mild[1] * k, row.medium[0] * k, "", 1}, {row.medium[0] * k, row.medium[1] * k, "Medium", 2},
               {row.medium[1] * k, row.strong * k, "", 2}, {row.strong * k, row.strong * k * 1.5, "Strong", 3}};
    s.max = row.strong * k * 1.5;
    s.src = "Schramm Table 8.1 · " + row.name + (f->schrammMatch == "closest" ? " (closest)" : "");
    s.lemonLime = matches(f->name, LEMON_LIME());
    return s;
  }
  const double V = batchGal;
  FruitScale s;
  s.zones = {{0, 1 * V, "", 0}, {1 * V, 2 * V, "Light", 1}, {2 * V, 4 * V, "", 2}, {4 * V, 6 * V, "Strong", 3}};
  s.max = 6 * V;
  s.src = "Piatz ch.9 · 1–2 lb/gal light, 4–5+ lb/gal strong";
  s.lemonLime = false;
  return s;
}
// Same words as the Fruit character card, so the bar and the text always agree.
std::string fruitLevelFor(const Database& db, const std::string& fruitId, double lb, double batchGal) {
  const FruitInfo* f = db.fruit(fruitId);
  const SchrammEntry* e = f && !f->schramm.empty() ? db.schrammRow(f->schramm) : nullptr;
  if (e && e->row.unit == "lb") return fruitIntensity(e->row, lb, batchGal).level;
  const double perGal = lb / batchGal;
  return perGal < 1 ? "Below light" : perGal <= 2 ? "Light" : perGal < 4 ? "Between light and strong" : "Strong";
}
std::string sweetKeyFor(double fg, const std::string& scaleName) {
  const auto& L = sweetnessScale(scaleName).levels;
  for (const auto& l : L) if (fg >= l.lo && fg <= l.hi) return l.key;
  return fg < L.front().lo ? L.front().key : L.back().key;
}
double targetFGFor(const Design& d, const std::string& scaleName) {
  return d.targetFG ? *d.targetFG : sweetnessByKey(d.sweetness, scaleName).target;
}

namespace {
double round4(double v) { return std::stod(toFixed(v, 4)); }
std::string signedWt(double delta) { return (delta >= 0 ? "+" : "") + wt(delta); }
}  // namespace

std::vector<SweetOption> sweetOptions(const Design& d, const EngineInput& eng, const Result& r, double t, const Settings& s) {
  (void)d;
  const double tol = eng.recipe.yeast.tolerance;
  const std::string& yName = eng.yeastName;
  const double lim = ogForDryABV(tol);
  const double honeyNow = r.honeyLb;
  std::vector<SweetOption> out;
  if (t <= 1.000) {
    if (r.fg.reachesDry) {
      SweetOption o; o.kind = "ok"; o.title = "Already set to ferment dry";
      o.text = "OG " + sg3(r.effectiveOG) + " is below " + yName + "'s limit of about " + sg3(lim) + " (Piatz's table).";
      o.src = "Piatz ch.9";
      out.push_back(o);
    } else {
      const SolveResult sv = solveHoney(eng.recipe, lim - 0.002, s);
      SweetOption o; o.kind = "lessHoney"; o.title = "Less honey";
      o.text = "Use about " + wt(sv.lb) + " (" + wt(sv.lb - honeyNow) + ") so the OG is about " + sg3(lim - 0.002) + ", under the yeast's limit, and it ferments dry.";
      o.src = "Piatz ch.9"; o.applyOG = round4(lim - 0.002); o.honeyLb = sv.lb;
      out.push_back(o);
      SweetOption y; y.kind = "strongerYeast"; y.title = "Or a stronger yeast";
      y.text = "This OG needs a yeast rated at least about " + toFixed(abvFromPoints(pts(r.effectiveOG)), 1) + "% to finish dry.";
      y.src = "Piatz ch.9";
      out.push_back(y);
    }
    return out;
  }
  const double ogA = ogForToleranceFinish(t, tol);
  const SolveResult sA = solveHoney(eng.recipe, ogA, s);
  {
    SweetOption o; o.kind = "A"; o.title = "A · Yeast stops sweet";
    const bool finite = std::isfinite(sA.lb);
    o.text = (finite ? wt(sA.lb) + " honey (" + signedWt(sA.lb - honeyNow) + ")" : std::string("Not reachable")) + " → OG " + sg3(ogA) +
             ", ≈" + toFixed(abv(ogA, t), 1) + "% ABV. Piatz: OG = 1 + (FG − 1) + (YT − 1)." +
             (ogA >= 1.14 ? " Above 1.140, where Schramm notes some strains struggle to start." : "");
    o.src = "Piatz ch.9";
    o.reachable = finite && sA.reachable;
    if (o.reachable) { o.applyOG = round4(ogA); o.honeyLb = sA.lb; }
    out.push_back(o);
  }
  const Backsweeten bs = backsweeten(r.volumes.final, C::DRY_FG, t, s.honeyPPG, s.honeyLbPerGal);
  if (r.fg.reachesDry) {
    SweetOption o; o.kind = "B"; o.title = "B · Ferment dry, then sweeten";
    o.text = "Keep this recipe (≈" + toFixed(r.fg.abv, 1) + "% ABV), stabilize, and add about " + wt(bs.lb) + " honey afterward. This is the most controllable route. Schramm: easy to sweeten a too-dry mead, hard to fix a too-sweet one.";
    o.src = "Schramm ch.5; Piatz ch.8";
    o.backsweetenLb = std::stod(toFixed(bs.lb, 3));
    out.push_back(o);
  } else {
    const SolveResult sB = solveHoney(eng.recipe, lim - 0.002, s);
    SweetOption o; o.kind = "B"; o.title = "B · Ferment dry, then sweeten";
    o.text = "Cut honey to about " + wt(sB.lb) + " so it ferments dry (OG ≈ " + sg3(lim - 0.002) + "), then add about " + wt(bs.lb) + " after stabilizing.";
    o.src = "Schramm ch.5; Piatz ch.8";
    o.honeyLb = sB.lb;
    out.push_back(o);
  }
  if (t < r.fg.central) {
    SweetOption o; o.kind = "balance"; o.title = "";
    o.text = "Perceived sweetness also drops with more acidity or tannin (Piatz's balance chapter). Adjust those after fermentation by tasting.";
    o.src = "Piatz ch.6";
    out.push_back(o);
  }
  return out;
}

// ---------------------------------------------------------------- schedule (port of advisor.js)
std::vector<ScheduleItem> schedule(const Design& d, const Result& res, const EngineInput& eng, const SNA* nutrients) {
  std::vector<ScheduleItem> items;
  auto it = [&](std::optional<double> day, std::string title, std::string detail, std::string src, std::string kind = "",
                std::optional<double> sg = std::nullopt) {
    ScheduleItem x;
    x.day = day; x.title = std::move(title); x.detail = std::move(detail); x.src = std::move(src); x.kind = std::move(kind); x.sg = sg;
    items.push_back(std::move(x));
  };
  const auto& F = eng.recipe.fruits;
  const auto& J = eng.recipe.juices;
  const bool hasPrimaryFruit = std::any_of(F.begin(), F.end(), [](const FruitIn& f) { return f.phase == Phase::Primary; });
  std::vector<std::string> sec;
  for (const auto& f : F) if (f.phase == Phase::Secondary) sec.push_back(f.name);
  for (const auto& j : J) if (j.phase == Phase::Secondary) sec.push_back(j.name);
  if (d.wild) {
    it(0.0, "Mix raw honey with cold or room-temperature dechlorinated water (1:4 by volume)", "Cover against flies. No added yeast.", "Katz");
    it(0.0, "Stir or shake vigorously several times a day", "Bubbles appear after a few days, then build.", "Katz");
    it(7.0, "Bubbling begins to subside (7–10 days)", "You can drink it young, or let the slow fructose fermentation continue for months.", "Katz");
    return items;
  }
  it(0.0, "Brew day: mix the must", "Pour in about " + toFixed(std::max(0.0, res.volumes.waterToAdd), 2) + " gal of water (all the water) and stir in the honey. Together they fill to about " +
                                       toFixed(res.volumes.preFruit, 2) + " gal and the hydrometer should read about " + sg3(res.mixingOG) +
                                       ". Use water without chlorine or chloramine (charcoal filter or Campden).",
     "Piatz ch.5, ch.10");
  items.back().brewDay = true;
  if (nutrients) {
    const SNA& n = *nutrients;
    it(0.0, "Rehydrate yeast: " + jsNum(n.goFerm) + " g Go-Ferm in about " + jsNum(n.rehydrationOz) + " oz of water at ~104°F",
       "Stir in the Go-Ferm, add the yeast, and wait 15–20 minutes (no more than 30), then pitch.", "Piatz ch.4–5");
    items.back().brewDay = true;
    for (size_t i = 0; i < n.doses.size(); ++i) {
      const Dose& dose = n.doses[i];
      if (dose.hours) {
        it(*dose.hours / 24, "Nutrient dose " + std::to_string(i + 1) + ": " + toFixed(dose.grams, 1) + " g of the Fermaid K/DAP mix",
           i == 0 ? "At initial mixing." : "Add after a stirring session, or dry powder can make it foam over.", "Piatz ch.5", "nutrient");
        if (i == 0) items.back().brewDay = true;
      } else {
        it(std::nullopt, "Nutrient dose " + std::to_string(i + 1) + ": " + toFixed(dose.grams, 1) + " g when gravity reaches " + sg3(dose.sg ? *dose.sg : 1),
           "The 30% point. Piatz's step-by-step version adds doses 2–4 on days 3, 5 and 7 instead.", "Piatz ch.5", "nutrient", dose.sg);
      }
    }
  }
  it(0.0, "Stir 2–3 times a day for 8 days (roughly every 8–12 hours)",
     hasPrimaryFruit ? "Stir until no new foam forms, and punch the fruit cap down. Stop stirring after day 8." : "Stir until no new foam forms. Stop stirring after day 8.",
     "Piatz ch.5");
  it(21.0, "Rack to a carboy (Piatz: day 21)",
     std::string("Leave the sediment") + (hasPrimaryFruit ? " and fruit" : "") +
         " behind and minimize headspace. Schramm racks about two weeks after airlock activity slows, when gravity is around 1.030 or less.",
     "Piatz ch.5; Schramm ch.3");
  if (!sec.empty()) {
    std::string list;
    for (size_t i = 0; i < sec.size(); ++i) list += (i ? ", " : "") + sec[i];
    it(21.0, "Add secondary ingredients: " + list, "Once primary slows (Schramm: 2–4 weeks). Frozen fruit releases its juice more readily.", "Schramm ch.8, ch.12");
    it(35.0, "Taste for fruit character, and rack off when you like it",
       "Schramm: recognizable fruit flavor can come in a week or two; he has left meads on fruit for up to a year. Rack a couple of times for clarity.", "Schramm ch.8, ch.12");
  }
  const double finishDay = sec.empty() ? 35 : 49;
  it(finishDay, "Confirm fermentation is finished",
     "The mead is starting to clear and the gravity hasn't changed over several days. Predicted around " + sg3(res.fg.central) + ".", "Piatz ch.5");
  it(finishDay + 7, "Bench-trial acidity and sweetness",
     "Make acid additions only after fermentation. Balance sweetness against acidity and tannin, and adjust in small measured samples.", "Schramm ch.5; Piatz ch.6, ch.8");
  if (d.backsweeten || d.sweetness != "dry")
    it(finishDay + 14, "If sweetening: stabilize first",
       "Schramm: add sorbate at ½ tsp/gal, wait at least a day, then add honey a cup at a time. Piatz: sulfite at least 40 ppm with sorbate, and dose sorbate by its label. Sorbate breaks down to off-notes within about 6–12 months, so Piatz doesn't recommend it for meads you'll age. Pasteurization is the alternative.",
       "Schramm ch.5; Piatz ch.8", "stabilize");
  it(finishDay + 30, "Bulk age",
     "Schramm: no-heat meads are often very drinkable in 6–8 months and free of harshness within a year. Heated meads need about a year, and round out between 12 and 24 months. Fruit character can go \"closed\" for a while and then return.",
     "Schramm ch.6");
  it(finishDay + 30 + 180, "Bottle when clear and with no sign of fermentation for two weeks", "Schramm: peak drinkability usually takes 6–9 months of aging.", "Schramm ch.3");
  return items;
}

// ---------------------------------------------------------------- brew-day directions (Piatz ch.5, with this recipe's numbers)
Directions directions(const Design& d, const Result& res, const EngineInput& eng) {
  Directions out;
  if (d.wild) return out;   // the Katz timeline covers a wild ferment
  const Volumes& v = res.volumes;
  const double V = v.final;
  const auto& F = eng.recipe.fruits;
  std::vector<std::string> primaryFruit, primaryAdds;
  for (const auto& f : F) if (f.phase == Phase::Primary) primaryFruit.push_back(toFixed(f.lb, 2) + " lb " + lower(f.name));
  for (const auto& j : eng.recipe.juices) if (j.phase == Phase::Primary) primaryAdds.push_back(toFixed(j.gal, 2) + " gal " + lower(j.name));
  for (const auto& sgr : eng.recipe.sugars) if (sgr.phase == Phase::Primary) primaryAdds.push_back(toFixed(sgr.lb, 2) + " lb " + lower(sgr.name));
  auto joinList = [](const std::vector<std::string>& xs) {
    std::string s;
    for (size_t i = 0; i < xs.size(); ++i) s += (i ? (i + 1 == xs.size() ? " and " : ", ") : "") + xs[i];
    return s;
  };
  const SNA n = piatzSNA(V, res.effectiveOG, res.fg.central);
  const double waterTotal = std::max(0.0, v.waterToAdd);
  const double startWater = std::min(waterTotal, std::round(V * 0.4 * 2) / 2);
  const double carboy = std::ceil(V);

  // ---- before brew day
  out.prep.push_back({"Gather the equipment",
                      "A food-grade plastic fermenter with a lid, about twice the batch size (Piatz uses a 10-gallon one for 5 gal), marked with volume lines; a " +
                          jsNum(carboy) + "-gallon carboy (close to the finished " + toFixed(V, 2) +
                          " gal, so there's little headspace) with a stopper and airlock; a hydrometer and test jar; a thermometer; a large spoon; a kitchen scale; an autosiphon and tubing" +
                          (primaryFruit.empty() ? "" : "; and a large mesh bag for the fruit") + ".",
                      "Piatz ch.5"});
  out.prep.push_back({"Clean, then sanitize",
                      "Wash everything that will touch the mead, then sanitize it. Piatz uses Star San (1 oz in 5 gal of water): wet every surface, wait 1–2 minutes, and don't rinse; the foam is harmless. Boil fabric mesh bags for a few minutes instead of using sanitizer.",
                      "Piatz ch.5"});
  out.prep.push_back({"Treat the water",
                      "You'll need about " + toFixed(waterTotal, 2) + " gal, plus a little extra. Remove chlorine and chloramine: one crushed Campden tablet treats up to 20 gal (wait 10–15 minutes), or use an activated-charcoal filter. Let it reach room temperature, 65–75°F.",
                      "Piatz ch.5"});
  out.prep.push_back({"Bag the nutrients",
                      "Mix " + toFixed(n.fermaidK, 1) + " g Fermaid K and " + toFixed(n.dap, 1) + " g DAP in a small resealable bag and keep it next to the fermenter. You'll add it in " +
                          toFixed(n.dose, 1) + " g doses.",
                      "Piatz ch.5"});

  // ---- brew day
  std::vector<std::string> honeys;
  for (const auto& h : d.honeys) if (h.lb > 0) honeys.push_back(toFixed(h.lb, 2) + " lb " + lower(h.name.empty() ? std::string("honey") : h.name + " honey"));
  out.brewDay.push_back({"Weigh in the honey",
                         "Put the sanitized fermenter on the scale, zero it, and pour in " + (honeys.empty() ? std::string("the honey") : joinList(honeys)) +
                             ". Scrape the container with a sanitized spatula. Crystallized honey is fine: soften the jug in hot water, or in an oven at 125–150°F for a couple of hours.",
                         "Piatz ch.5"});
  out.brewDay.push_back({"Dissolve it",
                         "Add about " + toFixed(startWater, 1) + " gal of the treated water (the exact amount isn't important) and stir until there's no thick honey left on the bottom. Vigorous stirring also gives the yeast the oxygen it needs." +
                             (primaryAdds.empty() ? std::string() : " Stir in the " + joinList(primaryAdds) + " now too."),
                         "Piatz ch.5"});
  std::string topUp = "Keep adding water a little at a time, stirring and reading the hydrometer, until it reads " + sg3(res.mixingOG) + ". That takes about " +
                      toFixed(waterTotal, 2) + " gal of water in all, and with the honey the bucket then holds about " + toFixed(v.preFruit, 2) +
                      " gal (the honey takes up the difference: it is not more water). Go by the hydrometer, adding smaller amounts as you get close. Write down the reading and the temperature.";
  if (res.mixingOG > 1.150)
    topUp += " Your hydrometer may not read that high: mix equal amounts of must and water in the test jar, and it should read about " + sg3(1 + (res.mixingOG - 1) / 2) + ".";
  out.brewDay.push_back({"Water up to the target gravity", topUp, res.mixingOG > 1.150 ? "Piatz ch.5, ch.10" : "Piatz ch.5"});
  if (eng.yeast) {
    out.brewDay.push_back({"Rehydrate the yeast",
                           "Warm about " + jsNum(n.water5gMl) + " mL of the treated water to 104°F (a little cooler is fine; never hotter). Stir in " + toFixed(n.goFerm5g, 1) +
                               " g Go-Ferm until dissolved, add " + std::to_string(n.packets) + " packet" + (n.packets > 1 ? "s" : "") + " of " + eng.yeast->name +
                               ", stir, cover with cling wrap, and wait 15–20 minutes (no more than 30).",
                           "Piatz ch.4–5; Lallemand: Go-Ferm 1.25 × yeast"});
  } else {
    out.brewDay.push_back({"Choose a yeast", "Pick a yeast on the Designer tab to get rehydration amounts. Piatz rehydrates dried yeast with Go-Ferm at about 104°F for 15–20 minutes.", "Piatz ch.4–5"});
  }
  out.brewDay.push_back({"First nutrient dose", "While the yeast rehydrates, add " + toFixed(n.dose, 1) + " g of the nutrient mix to the must.", "Piatz ch.5"});
  if (!primaryFruit.empty())
    out.brewDay.push_back({"Add the fruit",
                           "Put the " + joinList(primaryFruit) + " (whole or in chunks) into the boiled mesh bag and push it down into the must. Piatz adds fruit when he pitches the yeast.",
                           "Piatz ch.5"});
  {
    std::vector<std::string> spicesIn;
    for (const auto& a : d.adjuncts)
      if (a.when == "primary" && !a.name.empty()) spicesIn.push_back(lower(a.name) + (a.amount.empty() ? "" : " (" + a.amount + ")"));
    if (!spicesIn.empty())
      out.brewDay.push_back({"Add the spices",
                             "Put the " + joinList(spicesIn) + " in a sanitized mesh bag and drop it into the must. Piatz sanitizes spices with a quick spray or soak in cheap vodka (and adds the vodka too if it picked up flavour). Lift the bag out to stir, and taste as it goes: you can always add more.",
                             "Piatz ch.3"});
  }
  out.brewDay.push_back({"Pitch the yeast",
                         "Check both temperatures. If the yeast is more than 18°F (10°C) warmer than the must, stir small amounts of must into the yeast over about 5 minutes and wait 15–20 minutes; repeat until the gap is under 18°F. Then pour it in, stir, put the lid on, and note the date and time.",
                         "Piatz ch.5"});
  std::string where = "Keep the fermenter at room temperature, about 62–75°F";
  if (eng.yeast && !eng.yeast->tempRaw.empty()) where += " (" + eng.yeast->code + ": " + eng.yeast->tempRaw + ")";
  where += ". From here, follow the timeline below.";
  out.brewDay.push_back({"Where to keep it", where, eng.yeast ? "Piatz ch.5; " + eng.yeast->src : "Piatz ch.5"});
  return out;
}

// ---------------------------------------------------------------- styles (Piatz ch.2)
const std::vector<StyleInfo>& styles() {
  static const std::vector<StyleInfo> list = {
      {"traditional", "Traditional mead", "Just honey, water and yeast: no fruit or spices. All the sugar comes from the honey, so the honey's own character is the star. Made dry, semisweet or sweet."},
      {"melomel", "Melomel (fruit mead)", "A mead with fruit. The fruit and the honey should both come through, well blended, and the fruit's acid, tannin and colour shape the balance. This is the name for fruits without their own group below, or for a mix of fruits from different groups."},
      {"cyser", "Cyser (apple mead)", "A fruit mead made with apples or apple juice; traditionally honey added to apple juice with no extra water. It needs enough honey that it isn't just a cider. The apples' acid and tannin usually want some sweetness for balance."},
      {"pyment", "Pyment (grape mead)", "A fruit mead made with grapes, usually grape juice. It should taste wine-like but blended with the honey."},
      {"berry", "Berry mead", "A fruit mead made with berries: blackberries, blueberries, raspberries, strawberries, cranberries, elderberries and so on."},
      {"stone", "Stone fruit mead", "A fruit mead made with stone fruit: apricots, cherries, plums, nectarines, peaches."},
      {"metheglin", "Metheglin (spiced mead)", "Honey plus spices or herbs, without fruit. The spice and the honey should both come through without either taking over."},
      {"fruitspice", "Fruit and spice mead", "Fruit and spice together, such as pears with ginger, or fruit with citrus zest (Piatz counts citrus peel as a spice). A spiced grape mead (hippocras) belongs here too."},
      {"braggot", "Braggot", "Honey plus malted grain, mashed as in beer-making (hops optional). It should balance honey and grain, not be a honey beer."},
      {"historical", "Historical mead", "A historical or regional mead that fits nowhere else, such as Ethiopian t'ej."},
      {"experimental", "Experimental mead", "The catch-all: other fermentables (maple syrup, molasses, agave), unusual processes (bochet, made with boiled, caramelised honey; spirit-barrel oak), or combinations such as a braggot with fruit."}};
  return list;
}

const StyleInfo* styleByName(const std::string& n) {
  if (n.empty()) return nullptr;
  const std::string l = lower(n);
  for (const auto& st : styles()) if (st.key == l || lower(st.name) == l || lower(st.name).rfind(l, 0) == 0) return &st;
  if (l == "fruit & spice" || l == "fruit and spice") return &styles()[7];
  return nullptr;
}

StyleMatch detectStyle(const Design& d, const Database& db) {
  static const std::regex apple = icase("apple|cider"),
      grape = icase("grape|riesling|pinot|chardonnay|cabernet|merlot|zinfandel|sangiovese|gew(u|ü)rz|traminer|muscat|moscato|viognier|syrah|shiraz|sauvignon|semillon|tempranillo|malbec|grenache|niagara|concord|catawba|vidal|seyval|raisin"),
      berry = icase("berr"), stone = icase("apricot|cherr|plum|prune|nectarine|peach"),
      spice = icase("ginger|cinnamon|nutmeg|vanilla|clove|chile|chili|chipotle|pepper|mint|chamomile|zest|peel|cardamom|coriander|lavender|rose|hibiscus|allspice|anise|lemongrass|coffee|chocolate|cocoa|basil|thyme|rosemary|sage|juniper|hops|spice|herb"),
      notSpice = icase("pectinase|tannin|acid|carbonate|vodka|nutrient|energizer|finings|campden|sorbate|sulfite|honey for|oak|yeast|blend with|alternative|starter"),
      malt = icase("malt|grain|barley|crystal \\d+"), oddSugar = icase("agave|maple|molasses"),
      spirit = icase("bourbon|whiskey|whisky|\\brum\\b|brandy|cognac|spirit"), citrusPeel = icase("zest|peel(?!ed)");
  std::vector<std::string> fruitNames, spiceNames, maltNames, oddNames, spiritNames;
  std::vector<std::pair<std::string, double>> groups;   // group → weight (lb)
  auto addFruit = [&](const std::string& name, double lb) {
    if (matches(name, icase("starter"))) return;     // e.g. apple juice for a yeast starter
    fruitNames.push_back(lower(name));
    const std::string grp = matches(name, apple) ? "cyser" : matches(name, grape) ? "pyment" : matches(name, berry) ? "berry" : matches(name, stone) ? "stone" : "melomel";
    for (auto& gw : groups) if (gw.first == grp) { gw.second += lb; return; }
    groups.push_back({grp, lb});
  };
  for (const auto& f : d.fruits) if (f.lb > 0) {
    const FruitInfo* b = db.fruit(f.fruitId);
    addFruit(!f.name.empty() ? f.name : b ? b->name : "fruit", f.lb);
  }
  for (const auto& j : d.juices) if (j.gal > 0) addFruit(j.name.empty() ? "juice" : j.name, j.gal * C::WATER_LB_PER_GAL);
  for (const auto& sg : d.sugars) {
    if (sg.lb <= 0) continue;
    if (matches(sg.name, malt)) maltNames.push_back(lower(sg.name));
    else if (matches(sg.name, oddSugar)) oddNames.push_back(lower(sg.name));
    else if (matches(sg.name, icase("raisin|date|fig|prune"))) addFruit(sg.name, sg.lb);
  }
  for (const auto& a : d.adjuncts) {
    if (a.name.empty()) continue;
    const std::string nm = a.name.substr(0, a.name.find(" from "));
    if (matches(a.name, malt)) { maltNames.push_back(lower(nm)); continue; }
    if (matches(a.name, spirit)) { spiritNames.push_back(lower(nm)); continue; }
    if (matches(a.name, notSpice)) continue;
    if (matches(a.name, icase("juice"))) addFruit(nm, 0);
    else if (matches(a.name, spice)) spiceNames.push_back(lower(nm));
  }
  auto list = [](const std::vector<std::string>& xs) {
    std::string t;
    for (size_t i = 0; i < xs.size(); ++i) t += (i ? (i + 1 == xs.size() ? " and " : ", ") : "") + xs[i];
    return t;
  };
  auto byKey = [](const std::string& k) { for (const auto& st : styles()) if (st.key == k) return &st; return &styles()[0]; };
  if (!maltNames.empty() && !fruitNames.empty()) return {byKey("experimental"), "malt (" + list(maltNames) + ") with fruit (" + list(fruitNames) + "): Piatz puts a braggot with fruit in the experimental category"};
  if (!maltNames.empty()) return {byKey("braggot"), "it has malted grain (" + list(maltNames) + ")" + (spiceNames.empty() ? "" : "; hops are optional in a braggot")};
  if (!spiritNames.empty()) return {byKey("experimental"), list(spiritNames) + ": Piatz puts spirit-barrel aging in the experimental category"};
  if (!oddNames.empty()) return {byKey("experimental"), list(oddNames) + " is an additional fermentable, which Piatz puts in the experimental category"};
  if (!fruitNames.empty() && !spiceNames.empty()) {
    const bool peel = std::any_of(spiceNames.begin(), spiceNames.end(), [](const std::string& x) { return matches(x, citrusPeel); });
    return {byKey("fruitspice"), "fruit (" + list(fruitNames) + ") and spice (" + list(spiceNames) + ")" + (peel ? "; Piatz counts citrus peel as a spice" : "")};
  }
  if (!fruitNames.empty()) {
    double total = 0;
    for (const auto& gw : groups) total += gw.second;
    const auto top = std::max_element(groups.begin(), groups.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    if (groups.size() == 1 || (total > 0 && top->second >= 0.9 * total))   // one group is nearly all of it (e.g. a cyser with a few raisins)
      return {byKey(groups.size() == 1 ? groups[0].first : top->first), "it has " + list(fruitNames)};
    return {byKey("melomel"), "fruits from different groups (" + list(fruitNames) + ")"};
  }
  if (!spiceNames.empty()) return {byKey("metheglin"), "spices or herbs (" + list(spiceNames) + ") without fruit"};
  return {byKey("traditional"), "only honey, water and yeast"};
}

ProfileWords describeProfile(const Design& d, const Result& r, const Database& db, const std::string& scaleName) {
  ProfileWords w;
  const StyleInfo* chosen = styleByName(d.style);
  w.style = (chosen ? chosen : detectStyle(d, db).style)->name;
  const double a = r.fg.abv;
  if (a < 3.5) { w.strength = "below hydromel strength"; w.strengthPlain = "under 3.5% alcohol"; }
  else if (a < 7.5) { w.strength = "hydromel strength"; w.strengthPlain = "light: 3.5–7.5% alcohol (starting gravity about 1.035–1.080)"; }
  else if (a < 14) { w.strength = "standard strength"; w.strengthPlain = "7.5–14% alcohol (starting gravity about 1.080–1.120)"; }
  else if (a <= 18) { w.strength = "sack strength"; w.strengthPlain = "strong: 14–18% alcohol (starting gravity about 1.120–1.170)"; }
  else { w.strength = "above sack strength"; w.strengthPlain = "over 18% alcohol"; }
  const double fg = d.backsweeten ? d.backsweeten->toFG : r.fg.central;
  w.sweetness = lower(sweetnessClass(fg, scaleName));
  const SweetScale& sc = sweetnessScale(scaleName);
  w.sweetnessPlain = "finishing at " + sg3(fg) + (d.backsweeten ? " after sweetening" : "") + " on " + sc.name + "'s scale (";
  for (size_t i = 0; i < sc.levels.size(); ++i)
    w.sweetnessPlain += (i ? ", " : "") + lower(sc.levels[i].label) + " " + sg3(sc.levels[i].lo) + (sc.levels[i].key == "dessert" ? "+" : "–" + sg3(sc.levels[i].hi));
  w.sweetnessPlain += ")";
  return w;
}

// ---------------------------------------------------------------- goal planner
Plan plan(const Goal& goal, const Database& db, const Settings& s, const std::string& scaleName) {
  Plan p;
  auto say = [&](std::string text, std::string src) { p.why.push_back({std::move(text), std::move(src)}); };
  const double V = goal.volumeGal > 0 ? goal.volumeGal : 5;
  const SweetLevel& sw = sweetnessByKey(goal.sweetness, scaleName);
  const double abvT = goal.abv;

  // 1. fruit
  std::vector<DesignFruit> fruits;
  for (const auto& g : goal.fruits) {
    const FruitInfo* f = db.fruit(g.fruitId);
    if (!f) continue;
    const SchrammEntry* e = f->schramm.empty() ? nullptr : db.schrammRow(f->schramm);
    const bool ll = matches(f->name, LEMON_LIME());
    const std::string lvl = g.intensity.empty() ? "medium" : g.intensity;
    double lb = 0;
    const bool useAll = lvl == "all" && g.haveLb && *g.haveLb > 0;
    if (useAll) {
      lb = *g.haveLb;
      if (e && e->row.unit == "lb") {
        const FruitIntensity ev = fruitIntensity(e->row, lb, V);
        say(f->name + ": using all " + jsNum(lb) + " lb you have. That's " + toFixed(ev.per5, 1) + " lb per 5 gal: " + lower(ev.level) +
                " on Schramm's Table 8.1 (" + e->row.name + ": mild " + jsNum(e->row.mild[0]) + "–" + jsNum(e->row.mild[1]) + ", medium " +
                jsNum(e->row.medium[0]) + "–" + jsNum(e->row.medium[1]) + ", strong " + jsNum(e->row.strong) + "+ lb per 5 gal).",
            "Schramm ch.8");
      } else {
        say(f->name + ": using all " + jsNum(lb) + " lb you have, " + toFixed(lb / V, 1) + " lb/gal. Piatz's guide: 1–2 lb/gal is light, 4–5 lb/gal or more is strong.", "Piatz ch.9");
      }
    } else if (e && e->row.unit == "lb") {
      const std::string eff = lvl == "all" ? "medium" : lvl;
      const SchrammRow& row = e->row;
      const FruitRec rec = fruitRecommendation(row, eff, V, goal.sweetness, ll);
      lb = (eff == "strong" || ll) ? rec.lo : rec.mid;
      const std::string amount = eff == "strong" ? jsNum(row.strong) + " lb or more"
                                 : eff == "mild" ? jsNum(row.mild[0]) + "–" + jsNum(row.mild[1]) + " lb"
                                                 : jsNum(row.medium[0]) + "–" + jsNum(row.medium[1]) + " lb";
      std::string t = f->name + ": Schramm's Table 8.1 gives " + amount + " per 5 gal for " + eff + " character" +
                      (f->schrammMatch == "closest" ? " (closest row: \"" + row.name + "\")" : "") + ". Scaled to " + jsNum(V) + " gal";
      if (rec.adj != 1)
        t += std::string(", and ") + (rec.adj < 1 ? "reduced" : "increased") + " 10% because Schramm says to use \"a little " +
             (rec.adj < 1 ? "less for dry" : "more for sweet") + "\" meads (the 10% is the app's reading)";
      t += ", that's " + toFixed(lb, 1) + " lb.";
      if (ll) t += " Schramm says to use less for lemons and limes than for oranges, without giving a figure, so the app drops one level.";
      say(t, "Schramm ch.8");
    } else {
      const std::string eff = lvl == "all" ? "medium" : lvl;
      const double perGal = eff == "mild" ? 1.5 : eff == "strong" ? 4.5 : 3;
      lb = perGal * V;
      say(f->name + ": not in Schramm's table. Piatz's guide is 1–2 lb/gal for light character and 4–5 lb/gal or more for strong, so " +
              jsNum(perGal) + " lb/gal × " + jsNum(V) + " gal = " + toFixed(lb, 1) + " lb.",
          "Piatz ch.9");
    }
    if (!useAll && g.haveLb && *g.haveLb > 0) {
      const double have = *g.haveLb;
      if (have < lb * 0.95) {
        say("You have " + jsNum(have) + " lb of " + f->name + ", less than the " + toFixed(lb, 1) + " lb suggested. The plan uses what you have, so expect less " +
                lower(f->name) + " character.", "");
        lb = have;
      } else if (lvl == "strong" && have > lb) {
        say("You have " + jsNum(have) + " lb. Schramm's \"strong\" is open-ended (\"" + (e ? jsNum(e->row.strong) : std::string("4–5 lb/gal")) +
                " or more\"), so you can use more. The plan starts at the minimum.", "Schramm ch.8");
      }
    }
    Phase phase = Phase::Secondary;
    if (g.phase == "primary") phase = Phase::Primary;
    else if (g.phase == "secondary") phase = Phase::Secondary;
    else if (ll || matches(f->name, HIGH_ACID())) {
      phase = Phase::Secondary;
      say(f->name + " goes in secondary. Schramm advises against adding acid before fermentation is complete, and notes citrus adds considerable acid.", "Schramm ch.5");
    } else if (matches(f->name, PEACHLIKE())) {
      phase = Phase::Primary;
      say(f->name + " goes in primary, in a mesh bag. Schramm used fruit-in-primary for his Peach Ginger Melomel because peaches are unwieldy in fermentation. Secondary also works.", "Schramm ch.12");
    } else {
      phase = Phase::Secondary;
      say(f->name + " goes in secondary. Schramm's table is for fruit in secondary, and he says that keeps the truest fruit character. In Mambo in Your Mouth he adds fruit after 2–4 weeks, once primary slows.", "Schramm ch.8, ch.12");
    }
    DesignFruit df;
    df.fruitId = f->id;
    df.name = f->name;
    df.lb = useAll ? lb : std::ceil(lb * 4 - 1e-9) / 4;
    if (g.haveLb && *g.haveLb > 0) df.lb = std::min(df.lb, *g.haveLb);   // never plan more than you have on hand
    df.phase = phase;
    fruits.push_back(df);
  }
  if (std::any_of(fruits.begin(), fruits.end(), [](const DesignFruit& f) { return matches(f.name, LEMON_LIME()); }))
    say("Neither book gives amounts for lime or lemon zest. Add citrus in secondary to taste, using Piatz's small-sample blending method, or a tincture for fine-tuning.", "Piatz ch.8");

  // 2. yeast
  const YeastInfo* yeast = goal.yeastId.empty() ? nullptr : db.yeast(goal.yeastId);
  if (!yeast) {
    yeast = db.yeastByCode("71B-1122");
    static const char* poolCodes[] = {"71B-1122", "ICV D47", "EC 1118", "ICV-K1 (V 1116)", "RC 212"};
    std::vector<const YeastInfo*> pool;
    for (const auto& y : db.yeasts)
      if (y.tolerance && *y.tolerance != 0)
        for (const char* c : poolCodes) if (y.code == c) pool.push_back(&y);
    if (yeast && yeast->tolerance && std::fabs(*yeast->tolerance - abvT) > 1.5 && !pool.empty()) {
      const YeastInfo* best = pool[0];
      for (const auto* y : pool) if (std::fabs(*y->tolerance - abvT) < std::fabs(*best->tolerance - abvT)) best = y;
      yeast = best;
    }
    if (yeast) {
      const bool is71 = yeast->code.find("71B") != std::string::npos;
      say("Yeast: " + yeast->name + " (≈" + jsNum(yeast->tolerance ? *yeast->tolerance : 14) + "%)." +
              (is71 ? " It's Piatz's go-to strain and Schramm's preferred yeast for big-fruit melomels." : " Its tolerance is closest to your target."),
          is71 ? "Piatz ch.9; Schramm ch.5" : yeast->src);
    }
  }
  p.yeast = yeast;
  const double T = yeast && yeast->tolerance && *yeast->tolerance != 0 ? *yeast->tolerance : 14;

  std::string strategy = goal.strategy.empty() ? "auto" : goal.strategy;
  if (strategy == "auto") strategy = goal.sweetness == "dry" ? "dry" : (std::fabs(T - abvT) <= 1 ? "tolerance" : "dry");
  double targetOG;
  if (strategy == "tolerance") {
    targetOG = ogForToleranceFinish(sw.target, T);
    say("Strategy: let the yeast stop at its limit. Piatz's formula is OG = 1 + (FG − 1) + (YT − 1), where YT is the yeast's limit from his table (" +
            jsNum(T) + "% → " + sg3(ogForDryABV(T)) + "). For a " + lower(sw.label) + " finish of " + sg3(sw.target) + " (middle of " +
            sweetnessScale(scaleName).name + "'s range), the effective OG is " + sg3(targetOG) + ". Schramm prefers matching yeast and starting gravity this way.",
        "Piatz ch.9; Schramm ch.5");
    if (std::fabs(T - abvT) > 0.25)
      say("That gives about " + toFixed(abv(targetOG, sw.target), 1) + "% ABV rather than " + jsNum(abvT) + "%, because the yeast's limit sets the alcohol.", "Piatz ch.9");
  } else {
    targetOG = ogForDryABV(abvT);
    if (abvT > T + 0.5 && yeast)
      say(yeast->name + " is rated about " + jsNum(T) + "%, below your " + jsNum(abvT) + "% target, so it may stop sweet. Choose a more tolerant yeast or lower the target.", yeast->src);
    say("Strategy: ferment dry. Piatz's table gives OG " + sg3(targetOG) + " for " + jsNum(abvT) + "% when fermented to 1.000." +
            (goal.sweetness != "dry" ? " Then stabilize and sweeten to about " + sg3(sw.target) + ". Schramm notes it's easy to sweeten a too-dry mead and hard to fix a too-sweet one." : std::string()),
        goal.sweetness != "dry" ? "Piatz ch.9; Schramm ch.5" : "Piatz ch.9");
  }
  p.strategy = strategy;
  p.targetOG = targetOG;

  // 3. honey
  Design& d = p.design;
  d.name = goal.name.empty() ? "Planned mead" : goal.name;
  d.kind = "original";
  d.style = fruits.empty() ? "Traditional" : "Melomel";
  d.volumeGal = V;
  d.countFruitWater = true;
  d.sweetness = goal.sweetness;
  d.honeys = {{goal.honeyName.empty() ? "Wildflower" : goal.honeyName, 10}};
  d.fruits = fruits;
  d.yeastId = yeast ? yeast->id : "";
  const SolveResult solved = solveHoney(toEngine(d, db).recipe, targetOG, s);
  if (!solved.reachable || std::isnan(solved.lb)) {
    p.reachable = false;
    say("The target can't be reached in this volume, because the fruit and honey would overflow it. Lower the fruit or ABV, or raise the volume.", "");
  } else {
    // ¼ lb steps for 5 gal and up; finer (0.05 lb) for small batches, where ¼ lb is several gravity points
    const double step = V >= 5 ? 0.25 : 0.05;
    d.honeys[0].lb = std::stod(toFixed(std::round(solved.lb / step) * step, 2));
    const Result res = formulate(toEngine(d, db).recipe, s);
    say("Honey: " + jsNum(d.honeys[0].lb) + " lb gives an effective OG of " + sg3(res.effectiveOG) + " at " + jsNum(res.honeyPPG) +
            " points per lb per gallon (Piatz's chart and Schramm's rule). Fruit adds " + toFixed(res.volumes.fruitWater, 2) +
            " gal of water (weight × water% ÷ 8.33) and " + toFixed(res.points.fruit, 0) + " points (sugar lb × 46). Using Piatz's before-fruit method, pour in about " + toFixed(std::max(0.0, res.volumes.waterToAdd), 2) +
            " gal of water (all the water) and stir in the honey; together they fill to about " + toFixed(res.volumes.preFruit, 2) + " gal and read " + sg3(res.mixingOG) + ".",
        "Piatz ch.9; Schramm ch.5");
    if (std::max(res.mixingOG, res.primaryOG) >= 1.14)
      say("The must starts at " + sg3(res.mixingOG) + ", above the 1.140 where Schramm says some strains struggle to start. Choose a high-gravity yeast, or hold back some honey and feed it as fermentation slows.", "Schramm ch.5");
    if (strategy == "dry" && goal.sweetness != "dry") {
      // Sweeten from where the yeast actually stops: 1.000 if it can ferment dry, higher if it reaches its limit first.
      const double from = std::max(C::DRY_FG, res.fg.central);
      if (from >= sw.target - 0.0005) {
        say("The yeast stops at its limit around " + sg3(from) + ", which is already " + lower(sweetnessClass(from, scaleName)) +
                ", so no honey is needed afterwards. Taste it when it's finished; you can still sweeten a little more.",
            "Piatz ch.9");
      } else {
        const Backsweeten bs = backsweeten(V, from, sw.target, s.honeyPPG, s.honeyLbPerGal);
        d.backsweeten = PlannedBacksweeten{sw.target, bs.lb};
        say("Backsweetening: about " + toFixed(bs.lb, 1) + " lb of honey (" + jsNum(res.honeyPPG) + "-point rule) takes " + jsNum(V) + " gal from " + sg3(from) +
                " to " + sg3(sw.target) +
                (from > C::DRY_FG + 0.0005 ? " (it starts from " + sg3(from) + ", not 1.000, because the yeast stops at its limit with some sugar left)" : std::string()) +
                ". Schramm adds sorbate (½ tsp/gal), waits a day, then adds honey a cup at a time, tasting as he goes. Piatz wants sulfite of at least 40 ppm with sorbate.",
            "Schramm ch.5; Piatz ch.8");
      }
    }
  }
  say("Nutrients: rehydrate the yeast with Go-Ferm, then add Piatz's Fermaid K + DAP mix in staggered doses. See the Designer.", "Piatz ch.4–5");
  return p;
}

// ---------------------------------------------------------------- yeast suggestions and ratings
namespace {
bool hasDarkFruit(const Goal& g, const Database& db) {
  static const std::regex dark = icase("cherr|plum|raspberr|blackberr|blueberr|currant|grape|elderberr|pomegranate|boysenberr|marionberr|chokeberr|aronia|mulberr");
  for (const auto& gf : g.fruits) if (const FruitInfo* f = db.fruit(gf.fruitId)) if (std::regex_search(f->name, dark)) return true;
  return false;
}
}  // namespace

namespace {
struct Flavour { int score; std::string mark, text; };
// What the books' notes say about a strain, and how that suits the fruit in the goal.
Flavour flavourFor(const YeastInfo& y, bool hasFruit, bool dark, bool malic, const std::string& sweetness) {
  YeastInfo::Notes n = y.notes;
  if (!n.ready) { YeastInfo copy = y; readYeastNotes(copy); n = copy.notes; }
  const std::string who = y.src.rfind("Schramm", 0) == 0 ? "Schramm" : "Piatz";
  const std::string desc = y.comments.empty() ? ""
                           : y.notesParaphrased ? " (" + who + "'s notes, summarized: " + y.comments + ")"
                                                : " (" + who + ": \"" + y.comments + "\")";
  Flavour f{0, "✓", ""};
  if (n.stripsFruit) f = {-2, "*", "Works against fruit: it removes fruity character" + desc};
  else if (n.red && dark) f = {n.fruity ? 3 : 2, "✓", "A red-wine strain, which suits dark fruit" + desc};
  else if (n.red && hasFruit) f = {-1, "*", "A red-wine strain, better suited to dark fruit than to this" + desc};
  else if (n.strong) f = {-1, "*", "Strong yeast character that can cover delicate fruit" + desc};
  else if (n.fruity) f = {2, "✓", (hasFruit ? "Fruit-forward, good with fruit" : "Aromatic, good for showing off the honey") + desc};
  else if (n.neutral) f = {0, "✓", "Neutral: adds little of its own, so the honey and fruit carry the flavour" + desc};
  else f = {0, "✓", y.comments.empty() ? std::string("No flavour notes in the books") : "Mostly a practical strain" + desc};
  if (malic && n.malic) { f.score += 1; f.text += "; softens malic acid, which Piatz says helps high-malic fruit"; }
  if (sweetness == "dry" && n.residual) { f.score -= 1; f.mark = "*"; f.text += "; tends not to ferment fully dry"; }
  return f;
}
}  // namespace

void readYeastNotes(YeastInfo& y) {
  const std::string notes = y.comments + " " + y.schramm;
  static const std::regex strips = icase("remove color and fruity");
  static const std::regex red = icase("red wine|bordeaux|burgundy|pinot noir|cabernet|merlot|chianti|zinfandel|beaujolais|assmanshausen|french red|chateau red|pasteur red");
  static const std::regex fruity = icase("fruit|ester|floral|aroma|fragrant|riesling|gew");
  static const std::regex neutral = icase("neutral|crisp and dry|clean");
  static const std::regex strong = icase("dominating|strong yeast profile");
  static const std::regex residual = icase("residual sugar|not to ferment to dryness");
  static const std::regex malicRe = icase("malic");
  YeastInfo::Notes& n = y.notes;
  n.stripsFruit = std::regex_search(notes, strips);
  n.red = std::regex_search(y.comments + " " + y.common + " " + y.code, red);
  n.fruity = std::regex_search(notes, fruity);
  n.neutral = std::regex_search(notes, neutral);
  n.strong = std::regex_search(notes, strong);
  n.residual = std::regex_search(notes, residual);
  n.malic = std::regex_search(notes, malicRe);
  n.ready = true;
}

namespace {
double bookBonus(const YeastInfo& y) {
  if (y.code == "71B-1122") return 1.0;                       // Piatz's go-to; Schramm's preferred for melomels
  if (y.code == "ICV D47" || y.code == "ICV-K1 (V 1116)") return 0.5;   // both discussed by Schramm
  return 0;
}
}  // namespace

std::vector<YeastFit> rateYeasts(const Goal& g, const Database& db) {
  const bool dark = hasDarkFruit(g, db);
  const bool hasFruit = !g.fruits.empty();
  bool malic = false;
  for (const auto& gf : g.fruits) if (const FruitInfo* f = db.fruit(gf.fruitId)) if (f->acid.find("Malic") != std::string::npos) malic = true;
  std::vector<YeastFit> out;
  for (const auto& y : db.yeasts) {
    YeastFit r;
    r.yeast = &y;
    const Flavour fl = flavourFor(y, hasFruit, dark, malic, g.sweetness);
    r.flavourMark = fl.mark;
    r.flavour = fl.text;
    r.score = fl.score + bookBonus(y);
    bool reaches = false;
    if (!y.tolerance) {
      r.strengthMark = "?";
      r.strength = "No tolerance figure in the books (\"" + y.toleranceRaw + "\"), so the app can't predict where it stops";
    } else {
      const double tol = *y.tolerance;
      reaches = tol >= g.abv - 0.5;
      if (reaches) {
        r.strengthMark = "✓";
        r.strength = "Rated " + jsNum(tol) + "%, reaches " + jsNum(g.abv) + "%" + (tol >= g.abv + 4 ? " with plenty to spare (it will ferment everything dry)" : "");
      } else if (g.sweetness == "dry") {
        r.strengthMark = "✗";
        r.strength = "Stops around " + jsNum(tol) + "%, so it can't make a dry " + jsNum(g.abv) + "%";
      } else {
        r.strengthMark = "*";
        r.strength = "Stops around " + jsNum(tol) + "%, so you'd get about " + jsNum(tol) + "% and a sweeter finish than planned";
      }
    }
    std::vector<std::string> extra;
    if (y.code == "71B-1122") extra.push_back("Piatz's go-to strain and Schramm's preferred yeast for fruit meads");
    if (y.nitrogen == "high") extra.push_back("needs extra nutrients (high nitrogen need)");
    if (y.form == "liquid") extra.push_back("liquid yeast: the brew-day steps here are written for dry packets");
    for (size_t i = 0; i < extra.size(); ++i) r.extra += (i ? "; " : "") + extra[i];
    if (!r.extra.empty()) { r.extra[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r.extra[0]))); r.extra += "."; }

    if (r.strengthMark == "?") { r.mark = "?"; r.label = "Unknown strength"; r.rank = 4; }
    else if (r.strengthMark == "✗") { r.mark = "✗"; r.label = "Not suitable"; r.rank = 3; }
    else if (r.strengthMark == "*" || fl.score < 0 || fl.mark == "*") { r.mark = "*"; r.label = "Works, with a catch"; r.rank = 2; }
    else { r.mark = "✓"; r.label = "Good fit"; r.rank = 1; }
    out.push_back(r);
  }
  // ★: best flavour + book standing among dry yeasts that reach the target; closest strength breaks ties.
  YeastFit* star = nullptr;
  for (auto& r : out) {
    if (r.rank != 1 || r.yeast->form != "dry") continue;
    if (!star || r.score > star->score + 1e-9 ||
        (std::fabs(r.score - star->score) < 1e-9 && *r.yeast->tolerance < *star->yeast->tolerance)) star = &r;
  }
  if (star) { star->mark = "★"; star->label = "Suggested"; star->rank = 0; }
  // Best flavour match regardless of strength (dry yeasts).
  YeastFit* bestF = nullptr;
  for (auto& r : out)
    if (r.yeast->form == "dry" && r.yeast->tolerance && (!bestF || r.score > bestF->score + 1e-9)) bestF = &r;
  if (bestF) {
    bestF->bestFlavour = true;
    if (bestF != star)
      bestF->label += bestF->strengthMark == "✓" ? " · also a top flavour match"
                                                 : " · best flavour match, but stops around " + jsNum(*bestF->yeast->tolerance) + "%";
  }
  std::stable_sort(out.begin(), out.end(), [&](const YeastFit& a, const YeastFit& b) {
    // ★ first, then the best flavour match (even if it can't reach the strength), then the rest.
    const int ka = a.rank == 0 ? 0 : a.bestFlavour ? 1 : 2 + a.rank, kb = b.rank == 0 ? 0 : b.bestFlavour ? 1 : 2 + b.rank;
    if (ka != kb) return ka < kb;
    if (std::fabs(a.score - b.score) > 1e-9) return a.score > b.score;
    const double ta = a.yeast->tolerance ? std::fabs(*a.yeast->tolerance - g.abv) : 99, tb = b.yeast->tolerance ? std::fabs(*b.yeast->tolerance - g.abv) : 99;
    return ta < tb;
  });
  return out;
}

const YeastInfo* suggestYeast(const Goal& g, const Database& db) {
  const auto rated = rateYeasts(g, db);
  for (const auto& r : rated) if (r.mark == "★") return r.yeast;
  // Nothing dry reaches the target: the strongest dry yeast without a flavour catch.
  const YeastInfo* best = nullptr;
  for (const auto& r : rated)
    if (r.yeast->form == "dry" && r.yeast->tolerance && r.flavourMark == "✓" && (!best || *r.yeast->tolerance > *best->tolerance)) best = r.yeast;
  return best;
}

// ---------------------------------------------------------------- fitting a plan to the equipment
double primaryContentsGal(const Design& d, const Result& r, const Database& db) {
  (void)db;
  double fruitGal = 0;
  for (const auto& f : d.fruits) if (f.phase == Phase::Primary) fruitGal += f.lb / C::WATER_LB_PER_GAL;
  return r.volumes.preFruit + fruitGal;
}

namespace {
double laterFruitGal(const Design& d) {
  double g = 0;
  for (const auto& f : d.fruits) if (f.phase == Phase::Secondary) g += f.lb / C::WATER_LB_PER_GAL;
  for (const auto& j : d.juices) if (j.phase == Phase::Secondary) g += j.gal;
  return g;
}
}  // namespace

CitrusYield citrusYield(const std::string& fruitName, double lb, double batchGal) {
  const bool lemon = lower(fruitName).find("lemon") != std::string::npos;
  CitrusYield y;
  if (lb > 0) {
    y.juiceTbspLo = lb * 8;                    // ½ cup per pound
    y.juiceTbspHi = lb * (lemon ? 10 : 12);    // ⅝ cup (lemons) / ¾ cup (limes) per pound
    y.zestTbspLo = lb * (lemon ? 4 : 8.0 / 3);
    y.zestTbspHi = lb * (lemon ? 5 : 14.0 / 3);
  }
  y.startJuiceTbsp = batchGal * 2;                          // one fruit's juice per gallon
  y.startZestTbsp = batchGal * (lemon ? 1.0 : 2.0 / 3);     // one fruit's zest per gallon
  if (lb > 0) {
    y.startJuiceTbsp = std::min(y.startJuiceTbsp, y.juiceTbspLo);
    y.startZestTbsp = std::min(y.startZestTbsp, y.zestTbspLo);
  }
  return y;
}

std::string kitchenMeasure(double tbsp) {
  static const char* frac[] = {"", "¼", "½", "¾"};
  if (tbsp < 0.99) {
    const long tsp = std::max(1L, std::lround(tbsp * 3));
    return std::to_string(tsp) + " tsp";
  }
  if (tbsp < 6) {
    const long halves = std::lround(tbsp * 2);
    return (halves / 2 ? std::to_string(halves / 2) : std::string()) + (halves % 2 ? "½" : "") + " tbsp";
  }
  const long quarters = std::max(1L, std::lround(tbsp / 4));   // 16 tbsp per cup → quarter cups
  const long whole = quarters / 4, part = quarters % 4;
  return (whole ? std::to_string(whole) : std::string()) + frac[part] + (quarters > 4 ? " cups" : " cup");
}

std::string kitchenRange(double lo, double hi) {
  if (hi < 16) return std::to_string(std::lround(lo)) + "–" + std::to_string(std::lround(hi)) + " tbsp";
  const std::string a = kitchenMeasure(lo), b = kitchenMeasure(hi);
  const auto unit = [](const std::string& s) { return s.substr(0, s.find(' ')); };
  return unit(a) + "–" + unit(b) + " cups";
}

double fillFraction(const Goal& goal) { return goal.fill == "three-quarters" ? 0.75 : 2.0 / 3.0; }
std::string fillWords(const Goal& goal) { return goal.fill == "three-quarters" ? "three-quarters" : "two-thirds"; }

FittedPlan planForEquipment(const Goal& goal, const Database& db, const Settings& s, const std::string& scaleName, double fermenterGal) {
  FittedPlan out;
  Goal g = goal;
  auto result = [&](const Plan& pl) { return formulate(toEngine(pl.design, db).recipe, s); };
  // What the yeast starts in: with fruit in from brew day, the must plus that fruit; otherwise the honey must.
  auto startOf = [&](const Plan& pl) {
    const Result r = result(pl);
    const bool fruitIn = std::any_of(pl.design.fruits.begin(), pl.design.fruits.end(), [](const DesignFruit& f) { return f.phase == Phase::Primary; });
    return fruitIn ? r.primaryOG : r.mixingOG;
  };

  // A0. Lemons and limes aren't fermented as fruit: the zest is a spice (Piatz) and the juice is an acid addition,
  //     which Schramm says to make only after fermentation is complete. Neither book gives amounts.
  struct Citrus { std::string name; double lb; };
  std::vector<Citrus> citrus;
  for (auto it = g.fruits.begin(); it != g.fruits.end();) {
    const FruitInfo* f = db.fruit(it->fruitId);
    if (f && matches(f->name, LEMON_LIME())) {
      std::string base = f->name.substr(0, f->name.find(" ("));
      citrus.push_back({base, it->haveLb ? *it->haveLb : 0});
      it = g.fruits.erase(it);
    } else {
      ++it;
    }
  }

  // A. Fruit timing (only fruit left on "Recommend"; acidic fruit always goes in later).
  std::vector<std::string> early, late;
  for (auto& gf : g.fruits) {
    const FruitInfo* f = db.fruit(gf.fruitId);
    if (!f || (gf.phase != "auto" && !gf.phase.empty())) continue;
    if (matches(f->name, LEMON_LIME()) || matches(f->name, HIGH_ACID())) { gf.phase = "secondary"; late.push_back(lower(f->name)); continue; }
    gf.phase = g.fruitTiming == "schramm" ? "secondary" : "primary";
    (gf.phase == "primary" ? early : late).push_back(lower(f->name));
  }
  auto listOf = [](const std::vector<std::string>& xs) {
    std::string t;
    for (size_t i = 0; i < xs.size(); ++i) t += (i ? (i + 1 == xs.size() ? " and " : ", ") : "") + xs[i];
    return t;
  };
  if (!early.empty())
    out.fit.push_back({"The " + listOf(early) + " go in on brew day, in a mesh bag. That's Piatz's way: the fruit gives the yeast nutrients and melds from day one, and there's no risk of restarting fermentation weeks later. (Schramm prefers adding fruit after 2–4 weeks for the freshest fruit character; you can switch under \"Fruit timing\".)",
                       "Piatz ch.5; Schramm ch.8"});
  if (!late.empty() && g.fruitTiming == "schramm")
    out.fit.push_back({"The " + listOf(late) + " go in later, after 2–4 weeks once fermentation slows (Schramm's way, for the truest fruit character).", "Schramm ch.8, ch.12"});
  else if (!late.empty())
    out.fit.push_back({"The " + listOf(late) + " go in later, after 2–4 weeks once fermentation slows. They're very acidic (Schramm: lemons and limes average about 5% acid), and acid slows the yeast, so they wait until fermentation is mostly done.",
                       "Schramm ch.5, ch.8"});

  // A2. One yeast picker for everything: "Suggest one for me" uses the same ★ the Yeast list shows.
  if (goal.yeastId.empty()) {
    const auto rated = rateYeasts(g, db);
    const YeastFit* star = nullptr;
    const YeastFit* bestF = nullptr;
    for (const auto& r : rated) {
      if (r.mark == "★" && !star) star = &r;
      if (r.bestFlavour && !bestF) bestF = &r;
    }
    const YeastInfo* pick = star ? star->yeast : suggestYeast(g, db);
    if (pick) {
      g.yeastId = pick->id;
      auto shortFlavour = [](const std::string& f) { return f.substr(0, f.find(" (")); };
      std::string why = "Yeast: " + pick->name + " (★). ";
      if (star) why += "Strength: " + lower(star->strength.substr(0, 1)) + star->strength.substr(1) + ". Flavour: " + lower(shortFlavour(star->flavour).substr(0, 1)) + shortFlavour(star->flavour).substr(1) + ".";
      if (star && !star->extra.empty()) why += " " + star->extra;
      if (bestF && bestF->yeast != pick && bestF->strengthMark != "✓" && bestF->yeast->tolerance)
        why += " " + bestF->yeast->name + " is the best flavour match for this fruit" + (bestF->yeast->code == "71B-1122" ? " (Piatz's go-to and Schramm's preferred yeast for fruit meads)" : "") +
               ", but it stops around " + jsNum(*bestF->yeast->tolerance) + "%. To use it, press \"Use " + bestF->yeast->code + " at " + jsNum(*bestF->yeast->tolerance) +
               "%\" under Yeast options.";
      out.fit.push_back({why, "Piatz ch.4; Schramm ch.5"});
    }
  } else {
    // Your own choice: same format, so the plan always explains its yeast.
    // A3. Don't aim past the yeast's limit: more honey than it can finish only makes a thicker start and a sweeter,
    //     less predictable finish. (Not for "Yeast stops sweet", which uses the limit on purpose.)
    if (const YeastInfo* y = db.yeast(g.yeastId); y && y->tolerance && *y->tolerance > 0 && g.abv > *y->tolerance + 0.5 && g.strategy != "tolerance") {
      out.fit.push_back({y->name + " stops around " + jsNum(*y->tolerance) + "%, so the plan aims for " + jsNum(*y->tolerance) + "% instead of " + jsNum(g.abv) +
                             "%. Asking for more would only add honey the yeast can't finish: a thicker start (harder on the yeast) and leftover sugar you didn't plan. "
                             "For " + jsNum(g.abv) + "%, choose a yeast with ✓ for strength in the Yeast list.",
                         "Piatz ch.4, ch.9"});
      g.abv = *y->tolerance;
    }

    for (const auto& r : rateYeasts(g, db)) {
      if (r.yeast->id != goal.yeastId) continue;
      std::string why = "Yeast: " + r.yeast->name + " (your choice, " + r.mark + " " + lower(r.label.substr(0, r.label.find(" ·"))) + "). Strength: " +
                        lower(r.strength.substr(0, 1)) + r.strength.substr(1) + ". Flavour: " + lower(r.flavour.substr(0, 1)) + r.flavour.substr(1, r.flavour.find(" (") - 1) + ".";
      if (!r.extra.empty()) why += " " + r.extra;
      out.fit.push_back({why, "Piatz ch.4; Schramm ch.5"});
      break;
    }
  }

  Plan p = plan(g, db, s, scaleName);

  // B. Too strong a start for "yeast stops sweet" → ferment dry, then sweeten (only if the strategy was left on auto).
  //    Checked again after the batch is resized for the fermenter, since a smaller batch changes the start.
  bool switchedToDry = false;
  auto checkStrongStart = [&]() {
    if (switchedToDry) return;
    const double start = startOf(p);
    if (!(p.reachable && start >= 1.14 && (goal.strategy.empty() || goal.strategy == "auto") && p.strategy == "tolerance")) return;
    Goal g2 = g;
    g2.strategy = "dry";
    const Plan p2 = plan(g2, db, s, scaleName);
    const double start2 = startOf(p2);
    if (!(p2.reachable && start2 < start - 0.0005)) return;
    out.fit.push_back({"To finish sweet by letting the yeast stop at its limit, fermentation would start at " + sg3(start) +
                           ", above the 1.140 where Schramm says some yeasts struggle to start. So the plan ferments to " + jsNum(g.abv) +
                           "% dry (starting at about " + sg3(start2) + ") and you sweeten it afterwards with honey. Schramm: it's easy to sweeten a too-dry mead and hard to fix a too-sweet one. To keep the other approach, pick it under \"How to reach the sweetness\" and feed the honey 1–2 lb at a time as fermentation slows.",
                       "Schramm ch.5"});
    g = g2;
    p = p2;
    switchedToDry = true;
  };
  checkStrongStart();

  // C. Fermenter: Piatz fills the primary only half to two-thirds full at most (room for foam when stirring).
  //    You can allow three-quarters instead, for more mead from the same bucket.
  out.fermenterGal = fermenterGal;
  double contents = primaryContentsGal(p.design, result(p), db);
  const bool fuller = goal.fill == "three-quarters";
  const std::string fillW = fillWords(goal);
  const std::string fillWhy = fuller ? " You chose to allow three-quarters full, more than Piatz's two-thirds, so stir gently and keep a tray under the bucket in case it foams over."
                                     : "";
  if (fermenterGal > 0) {
    const double limit = fermenterGal * fillFraction(goal);
    out.fillLimitGal = limit;
    if (contents > limit + 1e-9) {
      // Find the biggest batch that fits, to 0.1 gal. A tiny batch can't hold the fruit and honey, so this isn't a
      // simple bisection: step down a whole gallon at a time until one fits, then come back up in tenths.
      auto fitsAt = [&](long tenths) {
        Goal t = g;
        t.volumeGal = static_cast<double>(tenths) / 10;
        const Plan tp = plan(t, db, s, scaleName);
        return tp.reachable && primaryContentsGal(tp.design, result(tp), db) <= limit;
      };
      const long top = static_cast<long>(std::floor(g.volumeGal * 10 + 1e-9));
      long found = 0;
      for (long tenths = top; tenths >= 5; tenths -= 10) {
        if (!fitsAt(tenths)) continue;
        found = tenths;
        while (found + 1 <= top && found + 1 < tenths + 10 && fitsAt(found + 1)) ++found;
        break;
      }
      if (found == 0)   // the whole-gallon steps can jump past a small window that fits: check every tenth
        for (long tenths = top; tenths >= 5 && found == 0; --tenths) if (fitsAt(tenths)) found = tenths;
      double lo = static_cast<double>(found) / 10;
      const double newV = std::floor(lo * 10 + 1e-9) / 10;   // to the nearest 0.1 gal below the limit
      if (newV >= 0.5) {
        Goal t = g;
        t.volumeGal = newV;
        const Plan tp = plan(t, db, s, scaleName);
        const double c2 = primaryContentsGal(tp.design, result(tp), db);
        std::string text = "Your " + jsNum(fermenterGal) + "-gal fermenter: Piatz fills his primary only half to two-thirds full at most, to leave room for the foam when you stir." + fillWhy + " " +
                           std::string(fuller ? "Three-quarters" : "Two-thirds") + " is " + toFixed(limit, 1) + " gal. A " + jsNum(g.volumeGal) + "-gal batch would put about " + toFixed(contents, 1) +
                           " gal in it on brew day (honey must plus fruit), so the plan makes " + jsNum(newV) + " gal instead (about " + toFixed(c2, 1) + " gal in the fermenter).";
        // Would adding the fruit later keep the full batch size?
        if (!early.empty() && g.fruitTiming != "schramm") {
          Goal alt = g;
          alt.fruitTiming = "schramm";
          for (auto& gf : alt.fruits) if (std::find(early.begin(), early.end(), lower(db.fruit(gf.fruitId) ? db.fruit(gf.fruitId)->name : "")) != early.end()) gf.phase = "secondary";
          const Plan ap = plan(alt, db, s, scaleName);
          const double ac = primaryContentsGal(ap.design, result(ap), db);
          if (ap.reachable && ac <= limit)
            text += " Or keep " + jsNum(g.volumeGal) + " gal by adding the " + listOf(early) + " later (Schramm's way): brew day would be about " + toFixed(ac, 1) +
                    " gal, and the fruit goes into the same bucket after 2–4 weeks.";
        }
        out.fit.push_back({text, "Piatz ch.9"});
        g = t;
        p = tp;
        contents = c2;
      } else {
        out.fits = false;
        out.fit.push_back({"Even a small batch with this much fruit won't fit your " + jsNum(fermenterGal) +
                               "-gal fermenter at " + fillW + " full (Piatz's limit for foam room is two-thirds). Use less fruit, add it later, or use a bigger fermenter.",
                           "Piatz ch.9"});
      }
    } else {
      out.fit.push_back({"Fits your " + jsNum(fermenterGal) + "-gal fermenter: about " + toFixed(contents, 1) + " gal on brew day, under " + fillW + " full (" + toFixed(limit, 1) +
                             " gal). Piatz fills no more than two-thirds to leave room for the foam when you stir." +
                             (fuller && contents > fermenterGal * 2.0 / 3.0 + 1e-9 ? fillWhy : std::string()),
                         "Piatz ch.9"});
    }
  }
  checkStrongStart();   // the resized batch may start stronger
  contents = primaryContentsGal(p.design, result(p), db);
  out.primaryGal = contents;

  // C2. Still a strong start (e.g. lots of fruit added later, or a very strong target).
  {
    const double start = startOf(p);
    if (p.reachable && start >= 1.14) {
      const bool laterFruit = std::any_of(p.design.fruits.begin(), p.design.fruits.end(), [](const DesignFruit& f) { return f.phase == Phase::Secondary; });
      out.fit.push_back({"Fermentation starts at about " + sg3(start) + ", above the 1.140 where Schramm says some yeasts struggle. Choose a yeast known for high gravity, or hold back part of the honey and stir it in 1–2 lb at a time as fermentation slows (Schramm describes this)." +
                             std::string(laterFruit ? " Adding the fruit on brew day instead (Piatz's way) also lowers the start, because its water goes in from day one." : ""),
                         "Schramm ch.5"});
    }
  }

  // C3. Brew-day fruit only thins the must as fast as its juice comes out. Piatz's numbers assume it does from day one.
  {
    const Result r = result(p);
    const bool fruitIn = std::any_of(p.design.fruits.begin(), p.design.fruits.end(), [](const DesignFruit& f) { return f.phase == Phase::Primary; });
    if (p.reachable && fruitIn && r.mixingOG >= 1.14 && r.primaryOG < 1.14)
      out.fit.push_back({"The honey water alone reads about " + sg3(r.mixingOG) + " before the fruit goes in. The plan's start of " + sg3(r.primaryOG) +
                             " counts on the fruit's juice mixing in from day one, as Piatz's recipes do (his Strawberry–Banana–Ginger mixes to 1.254 before fruit). "
                             "Freeze and thaw the fruit, or crush it, so the juice comes out quickly. Whole pieces in a bag give it up slowly, and the yeast would start "
                             "closer to " + sg3(r.mixingOG) + ", above the 1.140 where Schramm says yeasts struggle.",
                         "Piatz ch.10; Schramm ch.5"});
  }

  // D. Room for fruit added later.
  out.laterGal = laterFruitGal(p.design);
  if (out.laterGal > 0.01) {
    const Result r = result(p);
    const double total = r.volumes.preFruit + (contents - r.volumes.preFruit) + out.laterGal;
    std::string t = "When you add the later fruit (around day 21), the vessel holds about " + toFixed(contents, 1) + " gal of mead plus about " +
                    toFixed(out.laterGal, 1) + " gal of fruit, about " + toFixed(total, 1) + " gal.";
    if (fermenterGal > 0)
      t += total <= fermenterGal * 0.9 ? " That fits in your " + jsNum(fermenterGal) + "-gal fermenter."
                                      : " That's close to full for your " + jsNum(fermenterGal) + "-gal fermenter; use a bigger vessel for this step, or less fruit.";
    out.fit.push_back({t, ""});
  }
  p.design.style = "";   // style is worked out from the ingredients (see detectStyle)

  // F. The fruit alone is already stronger than the target: no honey can bring it down.
  {
    const Result r = result(p);
    double honey = 0;
    for (const auto& h : p.design.honeys) honey += h.lb;
    if (p.reachable && honey <= 0.001 && r.effectiveOG > p.targetOG + 0.001) {
      out.fits = false;
      out.fit.push_back({"The fruit alone brings this batch to about " + sg3(r.effectiveOG) + ", above the " + sg3(p.targetOG) +
                             " the plan needs, so it uses no honey at all and will come out stronger (or sweeter) than you asked. Use less fruit or make a bigger batch.",
                         "Piatz ch.9"});
    }
  }

  // F2. The fruit's own juice is more than the whole batch: nothing is left for honey and water.
  if (out.fits && p.reachable) {
    const Result r = result(p);
    if (r.volumes.preFruit < 0.05 || r.volumes.waterToAdd < -0.02) {
      out.fits = false;
      out.fit.push_back({"The fruit alone holds about " + toFixed(r.volumes.fruitWater, 2) + " gal of juice, which leaves no room for honey and water in a " +
                             jsNum(p.design.volumeGal) + "-gal batch (Piatz counts the fruit's water as part of the batch). Use less fruit or make a bigger batch.",
                         "Piatz ch.9"});
    }
  }

  // G. So much fruit that the honey and water before it would have to be thicker than any must in the books.
  //    Piatz's heaviest is 1.254 before fruit (Strawberry–Banana–Ginger, ch.10); honey itself is only about 1.42.
  if (out.fits && p.reachable) {
    const Result r = result(p);
    if (r.mixingOG > MAX_MIXING_OG + 1e-9) {
      out.fits = false;
      std::string t = "The fruit takes up so much of the batch that the honey and water before it would have to be mixed to about " + sg3(r.mixingOG) +
                      " in only " + toFixed(r.volumes.preFruit, 2) + " gal. That can't be mixed: it's thicker than any must in the books (Piatz's heaviest is " +
                      sg3(MAX_MIXING_OG) + " before fruit, in his Strawberry–Banana–Ginger) and close to honey itself (about 1.42).";
      double bigger = 0;
      for (long halves = static_cast<long>(std::ceil(p.design.volumeGal * 2 + 1e-9)) + 1; halves <= static_cast<long>(p.design.volumeGal * 8 + 1e-9) && bigger == 0; ++halves) {
        const double v = static_cast<double>(halves) / 2;
        Goal g2 = goal;
        g2.volumeGal = v;
        const Plan p2 = plan(g2, db, s, scaleName);
        if (p2.reachable && formulate(toEngine(p2.design, db).recipe, s).mixingOG <= MAX_MIXING_OG + 1e-9) bigger = v;
      }
      t += bigger > 0 ? " Use less fruit, or make about " + jsNum(bigger) + " gal or more with the same fruit." : " Use less fruit.";
      out.fit.push_back({t, "Piatz ch.10"});
    }
  }

  // E. Lemons / limes → zest at racking, juice after fermentation (as adjuncts; they don't change the gravity math).
  for (const auto& c : citrus) {
    std::string singular = c.name;
    if (!singular.empty() && singular.back() == 's') singular.pop_back();
    const std::string from = c.lb > 0 ? " from " + jsNum(c.lb) + " lb " + lower(c.name) : "";
    const CitrusYield cy = citrusYield(c.name, c.lb, p.design.volumeGal);
    const std::string zestStart = kitchenMeasure(cy.startZestTbsp), juiceStart = kitchenMeasure(cy.startJuiceTbsp);
    p.design.adjuncts.push_back({singular + " zest" + from + " (no white pith)", zestStart + " to start", "secondary"});
    p.design.adjuncts.push_back({singular + " juice" + from, juiceStart + " to start", "aging"});
    const double acidGPerL = cy.startJuiceTbsp * 14.79 * 0.05 / (p.design.volumeGal * C::GAL_L);
    std::string t = "The " + lower(c.name) + " aren't fermented as fruit. Their juice is acid: Schramm says lemons and limes average about 5% acid and strongly advises adding acid only after fermentation is complete, so all the juice waits until the mead has finished. The zest is treated as a spice (Piatz counts citrus peel as one) and goes in when you move the mead to the carboy. Neither book gives amounts, so these are the app's starting points (kitchen yields and the homebrewers' rule of about one " +
                    lower(singular) + " per gallon): start with " + zestStart + " of zest and, after fermentation, " + juiceStart + " of juice (about " + toFixed(acidGPerL, 1) +
                    " g of acid per litre). Then taste: try a measured amount in a small sample (Piatz's bench-trial method) and scale up. For more zest flavour later, use a vodka tincture.";
    if (c.lb > 0)
      t += " Your " + jsNum(c.lb) + " lb should give about " + kitchenRange(cy.zestTbspLo, cy.zestTbspHi) + " of zest and " +
           kitchenRange(cy.juiceTbspLo, cy.juiceTbspHi) + " of juice, more than you'll likely need. You don't have to use it all: the juice is for taste, not for the recipe's math.";
    out.fit.push_back({t, "Schramm ch.5, ch.8; Piatz ch.3, ch.8, ch.9"});
  }

  // E2. Spices, herbs, flowers and oak, at the level you chose. Book figures first; where the books give none,
  //     a homebrewers' starting point from the web, named as such. Always a starting point, worked up to taste.
  std::vector<std::string> spiceSeen;
  for (const auto& gs : goal.spices) {
    const SpiceInfo* sp = spiceById(gs.spiceId);
    if (!sp) continue;
    if (std::find(spiceSeen.begin(), spiceSeen.end(), sp->id) != spiceSeen.end()) {
      out.fit.push_back({sp->name + " is listed twice; the plan uses it once, at the first level you chose.", ""});
      continue;
    }
    spiceSeen.push_back(sp->id);
    const std::string level = gs.level == "subtle" || gs.level == "strong" ? gs.level : "noticeable";
    const double V = p.design.volumeGal;
    const std::string amount = spiceMeasure(*sp, spiceAmount(*sp, level, V));
    p.design.adjuncts.push_back({sp->name, amount + " to start", sp->when});
    const SpiceLevel& lv = spiceLevel(*sp, level);
    const std::string whenW = sp->when == "primary" ? "on brew day" : sp->when == "secondary" ? "around day 21, when you move it to the carboy" : "after fermentation has finished";
    std::string other;
    for (const char* l : {"subtle", "noticeable", "strong"})
      if (level != l) other += std::string(other.empty() ? "" : ", ") + l + " " + spiceMeasure(*sp, spiceAmount(*sp, l, V));
    std::string t = sp->name + " (" + level + "): about " + amount + " for " + jsNum(V) + " gal, added " + whenW + ". " + sp->how + " " + sp->note +
                    " Where the figure comes from: " + lv.src + ". Other levels for this batch: " + other +
                    ". This is a good starting point, not a rule: taste it as it goes and work up to your own liking. Piatz: use less than you think, because \"you can always add more, but it is hard to remove excessive spices.\"";
    std::string src = sp->src;
    for (const auto& url : sp->web) {
      const auto a = url.find("//"), b = url.find('/', a + 2);
      const std::string host = url.substr(a + 2, b - a - 2);
      if (src.find(host) == std::string::npos) src += "; web: " + host;
    }
    out.fit.push_back({t, src});
  }

  out.plan = p;
  return out;
}

// ---------------------------------------------------------------- edits
std::optional<Design> withHoneyForOG(const Design& d, const Database& db, double og, const Settings& s) {
  const SolveResult sv = solveHoney(toEngine(d, db).recipe, og, s);
  if (!std::isfinite(sv.lb)) return std::nullopt;
  Design out = d;
  if (out.honeys.empty()) out.honeys.push_back({"Honey", 0});
  for (size_t i = 0; i < out.honeys.size() && i < sv.honeys.size(); ++i) out.honeys[i].lb = std::stod(toFixed(sv.honeys[i].lb, 2));
  return out;
}

Design withFruitAmount(const Design& d, const Database& db, size_t index, double lb, bool holdOG, const Settings& s) {
  Design out = d;
  if (index >= out.fruits.size()) return out;
  const double keepOG = formulate(toEngine(d, db).recipe, s).effectiveOG;
  out.fruits[index].lb = lb;
  if (holdOG && !out.honeys.empty()) {
    const SolveResult sv = solveHoney(toEngine(out, db).recipe, keepOG, s);
    if (std::isfinite(sv.lb))
      for (size_t k = 0; k < out.honeys.size() && k < sv.honeys.size(); ++k) out.honeys[k].lb = std::stod(toFixed(sv.honeys[k].lb, 2));
  }
  return out;
}

Design withBacksweetening(const Design& d, double toFG, double lb, const std::string& scaleName) {
  Design out = d;
  out.adjuncts.erase(std::remove_if(out.adjuncts.begin(), out.adjuncts.end(),
                                    [](const DesignAdjunct& a) { return a.name.rfind("Honey for backsweetening", 0) == 0; }),
                     out.adjuncts.end());
  out.adjuncts.push_back({"Honey for backsweetening to " + sg3(toFG) + " (after stabilizing)", wt(lb), "bottling"});
  out.backsweeten = PlannedBacksweeten{toFG, lb};
  out.targetFG = toFG;
  out.sweetness = sweetKeyFor(toFG, scaleName);
  return out;
}

}  // namespace mead
