#include "mead_spices.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "mead_advisor.hpp"

namespace mead {

namespace {

const char* const GOTMEAD_SPICING = "https://gotmead.com/community/threads/spicing-a-mead-question.19941/latest";
const char* const GOTMEAD_METHEGLIN = "https://gotmead.com/community/threads/metheglin-questions.19635/latest";

std::vector<SpiceInfo> table();
std::vector<SpiceInfo> build()
{
  std::vector<SpiceInfo> t = table();
  for (auto& s : t) {
    while (!s.how.empty() && s.how.back() == ' ') s.how.pop_back();
    for (size_t i; (i = s.how.find("  ")) != std::string::npos;) s.how.erase(i, 1);
  }
  return t;
}
std::vector<SpiceInfo> table()
{
  const std::string bag = "In a sanitized mesh bag or tea bag, so you can pull it out when it tastes right. ";
  const std::string vodka = "Piatz sanitizes spices with a quick spray or soak in cheap vodka. ";
  return {
      {"ginger", "Ginger root, thin slices", "oz", "",
       {1.5, "Schramm ch.10: 1–2 oz gives subtle but pronounced character in 5 gal"},
       {7.5, "Schramm ch.10: ginger lovers use 1–2 oz per gallon"},
       {16, "Schramm ch.10: some use up to a pound in 5 gal"},
       "primary",
       "Slice it thin (no need to peel, Piatz says) and put it in a sanitized mesh bag in the fermenter from brew day; it comes out when you rack. " + vodka,
       "Raw ginger is intense. Piatz's Strawberry–Banana–Ginger uses ½ lb in 5 gal, and his award-winning ginger mead, \"an adult ginger ale\", used over 3 lb.",
       "Schramm ch.10; Piatz ch.3, ch.10", {}},
      {"cinnamon", "Cinnamon sticks, 3-inch", "each", "stick",
       {2, "Schramm ch.12: two 3-inch sticks in his Dry Nutmeg Metheglin (5 gal)"},
       {2.5, "web: half a stick per gallon is \"perceptible but not overpowering\""},
       {5, "web: one stick per gallon is \"definitely there\""},
       "secondary",
       bag + "Taste every week; cinnamon also adds tannin. " + vodka,
       "Most \"cinnamon\" sold in the US is cassia: bolder and warmer than true (Ceylon) cinnamon, which is softer and fruitier (Schramm). Piatz added cinnamon to a too-sweet cherry mead and the extra tannin improved the balance.",
       "Schramm ch.10, ch.12; Piatz ch.3", {GOTMEAD_SPICING}},
      {"nutmeg", "Nutmeg, freshly ground", "oz", "",
       {1, "Schramm ch.12: a scant 1 oz in his Dry Nutmeg Metheglin (5 gal)"},
       {2, "Schramm ch.10: within his 1–4 oz per 5 gal"},
       {4, "Schramm ch.10: up to 4 oz per 5 gal"},
       "secondary",
       "Grind it fresh, coarsely (powder can haze the mead), and add it in a tea bag. " + vodka,
       "Piatz finds nutmeg works better in a blend of spices than on its own.",
       "Schramm ch.10, ch.12; Piatz ch.3", {}},
      {"cloves", "Whole cloves", "each", "clove",
       {1, "Schramm ch.10: one or two cloves in 5 gal have an appreciable effect"},
       {2, "Schramm ch.10: one or two cloves in 5 gal"},
       {5, "web: about one clove per gallon"},
       "secondary",
       bag + "Cloves are strong and vary with freshness: taste after a week.",
       "Schramm's spiced-cyser tip: 1 tsp each of cinnamon and nutmeg plus three cloves (\"not three teaspoons\") in a muslin bag, hung in the carboy until it tastes right.",
       "Schramm ch.10, ch.8", {GOTMEAD_METHEGLIN, GOTMEAD_SPICING}},
      {"allspice", "Allspice berries, cracked", "each", "berry",
       {5, "web: about five crushed berries per 5 gal (one brewer's trial)"},
       {10, "web: two allspice berries per gallon"},
       {15, "web: three allspice berries per gallon"},
       "secondary",
       "Crack them coarsely rather than using ground allspice. " + bag,
       "Neither book gives an amount for allspice.",
       "Schramm ch.10", {GOTMEAD_METHEGLIN, GOTMEAD_SPICING}},
      {"vanilla", "Vanilla beans, split", "each", "bean",
       {2, "app: below the forum's one bean per gallon"},
       {5, "web: one bean per gallon is a good place to start"},
       {7, "web: up to about seven in 5 gal before it hides the honey"},
       "secondary",
       "Split the beans lengthwise and drop them into the carboy. Taste after a month or two; vanilla fades with time. " + vodka,
       "Schramm: vanilla builds the flavours around it (great with raspberry, cherry and other spices) and is \"not obvious in its presence but notable in its absence\". Neither book gives an amount.",
       "Schramm ch.10", {GOTMEAD_METHEGLIN, "https://gotmead.com/community/threads/adding-vanilla-beans.10343/latest"}},
      {"chiles", "Dried chiles (chipotle, ancho…)", "each", "chile",
       {1, "app: half of Piatz's amount"},
       {2, "Piatz ch.10: two dried peppers in secondary (Raspberry–Hot Pepper, Mesquite–Chipotle)"},
       {4, "Piatz ch.10: four more, steeped in a cup of vodka, added to taste at bottling"},
       "secondary",
       "Toast the dried chiles 20–30 seconds in a hot dry pan for a few brown spots, then add them in a mesh bag. Taste every few days: heat builds quickly. For fine-tuning, Piatz makes a tincture of four chiles in a cup of vodka.",
       "Piatz prefers dried chiles (fewer green, vegetal notes) and warns they easily overwhelm a mead: \"start with much less than you think will be needed.\"",
       "Piatz ch.3, ch.10; Schramm ch.10", {}},
      {"pepper", "Black peppercorns, cracked", "g", "",
       {1, "Schramm ch.10: a gram or two to add complexity"},
       {2, "Schramm ch.10: a gram or two to add complexity"},
       {5, "app: more, as the main spice (Schramm gives no figure for that)"},
       "secondary",
       "Crack the peppercorns coarsely. " + bag,
       "Schramm: pepper can be the defining flavour or just make other flavours bloom; finely ground pepper tastes stronger.",
       "Schramm ch.10", {}},
      {"mint", "Dried mint (spearmint)", "oz", "",
       {2, "Schramm ch.10: start with 2 oz of mint leaves in secondary"},
       {6, "app: between Schramm's starting dose and Piatz's medal-winner"},
       {12, "Piatz ch.10: ¾ lb of dried spearmint in his Spearmint mead (plus ¼ lb as a tincture)"},
       "secondary",
       "Schramm adds mint to the secondary 2 oz at a time and adds 2 oz more after tasting, as many times as needed. Piatz gets more consistent results from dried mint than fresh.",
       "Mint goes well with orange peel, lemon, chocolate and strong honeys (Schramm).",
       "Schramm ch.10; Piatz ch.3, ch.10", {}},
      {"lavender", "Dried lavender blossoms", "oz", "",
       {2, "Schramm ch.10: 2–3 oz at a time in secondary"},
       {3, "Schramm ch.10: 2–3 oz at a time in secondary"},
       {5, "app: a second dose, as Schramm suggests adding more after tasting"},
       "secondary",
       "Add to the secondary in a mesh bag; taste after a week or two before adding more.",
       "Lavender gives a soft, perfumed scent and a gentle blush, not a strong colour (Schramm).",
       "Schramm ch.10; Piatz ch.3", {}},
      {"rose", "Rose petals (edible, unsprayed)", "oz", "",
       {2, "Schramm ch.10: \"Two ounces at a time is a good starting point\""},
       {4, "app: a second 2-oz dose"},
       {6, "app: a third 2-oz dose"},
       "secondary",
       "Rack the mead onto the petals after primary, 2 oz at a time, and adjust upward after tasting.",
       "Use only edible petals: florist roses are usually sprayed (Piatz, Schramm). Rose tea or food-grade rose water also work.",
       "Schramm ch.10; Piatz ch.3", {}},
      {"chamomile", "Dried chamomile flowers", "oz", "",
       {0.5, "app: a light touch, scaled down from a chamomile-wine recipe"},
       {1, "app: scaled down from a chamomile-wine recipe"},
       {1.75, "web: ½ cup (about 10 g) of dried flowers per gallon in a chamomile wine"},
       "secondary",
       bag + "Taste after a week.",
       "Schramm: chamomile's apple, honey, berry, citrus and rose notes may be the best match for mead of any herb. Neither book gives an amount.",
       "Schramm ch.10; Piatz ch.3", {"https://practicalselfreliance.com/chamomile-wine-mead"}},
      {"hibiscus", "Dried hibiscus flowers", "oz", "",
       {1, "app: a light touch below the blog's rate"},
       {2, "app: about half the blog's rate"},
       {3.75, "web: about ¾ oz per gallon (a brewer's blog)"},
       "secondary",
       bag + "It colours the mead deep red quickly; taste after a few days.",
       "Hibiscus is slightly acidic (Piatz). Neither book gives an amount.",
       "Piatz ch.3", {"https://patrickbrianmooney.nfshost.com/~patrick/blogs/XPB/mead/2018/04/08/105.html"}},
      {"anise", "Star anise, cracked", "oz", "",
       {0.25, "app: a quarter of Schramm's ceiling"},
       {0.5, "app: half of Schramm's ceiling"},
       {1, "Schramm ch.10: under 1 oz per 5 gal, just above the tasting threshold"},
       "secondary",
       "Crack the pods just before use. " + bag,
       "Licorice-like; not everyone loves it, so Schramm uses it lightly, often in blends.",
       "Schramm ch.10", {}},
      {"coriander", "Coriander seed, cracked", "tsp", "",
       {1, "app: a light touch below the forum's rate"},
       {2.5, "web: about ½ tsp of cracked seed per gallon"},
       {5, "app: double the forum's rate (Schramm: coriander tolerates high amounts)"},
       "aging",
       "Crack the seeds (don't toast). Schramm adds it late, once the mead is near its peak, because its aroma fades. " + bag,
       "Orange-like and sweet (Schramm); unlike most spices it won't ruin a mead at high amounts.",
       "Schramm ch.10", {"https://gotmead.com/community/threads/coriander.8977/latest"}},
      {"cardamom", "Cardamom pods, cracked", "g", "",
       {3, "app: below the forum's starting point"},
       {7, "web: start with about ¼ oz (7 g) per 5 gal"},
       {14, "app: double the forum's starting point"},
       "secondary",
       "Crack the pods. " + bag + "A few pods alone can be too faint to taste.",
       "Neither book gives an amount for cardamom.",
       "Schramm ch.10", {"https://forum.homebrewersassociation.org/t/cardamom/10846"}},
      {"juniper", "Juniper berries, cracked", "oz", "",
       {0.5, "app: half the forum's figure"},
       {1, "web: about 1 oz per 5 gal"},
       {2.5, "web: ½ oz per gallon is a strong flavour"},
       "secondary",
       "Crack them, but don't crush or grind (Schramm). " + bag,
       "Gin's flavour: sprucy and clean with a little astringency; good with mint (Schramm). Use fresh berries.",
       "Schramm ch.10", {"https://www.homebrewtalk.com/threads/juniper-berries.96860/post-3987049"}},
      {"cocoa", "Cocoa nibs", "oz", "",
       {2, "web: from brewers' tests of 1–5 oz per 5 gal"},
       {5, "web: 5 oz per 5 gal was the favourite in one brewer's tests"},
       {8, "web: 6–8 oz per 5 gal"},
       "secondary",
       "Piatz adds nibs after the vigorous part of fermentation and leaves the floating cocoa fat behind when racking. Soak them in vodka first; taste after 1–2 weeks.",
       "Piatz: \"Cocoa nibs are the best chocolate for mead.\"",
       "Piatz ch.3", {"https://forum.homebrewersassociation.org/t/another-cacoa-nibs-post/15955"}},
      {"coffee", "Coffee (cold brew)", "cup", "",
       {2, "app: under half the forum's rate"},
       {5, "web: one cup of cold-steeped coffee per gallon"},
       {8, "app: more than the forum's rate"},
       "aging",
       "Use cold-steeped coffee, never boiled (Piatz). Stir it in after fermentation, tasting as you go. It adds a little volume.",
       "Piatz: the good coffee meads he has tasted were made with cold-pressed coffee, which avoids harshness.",
       "Piatz ch.3", {"https://www.homebrewtalk.com/threads/coffee-mead.84906/post-916953"}},
      {"oak", "Oak cubes (or chips)", "oz", "",
       {0.5, "web: about 0.1 oz per gallon for long contact"},
       {1, "Schramm ch.12: 1 oz of oak chips for 4–7 days before bottling (Dry Show Mead)"},
       {2, "app: double Schramm's amount; watch it closely"},
       "aging",
       "Chips work in days, cubes in weeks. Taste after a week (chips) or two (cubes), then every week, and pull the oak out when it's right. Sanitize with vodka, not chemicals (Piatz).",
       "Piatz: oak is an art; check often so you don't get too much wood. Toast level changes the flavour: light gives coconut and fresh oak, medium vanilla and caramel.",
       "Schramm ch.6, ch.12; Piatz ch.3", {"https://gotmead.com/community/threads/how-much-oak.17718/latest"}},
      {"orange", "Orange zest (no white pith)", "each", "orange's zest",
       {3, "web: at least 3 oranges' zest in 5 gal (one is very faint)"},
       {6, "web: about 6 oranges' zest in 5 gal"},
       {10, "web: up to 10 oranges' zest in 5 gal"},
       "secondary",
       "Zest only the coloured peel; the white pith is bitter. Add at racking in a mesh bag and taste after a week.",
       "Piatz counts citrus peel as a spice.",
       "Piatz ch.3", {"https://gotmead.com/community/threads/citrus-mead.8317/latest"}},
  };
}

std::string lowerStr(std::string s)
{
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
bool bookSrc(const std::string& src) { return src.rfind("Schramm", 0) == 0 || src.rfind("Piatz", 0) == 0; }

std::string quarters(double v, const char* unitOne, const char* unitMany)   // "1¼ lb", "½ oz"
{
  static const char* frac[] = {"", "¼", "½", "¾"};
  const long q = std::max(1L, std::lround(v * 4));
  const long whole = q / 4, part = q % 4;
  return (whole ? std::to_string(whole) : std::string()) + frac[part] + " " + (q > 4 ? unitMany : unitOne);
}

}  // namespace

bool SpiceInfo::fromBooks() const { return bookSrc(subtle.src) && bookSrc(noticeable.src) && bookSrc(strong.src); }

const std::vector<SpiceInfo>& spices()
{
  static const std::vector<SpiceInfo> list = build();
  return list;
}
const SpiceInfo* spiceById(const std::string& id)
{
  for (const auto& s : spices()) if (s.id == id) return &s;
  return nullptr;
}
const SpiceInfo* spiceForName(const std::string& adjunctName)
{
  const std::string n = lowerStr(adjunctName);
  auto has = [&](const char* w) { return n.find(w) != std::string::npos; };
  if (n.empty() || has("for backsweetening")) return nullptr;
  if (has("peppercorn") || has("black pepper")) return spiceById("pepper");
  if (has("chile") || has("chili") || has("chipotle") || has("ancho") || has("pepper")) return spiceById("chiles");
  if (has("orange") && (has("zest") || has("peel"))) return spiceById("orange");
  if (has("cocoa") || has("cacao") || has("chocolate")) return spiceById("cocoa");
  static const std::vector<std::pair<const char*, const char*>> words = {
      {"ginger", "ginger"}, {"cinnamon", "cinnamon"}, {"nutmeg", "nutmeg"}, {"allspice", "allspice"}, {"clove", "cloves"},
      {"vanilla", "vanilla"}, {"mint", "mint"}, {"lavender", "lavender"}, {"rose", "rose"}, {"chamomile", "chamomile"},
      {"hibiscus", "hibiscus"}, {"anise", "anise"}, {"coriander", "coriander"}, {"cardamom", "cardamom"},
      {"juniper", "juniper"}, {"coffee", "coffee"}};
  for (const auto& w : words) if (has(w.first)) return spiceById(w.second);
  if (n.rfind("oak", 0) == 0 || has(" oak")) return spiceById("oak");   // not "soak"
  return nullptr;
}

const SpiceLevel& spiceLevel(const SpiceInfo& s, const std::string& level)
{
  return level == "subtle" ? s.subtle : level == "strong" ? s.strong : s.noticeable;
}
double spiceAmount(const SpiceInfo& s, const std::string& level, double batchGal)
{
  return spiceLevel(s, level).per5 * std::max(0.0, batchGal) / 5;
}

std::string spiceMeasure(const SpiceInfo& s, double amount)
{
  if (s.unit == "each") {
    const long n = std::max(1L, std::lround(amount));
    std::string w = s.what;
    if (n > 1) {
      const auto sp = w.find(' ');
      std::string head = sp == std::string::npos ? w : w.substr(0, sp), tail = sp == std::string::npos ? "" : w.substr(sp);
      if (head.size() > 2 && head.substr(head.size() - 2) == "'s") w = head.substr(0, head.size() - 2) + "s'" + tail;   // orange's zest → oranges' zest
      else if (!head.empty() && head.back() == 'y') w = head.substr(0, head.size() - 1) + "ies" + tail;                    // berry → berries
      else w = head + "s" + tail;
    }
    return std::to_string(n) + " " + w;
  }
  if (s.unit == "oz") {
    if (amount >= 15.5) return quarters(amount / 16, "lb", "lb");
    if (amount < 0.375) return std::to_string(std::max(1L, std::lround(amount * 28.35))) + " g";
    return quarters(amount, "oz", "oz");
  }
  if (s.unit == "g") return std::to_string(std::max(1L, std::lround(amount))) + " g";
  if (s.unit == "tsp") {
    if (amount >= 3) {
      const long halves = std::lround(amount / 3 * 2);
      return (halves / 2 ? std::to_string(halves / 2) : std::string()) + (halves % 2 ? "½" : "") + " tbsp";
    }
    const long halves = std::max(1L, std::lround(amount * 2));
    return (halves / 2 ? std::to_string(halves / 2) : std::string()) + (halves % 2 ? "½" : "") + " tsp";
  }
  if (s.unit == "cup") return quarters(amount, "cup", "cups");
  return toFixed(amount, 2) + " " + s.unit;
}

std::string spiceGuide(const SpiceInfo& s, double batchGal)
{
  const std::string when = s.when == "primary" ? "brew day" : s.when == "secondary" ? "around day 21, at racking" : "after fermentation";
  std::string t = "For " + toFixed(batchGal, 2) + " gal: subtle " + spiceMeasure(s, spiceAmount(s, "subtle", batchGal)) + " · noticeable " +
                  spiceMeasure(s, spiceAmount(s, "noticeable", batchGal)) + " · strong " + spiceMeasure(s, spiceAmount(s, "strong", batchGal)) +
                  ". Add " + when + ". " + s.how + " These are starting points: taste and work up to your liking. (" + s.src +
                  (s.web.empty() ? "" : "; web figures from homebrewers' forums") + ")";
  return t;
}

}  // namespace mead
