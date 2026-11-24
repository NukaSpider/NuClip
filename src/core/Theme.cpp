#include "core/Theme.h"

#include "core/Settings.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QStyleHints>

#include <cmath>

namespace nuclip {
namespace {

double channel(double value)
{
    value /= 255.0;
    if (value <= 0.03928)
        return value / 12.92;
    return std::pow((value + 0.055) / 1.055, 2.4);
}

double luminance(const QColor &color)
{
    return 0.2126 * channel(color.red()) + 0.7152 * channel(color.green()) + 0.0722 * channel(color.blue());
}

QString firstFamily(std::initializer_list<const char *> names, const QString &fallback)
{
    const QStringList families = QFontDatabase::families();
    for (const char *name : names) {
        if (families.contains(QLatin1String(name)))
            return QString::fromLatin1(name);
    }
    return fallback;
}

struct AccentPair {
    const char *id;
    QColor dark;
    QColor light;
};

const AccentPair kAccents[] = {
    {"green", QColor(QStringLiteral("#35D69A")), QColor(QStringLiteral("#0E9F6E"))},
    {"blue", QColor(QStringLiteral("#5B9DFF")), QColor(QStringLiteral("#2563EB"))},
    {"purple", QColor(QStringLiteral("#A78BFA")), QColor(QStringLiteral("#7C3AED"))},
    {"red", QColor(QStringLiteral("#F07178")), QColor(QStringLiteral("#E11D48"))},
    {"orange", QColor(QStringLiteral("#F5A524")), QColor(QStringLiteral("#EA580C"))},
    {"pink", QColor(QStringLiteral("#F472B6")), QColor(QStringLiteral("#DB2777"))},
    {"cyan", QColor(QStringLiteral("#22D3EE")), QColor(QStringLiteral("#0891B2"))},
};

} // namespace

Theme::Theme(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_fontFamily = firstFamily({"Inter", "Inter Variable", "Segoe UI Variable Text", "Segoe UI"}, QStringLiteral("Segoe UI"));
    m_displayFamily = m_fontFamily;
    m_iconFamily = firstFamily({"Segoe Fluent Icons", "Segoe MDL2 Assets", "Segoe UI Symbol"}, QStringLiteral("Segoe UI"));
    m_monoFamily = firstFamily({"Cascadia Mono", "Cascadia Code", "Consolas"}, QStringLiteral("Consolas"));

    connect(m_settings, &Settings::themeModeChanged, this, &Theme::rebuild);
    connect(m_settings, &Settings::accentChanged, this, &Theme::rebuild);
    if (auto *hints = QGuiApplication::styleHints()) {
        connect(hints, &QStyleHints::colorSchemeChanged, this, &Theme::rebuild);
    }
    rebuild();
}

bool Theme::systemIsDark() const
{
    if (const auto *hints = QGuiApplication::styleHints())
        return hints->colorScheme() != Qt::ColorScheme::Light;
    return true;
}

void Theme::rebuild()
{
    const bool dark = m_settings->themeMode() == QLatin1String("light")
        ? false
        : (m_settings->themeMode() == QLatin1String("system") ? systemIsDark() : true);
    m_dark = dark;

    if (dark) {
        m_background = QColor(QStringLiteral("#080808"));
        m_sidebar = QColor(QStringLiteral("#0B0B0B"));
        m_content = QColor(QStringLiteral("#0E0E0E"));
        m_card = QColor(QStringLiteral("#141414"));
        m_elevated = QColor(QStringLiteral("#191919"));
        m_cardBorder = QColor(QStringLiteral("#242424"));
        m_control = QColor(QStringLiteral("#141414"));
        m_controlHover = QColor(QStringLiteral("#1C1C1C"));
        m_text = QColor(QStringLiteral("#F2F2F2"));
        m_textSecondary = QColor(QStringLiteral("#929292"));
        m_textMuted = QColor(QStringLiteral("#626262"));
        m_caption = QColor(QStringLiteral("#080808"));
        m_danger = QColor(QStringLiteral("#F07178"));
        m_warning = QColor(QStringLiteral("#E8B15A"));
    } else {
        m_background = QColor(QStringLiteral("#F3F2EF"));
        m_sidebar = QColor(QStringLiteral("#E7E6E2"));
        m_content = QColor(QStringLiteral("#F7F6F3"));
        m_card = QColor(QStringLiteral("#FFFCF8"));
        m_elevated = QColor(QStringLiteral("#FFFFFF"));
        m_cardBorder = QColor(QStringLiteral("#D5D3CC"));
        m_control = QColor(QStringLiteral("#E8E6E1"));
        m_controlHover = QColor(QStringLiteral("#DDDBD5"));
        m_text = QColor(QStringLiteral("#1A1917"));
        m_textSecondary = QColor(QStringLiteral("#4E4C47"));
        m_textMuted = QColor(QStringLiteral("#7A776F"));
        m_caption = QColor(QStringLiteral("#F3F2EF"));
        m_danger = QColor(QStringLiteral("#D92D20"));
        m_warning = QColor(QStringLiteral("#B54708"));
    }

    QColor accent;
    if (m_settings->accent() == QLatin1String("custom")) {
        accent = QColor(m_settings->customAccent());
        if (!accent.isValid())
            accent = QColor(QStringLiteral("#35D69A"));
    } else {
        accent = dark ? QColor(QStringLiteral("#35D69A")) : QColor(QStringLiteral("#0E9F6E"));
        for (const AccentPair &pair : kAccents) {
            if (m_settings->accent() == QLatin1String(pair.id)) {
                accent = dark ? pair.dark : pair.light;
                break;
            }
        }
    }

    m_accent = accent;
    m_accentSoft = accent;
    m_accentSoft.setAlpha(dark ? 42 : 32);
    m_onAccent = luminance(accent) > 0.42 ? QColor(QStringLiteral("#07140F")) : QColor(QStringLiteral("#FFFFFF"));
    m_success = dark ? QColor(QStringLiteral("#35D69A")) : QColor(QStringLiteral("#0E9F6E"));
    emit changed();
}

} // namespace nuclip
