#pragma once

#include <QString>

namespace nuclip {

struct AppFacts {
    QString exeName;
    QString exePath;
    QString windowTitle;
    QString windowClass;
    QString fileDescription;
    QString productName;
    QString companyName;
    quint32 pid = 0;
};

struct AppMatch {
    QString displayName;
    QString stableId;
    bool recognized = false;
};

class AppCatalog {
public:
    static AppMatch resolve(const AppFacts &facts);
    static QString displayForStored(const QString &stored);
    static QString fileToken(const QString &displayName);
    static bool isLauncher(const QString &exeName);
    static bool isRuntime(const QString &exeName);
    static bool isIgnored(const QString &exeName);
    static bool isGame(const QString &stableId);
};

} // namespace nuclip
