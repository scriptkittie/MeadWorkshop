// Colours and small number helpers shared by every screen (same palette as the browser version).
// Registered for the screens as the singleton "Theme" in MeadWorkshop.Core.
#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QVariant>

#include <cmath>

class Theme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY changed)
    Q_PROPERTY(QColor bg READ bg NOTIFY changed)
    Q_PROPERTY(QColor panel READ panel NOTIFY changed)
    Q_PROPERTY(QColor panel2 READ panel2 NOTIFY changed)
    Q_PROPERTY(QColor ink READ ink NOTIFY changed)
    Q_PROPERTY(QColor ink2 READ ink2 NOTIFY changed)
    Q_PROPERTY(QColor ink3 READ ink3 NOTIFY changed)
    Q_PROPERTY(QColor line READ line NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor onAccent READ onAccent NOTIFY changed)
    Q_PROPERTY(QColor warn READ warn NOTIFY changed)
    Q_PROPERTY(QColor info READ info NOTIFY changed)
    Q_PROPERTY(QColor good READ good NOTIFY changed)

public:
    using QObject::QObject;

    bool dark() const { return m_dark; }
    void setDark(bool d) { if (d != m_dark) { m_dark = d; emit changed(); } }

    QColor bg() const { return pick("#17130e", "#f7f3ea"); }
    QColor panel() const { return pick("#201a13", "#fffdf8"); }
    QColor panel2() const { return pick("#2a231a", "#f1ebdd"); }
    QColor ink() const { return pick("#f1e8d8", "#2a2116"); }
    QColor ink2() const { return pick("#c9bba4", "#5e5243"); }
    QColor ink3() const { return pick("#9a8c76", "#8a7d6b"); }
    QColor line() const { return pick("#3a3024", "#e2d8c4"); }
    QColor accent() const { return pick("#e0a043", "#a8640f"); }
    QColor onAccent() const { return pick("#1b1309", "#ffffff"); }
    QColor warn() const { return pick("#e8835a", "#b4441c"); }
    QColor info() const { return pick("#8fb3d9", "#2f5f8f"); }
    QColor good() const { return pick("#8cc48a", "#2f7a3b"); }

    // Zone shading for the bars: stronger amber the further along the scale.
    Q_INVOKABLE QColor zone(int shade) const
    {
        QColor c = accent();
        c.setAlphaF(static_cast<float>(0.10 + 0.13 * shade));
        return c;
    }
    // A colour with transparency, e.g. Theme.alpha(Theme.accent, 0.14).
    Q_INVOKABLE QColor alpha(const QColor &c, double a) const
    {
        QColor x = c;
        x.setAlphaF(static_cast<float>(a));
        return x;
    }
    // Specific gravity to 3 decimals; "—" when missing.
    Q_INVOKABLE QString sg3(const QVariant &v) const
    {
        bool ok = false;
        const double d = v.toDouble(&ok);
        return ok && std::isfinite(d) ? QString::number(d, 'f', 3) : QStringLiteral("—");
    }
    // A number for an input box: up to 3 decimals, no trailing zeros; blank stays blank.
    Q_INVOKABLE QString plain(const QVariant &v) const
    {
        if (!v.isValid() || v.isNull() || (v.typeId() == QMetaType::QString && v.toString().isEmpty())) return {};
        bool ok = false;
        const double d = v.toDouble(&ok);
        if (!ok || !std::isfinite(d)) return {};
        QString s = QString::number(d, 'f', 3);
        while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.')))) s.chop(1);
        return s == QStringLiteral("-0") ? QStringLiteral("0") : s;
    }

signals:
    void changed();

private:
    QColor pick(const char *darkHex, const char *lightHex) const { return QColor(QString::fromLatin1(m_dark ? darkHex : lightHex)); }
    bool m_dark = true;
};
