#include "designer.hpp"

#include "mead_text.hpp"
#include "mead_words.hpp"
#include "mead_spices.hpp"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <optional>

using namespace mead;

namespace {

std::string readResource(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll().toStdString();
}

QString qs(const std::string &s) { return QString::fromStdString(s); }
std::string ss(const QString &s) { return s.toStdString(); }
QString fixed(double v, int digits) { return qs(text::fixed(v, digits)); }
QString vol(double gal) { return qs(text::vol(gal)); }
QVariantList rowsToList(const std::vector<text::Row> &rows)
{
    QVariantList l;
    for (const auto &r : rows)
        l.append(QVariantMap{{QStringLiteral("name"), qs(r.name)}, {QStringLiteral("amount"), qs(r.amount)}, {QStringLiteral("when"), qs(r.when)}});
    return l;
}
QVariantMap noteToMap(const Note &n)
{
    return {{QStringLiteral("level"), qs(n.level)}, {QStringLiteral("text"), qs(n.text)}, {QStringLiteral("src"), qs(n.src)}};
}
QString phaseName(Phase p) { return p == Phase::Secondary ? QStringLiteral("secondary") : QStringLiteral("primary"); }
Phase phaseOf(const QVariant &v) { return v.toString() == QStringLiteral("secondary") ? Phase::Secondary : Phase::Primary; }

// A number typed by the user: blank → empty; accepts "1.5" and "1,5"; ignores the system locale.
std::optional<double> number(const QVariant &v)
{
    if (v.typeId() == QMetaType::Double || v.typeId() == QMetaType::Int || v.typeId() == QMetaType::LongLong)
        return v.toDouble();
    return text::parseNumber(ss(v.toString()));
}

QVariantList zonesToList(const std::vector<Zone> &zones)
{
    QVariantList out;
    for (const auto &z : zones)
        out.append(QVariantMap{{QStringLiteral("from"), z.from}, {QStringLiteral("to"), z.to},
                               {QStringLiteral("label"), qs(z.label)}, {QStringLiteral("shade"), z.shade}});
    return out;
}

QVariantList optionsToList(const std::vector<SweetOption> &opts)
{
    QVariantList out;
    for (const auto &o : opts) {
        QVariantMap m{{QStringLiteral("kind"), qs(o.kind)}, {QStringLiteral("title"), qs(o.title)},
                      {QStringLiteral("text"), qs(o.text)}, {QStringLiteral("src"), qs(o.src)},
                      {QStringLiteral("reachable"), o.reachable}};
        if (o.applyOG) m[QStringLiteral("applyOG")] = *o.applyOG;
        if (o.backsweetenLb) m[QStringLiteral("backsweetenLb")] = *o.backsweetenLb;
        if (o.honeyLb) m[QStringLiteral("honeyLb")] = *o.honeyLb;
        out.append(m);
    }
    return out;
}

Design defaultDesign()
{
    Design d;
    d.name = "New mead";
    d.honeys = {{"Orange blossom", 14}};
    d.yeastId = "lallemand-71b-1122";
    d.brewDate = QDate::currentDate().toString(Qt::ISODate).toStdString();
    return d;
}

// My recipes: one .json file per saved recipe (with "/" separators, for Qt's file functions).
QString myRecipesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/Mead Workshop/My recipes");
}

QString draftPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/draft.json");
}

}  // namespace

// ------------------------------------------------------------------------------------------------
Designer::Designer(QObject *parent) : QObject(parent)
{
    m_db = loadDatabase(readResource(QStringLiteral(":/data/fruits.json")), readResource(QStringLiteral(":/data/yeasts.json")),
                        readResource(QStringLiteral(":/data/schramm.json")), readResource(QStringLiteral(":/data/reference.json")));
    m_recipes = json::parse(readResource(QStringLiteral(":/data/recipes.json")));
    loadBookPack();
    loadSettings();
    loadDraft();
    // Which saved recipe (if any) the draft is, so Save goes back to the same file and unsaved changes show.
    m_savedText = json::stringify(designToJson(defaultDesign()));
    const QString file = QSettings().value(QStringLiteral("recipeFile")).toString();
    QFile saved(myRecipesDir() + QLatin1Char('/') + file + QStringLiteral(".json"));
    if (!file.isEmpty() && saved.open(QIODevice::ReadOnly)) {
        try {
            m_savedText = json::stringify(designToJson(designFromJson(json::parse(saved.readAll().toStdString()))));
            m_savedAs = file;
        } catch (const std::exception &) {
            // unreadable file: the recipe on screen counts as not saved
        }
    }
    m_unsaved = json::stringify(designToJson(m_design)) != m_savedText;
    loadGoal();
    m_designMap = toMap(m_design);
    m_results = compute();
    goalCommitted();
}

// ------------------------------------------------------------------------------------------------ goal planner
namespace {
Goal defaultGoal()
{
    Goal g;
    g.name = "Pear mead";
    g.volumeGal = 6;
    g.abv = 13;
    g.fruits = {{"pears", 20.0, "all", "auto"}};
    return g;
}
json::Value goalToJson(const Goal &g)
{
    using json::Value;
    Value o = Value::object();
    auto &m = o.obj();
    m["name"] = Value::string(g.name); m["honeyName"] = Value::string(g.honeyName); m["sweetness"] = Value::string(g.sweetness);
    m["volumeGal"] = Value::number(g.volumeGal); m["abv"] = Value::number(g.abv);
    m["yeastId"] = Value::string(g.yeastId); m["strategy"] = Value::string(g.strategy);
    m["fruitTiming"] = Value::string(g.fruitTiming);
    m["fill"] = Value::string(g.fill);
    Value fr = Value::array();
    for (const auto &f : g.fruits) {
        Value x = Value::object();
        x.obj()["fruitId"] = Value::string(f.fruitId);
        x.obj()["haveLb"] = f.haveLb ? Value::number(*f.haveLb) : Value::string("");
        x.obj()["intensity"] = Value::string(f.intensity);
        x.obj()["phase"] = Value::string(f.phase);
        fr.arr().push_back(x);
    }
    m["fruits"] = fr;
    Value sp = Value::array();
    for (const auto &x : g.spices) {
        Value o2 = Value::object();
        o2.obj()["spiceId"] = Value::string(x.spiceId);
        o2.obj()["level"] = Value::string(x.level);
        sp.arr().push_back(o2);
    }
    m["spices"] = sp;
    return o;
}
Goal goalFromJson(const json::Value &v)
{
    Goal g = defaultGoal();
    g.name = v["name"].str(); g.honeyName = v["honeyName"].str(); g.sweetness = v["sweetness"].str().empty() ? "medium" : v["sweetness"].str();
    g.volumeGal = v["volumeGal"].num(6); g.abv = v["abv"].num(13);
    g.yeastId = v["yeastId"].str(); g.strategy = v["strategy"].str().empty() ? "auto" : v["strategy"].str();
    g.fruitTiming = v["fruitTiming"].str() == "schramm" ? "schramm" : "piatz";
    g.fill = v["fill"].str() == "three-quarters" ? "three-quarters" : "two-thirds";
    g.fruits.clear();
    for (const auto &f : v["fruits"].items()) {
        GoalFruit x;
        x.fruitId = f["fruitId"].str();
        if (f["haveLb"].isNumber()) x.haveLb = f["haveLb"].num();
        x.intensity = f["intensity"].str().empty() ? "medium" : f["intensity"].str();
        x.phase = f["phase"].str().empty() ? "auto" : f["phase"].str();
        g.fruits.push_back(x);
    }
    for (const auto &x : v["spices"].items()) {
        Goal::Spice s;
        s.spiceId = x["spiceId"].str();
        s.level = x["level"].str() == "subtle" || x["level"].str() == "strong" ? x["level"].str() : "noticeable";
        g.spices.push_back(s);
    }
    return g;
}
}  // namespace

