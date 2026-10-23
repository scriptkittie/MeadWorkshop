#include "mead_text.hpp"

#include "mead_spices.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>

namespace mead::text {

// ------------------------------------------------------------------ formats
std::string fixed(double v, int digits) { return toFixed(v, digits); }
std::string wt(double lb) { return fixed(lb, 2) + " lb"; }
std::string vol(double gal) { return fixed(gal, 2) + " gal"; }
std::string grams(double g) { return std::round(g * 10) / 10 < 10 ? fixed(g, 1) + " g" : std::to_string(std::lround(g)) + " g"; }
std::string num(double v)
{
  if (!std::isfinite(v)) return "?";
  char buf[64];
  const auto res = std::to_chars(buf, buf + sizeof buf, v, std::chars_format::general, 6);
  std::string s(buf, res.ptr);
  // to_chars "general" may print 1e+06 as "1e+06" like %g; values here are small.
  return s;
}
std::string lower(std::string s)
{
  for (auto& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}
static std::string join(const std::vector<std::string>& xs, const std::string& sep)
{
  std::string out;
  for (size_t i = 0; i < xs.size(); ++i) out += (i ? sep : "") + xs[i];
  return out;
}
// "%1 … %2" with the arguments in order (like QString::arg for our fixed templates)
static std::string fmt(std::string t, const std::vector<std::string>& args)
{
  std::string out;
  for (size_t i = 0; i < t.size(); ++i) {
    if (t[i] == '%' && i + 1 < t.size() && t[i + 1] >= '1' && t[i + 1] <= '9') {
      const size_t k = static_cast<size_t>(t[i + 1] - '1');
      if (k < args.size()) { out += args[k]; ++i; continue; }
    }
    out += t[i];
  }
  return out;
}

// ------------------------------------------------------------------ dates
static long daysFromCivil(int y, int m, int d)
{
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const long yoe = y - era * 400;
  const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}
static Date civilFromDays(long z)
{
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const long doe = z - era * 146097;
  const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const long y = yoe + era * 400;
  const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const long mp = (5 * doy + 2) / 153;
  const long d = doy - (153 * mp + 2) / 5 + 1;
  const long m = mp + (mp < 10 ? 3 : -9);
  return {static_cast<int>(y + (m <= 2)), static_cast<int>(m), static_cast<int>(d)};
}
std::optional<Date> parseISO(const std::string& s)
{
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') return std::nullopt;
  for (size_t i : {0, 1, 2, 3, 5, 6, 8, 9}) if (s[i] < '0' || s[i] > '9') return std::nullopt;
  const Date d{std::stoi(s.substr(0, 4)), std::stoi(s.substr(5, 2)), std::stoi(s.substr(8, 2))};
  if (d.m < 1 || d.m > 12 || d.d < 1 || d.y < 1) return std::nullopt;
  const Date back = civilFromDays(daysFromCivil(d.y, d.m, d.d));
  if (back.y != d.y || back.m != d.m || back.d != d.d) return std::nullopt;   // 2026-02-30
  return d;
}
Date addDays(const Date& date, int days) { return civilFromDays(daysFromCivil(date.y, date.m, date.d) + days); }
int daysBetween(const Date& a, const Date& b)
{
  return static_cast<int>(daysFromCivil(b.y, b.m, b.d) - daysFromCivil(a.y, a.m, a.d));
}
std::string isoOf(const Date& d)
{
  char buf[16];
  std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", d.y, d.m, d.d);
  return buf;
}
std::string shortLabel(const Date& d)
{
  static const char* wd[] = {"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed"};   // day 0 = 1970-01-01, a Thursday
  static const char* mo[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const long z = daysFromCivil(d.y, d.m, d.d);
  const long w = ((z % 7) + 7) % 7;
  return std::string(wd[w]) + ", " + mo[d.m - 1] + " " + std::to_string(d.d);
}

// ------------------------------------------------------------------ Plan tab
Bottled bottledEstimate(const Design& d, const Result& r)
{
  const double fw = r.volumes.fruitWater, k = d.volumeGal / 5;
  Bottled b{r.volumes.final - 0.20 * fw - 2 * 0.25 * k, r.volumes.final - 0.10 * fw - 2 * 0.15 * k};
  b.lo = std::max(0.0, b.lo);
  b.hi = std::max(b.lo, b.hi);
  return b;
}

std::vector<MoreOption> moreMead(const FittedPlan& fp, const Goal& g, const Database& db, const Settings& s,
                                 const std::string& scaleName, double fermenterGal)
{
  std::vector<MoreOption> out;
  const Design& base = fp.plan.design;
  const double V0 = base.volumeGal;
  if (!fp.plan.reachable || !fp.fits || V0 <= 0) return out;
  const bool anyAll = std::any_of(g.fruits.begin(), g.fruits.end(), [](const GoalFruit& f) { return f.intensity == "all" && f.haveLb; });
  double honey0 = 0;
  for (const auto& h : base.honeys) honey0 += h.lb;
  for (double add : {0.5, 1.0, 1.5}) {
    for (bool moreFruit : {false, true}) {
      if (moreFruit && !anyAll) continue;
      Goal g2 = g;
      g2.volumeGal = V0 + add;
      g2.strategy = fp.plan.strategy;   // same approach as your plan, so only the size changes
      std::vector<std::string> fruitBits;
      for (auto& f : g2.fruits) {
        if (!moreFruit || f.intensity != "all" || !f.haveLb) continue;
        const double was = *f.haveLb;
        f.haveLb = std::ceil(was * (V0 + add) / V0 * 2 - 1e-9) / 2;   // to the half pound
        const FruitInfo* fi = db.fruit(f.fruitId);
        fruitBits.push_back(wt(*f.haveLb) + " " + lower(fi ? fi->name : f.fruitId) + " (" + wt(*f.haveLb - was) + " more)");
      }
      const FittedPlan big = planForEquipment(g2, db, s, scaleName, 0);   // no resizing: we want this size
      if (!big.plan.reachable || !big.fits) continue;
      MoreOption o;
      o.addGal = add;
      o.volumeGal = V0 + add;
      o.moreFruit = moreFruit;
      o.design = big.plan.design;
      const Result rb = formulate(toEngine(o.design, db).recipe, s);
      o.brewDayGal = primaryContentsGal(o.design, rb, db);
      o.fullPct = fermenterGal > 0 ? o.brewDayGal / fermenterGal * 100 : 0;
      o.withinFill = fermenterGal <= 0 || o.brewDayGal <= fermenterGal * fillFraction(g) + 1e-9;
      double honey = 0;
      for (const auto& h : o.design.honeys) honey += h.lb;
      const Bottled b = bottledEstimate(o.design, rb);
      std::string t = "+" + num(add) + " gal (a " + num(o.volumeGal) + "-gal batch): " + wt(honey) + " honey (" + wt(honey - honey0) + " more) and about " +
                      vol(std::max(0.0, rb.volumes.waterToAdd)) + " water";
      t += moreFruit ? ", with " + [&] { std::string j; for (size_t i = 0; i < fruitBits.size(); ++i) j += (i ? " and " : "") + fruitBits[i]; return j; }() +
                           " so the fruit tastes as strong"
                     : (o.design.fruits.empty() ? std::string() : ", same fruit (it will taste a little milder)");
      t += ". Bottles about " + fixed(b.lo, 1) + "–" + fixed(b.hi, 1) + " gal.";
      if (fermenterGal > 0)
        t += " Brew day: " + vol(o.brewDayGal) + " in your " + num(fermenterGal) + "-gal bucket (" + std::to_string(static_cast<int>(std::lround(o.fullPct))) + "% full" +
             (o.withinFill ? ")." : ", over the " + fillWords(g) + " you allow).");
      o.text = t;
      out.push_back(std::move(o));
    }
  }
  return out;
}

PlanText planText(const FittedPlan& fp, const Goal& g, const Result& r, const Database& db,
                  const std::string& scaleName, double fermenterGal, const Settings& s)
{
  const Plan& p = fp.plan;
  PlanText t;
  for (const auto& h : p.design.honeys) t.honeys.push_back({h.name + " honey", wt(h.lb), ""});
  for (const auto& f : p.design.fruits)
    t.fruits.push_back({f.name, wt(f.lb),
                        f.phase == Phase::Secondary ? "later: added around day 21, once fermentation slows (see Directions)"
                                                    : "brew day, in a mesh bag in the fermenter"});
  for (const auto& a : p.design.adjuncts)
    t.extras.push_back({a.name, "",
                        (a.when == "secondary" ? std::string("when you move it to the carboy (~day 21): ")
                         : a.when == "aging"   ? std::string("after fermentation has finished: ")
                         : a.when == "primary" ? std::string("brew day, in a mesh bag: ")
                                               : std::string()) + a.amount});

  t.waterHint = "all the water you pour in. With the honey stirred in, the level comes up to about " + vol(r.volumes.preFruit) + " (that's not more water: the honey takes up the rest). The hydrometer should read " + fixed(r.mixingOG, 3);
  const ProfileWords pw = describeProfile(p.design, r, db, scaleName);
  t.styleWords = pw.style + " · " + pw.strength + " · " + pw.sweetness;
  t.strategy = p.strategy == "tolerance" ? "Let the yeast stop sweet at its limit"
               : g.sweetness == "dry"     ? "Ferment dry"
               : p.design.backsweeten     ? "Ferment dry, then stabilize and backsweeten"
                                          : "Ferment as far as the yeast can go; it stops sweet enough on its own";

  // A plan that can't be made gets no brewing instructions, only the reason.
  if (!p.reachable || !fp.fits) {
    std::string why;
    for (const auto& w : p.why) if (w.text.find("can't be reached") != std::string::npos) why = w.text;
    for (const auto& w : fp.fit)
      if (w.text.find("won't fit") != std::string::npos || w.text.find("fruit alone") != std::string::npos || w.text.find("can't be mixed") != std::string::npos) why = w.text;
    t.problem = "This plan can't be made as asked. " + (why.empty() ? std::string("See the reasons below.") : why);
    t.summary = {t.problem};
    t.yeastLine = p.yeast ? p.yeast->name : "—";
    return t;
  }

  // In short
  double honeyLb = 0;
  for (const auto& h : p.design.honeys) honeyLb += h.lb;
  std::vector<std::string> early, later, atRack, atEnd, brewSpices;
  for (const auto& f : p.design.fruits) (f.phase == Phase::Primary ? early : later).push_back(wt(f.lb) + " " + lower(f.name));
  // In sentences, a short name: "Ginger root, thin slices" → "ginger root"; "Lime zest from 3 lb limes" → "lime zest".
  auto shortName = [](const std::string& n) { return lower(n.substr(0, std::min({n.find(" from "), n.find(", "), n.find(" (")}))); };
  for (const auto& a : p.design.adjuncts)
    (a.when == "secondary" ? atRack : a.when == "primary" ? brewSpices : atEnd).push_back(shortName(a.name) + (a.amount.empty() ? "" : " (" + a.amount + ")"));
  auto& S = t.summary;
  S.push_back("Make " + vol(p.design.volumeGal) + (fermenterGal > 0 ? " in your " + num(fermenterGal) + "-gal fermenter" : "") + ".");
  S.push_back(fmt("Brew day: pour about %2 of water into the bucket and stir in %1 of honey. That's all the water: with the honey in, the level comes up to about %3 (the honey takes up %4). Check that the hydrometer reads %5, adding a splash more water if it reads higher%6.",
                  {wt(honeyLb), vol(std::max(0.0, r.volumes.waterToAdd)), vol(r.volumes.preFruit), vol(r.volumes.honey), fixed(r.mixingOG, 3),
                   (early.empty() ? std::string() : ". Then add " + join(early, " and ") + " in a mesh bag (freeze and thaw, or crush, the fruit first so its juice comes out)") +
                       (brewSpices.empty() ? std::string() : ". Add the " + join(brewSpices, " and ") + " in a mesh bag")}));
  if (!later.empty() || !atRack.empty()) {
    std::vector<std::string> both = later;
    both.insert(both.end(), atRack.begin(), atRack.end());
    S.push_back("Around day 21, when you move it to the carboy: add " + join(both, " and ") + ".");
  }
  S.push_back(p.strategy == "tolerance"
                  ? fmt("The yeast should stop on its own around %1 (%2), at about %3% alcohol.",
                        {fixed(r.fg.central, 3), sweetnessClass(r.fg.central, scaleName), fixed(r.fg.abv, 1)})
              : (r.fg.reachesDry || r.fg.central <= 1.0015)
                  ? fmt("It ferments dry, to about %1% alcohol.", {fixed(r.fg.abv, 1)})
                  : fmt("The yeast should stop around %1 at about %2% alcohol (its limit), so it won't be fully dry; choose a stronger yeast for a dry finish.",
                        {fixed(r.fg.central, 3), fixed(r.fg.abv, 1)}));
  if (!atEnd.empty())
    S.push_back("When fermentation has finished: add " + join(atEnd, " and ") +
                ", then more to taste. Try it in a small measured sample first, then scale up.");
  if (p.design.backsweeten)
    S.push_back(fmt("When it's finished and clear: stabilize, then stir in about %1 of honey to taste (to about %2).",
                    {wt(p.design.backsweeten->lb), fixed(p.design.backsweeten->toFG, 3)}));
  {
    const Bottled b = bottledEstimate(p.design, r);
    t.bottledLo = b.lo;
    t.bottledHi = b.hi;
    t.bottled = "about " + fixed(b.lo, 1) + "–" + fixed(b.hi, 1) + " gal: some juice stays in the fruit pulp and you leave sediment behind when racking (the app's rough estimate, not from the books)";
    S.push_back("Expect to bottle " + t.bottled.substr(0, t.bottled.find(':')) + " of the " + vol(r.volumes.final) + ".");
  }
  for (const auto& mo : moreMead(fp, g, db, s, scaleName, fermenterGal)) t.more.push_back(mo.text);
  for (const auto& yr : rateYeasts(g, db)) {
    if (!yr.bestFlavour || yr.yeast == p.yeast || !yr.yeast->tolerance || yr.strengthMark == "✓") continue;
    S.push_back(fmt("Yeast: %1 (★). %2 is the best flavour match but stops around %3%; press \"Use %4 at %3%\" under Yeast options to plan with it instead.",
                    {p.yeast ? p.yeast->name : "—", yr.yeast->name, num(*yr.yeast->tolerance), yr.yeast->code}));
  }

  t.yeastLine = "—";
  if (p.yeast) {
    const SNA sna = piatzSNA(r.volumes.final, r.effectiveOG, r.fg.central);
    t.yeastLine = fmt("%1 · %2 packet%3 (5 g each; Piatz: one per 5 gal) · rehydrate with %4 Go-Ferm in about %5 mL of water at ~104°F",
                      {p.yeast->name, std::to_string(sna.packets), sna.packets > 1 ? "s" : "", grams(sna.goFerm5g), num(sna.water5gMl)});
  }
  if (p.design.backsweeten)
    t.backsweeten = fmt("%1 honey after stabilizing, to about %2", {wt(p.design.backsweeten->lb), fixed(p.design.backsweeten->toFG, 3)});
  return t;
}

std::string suggestChoiceLabel(const YeastInfo* pick)
{
  return pick ? "Suggest one for me (★ " + pick->vendor + " " + pick->code + ")" : "Suggest one for me";
}
std::string yeastChoiceLabel(const YeastFit& r)
{
  const YeastInfo& y = *r.yeast;
  return r.mark + "  " + y.vendor + " " + y.code + (y.common.empty() ? "" : " – " + y.common) + " (" +
         (y.toleranceRaw.empty() ? "?" : y.toleranceRaw) + ") · " + r.label;
}

std::vector<YeastFit> yeastOptionsShown(const Goal& g, const Database& db, const YeastInfo* chosen)
{
  std::vector<YeastFit> out;
  int shown = 0;
  for (const auto& yr : rateYeasts(g, db)) {
    const bool isChosen = chosen == yr.yeast;
    if (shown >= 4 && !isChosen && !yr.bestFlavour) continue;
    if (!isChosen && !yr.bestFlavour) ++shown;
    out.push_back(yr);
  }
  return out;
}

Design designFromPlan(const FittedPlan& fp, const std::string& brewDateISO)
{
  Design d = fp.plan.design;
  d.brewDate = brewDateISO;
  std::string notes = "Planned from goal. Reasoning:";
  size_t k = 0;
  for (const auto& w : fp.fit) notes += "\n" + std::to_string(++k) + ". " + w.text;
  for (const auto& w : fp.plan.why) notes += "\n" + std::to_string(++k) + ". " + w.text;
  d.notes = notes;
  if (d.backsweeten)
    d.adjuncts.push_back({"Honey for backsweetening to " + toFixed(d.backsweeten->toFG, 3) + " (after stabilizing)",
                          toFixed(d.backsweeten->lb, 2) + " lb", "bottling"});
  return d;
}

// ------------------------------------------------------------------ Designer tab
std::string yeastAmountLine(const SNA& sna)
{
  return fmt("How much: %1 packet%2 (5 g each; Piatz: one per 5 gal), rehydrated with %3 Go-Ferm in about %4 mL of water at ~104°F.",
             {std::to_string(sna.packets), sna.packets > 1 ? "s" : "", grams(sna.goFerm5g), num(sna.water5gMl)});
}
std::string yeastInfo(const YeastInfo& y)
{
  std::string info = y.name + " — tolerance " + (y.toleranceRaw.empty() ? "?" : y.toleranceRaw) + ", " +
                     (y.tempRaw.empty() ? "temp n/a" : y.tempRaw) +
                     (y.nitrogen.empty() ? "" : ", " + y.nitrogen + " nitrogen need") + ". " + y.comments;
  if (!y.schramm.empty()) info += "\n" + y.schramm;
  return info;
}
NutrientText nutrientText(const SNA& n)
{
  NutrientText t;
  for (const auto& x : n.doses) t.doses.push_back({x.when, grams(x.grams) + " (≈" + fixed(x.grams / 2 * 0.75, 2) + " tsp)", ""});
  t.goFerm = fmt("%1 in about %2 oz of water at ~104°F (40°C), 15–20 min (Piatz, for his 4 g packet). Today's Lalvin packets are 5 g, so Lallemand's 1.25 × yeast weight comes to %3 in about %4 mL of water (20 × the Go-Ferm).",
                 {grams(n.goFerm), num(n.rehydrationOz), grams(n.goFerm5g), num(n.water5gMl)});
  t.mix = fmt("Fermaid K %1 + DAP %2 = %3", {grams(n.fermaidK), grams(n.dap), grams(n.mix)});
  t.caveat = fmt("As printed for 5 gal and scaled to your batch, which Piatz says to do. Two things in the book don't line up. Four 2 g doses use %1 of the %2 mix. And his step-by-step adds doses 2–4 on days 3, 5 and 7 instead of at 48 h, 96 h and the 30% point. Piatz says yeasts with high nitrogen needs may need more than the typical dosage.",
                 {grams(n.dosed), grams(n.mix)});
  t.schramm = "Schramm's alternative (ch.3, ch.5): 2 tsp yeast nutrient + 1 tsp energizer per 5 gal, or about 5 g Fermaid K (he gives Lalvin's rate as about 1 g per gallon).";
  t.chemistry = "Chemistry check: Schramm says DAP at 1 g/L adds 258 ppm of nitrogen. DAP is 21.2% nitrogen by weight, so it's actually 212 mg N/L. Piatz's 8 g DAP in 5 gal is about 0.42 g/L, or roughly 90 mg N/L. Lallemand's current Fermaid K spec sheet gives 35–40 g/hL per addition (about 1.3–1.5 g per gallon each time), well above Piatz's 4 g per 5 gal in total. The books' amounts are a light baseline, so follow your product labels when they differ.";
  return t;
}
std::string fruitBarLabel(const Database& db, const std::string& fruitId, double lb, double batchGal)
{
  return wt(lb) + " · " + fruitLevelFor(db, fruitId, lb, batchGal);
}

ExtraNotes designerNotes(const Design& d, const Result& r, const Database& db, double fermenterGal, double fillFraction)
{
  ExtraNotes x;
  if (fermenterGal > 0 && !d.wild) {
    const double contents = primaryContentsGal(d, r, db);
    if (contents > fermenterGal * fillFraction + 1e-9)
      x.overfull = Note{"warn",
                        fmt("About %1 gal goes in your %2-gal fermenter on brew day (honey must plus fruit). Piatz fills his primary only half to two-thirds full at most to leave room for the foam when you stir; the limit you chose is %3 gal. Make a smaller batch, add the fruit later, or use a bigger fermenter.",
                            {fixed(contents, 1), num(fermenterGal), fixed(fermenterGal * fillFraction, 1)}),
                        "Piatz ch.9"};
  }
  if (!d.wild && r.mixingOG > MAX_MIXING_OG + 1e-9 && r.volumes.waterToAdd >= -0.02)
    x.tooThick = Note{"warn",
                      fmt("Before the fruit, the honey and water would have to be mixed to about %1 in %2. That's thicker than any must in the books (Piatz's heaviest is %3 before fruit, in his Strawberry–Banana–Ginger) and close to honey itself (about 1.42), so it can't really be mixed. Use less fruit, or a bigger batch.",
                          {fixed(r.mixingOG, 3), vol(r.volumes.preFruit), fixed(MAX_MIXING_OG, 3)}),
                      "Piatz ch.10"};
  const StyleMatch sm = detectStyle(d, db);
  const StyleInfo* named = styleByName(d.style);
  if (named && named->key != sm.style->key)
    x.styleMismatch = Note{"info",
                           fmt("It's labelled %1, but the ingredients make it a %2: %3. Set Style to Auto to follow the ingredients, or keep your label (book recipes keep their printed style).",
                               {named->name, sm.style->name, sm.because}),
                           "Piatz ch.2"};
  return x;
}

std::string publishedFigures(const json::Value& rec)
{
  std::string figures;
  if (rec["og"].isNumber()) {
    figures = "OG " + fixed(rec["og"].num(), 3) + (rec["ogBasis"].str() == "pre-fruit" ? " (before fruit)" : "");
    if (rec["effOG"].isNumber()) figures += ", effective " + fixed(rec["effOG"].num(), 3);
    if (rec["fg"].isNumber()) figures += ", FG " + fixed(rec["fg"].num(), 3);
    figures += ".";
  }
  return figures;
}
std::string publishedNotes(const json::Value& rec)
{
  std::vector<std::string> bits;
  for (const char* k : {"ogNote", "assumptions", "volumeNote"})
    if (!rec[k].str().empty()) bits.push_back(rec[k].str());
  return join(bits, " ");
}

// ------------------------------------------------------------------ Directions tab
std::string whenWords(const std::string& when)
{
  if (when == "primary") return "brew day";
  if (when == "secondary") return "around day 21, when you move it to the carboy";
  if (when == "aging") return "after fermentation has finished";
  if (when == "bottling") return "at the end, after stabilizing";
  return when;
}

std::vector<ShoppingItem> shoppingList(const Design& d, const Result& r, const EngineInput& eng)
{
  std::vector<ShoppingItem> out;
  auto item = [&out](std::string what, std::string amount, std::string note) { out.push_back({std::move(what), std::move(amount), std::move(note)}); };
  for (const auto& h : d.honeys) if (h.lb > 0) item(h.name.empty() ? std::string("Honey") : h.name + " honey", wt(h.lb), "");
  item("Water (dechlorinated)", vol(std::max(0.0, r.volumes.waterToAdd)),
       "all the water you pour in (about); go by the hydrometer, " + fixed(r.mixingOG, 3) + ". Have a little extra on hand");
  for (size_t i = 0; i < d.fruits.size(); ++i)
    item(eng.recipe.fruits[i].name, wt(d.fruits[i].lb), d.fruits[i].phase == Phase::Secondary ? "added later (secondary)" : "brew day, in a mesh bag");
  for (const auto& j : d.juices)
    item(j.name.empty() ? std::string("Juice") : j.name, vol(j.gal), j.phase == Phase::Secondary ? "added later (secondary)" : "brew day");
  for (const auto& s : d.sugars) item(s.name, wt(s.lb), s.phase == Phase::Secondary ? "added later" : "brew day");
  for (const auto& a : d.adjuncts) {
    if (a.name.empty()) continue;
    // A short quantity ("4.13 lb", "2 tsp") goes in the amount column; an instruction goes with the timing.
    const bool shortAmount = a.amount.size() <= kShortAmount;
    const std::string when = whenWords(a.when);
    std::string note = shortAmount ? when : (when.empty() ? a.amount : when + ": " + a.amount);
    if (!note.empty()) note[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(note[0])));
    item(a.name, shortAmount ? a.amount : std::string(), note);
  }
  if (!d.wild && eng.yeast) {
    const SNA sna = piatzSNA(r.volumes.final, r.effectiveOG, r.fg.central);
    item(eng.yeast->name, std::to_string(sna.packets) + " packet" + (sna.packets > 1 ? "s" : ""), "5 g each");
    item("Go-Ferm", grams(sna.goFerm5g), "for rehydrating the yeast");
    item("Fermaid K", grams(sna.fermaidK), "nutrient");
    item("DAP (diammonium phosphate)", grams(sna.dap), "nutrient");
    item("Campden tablets", "1", "treats up to 20 gal of water");
    item("Star San sanitizer", "1 oz", "per 5 gal of water; no rinsing needed");
  }
  if (d.backsweeten || d.sweetness != "dry")
    item("Potassium sorbate + metabisulfite", "per label",
         d.backsweeten ? "to stabilize before you sweeten at the end (this plan does)" : "only if you sweeten after fermentation");
  return out;
}

std::vector<TimelineEntry> timeline(const Design& d, const Result& r, const EngineInput& eng)
{
  const std::optional<Date> brew = parseISO(d.brewDate);
  const SNA sna = piatzSNA(r.volumes.final, r.effectiveOG, r.fg.central);
  std::vector<ScheduleItem> items;
  for (const auto& x : schedule(d, r, eng, d.wild ? nullptr : &sna)) if (!x.brewDay) items.push_back(x);
  if (!d.wild) {
    const bool laterFruit = std::any_of(eng.recipe.fruits.begin(), eng.recipe.fruits.end(), [](const FruitIn& f) { return f.phase == Phase::Secondary; }) ||
                            std::any_of(eng.recipe.juices.begin(), eng.recipe.juices.end(), [](const JuiceIn& j) { return j.phase == Phase::Secondary; });
    const double finishDay = laterFruit ? 49 : 35;
    for (const auto& a : d.adjuncts) {
      if (a.name.empty() || a.name.rfind("Honey for backsweetening", 0) == 0) continue;
      ScheduleItem x;
      x.title = "Add: " + a.name;
      x.detail = a.amount;
      const SpiceInfo* sp = spiceForName(a.name);
      if (a.when == "secondary") {
        x.day = 21.0;
        x.detail = "When you move it to the carboy: " + a.amount + ". " + (sp ? sp->how + " Taste every week and add more if you want it stronger." : std::string("Piatz sanitizes spices with a quick spray or soak in vodka."));
        x.src = sp ? sp->src : "Piatz ch.3";
      }
      else if (a.when == "aging") {
        x.day = finishDay + 7;
        x.detail = "After fermentation is complete: " + a.amount + ". " +
                   (sp ? sp->how : std::string("Try a measured amount in a small sample first, taste, then scale up to the whole batch."));
        x.src = sp ? sp->src + "; Piatz ch.8" : "Schramm ch.5; Piatz ch.8";
      }
      else if (a.when == "bottling") { x.day = finishDay + 14; x.detail = a.amount; }
      else continue;
      items.push_back(x);
    }
    std::stable_sort(items.begin(), items.end(), [](const ScheduleItem& a, const ScheduleItem& b) {
      return (a.day ? *a.day : 4.5) < (b.day ? *b.day : 4.5);
    });
  }
  std::vector<TimelineEntry> out;
  for (const auto& x : items) {
    TimelineEntry e{"", x.title, x.detail, x.src, x.kind, std::nullopt, std::nullopt};
    if (x.day) {
      const int days = static_cast<int>(std::floor(*x.day + 1e-9));
      e.day = days;
      e.when = days == 0 ? "Brew day" : "Day " + std::to_string(days);
      if (brew) {
        const Date on = addDays(*brew, days);
        e.date = isoOf(on);
        e.when += " · " + shortLabel(on);
      }
    } else {
      e.when = "At " + fixed(x.sg ? *x.sg : 1, 3);
    }
    out.push_back(std::move(e));
  }
  return out;
}

// ------------------------------------------------------------------ the math
std::vector<MathLine> mathLines(const Result& r)
{
  const Volumes& v = r.volumes;
  return {
      {"Honey", fmt("%1 × %2 = %3 points · takes up %4 (%5 lb/gal)", {wt(r.honeyLb), num(r.honeyPPG), fixed(r.points.honey, 0), vol(v.honey), num(r.honeyLbPerGal)})},
      {"Juice", fmt("%1 points (gallons × (SG − 1) × 1000)", {fixed(r.points.juice, 0)})},
      {"Other sugars", fmt("%1 points", {fixed(r.points.sugar, 0)})},
      {"Fruit", fmt("%1 → sugar %2 × 46 = %3 points (FS)", {wt(r.fruitLb), wt(r.fruitSugarLb), fixed(r.points.fruit, 0)})},
      {"Fruit water", fmt("%1 = Σ lb × water% ÷ 8.33 (%2 primary, %3 secondary)", {vol(v.fruitWater), vol(v.fruitWaterPrimary), vol(v.fruitWaterSecondary)})},
      {"Before-fruit size", fmt("NS = %1 − fruit water%2 = %3", {vol(v.final), v.juiceSecondary != 0 ? " − later juice" : "", vol(v.preFruit)})},
      {"Mixing OG", fmt("1 + %1 ÷ %2 gal ÷ 1000 = %3", {fixed(r.points.mix, 0), fixed(v.preFruit, 2), fixed(r.mixingOG, 3)})},
      {"Effective OG", fmt("1 + %1 ÷ %2 gal ÷ 1000 = %3", {fixed(r.points.total, 0), fixed(v.final, 2), fixed(r.effectiveOG, 3)})},
      {"Yeast limit", fmt("%1% → YT %2 (Piatz table), so FG = max(1.000, OG − (YT − 1)) = %3", {num(r.tolerance), fixed(1 + r.fg.ytPoints / 1000, 3), fixed(r.fg.central, 3)})},
      {"ABV", fmt("131 × (%1 − %2) = %3%; if fermented dry %4%", {fixed(r.effectiveOG, 3), fixed(r.fg.central, 3), fixed(r.fg.abv, 1), fixed(r.dryABV, 1)})},
  };
}

// ------------------------------------------------------------------ solvers
std::optional<double> parseNumber(const std::string& raw)
{
  size_t a = 0, b = raw.size();
  while (a < b && (raw[a] == ' ' || raw[a] == '\t')) ++a;
  while (b > a && (raw[b - 1] == ' ' || raw[b - 1] == '\t')) --b;
  std::string t = raw.substr(a, b - a);
  if (t.empty()) return std::nullopt;
  std::replace(t.begin(), t.end(), ',', '.');
  if (t[0] == '+') t.erase(0, 1);
  double v = 0;
  const auto res = std::from_chars(t.data(), t.data() + t.size(), v);
  if (res.ec != std::errc() || res.ptr != t.data() + t.size() || !std::isfinite(v)) return std::nullopt;
  return v;
}
static double honeyTotal(const Design& d)
{
  double t = 0;
  for (const auto& h : d.honeys) t += h.lb;
  return t;
}
Solved solveOG(const Design& d, const Database& db, const std::string& ogText, const Settings& s)
{
  const auto target = parseNumber(ogText);
  if (!target || *target <= 1) return {std::nullopt, "Enter a target OG like 1.115"};
  auto nd = withHoneyForOG(d, db, *target, s);
  if (!nd) return {std::nullopt, "That gravity can't be reached in this volume"};
  return {nd, "Honey set to " + wt(honeyTotal(*nd)) + " for OG " + fixed(*target, 3)};
}
Solved solveABV(const Design& d, const Database& db, const std::string& abvText, const Settings& s, const std::string& scaleName)
{
  const auto abvT = parseNumber(abvText);
  if (!abvT || *abvT <= 0) return {std::nullopt, "Enter a target ABV"};
  const EngineInput eng = toEngine(d, db);
  const SweetLevel& sw = sweetnessByKey(d.sweetness, scaleName);
  const double tol = eng.recipe.yeast.tolerance;
  const bool stopSweet = std::fabs(tol - *abvT) <= 1 && d.sweetness != "dry";
  const double target = stopSweet ? ogForToleranceFinish(sw.target, tol) : ogForDryABV(*abvT);
  auto nd = withHoneyForOG(d, db, target, s);
  if (!nd) return {std::nullopt, "That gravity can't be reached in this volume"};
  return {nd, stopSweet ? "Yeast-stops-sweet strategy: OG " + fixed(target, 3)
                        : "Ferment dry to " + num(*abvT) + "%: OG " + fixed(target, 3) + (nd->sweetness != "dry" ? ", then backsweeten" : "")};
}
Solved applyOption(const Design& d, const Database& db, double og, double fg, const Settings& s, const std::string& scaleName)
{
  auto nd = withHoneyForOG(d, db, og, s);
  if (!nd) return {std::nullopt, "Not reachable in this volume"};
  nd->targetFG = parseNumber(toFixed(fg, 3)).value_or(fg);
  nd->sweetness = sweetKeyFor(*nd->targetFG, scaleName);
  return {nd, "Honey set to " + wt(honeyTotal(*nd)) + " for OG " + fixed(og, 3)};
}

// ------------------------------------------------------------------------------------------------ My recipes
std::string recipeFileStem(const std::string& name)
{
  std::string out;
  bool space = false;
  for (unsigned char c : name) {
    const bool bad = c < 32 || c == 127 || std::string("\\/:*?\"<>|").find(static_cast<char>(c)) != std::string::npos;
    if (bad || c == ' ' || c == '\t') { space = !out.empty(); continue; }   // runs of spaces / bad characters → one space
    if (space) { out += ' '; space = false; }
    out += static_cast<char>(c);
  }
  if (out.size() > 80) {
    size_t cut = 80;
    while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) --cut;   // don't split a UTF-8 letter
    out.resize(cut);
  }
  while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
  std::string base = out.substr(0, out.find('.'));
  for (auto& ch : base) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  while (!base.empty() && base.back() == ' ') base.pop_back();
  static const char* reserved[] = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                   "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
  for (const char* r : reserved)
    if (base == r) { out = "Recipe " + out; break; }
  return out.empty() ? std::string("Untitled recipe") : out;
}

std::string uniqueStem(const std::string& stem, const std::function<bool(const std::string&)>& taken)
{
  if (!taken(stem)) return stem;
  for (int n = 2; n < 10000; ++n) {
    const std::string s = stem + " (" + std::to_string(n) + ")";
    if (!taken(s)) return s;
  }
  return stem + " (copy)";
}

}  // namespace mead::text
