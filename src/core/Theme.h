#pragma once

#include <QColor>
#include <QObject>
#include <QString>

namespace nuclip {

class Settings;

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor sidebar READ sidebar NOTIFY changed)
    Q_PROPERTY(QColor content READ content NOTIFY changed)
    Q_PROPERTY(QColor card READ card NOTIFY changed)
    Q_PROPERTY(QColor elevated READ elevated NOTIFY changed)
    Q_PROPERTY(QColor cardBorder READ cardBorder NOTIFY changed)
    Q_PROPERTY(QColor control READ control NOTIFY changed)
    Q_PROPERTY(QColor controlHover READ controlHover NOTIFY changed)
    Q_PROPERTY(QColor text READ text NOTIFY changed)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY changed)
    Q_PROPERTY(QColor textMuted READ textMuted NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor accentSoft READ accentSoft NOTIFY changed)
    Q_PROPERTY(QColor onAccent READ onAccent NOTIFY changed)
    Q_PROPERTY(QColor danger READ danger NOTIFY changed)
    Q_PROPERTY(QColor warning READ warning NOTIFY changed)
    Q_PROPERTY(QColor success READ success NOTIFY changed)
    Q_PROPERTY(QColor caption READ caption NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)
    Q_PROPERTY(QString displayFamily READ displayFamily CONSTANT)
    Q_PROPERTY(QString iconFamily READ iconFamily CONSTANT)
    Q_PROPERTY(QString monoFamily READ monoFamily CONSTANT)
    Q_PROPERTY(int titleSize READ titleSize CONSTANT)
    Q_PROPERTY(int bodySize READ bodySize CONSTANT)
    Q_PROPERTY(int labelSize READ labelSize CONSTANT)
    Q_PROPERTY(int smallSize READ smallSize CONSTANT)

public:
    explicit Theme(Settings *settings, QObject *parent = nullptr);

    bool dark() const { return m_dark; }
    QColor background() const { return m_background; }
    QColor sidebar() const { return m_sidebar; }
    QColor content() const { return m_content; }
    QColor card() const { return m_card; }
    QColor elevated() const { return m_elevated; }
    QColor cardBorder() const { return m_cardBorder; }
    QColor control() const { return m_control; }
    QColor controlHover() const { return m_controlHover; }
    QColor text() const { return m_text; }
    QColor textSecondary() const { return m_textSecondary; }
    QColor textMuted() const { return m_textMuted; }
    QColor accent() const { return m_accent; }
    QColor accentSoft() const { return m_accentSoft; }
    QColor onAccent() const { return m_onAccent; }
    QColor danger() const { return m_danger; }
    QColor warning() const { return m_warning; }
    QColor success() const { return m_success; }
    QColor caption() const { return m_caption; }
    QString fontFamily() const { return m_fontFamily; }
    QString displayFamily() const { return m_displayFamily; }
    QString iconFamily() const { return m_iconFamily; }
    QString monoFamily() const { return m_monoFamily; }
    int titleSize() const { return 20; }
    int bodySize() const { return 13; }
    int labelSize() const { return 12; }
    int smallSize() const { return 11; }

signals:
    void changed();

private:
    void rebuild();
    bool systemIsDark() const;

    Settings *m_settings = nullptr;
    bool m_dark = true;
    QColor m_background;
    QColor m_sidebar;
    QColor m_content;
    QColor m_card;
    QColor m_elevated;
    QColor m_cardBorder;
    QColor m_control;
    QColor m_controlHover;
    QColor m_text;
    QColor m_textSecondary;
    QColor m_textMuted;
    QColor m_accent;
    QColor m_accentSoft;
    QColor m_onAccent;
    QColor m_danger;
    QColor m_warning;
    QColor m_success;
    QColor m_caption;
    QString m_fontFamily;
    QString m_displayFamily;
    QString m_iconFamily;
    QString m_monoFamily;
};

} // namespace nuclip