void Designer::loadGoal()
{
    m_goal = defaultGoal();
    const QString text = QSettings().value(QStringLiteral("planGoal")).toString();
    if (text.isEmpty()) return;
    try { m_goal = goalFromJson(json::parse(text.toStdString())); } catch (const std::exception &) { m_goal = defaultGoal(); }
}
void Designer::saveGoal() const
{
    QSettings().setValue(QStringLiteral("planGoal"), qs(json::stringify(goalToJson(m_goal))));
}

void Designer::goalCommitted()
{
    QVariantList fruits;
    for (const auto &f : m_goal.fruits)
        fruits.append(QVariantMap{{QStringLiteral("fruitId"), qs(f.fruitId)},
                                  {QStringLiteral("haveLb"), f.haveLb ? QVariant(*f.haveLb) : QVariant(QString())},
                                  {QStringLiteral("intensity"), qs(f.intensity)}, {QStringLiteral("phase"), qs(f.phase)}});
    m_goalMap = {{QStringLiteral("name"), qs(m_goal.name)}, {QStringLiteral("honeyName"), qs(m_goal.honeyName)},
                 {QStringLiteral("sweetness"), qs(m_goal.sweetness)}, {QStringLiteral("volumeGal"), m_goal.volumeGal},
                 {QStringLiteral("abv"), m_goal.abv}, {QStringLiteral("yeastId"), qs(m_goal.yeastId)},
                 {QStringLiteral("strategy"), qs(m_goal.strategy)}, {QStringLiteral("fruitTiming"), qs(m_goal.fruitTiming)},
                 {QStringLiteral("fill"), qs(m_goal.fill)},
                 {QStringLiteral("spices"), [this] {
                      QVariantList l;
                      for (const auto &s : m_goal.spices)
                          l.append(QVariantMap{{QStringLiteral("spiceId"), qs(s.spiceId)}, {QStringLiteral("level"), qs(s.level)}});
                      return l;
                  }()},
                 {QStringLiteral("fruits"), fruits}};
    // Yeast list for this goal, best first, each marked.
    Goal g = m_goal;
    g.fruits.erase(std::remove_if(g.fruits.begin(), g.fruits.end(), [](const GoalFruit &f) { return f.fruitId.empty(); }), g.fruits.end());
    const auto rated = rateYeasts(g, m_db);
    const YeastInfo *pick = suggestYeast(g, m_db);
    m_yeastChoices = {QVariantMap{{QStringLiteral("id"), QString()},
                                  {QStringLiteral("name"), qs(text::suggestChoiceLabel(pick))}}};
    for (const auto &r : rated) {
        const YeastInfo &y = *r.yeast;
        m_yeastChoices.append(QVariantMap{{QStringLiteral("id"), qs(y.id)},
                                          {QStringLiteral("name"), qs(text::yeastChoiceLabel(r))}});
    }
    saveGoal();
    emit goalChanged();
}

void Designer::setGoalField(const QString &path, const QVariant &value)
{
    const QStringList p = path.split(QLatin1Char('.'));
    const QString text = value.toString();
    const auto n = number(value);
    Goal &g = m_goal;
    if (p.size() == 1) {
        const QString &k = p[0];
        if (k == QStringLiteral("name")) g.name = ss(text);
        else if (k == QStringLiteral("honeyName")) g.honeyName = ss(text);
        else if (k == QStringLiteral("sweetness")) g.sweetness = ss(text);
        else if (k == QStringLiteral("volumeGal")) { if (n && *n > 0) g.volumeGal = *n; }
        else if (k == QStringLiteral("abv")) { if (n && *n > 0) g.abv = *n; }
        else if (k == QStringLiteral("yeastId")) g.yeastId = ss(text);
        else if (k == QStringLiteral("strategy")) g.strategy = ss(text);
        else if (k == QStringLiteral("fruitTiming")) g.fruitTiming = text == QStringLiteral("schramm") ? "schramm" : "piatz";
        else if (k == QStringLiteral("fill")) g.fill = text == QStringLiteral("three-quarters") ? "three-quarters" : "two-thirds";
        else return;
    } else if (p.size() == 3 && p[0] == QStringLiteral("spices")) {
        const int i = p[1].toInt();
        if (i < 0 || static_cast<size_t>(i) >= g.spices.size()) return;
        auto &s = g.spices[static_cast<size_t>(i)];
        if (p[2] == QStringLiteral("spiceId")) s.spiceId = ss(text);
        else if (p[2] == QStringLiteral("level")) s.level = text == QStringLiteral("subtle") || text == QStringLiteral("strong") ? ss(text) : "noticeable";
        else return;
    } else if (p.size() == 3 && p[0] == QStringLiteral("fruits")) {
        const int i = p[1].toInt();
        if (i < 0 || static_cast<size_t>(i) >= g.fruits.size()) return;
        auto &f = g.fruits[static_cast<size_t>(i)];
        const QString &k = p[2];
        if (k == QStringLiteral("fruitId")) f.fruitId = ss(text);
        else if (k == QStringLiteral("haveLb")) f.haveLb = n && *n > 0 ? n : std::nullopt;
        else if (k == QStringLiteral("intensity")) f.intensity = ss(text);
        else if (k == QStringLiteral("phase")) f.phase = ss(text);
        else return;
    } else {
        return;
    }
    goalCommitted();
}
void Designer::addGoalFruit()
{
    m_goal.fruits.push_back({});
    goalCommitted();
}
void Designer::addGoalSpice()
{
    m_goal.spices.push_back({});
    goalCommitted();
}
void Designer::removeGoalSpice(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_goal.spices.size()) return;
    m_goal.spices.erase(m_goal.spices.begin() + index);
    goalCommitted();
}
QVariantList Designer::spiceOptions() const
{
    QVariantList l{QVariantMap{{QStringLiteral("id"), QString()}, {QStringLiteral("name"), QStringLiteral("Choose…")}}};
    for (const auto &s : spices()) l.append(QVariantMap{{QStringLiteral("id"), qs(s.id)}, {QStringLiteral("name"), qs(s.name)}});
    return l;
}
QString Designer::spiceGuide(const QString &name, double gal) const
{
    const SpiceInfo *s = spiceForName(ss(name));
    return s && gal > 0 ? qs(mead::spiceGuide(*s, gal)) : QString();
}

void Designer::removeGoalFruit(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_goal.fruits.size()) return;
    m_goal.fruits.erase(m_goal.fruits.begin() + index);
    goalCommitted();
}

