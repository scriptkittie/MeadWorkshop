#include "mead_words.hpp"

#include <algorithm>
#include <cctype>

#include "mead_advisor.hpp"

namespace mead::words {

namespace {

const char* const READINGS = "Readings and numbers";
const char* const INGREDIENTS = "Ingredients and additives";
const char* const STEPS = "Steps";
const char* const EQUIPMENT = "Equipment";
const char* const STYLES = "Styles and strengths";

std::vector<Entry> build()
{
  std::vector<Entry> g = {
      // ---- readings and numbers ----
      {"sg", "SG (specific gravity)", "Sugar reading", READINGS,
       "How heavy the liquid is compared with plain water, which reads 1.000. Dissolved sugar makes it heavier, so a higher number means more sugar. "
       "Alcohol is lighter than water, so a finished dry mead can read a little below 1.000. You read it with a hydrometer.",
       "Piatz ch.9", {"specific gravity", "gravity", "SG"}},
      {"points", "Gravity points", "Sugar points", READINGS,
       "The digits after the \"1.\" in a reading: 1.105 is 105 points. They add up simply, which is why the books use them: "
       "a pound of honey adds about 40 points to one gallon, so 12 lb in 5 gal is about 12 × 40 ÷ 5 = 96 points, a reading of about 1.096.",
       "Piatz ch.9; Schramm ch.5", {"points", "gravity points"}},
      {"ppg", "Points per pound per gallon (PPG)", "How much sugar a pound adds", READINGS,
       "How many gravity points one pound of an ingredient adds to one gallon. The books use 40 for honey and about 46 for table sugar. "
       "You can switch honey between 38, 40 and 42 in Settings.",
       "Piatz ch.9; Schramm ch.5", {"points per lb per gallon", "points/lb/gal", "PPG"}},
      {"og", "OG (original gravity)", "Starting sugar reading", READINGS,
       "The sugar reading before the yeast starts eating. The more sugar at the start, the more alcohol it can make. "
       "Schramm notes that above about 1.140 some yeasts struggle to get going.",
       "Piatz ch.9; Schramm ch.5", {"original gravity", "starting gravity", "OG"}},
      {"mixingOG", "Mixing OG", "Brew-day reading (before fruit)", READINGS,
       "The reading of just the honey and water, before any whole fruit goes in. This is the number you check with the hydrometer on brew day, "
       "because a hydrometer can't read fruit pieces. The fruit's sugar and juice come in on top of it.",
       "Piatz ch.9", {"mixing OG"}},
      {"effOG", "Effective OG", "Starting sugar, counting the fruit", READINGS,
       "What the whole batch would read if all the fruit's sugar were dissolved in it. The app uses this number to predict the alcohol, "
       "because the yeast eats the fruit sugar too.",
       "Piatz ch.9", {"effective OG"}},
      {"fg", "FG (final gravity)", "Ending sugar reading", READINGS,
       "The reading when fermentation has stopped. Around 1.000 or below is dry (no sweetness left); higher means sugar is left, so it tastes sweeter. "
       "Where each sweetness level starts depends on the scale chosen in Settings (Piatz's or Schramm's).",
       "Piatz ch.2, ch.9", {"final gravity", "FG"}},
      {"abv", "ABV (alcohol by volume)", "Alcohol %", READINGS,
       "How much of the mead is alcohol, as a percentage. Piatz works it out from the readings: ABV = 131 × (OG − FG). "
       "Example: 1.110 down to 1.010 is 131 × 0.100 = 13.1%.",
       "Piatz ch.9", {"alcohol by volume", "ABV"}},
      {"tolerance", "Alcohol tolerance (YT)", "How strong the yeast can go", READINGS,
       "The alcohol level where a yeast gives up and stops. A yeast rated 14% stops around 14% even if sugar is left, which leaves the mead sweet. "
       "Piatz turns the rating into a number of gravity points the yeast can eat (his table); that is the YT in the math.",
       "Piatz ch.4, ch.9", {"tolerance", "YT"}},
      {"dry", "Dry, semisweet, sweet, dessert", "How sweet it ends", READINGS,
       "Sweetness is judged by the ending sugar reading (FG). Dry tastes of no sugar; sweet and dessert meads keep a lot. "
       "Piatz and Schramm draw the lines in slightly different places; the Settings box shows both.",
       "Piatz ch.2; Schramm ch.5", {"semisweet", "dessert"}},
      {"fruitWater", "Fruit water", "Juice inside the fruit", READINGS,
       "Fruit is mostly water. Piatz counts that juice as part of the batch, so the more fruit you add, the less plain water you need. "
       "The app works it out as pounds × water% ÷ 8.33 (a gallon of water weighs 8.33 lb).",
       "Piatz ch.9", {"fruit water"}},

      // ---- ingredients and additives ----
      {"must", "Must", "Unfermented honey water", INGREDIENTS,
       "The mix of honey, water (and any fruit) before it becomes mead. It's called must until the yeast has done its work.",
       "Piatz ch.5", {"honey must", "the must"}},
      {"nutrient", "Yeast nutrient", "Yeast food", INGREDIENTS,
       "Honey has very little of the nitrogen and vitamins yeast needs, so meads ferment slowly and can smell bad without help. "
       "Nutrient feeds the yeast. Piatz uses Fermaid K plus DAP in small doses; Schramm gives his own amounts.",
       "Piatz ch.4–5; Schramm ch.3, ch.5", {"nutrient", "nutrients"}},
      {"goferm", "Go-Ferm", "Yeast wake-up nutrient", INGREDIENTS,
       "A powder you stir into the warm water before adding dry yeast. It gives the yeast what it needs to rebuild after drying, "
       "so it starts strong. Lallemand's rate is 1.25 × the yeast's weight.",
       "Piatz ch.4–5", {"Go-Ferm"}},
      {"fermaidk", "Fermaid K", "Complete yeast food", INGREDIENTS,
       "A yeast nutrient with nitrogen, vitamins and minerals. Piatz mixes it with DAP and adds the mix in several small doses.",
       "Piatz ch.4–5", {"Fermaid K"}},
      {"dap", "DAP (diammonium phosphate)", "Nitrogen yeast food", INGREDIENTS,
       "A simple nitrogen salt the yeast uses to grow. Too much at once, or late in fermentation, can leave a harsh taste, which is why it goes in small doses early.",
       "Piatz ch.4–5", {"diammonium phosphate", "DAP"}},
      {"campden", "Campden tablet (potassium metabisulfite)", "Sulfite tablet", INGREDIENTS,
       "A small tablet that releases sulfite. One tablet clears the chlorine from tap water for brewing; at bottling time, sulfite together with "
       "sorbate stops the yeast before you sweeten. Piatz wants at least 40 ppm of sulfite with sorbate.",
       "Piatz ch.5, ch.8; Schramm ch.5", {"Campden", "metabisulfite", "sulfite"}},
      {"sorbate", "Potassium sorbate", "Yeast stopper", INGREDIENTS,
       "Keeps yeast from multiplying, so it can't restart and eat honey you add to sweeten. It doesn't kill yeast that's busy fermenting, "
       "so add it only when fermentation is finished. Schramm uses about ½ tsp per gallon.",
       "Schramm ch.5; Piatz ch.8", {"sorbate"}},
      {"starsan", "Star San", "No-rinse sanitizer", INGREDIENTS,
       "An acid sanitizer: mix 1 oz in 5 gal of water, wet everything that touches the mead, and let it drain. No rinsing needed; the foam is harmless.",
       "Piatz ch.5", {"Star San", "sanitizer", "sanitize"}},
      {"zest", "Zest", "Coloured outer peel", INGREDIENTS,
       "The thin coloured layer of a citrus peel, where the oils and aroma are. Leave the white pith underneath behind: it's bitter. "
       "Piatz counts citrus peel as a spice.",
       "Piatz ch.3", {"zest", "pith"}},
      {"acid", "Acid", "Tartness", INGREDIENTS,
       "What makes lemons and limes taste sour. A little brightens a mead, but acid slows the yeast, so Schramm adds acidic fruit and juice "
       "only after fermentation is mostly done. Lemons and limes are about 5% acid.",
       "Schramm ch.5, ch.8", {"acidic", "acidity"}},
      {"tincture", "Tincture", "Flavour soaked into vodka", INGREDIENTS,
       "Spice, zest or herbs steeped in a little vodka for a few days. You add it a few drops at a time to fine-tune the flavour at the end.",
       "Piatz ch.8", {"tincture"}},
      {"meshbag", "Mesh bag", "Straining bag for fruit", EQUIPMENT,
       "A fine nylon bag that holds the fruit in the fermenter, so you can lift it out at racking instead of fishing for pulp.",
       "Piatz ch.5", {"mesh bag"}},
      {"water", "Dechlorinated water", "Water with the chlorine removed", INGREDIENTS,
       "Chlorine and chloramine in tap water can give medicinal flavours. Use a carbon filter or a Campden tablet, or spring water.",
       "Piatz ch.5", {"dechlorinated"}},

      // ---- steps ----
      {"primary", "Primary (fermentation)", "The first, busy fermentation", STEPS,
       "The first few weeks, from brew day to about day 21, when the yeast eats most of the sugar, foams and bubbles hard. "
       "It happens in the big bucket (the primary fermenter).",
       "Piatz ch.5", {"primary", "primary fermentation"}},
      {"secondary", "Secondary", "The quieter second stage", STEPS,
       "After racking into a carboy, the mead finishes fermenting slowly and starts to clear. Fruit \"in secondary\" means it goes in at this point "
       "(around day 21) instead of on brew day.",
       "Piatz ch.5; Schramm ch.8", {"secondary"}},
      {"pitch", "Pitch", "Add the yeast", STEPS,
       "Adding the yeast to the must. Dry yeast is first rehydrated in warm water (about 104°F) with Go-Ferm for 15–20 minutes.",
       "Piatz ch.5", {"pitch", "pitching"}},
      {"rehydrate", "Rehydrate", "Wake the dry yeast", STEPS,
       "Stirring dry yeast into warm water (about 104°F) with Go-Ferm and waiting 15–20 minutes before adding it to the must. "
       "It gives the yeast a much stronger start than sprinkling it in dry.",
       "Piatz ch.4–5", {"rehydrate", "rehydrated", "rehydrating"}},
      {"sna", "Staggered nutrient additions", "Feeding the yeast in small doses", STEPS,
       "Instead of all the nutrient at once, Piatz splits it into several small doses over the first days, ending at the 30% point. "
       "The yeast gets food when it can use it.",
       "Piatz ch.5", {"nutrient dose", "staggered"}},
      {"break", "The 30% point (⅓ sugar break)", "When a third of the sugar is gone", STEPS,
       "The moment the yeast has eaten about 30% of the starting sugar, which you spot with the hydrometer. Piatz's last nutrient dose goes in here; "
       "later, the yeast can't use it well.",
       "Piatz ch.5", {"30% point", "sugar break"}},
      {"degas", "Degassing", "Stirring out the bubbles", STEPS,
       "Stirring hard to release carbon dioxide during the first week. It gives the yeast air and helps it work. "
       "Add nutrient after stirring, not before, or the powder can make it foam over.",
       "Piatz ch.5", {"degas", "degassing"}},
      {"rack", "Racking", "Moving the mead off the sediment", STEPS,
       "Siphoning the mead into a clean container, leaving the sludge at the bottom behind. Piatz racks to a carboy around day 21.",
       "Piatz ch.5; Schramm ch.3", {"rack", "racking", "racked"}},
      {"lees", "Lees", "Sediment", STEPS,
       "The layer of dead yeast and fruit bits that settles at the bottom. Left too long, it can add off flavours, which is why you rack off it.",
       "Piatz ch.5", {"lees", "sediment"}},
      {"stabilize", "Stabilizing", "Stopping the yeast for good", STEPS,
       "Adding sorbate and sulfite once fermentation is finished, so the yeast can't start again. You must do this before sweetening, "
       "or the yeast will eat the new honey and can blow corks or burst bottles.",
       "Schramm ch.5; Piatz ch.8", {"stabilize", "stabilizing", "stabilized"}},
      {"backsweeten", "Backsweetening", "Sweetening at the end", STEPS,
       "Fermenting dry, stabilizing, then stirring in honey to taste. Schramm: it's easy to sweeten a too-dry mead and hard to fix a too-sweet one.",
       "Schramm ch.5", {"backsweeten", "backsweetening", "sweeten afterwards"}},
      {"bench", "Bench trial", "Taste test in a small sample", STEPS,
       "Before adding acid, sweetness or spice to the whole batch, try measured amounts in a small glass, taste, then scale up what worked.",
       "Piatz ch.8", {"bench trial", "bench-trial"}},
      {"aging", "Bulk aging", "Letting it mellow in the carboy", STEPS,
       "Leaving the clear mead in the carboy for months before bottling. Harsh edges soften and flavours blend.",
       "Piatz ch.5", {"bulk age", "bulk aging", "aging"}},
      {"wild", "Wild fermentation", "Fermenting with no added yeast", STEPS,
       "Raw honey and water left to ferment with the yeasts already around (Katz's method). Less predictable, so the app can't forecast the numbers.",
       "Katz", {"wild fermentation"}},

      // ---- equipment ----
      {"hydrometer", "Hydrometer", "Sugar-reading float", EQUIPMENT,
       "A glass float you drop into a sample: the more sugar, the higher it floats. Read the number at the liquid's surface. "
       "Readings are calibrated for about 60°F.",
       "Piatz ch.5, ch.9", {"hydrometer"}},
      {"fermenter", "Primary fermenter", "Big bucket for the first weeks", EQUIPMENT,
       "A food-grade bucket with a lid. Piatz fills it only half to two-thirds full at most, to leave room for foam when you stir.",
       "Piatz ch.9", {"fermenter", "bucket"}},
      {"carboy", "Carboy", "Narrow-necked jug", EQUIPMENT,
       "A glass or plastic jug with a narrow neck, used after racking. Fill it close to the neck so little air touches the mead.",
       "Piatz ch.5", {"carboy"}},
      {"airlock", "Airlock", "One-way vent", EQUIPMENT,
       "A small water-filled valve in the lid or stopper: gas gets out, air and bugs don't. Bubbles through it show fermentation is going.",
       "Piatz ch.5", {"airlock"}},
      {"headspace", "Headspace", "Empty air space at the top", EQUIPMENT,
       "The gap between the liquid and the lid. In the bucket you want plenty (for foam); in the carboy you want very little (air spoils mead over time).",
       "Piatz ch.5, ch.9", {"headspace"}},
  };

  // ---- styles (Piatz ch.2, following the BJCP) and strengths ----
  for (const auto& st : styles()) {
    Entry e;
    e.key = "style-" + st.key;
    e.term = st.name;
    const auto paren = st.name.find(" (");
    e.plain = paren == std::string::npos ? st.name : st.name.substr(paren + 2, st.name.size() - paren - 3);
    if (st.key == "traditional") e.plain = "Honey-only mead";
    else if (st.key == "braggot") e.plain = "Honey and malt mead";
    else if (st.key == "historical") e.plain = "Old or regional mead";
    else if (st.key == "experimental") e.plain = "Anything else";
    e.group = STYLES;
    e.what = st.plain;
    e.src = "Piatz ch.2";
    std::string word = paren == std::string::npos ? st.name : st.name.substr(0, paren);
    e.match = {word};
    if (word == "Fruit and spice mead") e.match.push_back("fruit and spice");
    g.push_back(e);
  }
  g.push_back({"hydromel", "Hydromel", "Light mead (3.5–7.5%)", STYLES,
               "A light, low-alcohol mead, about 3.5–7.5% ABV. Quick to make and ready to drink sooner.", "Piatz ch.2", {"hydromel"}});
  g.push_back({"standard", "Standard strength", "Wine-strength mead (7.5–14%)", STYLES,
               "The usual mead strength, about 7.5–14% ABV, similar to wine.", "Piatz ch.2", {"standard strength"}});
  g.push_back({"sack", "Sack", "Strong mead (14–18%)", STYLES,
               "A strong mead, about 14–18% ABV. Needs a tough yeast and patience; it takes longer to smooth out.", "Piatz ch.2", {"sack"}});
  return g;
}

bool isWordChar(unsigned char c) { return std::isalnum(c) || c == '_'; }
bool allCaps(const std::string& w)
{
  bool letter = false;
  for (unsigned char c : w) {
    if (std::islower(c)) return false;
    if (std::isupper(c)) letter = true;
  }
  return letter;
}
bool sameAt(const std::string& t, size_t i, const std::string& w, bool exactCase)
{
  if (i + w.size() > t.size()) return false;
  for (size_t k = 0; k < w.size(); ++k) {
    const unsigned char a = static_cast<unsigned char>(t[i + k]), b = static_cast<unsigned char>(w[k]);
    if (exactCase ? a != b : std::tolower(a) != std::tolower(b)) return false;
  }
  return true;
}
std::string escape(const std::string& s)
{
  std::string o;
  for (char c : s) {
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '\n') o += "<br>";
    else o += c;
  }
  return o;
}

}  // namespace

