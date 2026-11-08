#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QTextStream>
#include <QTemporaryDir>
#include <QDateTime>
#include <QJsonArray>
#include "backend.h"
#include "signpreview.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char **argv) {
    const QString command = argc > 1 ? QString::fromLocal8Bit(argv[1]) : "open";
    const bool preview = command == "preview";
    if (command != "open") qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    app.setApplicationName("arch-sign");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("arch-sign");
    QTemporaryDir scratch;
    if (preview) {
        QFile file(scratch.path() + "/status.json");
        if (!file.open(QIODevice::WriteOnly)) return 1;
        file.write(QJsonDocument(QJsonObject{{"ok", true}, {"mode", "theme"}, {"theme", "Nord"},
            {"updated_at", QDateTime::currentSecsSinceEpoch()}, {"brightness", 96},
            {"palette", QJsonObject{{"arch", QJsonArray{23,147,209}}, {"omarchy", QJsonArray{130,251,156}}}}}).toJson());
    }
    Backend backend(preview ? scratch.path() : QString());
    const QStringList args = app.arguments();
    if (command != "open" && !preview) {
        bool ok = true;
        if (command == "status") {
            QFile file(backend.directory() + "/status.json");
            if (file.open(QIODevice::ReadOnly)) { QTextStream(stdout) << file.readAll() << '\n'; return 0; }
            ok = false;
        } else if (command == "save-theme" && args.size() >= 3) ok = backend.saveToTheme(args[2]);
        else if (command == "reset-theme" && args.size() >= 3) ok = backend.resetTheme(args[2]);
        else if (command == "theme") ok = backend.followTheme();
        else if (command == "off") ok = backend.turnOff();
        else if (command == "manual" && args.size() >= 5) {
            bool valid;
            const int brightness = args[4].toInt(&valid);
            ok = valid && backend.save({{"mode", "manual"}, {"arch", args[2]}, {"omarchy", args[3]}, {"brightness", brightness}});
        } else if (command == "skill" && args.value(2) == "install") {
            const QString directory = QDir::homePath() + "/.agents/skills/arch-sign";
            QDir().mkpath(directory);
            QFile source(":/SKILL.md"); QSaveFile destination(directory + "/SKILL.md");
            ok = source.open(QIODevice::ReadOnly) && destination.open(QIODevice::WriteOnly);
            if (ok) { const auto data = source.readAll(); ok = destination.write(data) == data.size() && destination.commit(); }
        } else {
            QTextStream(stderr) << "Usage: arch-sign [open|status|theme|off|save-theme <theme>|reset-theme <theme>|manual '#rrggbb' '#rrggbb' 0..255|skill install] [--json]\n";
            return 1;
        }
        if (!ok) QTextStream(stderr) << backend.status() << '\n';
        QTextStream(stdout) << QJsonDocument(QJsonObject{{"ok", ok}}).toJson(QJsonDocument::Compact) << '\n';
        return ok ? 0 : 1;
    }
    qmlRegisterType<SignPreview>("ArchSign", 1, 0, "SignPreview");
    QQuickStyle::setStyle("Material");
    QString languagePath;
    if (preview) {
        languagePath = scratch.path() + "/language";
        QFile file(languagePath); if (!file.open(QIODevice::WriteOnly)) return 1; file.write(args.value(3, "en").toUtf8());
    }
    Translator translator(languagePath);
    app.installTranslator(&translator);
    Theme theme(preview ? args.value(4, scratch.path()) : QString());
    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    if (preview) QTimer::singleShot(1000, &app, [&] {
        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        const bool ok = window && window->grabWindow().save(args.value(2, "/tmp/arch-sign.png"));
        app.exit(ok ? 0 : 1);
    });
    return app.exec();
}