void Designer::runPlan()
{
    Goal g = m_goal;
    g.fruits.erase(std::remove_if(g.fruits.begin(), g.fruits.end(), [](const GoalFruit &f) { return f.fruitId.empty(); }), g.fruits.end());
    const FittedPlan fp = planForEquipment(g, m_db, m_settings, m_scale, m_fermenterGal);
    const Plan &p = fp.plan;
    const EngineInput eng = toEngine(p.design, m_db);
    const Result r = formulate(eng.recipe, m_settings);

    const text::PlanText pt = text::planText(fp, g, r, m_db, m_scale, m_fermenterGal, m_settings);
    QVariantList why, notes, fit, summary;
    for (const auto &w : fp.fit) fit.append(QVariantMap{{QStringLiteral("text"), qs(w.text)}, {QStringLiteral("src"), qs(w.src)}});
    for (const auto &w : p.why) why.append(QVariantMap{{QStringLiteral("text"), qs(w.text)}, {QStringLiteral("src"), qs(w.src)}});
    for (const auto &nn : review(p.design, r, eng, m_scale)) notes.append(noteToMap(nn));
    for (const auto &line : pt.summary) summary.append(qs(line));

    QVariantList yeastOptions;
    {
        for (const auto &yr : text::yeastOptionsShown(g, m_db, p.yeast)) {
            const bool chosen = p.yeast == yr.yeast;
            yeastOptions.append(QVariantMap{{QStringLiteral("mark"), qs(yr.mark)}, {QStringLiteral("label"), qs(yr.label)},
                                            {QStringLiteral("name"), qs(yr.yeast->name)},
                                            {QStringLiteral("strengthMark"), qs(yr.strengthMark)}, {QStringLiteral("strength"), qs(yr.strength)},
                                            {QStringLiteral("flavourMark"), qs(yr.flavourMark)}, {QStringLiteral("flavour"), qs(yr.flavour)},
                                            {QStringLiteral("extra"), qs(yr.extra)}, {QStringLiteral("chosen"), chosen},
                                            {QStringLiteral("id"), qs(yr.yeast->id)},
                                            {QStringLiteral("code"), qs(yr.yeast->code)},
                                            {QStringLiteral("tolerance"), yr.yeast->tolerance ? QVariant(*yr.yeast->tolerance) : QVariant()},
                                            {QStringLiteral("reaches"), yr.strengthMark == "✓"}});
        }
    }
    QVariantMap v{{QStringLiteral("ready"), true}, {QStringLiteral("name"), qs(p.design.name)}, {QStringLiteral("reachable"), p.reachable && fp.fits},
                  {QStringLiteral("strategy"), qs(pt.strategy)}, {QStringLiteral("targetOG"), p.targetOG},
                  {QStringLiteral("effectiveOG"), r.effectiveOG}, {QStringLiteral("mixingOG"), r.mixingOG}, {QStringLiteral("fg"), r.fg.central},
                  {QStringLiteral("abv"), r.fg.abv}, {QStringLiteral("strength"), qs(r.strength)},
                  {QStringLiteral("sweetnessLabel"), qs(sweetnessClass(r.fg.central, m_scale))},
                  {QStringLiteral("waterToAdd"), vol(r.volumes.waterToAdd)}, {QStringLiteral("preFruit"), vol(r.volumes.preFruit)},
                  {QStringLiteral("finalVolume"), vol(r.volumes.final)}, {QStringLiteral("honeys"), rowsToList(pt.honeys)}, {QStringLiteral("fruits"), rowsToList(pt.fruits)},
                  {QStringLiteral("yeast"), qs(pt.yeastLine)}, {QStringLiteral("why"), why}, {QStringLiteral("notes"), notes},
                  {QStringLiteral("fit"), fit}, {QStringLiteral("summary"), summary}, {QStringLiteral("extras"), rowsToList(pt.extras)},
                  {QStringLiteral("yeastOptions"), yeastOptions},
                  {QStringLiteral("style"), [&]() {
                       const StyleMatch sm = detectStyle(p.design, m_db);
                       const ProfileWords pw = describeProfile(p.design, r, m_db, m_scale);
                       return QVariantMap{{QStringLiteral("name"), qs(sm.style->name)}, {QStringLiteral("plain"), qs(sm.style->plain)},
                                          {QStringLiteral("because"), qs(sm.because)},
                                          {QStringLiteral("words"), qs(pt.styleWords)},
                                          {QStringLiteral("strengthPlain"), qs(pw.strengthPlain)}, {QStringLiteral("sweetnessPlain"), qs(pw.sweetnessPlain)}};
                   }()},
                  {QStringLiteral("primaryGal"), vol(fp.primaryGal)}};
    if (!pt.backsweeten.empty()) v[QStringLiteral("backsweeten")] = qs(pt.backsweeten);
    v[QStringLiteral("waterHint")] = qs(pt.waterHint);
    v[QStringLiteral("bottled")] = qs(pt.bottled);
    QVariantList more;
    for (const auto &line : pt.more) more.append(qs(line));
    v[QStringLiteral("more")] = more;
    m_planView = v;
    emit planChanged();
}

// ------------------------------------------------------------------------------------------------ word list
namespace {
QVariantMap entryToMap(const words::Entry &e)
{
    return {{QStringLiteral("key"), qs(e.key)}, {QStringLiteral("term"), qs(e.term)}, {QStringLiteral("plain"), qs(e.plain)},
            {QStringLiteral("group"), qs(e.group)}, {QStringLiteral("what"), qs(e.what)}, {QStringLiteral("src"), qs(e.src)}};
}
}  // namespace

QString Designer::linked(const QString &text) const { return qs(words::linkTerms(ss(text))); }
QVariantList Designer::glossary() const
{
    QVariantList l;
    for (const auto &e : words::glossary()) l.append(entryToMap(e));
    return l;
}
QStringList Designer::glossaryGroups() const
{
    QStringList l;
    for (const auto &g : words::groups()) l << qs(g);
    return l;
}
QVariantMap Designer::term(const QString &key) const
{
    const words::Entry *e = words::find(ss(key));
    return e ? entryToMap(*e) : QVariantMap{};
}
void Designer::explainTerm(const QString &linkOrKey)
{
    const QString key = linkOrKey.startsWith(QStringLiteral("term:")) ? linkOrKey.mid(5) : linkOrKey;
    if (words::find(ss(key))) emit termRequested(key);
}

void Designer::useYeast(const QString &yeastId, double abv)
{
    m_goal.yeastId = ss(yeastId);
    if (abv > 0) m_goal.abv = abv;
    goalCommitted();
    runPlan();
}

void Designer::usePlan()
{
    Goal g = m_goal;
    g.fruits.erase(std::remove_if(g.fruits.begin(), g.fruits.end(), [](const GoalFruit &f) { return f.fruitId.empty(); }), g.fruits.end());
    const FittedPlan fp = planForEquipment(g, m_db, m_settings, m_scale, m_fermenterGal);
    m_design = text::designFromPlan(fp, ss(QDate::currentDate().toString(Qt::ISODate)));
    forgetSaved();
    commit();
    say(QStringLiteral("Plan opened in the Designer"));
    emit openDesigner();
}

void Designer::setSweetnessScale(const QString &s)
{
    const std::string v = s == QStringLiteral("schramm") ? "schramm" : "piatz";
    if (v == m_scale) return;
    m_scale = v;
    saveSettings();
    commit();
}
void Designer::setHoneyPPG(double v)
{
    if (!(v > 0) || v == m_settings.honeyPPG) return;
    m_settings.honeyPPG = v;
    saveSettings();
    commit();
}
void Designer::setHoldOG(bool v)
{
    if (v == m_holdOG) return;
    m_holdOG = v;
    saveSettings();
    emit changed();
}