const std::vector<Entry>& glossary()
{
  static const std::vector<Entry> g = build();
  return g;
}
const std::vector<std::string>& groups()
{
  static const std::vector<std::string> v = {READINGS, STEPS, INGREDIENTS, EQUIPMENT, STYLES};
  return v;
}
const Entry* find(const std::string& key)
{
  for (const auto& e : glossary()) if (e.key == key) return &e;
  return nullptr;
}
std::string linkTerms(const std::string& text)
{
  struct Hit { size_t at, len; const Entry* e; };
  // Every term's match words, indexed by their first letter (lower case), built once.
  struct Word { std::string w; bool exact; const Entry* e; };
  static const std::vector<std::vector<Word>> byFirst = [] {
    std::vector<std::vector<Word>> idx(256);
    for (const auto& e : glossary())
      for (const auto& w : e.match)
        if (!w.empty()) idx[static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(w[0])))].push_back({w, allCaps(w), &e});
    return idx;
  }();
  std::vector<Hit> hits;   // every whole-word mention of every term, found in one pass
  for (size_t i = 0; i < text.size(); ++i) {
    if (i > 0 && isWordChar(static_cast<unsigned char>(text[i - 1]))) continue;   // only at the start of a word
    for (const auto& c : byFirst[static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(text[i])))]) {
      if (!sameAt(text, i, c.w, c.exact)) continue;
      const size_t end = i + c.w.size();
      if (end == text.size() || !isWordChar(static_cast<unsigned char>(text[end]))) hits.push_back({i, c.w.size(), c.e});
    }
  }
  // Earliest first; at the same spot the longer phrase wins ("effective OG" over "OG").
  // Keep a mention only if it doesn't overlap one already kept and its term isn't linked yet.
  std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.at != b.at ? a.at < b.at : a.len > b.len; });
  std::vector<Hit> kept;
  size_t end = 0;
  for (const auto& h : hits) {
    if (h.at < end) continue;
    if (std::any_of(kept.begin(), kept.end(), [&](const Hit& k) { return k.e == h.e; })) continue;
    kept.push_back(h);
    end = h.at + h.len;
  }
  std::string out;
  size_t pos = 0;
  for (const auto& h : kept) {
    out += escape(text.substr(pos, h.at - pos));
    out += "<a href=\"term:" + h.e->key + "\">" + escape(text.substr(h.at, h.len)) + "</a>";
    pos = h.at + h.len;
  }
  out += escape(text.substr(pos));
  return out;
}

std::string stripLinks(const std::string& s)
{
  std::string o;
  for (size_t i = 0; i < s.size();) {
    if (s.compare(i, 4, "<br>") == 0) { o += '\n'; i += 4; }
    else if (s[i] == '<') { const size_t j = s.find('>', i); i = j == std::string::npos ? s.size() : j + 1; }
    else if (s.compare(i, 5, "&amp;") == 0) { o += '&'; i += 5; }
    else if (s.compare(i, 4, "&lt;") == 0) { o += '<'; i += 4; }
    else if (s.compare(i, 4, "&gt;") == 0) { o += '>'; i += 4; }
    else o += s[i++];
  }
  return o;
}

}  // namespace mead::words
