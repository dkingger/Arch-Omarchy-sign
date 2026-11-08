#include <QtTest>
#include <QDesktopServices>
#include <QDateTime>
#include <QTemporaryDir>
#include <QPainter>
#include <QSaveFile>
#include <QFile>
#include <QJsonDocument>
#include "../src/backend.h"
#include "../src/signpreview.h"
#include "../src/theme.h"
#include "../src/translator.h"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
class UrlReceiver : public QObject {
    Q_OBJECT
public:
    QUrl url;
public slots:
    void capture(const QUrl &value) { url = value; }
};
class SignTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { qmlRegisterType<SignPreview>("ArchSign", 1, 0, "SignPreview"); }
    void themeFollowsPaletteReplacementWithUnchangedAccent() {
        QTemporaryDir dir; QDir().mkpath(dir.path() + "/theme");
        const QString path = dir.path() + "/theme/colors.toml";
        auto write = [&](const QByteArray &data) {
            QSaveFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(data), data.size()); QVERIFY(file.commit());
        };
        write("mode = \"dark\"\nbackground = \"#101010\"\nforeground = \"#eeeeee\"\naccent = \"#ff0000\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.colors().value("background").toString(), QString("#101010"));
        QVERIFY(!theme.light());
        write("mode = \"light\"\nbackground = \"#fafafa\"\nforeground = \"#222222\"\naccent = \"#ff0000\"\n");
        QTRY_COMPARE(theme.colors().value("background").toString(), QString("#fafafa"));
        QVERIFY(theme.light());
        QCOMPARE(theme.colors().value("foreground").toString(), QString("#222222"));
        QCOMPARE(theme.accent(), QString("#ff0000"));
    }
    void savesAndResetsOnlyTheSelectedTheme() {
        QTemporaryDir dir;
        QFile status(dir.path() + "/status.json"); QVERIFY(status.open(QIODevice::WriteOnly));
        status.write(QJsonDocument(QJsonObject{{"ok", true}, {"theme", "retro-82"},
            {"mode", "manual"}, {"updated_at", QDateTime::currentSecsSinceEpoch()}}).toJson()); status.close();
        Backend backend(dir.path());
        backend.setArch("#112233"); backend.setOmarchy("#abcdef"); backend.setBrightness(145);
        QVERIFY(backend.apply()); QVERIFY(backend.saveToTheme("retro-82"));
        QVERIFY(backend.hasThemePreset());
        QVERIFY(!backend.resetTheme("different-theme"));
        QVERIFY(backend.hasThemePreset());
        QVERIFY(backend.followTheme());
        Backend restored(dir.path()); QVERIFY(restored.hasThemePreset());
        QFile file(dir.path() + "/control.json"); QVERIFY(file.open(QIODevice::ReadOnly));
        auto presets = QJsonDocument::fromJson(file.readAll()).object().value("theme_presets").toObject();
        file.close();
        QCOMPARE(presets.value("retro-82").toObject().value("brightness").toInt(), 145);
        // Preserve presets belonging to other themes while resetting the active one.
        presets.insert("nord", QJsonObject{{"arch", "#445566"}});
        QVERIFY(backend.save({{"mode", "theme"}, {"theme_presets", presets}}));
        QVERIFY(backend.resetTheme("retro-82"));
        QVERIFY(!backend.hasThemePreset());
        QVERIFY(file.open(QIODevice::ReadOnly));
        presets = QJsonDocument::fromJson(file.readAll()).object().value("theme_presets").toObject();
        QVERIFY(presets.contains("nord"));
        QCOMPARE(backend.mode(), QString("theme"));
    }
    void desktopPickerUpdatesOnlyTheSelectedLogo() {
        QTemporaryDir dir;
        const QString executable = dir.path() + "/picker";
        QFile file(executable); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#!/bin/sh\nfor arg in \"$@\"; do [ \"$arg\" = --quiet ] && exit 0; done\nprintf '[LOG] Starting picker\\n#12AB34\\n[LOG] Finished\\n'\n"); file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        Backend backend(dir.path(), nullptr, executable);
        QVERIFY(backend.apply());
        const auto oldOmarchy = backend.omarchy();
        QSignalSpy finished(&backend, &Backend::screenPickFinished);
        QVERIFY(backend.pickScreenColor(true));
        QVERIFY(!backend.pickScreenColor(false));
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(backend.arch(), QString("#12ab34"));
        QCOMPARE(backend.omarchy(), oldOmarchy);
        QVERIFY(backend.dirty());
        QVERIFY(backend.pickScreenColor(false));
        QTRY_COMPARE(finished.count(), 2);
        QCOMPARE(backend.omarchy(), QString("#12ab34"));
    }
    void desktopPickerFailureKeepsExistingColors() {
        QTemporaryDir dir;
        Backend backend(dir.path(), nullptr, dir.path() + "/missing");
        QVERIFY(backend.apply());
        const auto original = backend.arch();
        QSignalSpy finished(&backend, &Backend::screenPickFinished);
        QVERIFY(backend.pickScreenColor(true));
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(backend.arch(), original);
        QVERIFY(!backend.dirty());
    }
    void desktopPickerCancellationKeepsExistingColors() {
        QTemporaryDir dir;
        const QString executable = dir.path() + "/cancel";
        QFile file(executable); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#!/bin/sh\nexit 1\n"); file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        Backend backend(dir.path(), nullptr, executable); QVERIFY(backend.apply());
        const auto original = backend.arch();
        QSignalSpy finished(&backend, &Backend::screenPickFinished);
        QVERIFY(backend.pickScreenColor(true));
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(backend.arch(), original); QVERIFY(!backend.dirty());
    }
    void previewGlowsOutsideTheFaceAndTurnsOff() {
        SignPreview preview;
        preview.setWidth(620); preview.setHeight(650);
        preview.setProperty("archColor", QColor("#0000ff"));
        auto render = [&] {
            QImage image(620, 650, QImage::Format_ARGB32_Premultiplied);
            image.fill(QColor("#0b0e14"));
            QPainter painter(&image); preview.paint(&painter); painter.end();
            return image;
        };
        const auto lit = render();
        preview.setProperty("lit", false);
        const auto off = render();
        QVERIFY(lit.pixelColor(310, 80).blue() > off.pixelColor(310, 80).blue());
        preview.setProperty("lit", true); preview.setProperty("brightness", 0);
        QCOMPARE(render(), off);
    }
    void qmlButtonsApplyManualAndRestoreTheme() {
        QTemporaryDir dir;
        Backend backend(dir.path()); Theme theme(dir.path()); Translator translator(dir.path() + "/language");
        QQuickStyle::setStyle("Material");
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto root = engine.rootObjects().first();
        auto shortcuts = root->findChild<QObject *>("shortcutsButton");
        auto help = root->findChild<QObject *>("helpDialog");
        QVERIFY(shortcuts); QVERIFY(help);
        QVERIFY(QMetaObject::invokeMethod(shortcuts, "clicked"));
        QCOMPARE(help->property("visible").toBool(), true);
        QVERIFY(QMetaObject::invokeMethod(help, "close"));
        UrlReceiver receiver;
        QDesktopServices::setUrlHandler("https", &receiver, "capture");
        auto github = root->findChild<QObject *>("githubLink");
        QVERIFY(github); QVERIFY(QMetaObject::invokeMethod(github, "clicked"));
        QDesktopServices::unsetUrlHandler("https");
        QCOMPARE(receiver.url, QUrl("https://github.com/dkingger"));

        auto manual = root->findChild<QObject *>("manualMode");
        QVERIFY(manual); QVERIFY(QMetaObject::invokeMethod(manual, "clicked"));
        QCOMPARE(backend.mode(), QString("manual"));
        backend.setArch("#112233"); backend.setOmarchy("#abcdef");
        auto apply = root->findChild<QObject *>("applyButton");
        QVERIFY(apply); QVERIFY(QMetaObject::invokeMethod(apply, "clicked"));
        QCOMPARE(backend.mode(), QString("manual"));
        Backend restored(dir.path()); QCOMPARE(restored.arch(), QString("#112233"));
        auto themeButton = root->findChild<QObject *>("themeButton");
        QVERIFY(themeButton); QVERIFY(QMetaObject::invokeMethod(themeButton, "clicked"));
        QCOMPARE(backend.mode(), QString("theme"));
        QVERIFY(QMetaObject::invokeMethod(manual, "clicked"));
        QCOMPARE(backend.arch(), QString("#112233"));
        QCOMPARE(backend.omarchy(), QString("#abcdef"));
        auto off = root->findChild<QObject *>("offButton");
        QVERIFY(off); QVERIFY(QMetaObject::invokeMethod(off, "clicked"));
        QCOMPARE(backend.mode(), QString("off"));
    }
    void manualChoiceSurvivesRestart() {
        QTemporaryDir dir;
        Backend backend(dir.path());
        backend.setArch("#123456"); backend.setOmarchy("#abcdef"); backend.setBrightness(150);
        QVERIFY(backend.apply());
        Backend restored(dir.path());
        QCOMPARE(restored.mode(), QString("manual"));
        QCOMPARE(restored.arch(), QString("#123456"));
        QCOMPARE(restored.brightness(), 150);
    }
    void invalidChoiceDoesNotReplacePrevious() {
        QTemporaryDir dir;
        Backend backend(dir.path());
        QVERIFY(backend.turnOff());
        backend.setArch("broken");
        QVERIFY(!backend.apply());
        Backend restored(dir.path());
        QCOMPARE(restored.mode(), QString("off"));
        QVERIFY(backend.followTheme());
        QCOMPARE(backend.mode(), QString("theme"));
    }
    void negativeBrightnessRejected() {
        QTemporaryDir dir; Backend backend(dir.path());
        backend.setBrightness(-1); QVERIFY(!backend.apply());
        QVERIFY(!QFile::exists(dir.path() + "/control.json"));
    }
};
QTEST_MAIN(SignTests)
#include "arch-sign_tests.moc"