void Designer::loadSettings()
{
    QSettings s;
    m_scale = s.value(QStringLiteral("sweetnessScale"), QStringLiteral("piatz")).toString() == QStringLiteral("schramm") ? "schramm" : "piatz";
    m_settings.honeyPPG = s.value(QStringLiteral("honeyPPG"), C::HONEY_PPG).toDouble();
    if (!(m_settings.honeyPPG > 0)) m_settings.honeyPPG = C::HONEY_PPG;
    m_holdOG = s.value(QStringLiteral("holdOG"), true).toBool();
    m_fermenterGal = s.value(QStringLiteral("fermenterGal"), 7.2).toDouble();
}
void Designer::saveSettings() const
{
    QSettings s;
    s.setValue(QStringLiteral("sweetnessScale"), qs(m_scale));
    s.setValue(QStringLiteral("honeyPPG"), m_settings.honeyPPG);
    s.setValue(QStringLiteral("holdOG"), m_holdOG);
    s.setValue(QStringLiteral("fermenterGal"), m_fermenterGal);
}
void Designer::setFermenterGal(double v)
{
    if (!(v >= 0) || v == m_fermenterGal) return;
    m_fermenterGal = v;
    saveSettings();
    commit();
}

void Designer::loadDraft()
{
    m_design = defaultDesign();
    QFile f(draftPath());
    if (!f.open(QIODevice::ReadOnly)) return;
    try {
        m_design = designFromJson(json::parse(f.readAll().toStdString()));
    } catch (const std::exception &) {
        m_design = defaultDesign();   // unreadable draft: start fresh rather than fail
    }
}
void Designer::saveDraft() const
{
    QSaveFile f(draftPath());
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QByteArray::fromStdString(json::stringify(designToJson(m_design))));
    f.commit();
}

void Designer::commit()
{
    m_designMap = toMap(m_design);
    m_results = compute();
    m_unsaved = json::stringify(designToJson(m_design)) != m_savedText;
    saveDraft();
    emit changed();
}
void Designer::say(const QString &msg)
{
    m_message = msg;
    emit messageChanged();
}

const json::Value *Designer::publishedById(const std::string &id) const
{
    for (const auto &r : m_recipes.items())
        if (r["id"].str() == id) return &r;
    return nullptr;
}

// ------------------------------------------------------------------------------------------------ lists
QVariantList Designer::fruitOptions() const
{
    std::vector<const FruitInfo *> whole;
    for (const auto &f : m_db.fruits) if (!f.juice) whole.push_back(&f);
    std::sort(whole.begin(), whole.end(), [](const FruitInfo *a, const FruitInfo *b) {
        return QString::localeAwareCompare(qs(a->name), qs(b->name)) < 0;
    });
    QVariantList out{QVariantMap{{QStringLiteral("id"), QString()}, {QStringLiteral("name"), QStringLiteral("Choose fruit…")}}};
    for (const auto *f : whole)
        out.append(QVariantMap{{QStringLiteral("id"), qs(f->id)}, {QStringLiteral("name"), qs(f->name)}});
    return out;
}
QVariantList Designer::juiceOptions() const
{
    QVariantList out{QVariantMap{{QStringLiteral("id"), QString()}, {QStringLiteral("name"), QStringLiteral("Choose juice…")}}};
    for (const auto &j : m_db.juices)
        out.append(QVariantMap{{QStringLiteral("id"), qs(j.id)}, {QStringLiteral("name"), qs(j.name)}});
    for (const auto &f : m_db.fruits) {
        if (!f.juice) continue;
        const bool covered = std::any_of(m_db.juices.begin(), m_db.juices.end(), [&](const JuiceInfo &j) { return j.fromFruit == f.id; });
        if (!covered)
            out.append(QVariantMap{{QStringLiteral("id"), QStringLiteral("fruit:") + qs(f.id)}, {QStringLiteral("name"), qs(f.name) + QStringLiteral(" (fruit table)")}});
    }
    return out;
}
QVariantList Designer::yeastOptions() const
{
    QVariantList out{QVariantMap{{QStringLiteral("id"), QString()}, {QStringLiteral("name"), QStringLiteral("Choose yeast…")}}};
    for (const auto &y : m_db.yeasts) {
        const QString label = qs(y.vendor) + QStringLiteral(" ") + qs(y.code) + (y.common.empty() ? QString() : QStringLiteral(" – ") + qs(y.common)) +
                              QStringLiteral(" (") + (y.toleranceRaw.empty() ? QStringLiteral("?") : qs(y.toleranceRaw)) + QStringLiteral(")");
        out.append(QVariantMap{{QStringLiteral("id"), qs(y.id)}, {QStringLiteral("name"), label}});
    }
    return out;
}
QStringList Designer::honeyNames() const
{
    QStringList out;
    for (const auto &h : m_db.honeys) out << qs(h.name);
    return out;
}
QStringList Designer::sugarNames() const
{
    QStringList out;
    for (const auto &s : m_db.sugars) out << qs(s.name);
    return out;
}
QStringList Designer::adjunctNames() const
{
    QStringList out;
    for (const auto &s : spices()) out << qs(s.name);   // with amount guidance
    for (const auto &a : m_db.adjuncts) {
        const SpiceInfo *s = spiceForName(a);
        if (!s) out << qs(a);                            // the rest (pectinase, tannin, acid blend…)
    }
    return out;
}
QVariantList Designer::styleChoices() const
{
    QVariantList out{QVariantMap{{QStringLiteral("id"), QString()}, {QStringLiteral("name"), QStringLiteral("Auto (from the ingredients)")}}};
    for (const auto &st : styles())
        out.append(QVariantMap{{QStringLiteral("id"), qs(st.name)}, {QStringLiteral("name"), qs(st.name)}});
    return out;
}

QVariantList Designer::publishedRecipes() const
{
    QVariantList out;
    for (const auto &r : m_recipes.items()) {
        const std::string src = r["src"].str();
        const QString author = src == "piatz" ? QStringLiteral("Piatz") : src == "schramm" ? QStringLiteral("Schramm") : QStringLiteral("Katz");
        out.append(QVariantMap{{QStringLiteral("id"), qs(r["id"].str())}, {QStringLiteral("name"), qs(r["name"].str())},
                               {QStringLiteral("author"), author}, {QStringLiteral("style"), qs(r["style"].str())},
                               {QStringLiteral("volumeGal"), r["volumeGal"].num(5)}, {QStringLiteral("ref"), qs(r["ref"].str())},
                               {QStringLiteral("full"), recipeHasIngredients(r)}});
    }
    return out;
}

// The books' recipes are built in as an index (name and chapter). If you own the books, put the
// full recipes in a "book-pack" folder (recipes.json) next to the app or in its data folder, and they're used instead.
void Designer::loadBookPack()
{
    const QStringList places = {QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/book-pack/recipes.json"),
                                QCoreApplication::applicationDirPath() + QStringLiteral("/book-pack/recipes.json")};
    for (const QString &path : places) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        try {
            json::Value pack = json::parse(f.readAll().toStdString());
            if (pack.isArray() && !pack.items().empty()) {
                m_recipes = std::move(pack);
                m_bookPack = path;
                return;
            }
        } catch (const std::exception &) {
            // unreadable pack: keep the built-in list
        }
    }
}
QVariantList Designer::sweetnessChoices() const
{
    QVariantList out;
    for (const auto &l : mead::sweetnessScale(m_scale).levels)
        out.append(QVariantMap{{QStringLiteral("key"), qs(l.key)},
                               {QStringLiteral("label"), qs(l.label) + QStringLiteral(" (") + fixed(l.lo, 3) + QStringLiteral("–") +
                                                             (l.key == "dessert" ? QString() : fixed(l.hi, 3)) + QStringLiteral(")")}});
    return out;
}

