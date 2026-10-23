#include "meadbridge.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "mead_engine.hpp"

namespace {
QJsonArray loadArray(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).array();
}
}  // namespace

MeadBridge::MeadBridge(QObject *parent)
    : QObject(parent),
      m_fruits(loadArray(QStringLiteral(":/data/fruits.json"))),
      m_yeasts(loadArray(QStringLiteral(":/data/yeasts.json"))),
      m_recipes(loadArray(QStringLiteral(":/data/recipes.json")))
{
}

QVariantMap MeadBridge::quickCheck(double volumeGal, double honeyLb, double yeastTolerance,
                                   const QString &sweetnessScale) const
{
    mead::Recipe r;
    r.volumeGal = volumeGal > 0 ? volumeGal : 5;
    r.honeys = {{"Honey", honeyLb > 0 ? honeyLb : 0}};
    r.yeast.tolerance = yeastTolerance > 0 ? yeastTolerance : 14;
    const mead::Result res = mead::formulate(r);
    const std::string scale = sweetnessScale.toStdString();

    QVariantMap m;
    m[QStringLiteral("og")] = res.effectiveOG;
    m[QStringLiteral("fg")] = res.fg.central;
    m[QStringLiteral("abv")] = res.fg.abv;
    m[QStringLiteral("abvBalling")] = mead::abvBalling(res.effectiveOG, res.fg.central);
    m[QStringLiteral("reachesDry")] = res.fg.reachesDry;
    m[QStringLiteral("strength")] = QString::fromStdString(res.strength);
    m[QStringLiteral("sweetness")] = QString::fromStdString(mead::sweetnessClass(res.fg.central, scale));
    m[QStringLiteral("waterToAdd")] = res.volumes.waterToAdd;
    return m;
}

QVariantList MeadBridge::sweetnessLevels(const QString &scale) const
{
    QVariantList out;
    for (const auto &l : mead::sweetnessScale(scale.toStdString()).levels) {
        QVariantMap m;
        m[QStringLiteral("key")] = QString::fromStdString(l.key);
        m[QStringLiteral("label")] = QString::fromStdString(l.label);
        m[QStringLiteral("lo")] = l.lo;
        m[QStringLiteral("hi")] = l.hi;
        out.append(m);
    }
    return out;
}

QVariantList MeadBridge::yeasts() const
{
    QVariantList out;
    for (const auto &v : m_yeasts) {
        const QJsonObject y = v.toObject();
        if (y.value(QStringLiteral("tolerance")).isNull())
            continue;
        QVariantMap m;
        m[QStringLiteral("name")] = y.value(QStringLiteral("name")).toString();
        m[QStringLiteral("tolerance")] = y.value(QStringLiteral("tolerance")).toDouble();
        m[QStringLiteral("temp")] = y.value(QStringLiteral("tempRaw")).toString();
        out.append(m);
    }
    return out;
}

QString MeadBridge::dataSummary() const
{
    return QStringLiteral("%1 fruits · %2 yeasts · %3 recipes")
        .arg(m_fruits.size())
        .arg(m_yeasts.size())
        .arg(m_recipes.size());
}

QString MeadBridge::selfCheck() const
{
    // Piatz ch.9: BS 5 gal, TOG 1.115, 12 lb fruit at 86% water / 6% sugar → NS 3.76, FS 33.12, BOG 1.144
    mead::Recipe r;
    r.volumeGal = 5;
    r.honeys = {{"Honey", 1}};
    r.fruits = {{"Fruit", 12, 86, 6.0, mead::Phase::Primary}};
    r.honeys = mead::solveHoney(r, 1.115).honeys;
    const mead::Result res = mead::formulate(r);
    const bool pass = std::fabs(res.volumes.preFruit - 3.76) < 0.005 && std::fabs(res.points.fruit - 33.12) < 0.01 &&
                      std::fabs(res.mixingOG - 1.144) < 0.0005;
    return QStringLiteral("Piatz's worked example: NS %1 gal, FS %2, BOG %3 — %4")
        .arg(res.volumes.preFruit, 0, 'f', 2)
        .arg(res.points.fruit, 0, 'f', 2)
        .arg(res.mixingOG, 0, 'f', 3)
        .arg(pass ? QStringLiteral("matches the book ✓") : QStringLiteral("DOES NOT MATCH ✗"));
}
