// Bridge between the QML screens and the C++ brewing engine.
// Everything the screens need goes through here as plain values/maps, so the engine stays Qt-free.
#pragma once

#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class MeadBridge : public QObject
{
    Q_OBJECT
public:
    explicit MeadBridge(QObject *parent = nullptr);

    // Stage 1 preview: honey-only batch → OG, predicted FG, ABV, strength and sweetness.
    Q_INVOKABLE QVariantMap quickCheck(double volumeGal, double honeyLb, double yeastTolerance,
                                       const QString &sweetnessScale) const;

    // Sweetness levels of a scale ("piatz" or "schramm") for drawing the bar.
    Q_INVOKABLE QVariantList sweetnessLevels(const QString &scale) const;

    // Yeasts from the reference data (name, tolerance, temperature range).
    Q_INVOKABLE QVariantList yeasts() const;

    // "48 fruits · 45 yeasts · 43 recipes" — proves the built-in data loaded.
    Q_INVOKABLE QString dataSummary() const;

    // Re-runs Piatz's worked fruit example on this computer (NS 3.76, FS 33.12, BOG 1.144).
    Q_INVOKABLE QString selfCheck() const;

private:
    QJsonArray m_fruits, m_yeasts, m_recipes;
};