// ------------------------------------------------------------------------------------------------ edits
void Designer::setField(const QString &path, const QVariant &value)
{
    const QStringList p = path.split(QLatin1Char('.'));
    Design &d = m_design;
    const QString text = value.toString();
    const auto n = number(value);

    if (p.size() == 1) {
        const QString &k = p[0];
        if (k == QStringLiteral("name")) d.name = ss(text);
        else if (k == QStringLiteral("style")) d.style = ss(text);
        else if (k == QStringLiteral("notes")) d.notes = ss(text);
        else if (k == QStringLiteral("brewDate")) d.brewDate = ss(text);
        else if (k == QStringLiteral("volumeGal")) { if (n && *n > 0) d.volumeGal = *n; }
        else if (k == QStringLiteral("countFruitWater")) d.countFruitWater = value.toBool();
        else if (k == QStringLiteral("sweetness")) { d.sweetness = ss(text); d.targetFG.reset(); }
        else if (k == QStringLiteral("yeastId")) { d.yeastId = ss(text); d.yeastTolerance.reset(); }
        else if (k == QStringLiteral("yeastTolerance")) d.yeastTolerance = n;
        else return;
        commit();
        return;
    }
    if (p.size() != 3) return;
    bool okIndex = false;
    const int i = p[1].toInt(&okIndex);
    if (!okIndex || i < 0) return;
    const auto idx = static_cast<size_t>(i);
    const QString &list = p[0], &k = p[2];

    if (list == QStringLiteral("honeys") && idx < d.honeys.size()) {
        auto &h = d.honeys[idx];
        if (k == QStringLiteral("name")) h.name = ss(text);
        else if (k == QStringLiteral("lb")) h.lb = n ? std::max(0.0, *n) : 0;
    } else if (list == QStringLiteral("fruits") && idx < d.fruits.size()) {
        auto &f = d.fruits[idx];
        if (k == QStringLiteral("fruitId")) {
            f.fruitId = ss(text);
            const FruitInfo *b = m_db.fruit(f.fruitId);
            f.name = b ? b->name : "";
            f.water = b ? std::optional<double>(b->water) : std::nullopt;
            f.sugar = b ? b->sugar : std::nullopt;
            f.note.clear();
        } else if (k == QStringLiteral("lb")) f.lb = n ? std::max(0.0, *n) : 0;
        else if (k == QStringLiteral("phase")) f.phase = phaseOf(value);
        else if (k == QStringLiteral("water")) f.water = n;
        else if (k == QStringLiteral("sugar")) f.sugar = n;
    } else if (list == QStringLiteral("juices") && idx < d.juices.size()) {
        auto &j = d.juices[idx];
        if (k == QStringLiteral("juiceId")) {
            j.juiceId = ss(text);
            if (text.startsWith(QStringLiteral("fruit:"))) {
                const FruitInfo *f = m_db.fruit(ss(text.mid(6)));
                j.name = f ? f->name : "Juice";
                j.sg = juiceSGFromSugar(f && f->sugar ? *f->sugar : 0);
                j.sgEstimated = true;
                j.ratio = 1;
            } else if (const JuiceInfo *b = m_db.juiceInfo(j.juiceId)) {
                const auto def = juiceDefaultSG(m_db, *b);
                j.name = b->name;
                j.sg = def.sg ? *def.sg : 1;
                j.sgEstimated = def.estimated;
                j.ratio = b->ratio;
            } else {
                j.name.clear();
            }
        } else if (k == QStringLiteral("gal")) j.gal = n ? std::max(0.0, *n) : 0;
        else if (k == QStringLiteral("sg")) { j.sg = n ? *n : 1; j.sgEstimated = false; }
        else if (k == QStringLiteral("ratio")) j.ratio = n && *n > 0 ? *n : 1;
        else if (k == QStringLiteral("phase")) j.phase = phaseOf(value);
    } else if (list == QStringLiteral("sugars") && idx < d.sugars.size()) {
        auto &s = d.sugars[idx];
        if (k == QStringLiteral("name")) {
            s.name = ss(text);
            for (const auto &preset : m_db.sugars)
                if (preset.name == s.name) s.ppg = preset.ppg;
        } else if (k == QStringLiteral("lb")) s.lb = n ? std::max(0.0, *n) : 0;
        else if (k == QStringLiteral("ppg")) s.ppg = n ? *n : C::SUGAR_PPG;
        else if (k == QStringLiteral("phase")) s.phase = phaseOf(value);
    } else if (list == QStringLiteral("adjuncts") && idx < d.adjuncts.size()) {
        auto &a = d.adjuncts[idx];
        if (k == QStringLiteral("name")) a.name = ss(text);
        else if (k == QStringLiteral("amount")) a.amount = ss(text);
        else if (k == QStringLiteral("when")) a.when = ss(text);
    } else {
        return;
    }
    commit();
}

void Designer::addRow(const QString &list)
{
    Design &d = m_design;
    if (list == QStringLiteral("honeys")) d.honeys.push_back({"Wildflower", 3});
    else if (list == QStringLiteral("fruits")) { DesignFruit f; f.lb = 5; f.phase = Phase::Secondary; d.fruits.push_back(f); }
    else if (list == QStringLiteral("juices")) { DesignJuice j; j.gal = 1; j.sg = 1; d.juices.push_back(j); }
    else if (list == QStringLiteral("sugars")) d.sugars.push_back({"Cane sugar (sucrose)", 1, 46, Phase::Primary});
    else if (list == QStringLiteral("adjuncts")) d.adjuncts.push_back({});
    else return;
    commit();
}

void Designer::removeRow(const QString &list, int index)
{
    Design &d = m_design;
    auto erase = [index](auto &v) {
        if (index >= 0 && static_cast<size_t>(index) < v.size()) v.erase(v.begin() + index);
    };
    if (list == QStringLiteral("honeys")) erase(d.honeys);
    else if (list == QStringLiteral("fruits")) erase(d.fruits);
    else if (list == QStringLiteral("juices")) erase(d.juices);
    else if (list == QStringLiteral("sugars")) erase(d.sugars);
    else if (list == QStringLiteral("adjuncts")) {
        if (index >= 0 && static_cast<size_t>(index) < d.adjuncts.size() &&
            d.adjuncts[static_cast<size_t>(index)].name.rfind("Honey for backsweetening", 0) == 0)
            d.backsweeten.reset();
        erase(d.adjuncts);
    } else return;
    commit();
}

void Designer::newDesign()
{
    m_design = defaultDesign();
    forgetSaved();
    commit();
    say(QStringLiteral("Started a new recipe"));
}

// ------------------------------------------------------------------------------------------------ My recipes
// One readable .json file per recipe in Documents/Mead Workshop/My recipes (easy to find, back up or share).
// The recipe on screen is also kept in draft.json, so nothing is lost if the app closes before you save.
QString Designer::recipesFolder() const { return QDir::toNativeSeparators(myRecipesDir()); }   // as Windows shows it

void Designer::forgetSaved()
{
    m_savedAs.clear();
    m_savedText = json::stringify(designToJson(m_design));   // a fresh start has nothing unsaved yet
    QSettings().remove(QStringLiteral("recipeFile"));
}

bool Designer::writeRecipe(const QString &stem)
{
    const QString dir = myRecipesDir();
    QDir().mkpath(dir);
    const std::string text = json::stringify(designToJson(m_design));
    QSaveFile f(dir + QLatin1Char('/') + stem + QStringLiteral(".json"));
    if (!f.open(QIODevice::WriteOnly) || f.write(QByteArray::fromStdString(text)) < 0 || !f.commit()) {
        say(QStringLiteral("Couldn't save to %1").arg(QDir::toNativeSeparators(dir)));
        return false;
    }
    m_savedAs = stem;
    m_savedText = text;
    QSettings().setValue(QStringLiteral("recipeFile"), stem);
    return true;
}

