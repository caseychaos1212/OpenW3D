#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QStringList>

#include "MainWindow.h"
#include "WWConfigBackend.h"
#include "../WWConfig/wwconfig_ids.h"

namespace
{
constexpr int kInvalidLanguage = -999;

int LanguageFromString(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == "english" || normalized == "en") {
        return IDL_ENGLISH;
    }
    if (normalized == "french" || normalized == "fr") {
        return IDL_FRENCH;
    }
    if (normalized == "german" || normalized == "de") {
        return IDL_GERMAN;
    }
    if (normalized == "japanese" || normalized == "jp" || normalized == "ja") {
        return IDL_JAPANESE;
    }
    if (normalized == "korean" || normalized == "ko") {
        return IDL_KOREAN;
    }
    if (normalized == "chinese" || normalized == "zh") {
        return IDL_CHINESE;
    }
    if (normalized.isEmpty()) {
        return -1;
    }
    return kInvalidLanguage;
}
} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("WWConfigQt"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Renegade configuration (Qt prototype)"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption languageOption(
        {QStringLiteral("l"), QStringLiteral("language")},
        QObject::tr("Override the UI language (english, french, german, japanese, korean, chinese)."),
        QObject::tr("language"));
    parser.addOption(languageOption);

    parser.process(app);

    int languageOverride = -1;
    if (parser.isSet(languageOption)) {
        languageOverride = LanguageFromString(parser.value(languageOption));
        if (languageOverride == kInvalidLanguage) {
            qWarning() << "Unknown language override:" << parser.value(languageOption);
            languageOverride = -1;
        }
    }

    WWConfigBackend backend;
    if (!backend.initializeLocale(languageOverride)) {
        qWarning() << "Failed to initialize locale bank, continuing with built-in strings.";
    }

    MainWindow window(backend);
    window.show();
    return app.exec();
}