void Designer::saveRecipe()
{
    const QString dir = myRecipesDir();
    const QString wanted = qs(text::recipeFileStem(m_design.name));
    if (!m_savedAs.isEmpty() && wanted.compare(m_savedAs, Qt::CaseInsensitive) == 0) {
        if (writeRecipe(m_savedAs)) { commit(); say(QStringLiteral("Saved “%1”").arg(m_savedAs)); }
        return;
    }
    // First save, or the recipe was renamed: save under the new name, then remove the old file.
    const QString old = m_savedAs;
    const auto taken = [&](const std::string &stem) {
        const QString q = qs(stem);
        if (!old.isEmpty() && q.compare(old, Qt::CaseInsensitive) == 0) return false;
        return QFileInfo::exists(dir + QLatin1Char('/') + q + QStringLiteral(".json"));
    };
    const QString stem = qs(text::uniqueStem(ss(wanted), taken));
    if (!writeRecipe(stem)) return;
    if (!old.isEmpty() && old.compare(stem, Qt::CaseInsensitive) != 0) QFile::remove(dir + QLatin1Char('/') + old + QStringLiteral(".json"));
    commit();
    say(old.isEmpty() ? QStringLiteral("Saved “%1” in My recipes").arg(stem) : QStringLiteral("Saved, and renamed to “%1”").arg(stem));
}

void Designer::saveRecipeAs(const QString &name)
{
    const QString n = name.trimmed();
    if (!n.isEmpty()) m_design.name = ss(n);
    const QString dir = myRecipesDir();
    const auto taken = [&](const std::string &stem) { return QFileInfo::exists(dir + QLatin1Char('/') + qs(stem) + QStringLiteral(".json")); };
    const QString stem = qs(text::uniqueStem(text::recipeFileStem(m_design.name), taken));
    if (!writeRecipe(stem)) return;
    commit();
    say(QStringLiteral("Saved a copy as “%1”").arg(stem));
}

QVariantList Designer::myRecipes() const
{
    QVariantList out;
    const QFileInfoList files = QDir(myRecipesDir()).entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Time);
    for (const QFileInfo &fi : files) {
        QVariantMap m{{QStringLiteral("file"), fi.completeBaseName()}, {QStringLiteral("name"), fi.completeBaseName()},
                      {QStringLiteral("saved"), fi.lastModified().toString(QStringLiteral("MMM d, yyyy"))}, {QStringLiteral("ok"), false},
                      {QStringLiteral("style"), QString()}, {QStringLiteral("volumeGal"), 0.0}, {QStringLiteral("kind"), QString()}};
        QFile f(fi.absoluteFilePath());
        if (f.open(QIODevice::ReadOnly)) {
            try {
                const Design d = designFromJson(json::parse(f.readAll().toStdString()));
                if (!d.name.empty()) m[QStringLiteral("name")] = qs(d.name);
                m[QStringLiteral("style")] = qs(d.style);
                m[QStringLiteral("volumeGal")] = d.volumeGal;
                m[QStringLiteral("kind")] = qs(d.kind);
                m[QStringLiteral("ok")] = true;
            } catch (const std::exception &) {
                // listed, but marked as unreadable
            }
        }
        out.append(m);
    }
    return out;
}

void Designer::openRecipe(const QString &file)
{
    QFile f(myRecipesDir() + QLatin1Char('/') + file + QStringLiteral(".json"));
    if (!f.open(QIODevice::ReadOnly)) { say(QStringLiteral("Couldn't open “%1”").arg(file)); return; }
    try {
        m_design = designFromJson(json::parse(f.readAll().toStdString()));
    } catch (const std::exception &) {
        say(QStringLiteral("“%1” can't be read (the file is damaged)").arg(file));
        return;
    }
    m_savedAs = file;
    m_savedText = json::stringify(designToJson(m_design));
    QSettings().setValue(QStringLiteral("recipeFile"), file);
    commit();
    say(QStringLiteral("Opened “%1”").arg(file));
}

void Designer::deleteRecipe(const QString &file)
{
#ifdef Q_OS_WIN
    const QString bin = QStringLiteral("the Recycle Bin");
#else
    const QString bin = QStringLiteral("the Trash");
#endif
    const QString path = myRecipesDir() + QLatin1Char('/') + file + QStringLiteral(".json");
    if (!QFile::moveToTrash(path)) { say(QStringLiteral("Couldn't move “%1” to %2").arg(file, bin)); return; }
    if (file.compare(m_savedAs, Qt::CaseInsensitive) == 0) {   // the recipe on screen stays, as an unsaved recipe
        m_savedAs.clear();
        m_savedText.clear();
        QSettings().remove(QStringLiteral("recipeFile"));
        commit();
    }
    say(QStringLiteral("Moved “%1” to %2").arg(file, bin));
}

void Designer::showRecipesFolder() const
{
    QDir().mkpath(myRecipesDir());
    QDesktopServices::openUrl(QUrl::fromLocalFile(myRecipesDir()));
}

void Designer::adaptPublished(const QString &recipeId, double volumeGal)
{
    const json::Value *r = publishedById(ss(recipeId));
    if (!r) return;
    if (!recipeHasIngredients(*r)) {
        say(QStringLiteral("Mead Workshop lists that recipe by name only. It's in %1 (%2), or add a book pack with the full recipes.")
                .arg((*r)["src"].str() == "schramm" ? QStringLiteral("Schramm's book") : (*r)["src"].str() == "katz" ? QStringLiteral("Katz's book") : QStringLiteral("Piatz's book"),
                     qs((*r)["ref"].str())));
        return;
    }
    Design d = designFromPublished(*r, m_db);
    d.kind = "adapted";
    d.name += " (adapted)";
    d.brewDate = QDate::currentDate().toString(Qt::ISODate).toStdString();
    if (volumeGal > 0 && std::fabs(volumeGal - d.volumeGal) > 1e-9) {
        const double k = volumeGal / d.volumeGal;   // scale every amount (same as the browser's scaleDesign)
        d.volumeGal = volumeGal;
        for (auto &h : d.honeys) h.lb = std::stod(toFixed(h.lb * k, 3));
        for (auto &f : d.fruits) f.lb = std::stod(toFixed(f.lb * k, 3));
        for (auto &j : d.juices) j.gal = std::stod(toFixed(j.gal * k, 3));
        for (auto &s : d.sugars) s.lb = std::stod(toFixed(s.lb * k, 3));
    }
    m_design = d;
    forgetSaved();
    commit();
    say(QStringLiteral("Adapted copy created. The published recipe is unchanged."));
}

void Designer::solveForOG(const QString &og)
{
    const text::Solved r = text::solveOG(m_design, m_db, ss(og), m_settings);
    if (r.design) { m_design = *r.design; commit(); }
    say(qs(r.message));
}

void Designer::solveForABV(const QString &abvText)
{
    const text::Solved r = text::solveABV(m_design, m_db, ss(abvText), m_settings, m_scale);
    if (r.design) { m_design = *r.design; commit(); }
    say(qs(r.message));
}

// ------------------------------------------------------------------------------------------------ profile
QVariantList Designer::previewSweetOptions(double targetFG) const
{
    const EngineInput eng = toEngine(m_design, m_db);
    const Result r = formulate(eng.recipe, m_settings);
    return optionsToList(sweetOptions(m_design, eng, r, targetFG, m_settings));
}

void Designer::setTargetFG(double targetFG)
{
    m_design.targetFG = std::stod(toFixed(targetFG, 3));
    m_design.sweetness = sweetKeyFor(*m_design.targetFG, m_scale);
    commit();
}

void Designer::applyOption(double og, double fg)
{
    const text::Solved r = text::applyOption(m_design, m_db, og, fg, m_settings, m_scale);
    if (r.design) { m_design = *r.design; commit(); }
    say(qs(r.message));
}

void Designer::addBacksweetening(double lb, double fg)
{
    m_design = withBacksweetening(m_design, std::stod(toFixed(fg, 3)), lb, m_scale);
    commit();
    say(QStringLiteral("Backsweetening added to the recipe"));
}

QString Designer::previewFruitLevel(int index, double lb) const
{
    if (index < 0 || static_cast<size_t>(index) >= m_design.fruits.size()) return {};
    const Result r = formulate(toEngine(m_design, m_db).recipe, m_settings);
    return qs(text::fruitBarLabel(m_db, m_design.fruits[static_cast<size_t>(index)].fruitId, lb, r.volumes.final));
}

void Designer::setFruitAmount(int index, double lb)
{
    if (index < 0) return;
    m_design = withFruitAmount(m_design, m_db, static_cast<size_t>(index), std::round(lb * 4) / 4, m_holdOG, m_settings);
    commit();
}

// ------------------------------------------------------------------------------------------------ views
QVariantMap Designer::toMap(const Design &d) const
{
    auto opt = [](const std::optional<double> &o) { return o ? QVariant(*o) : QVariant(QString()); };
    QVariantList honeys, fruits, juices, sugars, adjuncts;
    for (const auto &h : d.honeys)
        honeys.append(QVariantMap{{QStringLiteral("name"), qs(h.name)}, {QStringLiteral("lb"), h.lb}});
    for (const auto &f : d.fruits) {
        const FruitInfo *b = m_db.fruit(f.fruitId);
        QString meta;
        if (b) {
            QStringList bits;
            if (!f.name.empty() && f.name != b->name) bits << qs(f.name);
            if (!b->acid.empty()) bits << QStringLiteral("Acid: ") + qs(b->acid);
            if (!b->pectin.empty()) bits << QStringLiteral("Pectin: ") + qs(b->pectin);
            bits << qs(b->src);
            if (!f.note.empty()) bits << qs(f.note);
            meta = bits.join(QStringLiteral(" · "));
        }
        fruits.append(QVariantMap{{QStringLiteral("fruitId"), qs(f.fruitId)}, {QStringLiteral("name"), qs(f.name)},
                                  {QStringLiteral("lb"), f.lb}, {QStringLiteral("phase"), phaseName(f.phase)},
                                  {QStringLiteral("water"), opt(f.water)}, {QStringLiteral("sugar"), opt(f.sugar)},
                                  {QStringLiteral("meta"), meta}});
    }
    for (const auto &j : d.juices) {
        const JuiceInfo *b = m_db.juiceInfo(j.juiceId);
        QString meta = qs(j.name);
        if (b && !b->note.empty()) meta += QStringLiteral(" · ") + qs(b->note);
        if (j.sgEstimated) meta += QStringLiteral(" · SG estimated from USDA sugar %. A hydrometer reading of your juice is better.");
        juices.append(QVariantMap{{QStringLiteral("juiceId"), qs(j.juiceId)}, {QStringLiteral("name"), qs(j.name)},
                                  {QStringLiteral("gal"), j.gal}, {QStringLiteral("sg"), j.sg}, {QStringLiteral("ratio"), j.ratio},
                                  {QStringLiteral("phase"), phaseName(j.phase)}, {QStringLiteral("sgEstimated"), j.sgEstimated},
                                  {QStringLiteral("meta"), meta}});
    }
    for (const auto &s : d.sugars)
        sugars.append(QVariantMap{{QStringLiteral("name"), qs(s.name)}, {QStringLiteral("lb"), s.lb},
                                  {QStringLiteral("ppg"), s.ppg}, {QStringLiteral("phase"), phaseName(s.phase)}});
    for (const auto &a : d.adjuncts)
        adjuncts.append(QVariantMap{{QStringLiteral("name"), qs(a.name)}, {QStringLiteral("amount"), qs(a.amount)},
                                    {QStringLiteral("when"), qs(a.when)}});
    QString parentName;
    if (!d.parentId.empty())
        if (const json::Value *r = publishedById(d.parentId)) parentName = qs((*r)["name"].str());
    const StyleInfo *named = styleByName(d.style);
    return {{QStringLiteral("name"), qs(d.name)}, {QStringLiteral("style"), named ? qs(named->name) : QString()}, {QStringLiteral("kind"), qs(d.kind)},
            {QStringLiteral("parentName"), parentName}, {QStringLiteral("notes"), qs(d.notes)},
            {QStringLiteral("brewDate"), qs(d.brewDate)}, {QStringLiteral("volumeGal"), d.volumeGal},
            {QStringLiteral("countFruitWater"), d.countFruitWater}, {QStringLiteral("sweetness"), qs(d.sweetness)},
            {QStringLiteral("targetFG"), opt(d.targetFG)}, {QStringLiteral("yeastId"), qs(d.yeastId)},
            {QStringLiteral("yeastTolerance"), opt(d.yeastTolerance)}, {QStringLiteral("wild"), d.wild},
            {QStringLiteral("honeys"), honeys}, {QStringLiteral("fruits"), fruits}, {QStringLiteral("juices"), juices},
            {QStringLiteral("sugars"), sugars}, {QStringLiteral("adjuncts"), adjuncts}};
}

QVariantMap Designer::compute() const
{
    const Design &d = m_design;
    const EngineInput eng = toEngine(d, m_db);
    const Result r = formulate(eng.recipe, m_settings);
    const Volumes &v = r.volumes;
    const double tol = eng.recipe.yeast.tolerance;
    const double target = targetFGFor(d, m_scale);
    const SweetScale &scale = mead::sweetnessScale(m_scale);

    QVariantMap m;
    m[QStringLiteral("mixingOG")] = r.mixingOG;
    m[QStringLiteral("effectiveOG")] = r.effectiveOG;
    m[QStringLiteral("fg")] = r.fg.central;
    m[QStringLiteral("fgLow")] = r.fg.low;
    m[QStringLiteral("fgHigh")] = r.fg.high;
    m[QStringLiteral("reachesDry")] = r.fg.reachesDry;
    m[QStringLiteral("abv")] = r.fg.abv;
    m[QStringLiteral("abvBalling")] = abvBalling(r.effectiveOG, r.fg.central);
    m[QStringLiteral("strength")] = qs(r.strength);
    m[QStringLiteral("sweetnessLabel")] = qs(sweetnessClass(r.fg.central, m_scale));
    m[QStringLiteral("waterToAdd")] = v.waterToAdd;
    m[QStringLiteral("preFruit")] = v.preFruit;
    m[QStringLiteral("finalVolume")] = v.final;
    m[QStringLiteral("fruitWater")] = v.fruitWater;
    m[QStringLiteral("tolerance")] = tol;

    // profile bars
    m[QStringLiteral("scaleName")] = qs(scale.name);
    m[QStringLiteral("scaleSrc")] = qs(scale.src);
    m[QStringLiteral("sweetZones")] = zonesToList(sweetnessZones(m_scale));
    m[QStringLiteral("strengthZones")] = zonesToList(strengthZones());
    m[QStringLiteral("targetFG")] = target;
    m[QStringLiteral("sweetOptions")] = d.wild ? QVariantList() : optionsToList(sweetOptions(d, eng, r, target, m_settings));
    QVariantList bars;
    for (size_t i = 0; i < d.fruits.size(); ++i) {
        const auto &f = d.fruits[i];
        if (f.fruitId.empty()) continue;
        const FruitScale sc = fruitScale(m_db, f.fruitId, v.final);
        const double max = std::max(sc.max, f.lb * 1.05);
        bars.append(QVariantMap{{QStringLiteral("index"), static_cast<int>(i)},
                                {QStringLiteral("name"), qs(eng.recipe.fruits[i].name)},
                                {QStringLiteral("lb"), f.lb},
                                {QStringLiteral("label"), qs(text::fruitBarLabel(m_db, f.fruitId, f.lb, v.final))},
                                {QStringLiteral("zones"), zonesToList(sc.zones)},
                                {QStringLiteral("max"), max},
                                {QStringLiteral("src"), qs(sc.src) + (sc.lemonLime ? QStringLiteral(" · Schramm: use less for lemons and limes") : QString())}});
    }
    m[QStringLiteral("fruitBars")] = bars;

    // review, fruit character
    QVariantList notes;
    const text::ExtraNotes extra = text::designerNotes(d, r, m_db, m_fermenterGal, fillFraction(m_goal));
    if (extra.overfull) notes.append(noteToMap(*extra.overfull));
    if (extra.tooThick) notes.append(noteToMap(*extra.tooThick));
    for (const auto &n : review(d, r, eng, m_scale)) notes.append(noteToMap(n));
    if (extra.styleMismatch) notes.append(noteToMap(*extra.styleMismatch));
    {
        const StyleMatch sm = detectStyle(d, m_db);
        const StyleInfo *named = styleByName(d.style);
        const StyleInfo *shown = named ? named : sm.style;
        const ProfileWords pw = describeProfile(d, r, m_db, m_scale);
        m[QStringLiteral("style")] = QVariantMap{{QStringLiteral("name"), qs(shown->name)}, {QStringLiteral("plain"), qs(shown->plain)},
                                                 {QStringLiteral("auto"), named == nullptr},
                                                 {QStringLiteral("because"), qs(sm.because)},
                                                 {QStringLiteral("words"), qs(pw.style + " · " + pw.strength + " · " + pw.sweetness)},
                                                 {QStringLiteral("strengthPlain"), qs(pw.strengthPlain)}, {QStringLiteral("sweetnessPlain"), qs(pw.sweetnessPlain)}};
    }
    m[QStringLiteral("notes")] = notes;
    QVariantList fc;
    for (const auto &c : fruitCharacter(eng, r, m_db, d.sweetness)) {
        QStringList names;
        for (const auto &x : c.names) names << qs(x);
        fc.append(QVariantMap{{QStringLiteral("names"), names.join(QStringLiteral(", "))}, {QStringLiteral("level"), qs(c.level)},
                              {QStringLiteral("basis"), qs(c.basis)}, {QStringLiteral("note"), qs(c.note)}, {QStringLiteral("src"), qs(c.src)}});
    }
    m[QStringLiteral("fruitCharacter")] = fc;

    // yeast
    if (eng.yeast) {
        const YeastInfo &y = *eng.yeast;
        const QString info = qs(text::yeastInfo(y));
        if (!d.wild)
            m[QStringLiteral("yeastAmount")] = qs(text::yeastAmountLine(piatzSNA(v.final, r.effectiveOG, r.fg.central)));
        m[QStringLiteral("yeastInfo")] = info;
        m[QStringLiteral("yeastSrc")] = qs(y.src);
    } else {
        m[QStringLiteral("yeastInfo")] = d.wild ? QStringLiteral("Wild fermentation, no added yeast.")
                                                : QStringLiteral("Choose a yeast to predict where the fermentation will stop.");
        m[QStringLiteral("yeastSrc")] = QString();
    }

    // nutrients (Piatz ch.4–5)
    if (!d.wild) {
        const SNA n = piatzSNA(v.final, r.effectiveOG, r.fg.central);
        const text::NutrientText t = text::nutrientText(n);
        QVariantList doses;
        for (const auto &x : t.doses) doses.append(QVariantMap{{QStringLiteral("when"), qs(x.name)}, {QStringLiteral("amount"), qs(x.amount)}});
        m[QStringLiteral("nutrients")] = QVariantMap{{QStringLiteral("packets"), n.packets}, {QStringLiteral("goFerm"), qs(t.goFerm)},
                                                     {QStringLiteral("mix"), qs(t.mix)}, {QStringLiteral("doses"), doses},
                                                     {QStringLiteral("caveat"), qs(t.caveat)}, {QStringLiteral("schramm"), qs(t.schramm)},
                                                     {QStringLiteral("chemistry"), qs(t.chemistry)}};
    }

    // published original
    if (!d.parentId.empty()) {
        if (const json::Value *pr = publishedById(d.parentId)) {
            const Design parent = designFromPublished(*pr, m_db);
            const Faithfulness f = faithfulness(d, parent);
            QStringList changes;
            for (const auto &c : f.changes) changes << qs(c);
            m[QStringLiteral("faithfulness")] = QVariantMap{{QStringLiteral("level"), qs(f.level)}, {QStringLiteral("changes"), changes},
                                                            {QStringLiteral("scale"), f.scale}};
            const auto &rec = *pr;
            const QString figures = qs(text::publishedFigures(rec));
            const QString bookNotes = qs(text::publishedNotes(rec));
            m[QStringLiteral("published")] = QVariantMap{{QStringLiteral("name"), qs(rec["name"].str())}, {QStringLiteral("ref"), qs(rec["ref"].str())},
                                                         {QStringLiteral("figures"), figures}, {QStringLiteral("notes"), bookNotes}};
        }
    }

    // directions (Directions tab): shopping list, before brew day, brew day, timeline with dates
    {
        const Directions dir = directions(d, r, eng);
        auto steps = [](const std::vector<Step> &xs) {
            QVariantList l;
            for (const auto &x : xs)
                l.append(QVariantMap{{QStringLiteral("title"), qs(x.title)}, {QStringLiteral("detail"), qs(x.detail)}, {QStringLiteral("src"), qs(x.src)}});
            return l;
        };
        QVariantList shopping;
        for (const auto &it : text::shoppingList(d, r, eng))
            shopping.append(QVariantMap{{QStringLiteral("what"), qs(it.what)}, {QStringLiteral("amount"), qs(it.amount)}, {QStringLiteral("note"), qs(it.note)}});
        QVariantList timeline;
        for (const auto &x : text::timeline(d, r, eng))
            timeline.append(QVariantMap{{QStringLiteral("when"), qs(x.when)}, {QStringLiteral("title"), qs(x.title)}, {QStringLiteral("detail"), qs(x.detail)},
                                        {QStringLiteral("src"), qs(x.src)}, {QStringLiteral("kind"), qs(x.kind)}});
        m[QStringLiteral("directions")] = QVariantMap{{QStringLiteral("shopping"), shopping}, {QStringLiteral("prep"), steps(dir.prep)},
                                                      {QStringLiteral("brewDay"), steps(dir.brewDay)}, {QStringLiteral("timeline"), timeline},
                                                      {QStringLiteral("brewDateValid"), text::parseISO(d.brewDate).has_value()}};
    }

    // the math
    QVariantList math;
    for (const auto &l : text::mathLines(r)) math.append(QVariantMap{{QStringLiteral("k"), qs(l.k)}, {QStringLiteral("v"), qs(l.v)}});
    m[QStringLiteral("math")] = math;
    return m;
}
