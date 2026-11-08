#include "backend.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

static QJsonObject readObject(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

Backend::Backend(QString directory, QObject *parent, QString pickerExecutable) : QObject(parent),
    m_directory(directory.isEmpty() ? QDir::homePath() + "/.local/state/arch-sign" : directory) {
    m_configFile = directory.isEmpty() ? QDir::homePath() + "/.config/arch-sign/config.json"
                                      : m_directory + "/config.json";
    m_setupTimeout.setSingleShot(true); m_setupTimeout.setInterval(90000);
    connect(&m_setupTimeout, &QTimer::timeout, this, [this] { m_setup.kill(); });
    connect(&m_setup, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_setupBusy) {
            m_setupBusy = false; m_setupTimeout.stop();
            m_notice = tr("Could not start setup. Reinstall the Arch Sign package.");
            emit changed(); emit setupFinished(false);
        }
    });
    connect(&m_setup, &QProcess::finished, this, [this](int code, QProcess::ExitStatus exitStatus) {
        if (!m_setupBusy) return;
        m_setupBusy = false; m_setupTimeout.stop();
        const auto result = QJsonDocument::fromJson(m_setup.readAllStandardOutput()).object();
        const bool ok = code == 0 && exitStatus == QProcess::NormalExit && result.value("ok").toBool();
        const auto error = result.value("error").toString(QString::fromUtf8(m_setup.readAllStandardError()));
        refresh();
        m_notice = ok ? tr("Setup complete. Waiting for the sign…") : tr("Setup failed: %1").arg(error);
        emit changed(); emit setupFinished(ok);
    });
    m_pickerExecutable = pickerExecutable.isEmpty() ? QStandardPaths::findExecutable("hyprpicker") : pickerExecutable;
    m_pickerTimeout.setSingleShot(true);
    m_pickerTimeout.setInterval(120000);
    connect(&m_pickerTimeout, &QTimer::timeout, this, [this] { m_picker.kill(); });
    connect(&m_picker, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finishScreenPick(tr("Could not start the desktop color picker."));
    });
    connect(&m_picker, &QProcess::finished, this, [this](int code, QProcess::ExitStatus exitStatus) {
        if (!m_picking) return;
        const QString output = QString::fromUtf8(m_picker.readAllStandardOutput());
        QString color;
        // Hyprpicker writes both diagnostics and the selected hex color to stdout.
        for (const QString &line : output.split('\n'))
            if (validColor(line.trimmed())) color = line.trimmed();
        if (code == 0 && exitStatus == QProcess::NormalExit && validColor(color)) {
            if (m_pickArch) setArch(color.toLower()); else setOmarchy(color.toLower());
            finishScreenPick(tr("Color picked. Apply your colors when ready."));
        } else if (code == 0 && !output.trimmed().isEmpty()) {
            finishScreenPick(tr("The color picker returned an invalid color."));
        } else finishScreenPick(tr("Color picking cancelled."));
    });
    load();
    connect(&m_poll, &QTimer::timeout, this, &Backend::refresh);
    m_poll.start(1000);
    refresh();
}

Backend::~Backend() {
    if (m_setup.state() != QProcess::NotRunning) { m_setup.kill(); m_setup.waitForFinished(1000); }
    if (m_picker.state() != QProcess::NotRunning) {
        m_picker.kill(); m_picker.waitForFinished(1000);
    }
}

bool Backend::configureDevice(const QString &value, bool bar) {
    if (m_setupBusy) return false;
    const QUrl url(value.trimmed());
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https") || url.host().isEmpty()) {
        m_notice = tr("Enter an address like http://192.168.1.120"); emit changed(); return false;
    }
    m_setupBusy = true; m_notice.clear(); emit changed();
    QStringList arguments{"--url", value.trimmed(), "--json"};
    if (bar) arguments << "--bar";
    m_setup.start("arch-sign-setup", arguments); m_setupTimeout.start();
    return true;
}

void Backend::finishScreenPick(const QString &message) {
    if (!m_picking) return;
    m_picking = false; m_pickerTimeout.stop();
    m_notice = message; emit changed(); emit screenPickFinished();
}

bool Backend::pickScreenColor(bool archTarget) {
    if (m_picking || m_mode != "manual") return false;
    if (m_pickerExecutable.isEmpty()) {
        m_notice = tr("Install hyprpicker to pick desktop colors."); emit changed(); return false;
    }
    m_picking = true; m_pickArch = archTarget;
    m_notice = tr("Click a pixel on the desktop. Press Esc to cancel."); emit changed();
    // Let the window disappear before hyprpicker captures the desktop.
    QTimer::singleShot(200, this, [this] {
        if (!m_picking) return;
        m_picker.start(m_pickerExecutable, {"--format=hex", "--no-fancy"});
        m_pickerTimeout.start();
    });
    return true;
}

bool Backend::validColor(const QString &value) {
    return QRegularExpression("^#[0-9a-fA-F]{6}$").match(value).hasMatch();
}
void Backend::setArch(const QString &value) {
    if (m_arch == value) return;
    m_arch = value; m_dirty = true; emit changed();
}
void Backend::setOmarchy(const QString &value) {
    if (m_omarchy == value) return;
    m_omarchy = value; m_dirty = true; emit changed();
}
void Backend::setBrightness(int value) {
    if (m_brightness == value) return;
    m_brightness = value; m_dirty = true; emit changed();
}
void Backend::load() {
    const auto control = readObject(m_directory + "/control.json");
    m_mode = control.value("mode").toString("theme");
    if (m_mode == "manual") {
        m_arch = control.value("arch").toString(m_arch);
        m_omarchy = control.value("omarchy").toString(m_omarchy);
        m_brightness = control.value("brightness").toInt(96);
    }
}

bool Backend::save(const QJsonObject &control) {
    const QString mode = control.value("mode").toString();
    if (mode != "manual" && mode != "theme" && mode != "off") {
        m_status = tr("Unknown mode."); emit changed(); return false;
    }
    if (mode == "manual" && (!validColor(control.value("arch").toString()) ||
        !validColor(control.value("omarchy").toString()) ||
        control.value("brightness").toInt(-1) < 0 || control.value("brightness").toInt(256) > 255)) {
        m_status = tr("Use colors like #0088ff and brightness from 0 to 255.");
        emit changed(); return false;
    }
    QDir().mkpath(m_directory);
    QSaveFile file(m_directory + "/control.json");
    QJsonObject stored = control;
    const auto oldControl = readObject(m_directory + "/control.json");
    if (!stored.contains("theme_presets") && oldControl.contains("theme_presets"))
        stored.insert("theme_presets", oldControl.value("theme_presets"));
    if (mode != "manual") {
        const auto previous = readObject(m_directory + "/control.json");
        for (const QString &key : {QString("arch"), QString("omarchy"), QString("brightness")})
            if (previous.contains(key)) stored.insert(key, previous.value(key));
    }
    const auto data = QJsonDocument(stored).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        m_status = tr("Could not save: %1").arg(file.errorString()); emit changed(); return false;
    }
    m_mode = mode; m_dirty = false;
    m_status = tr("Waiting for the sign…");
    emit changed(); return true;
}
bool Backend::hasThemePreset() const {
    return readObject(m_directory + "/control.json").value("theme_presets").toObject().contains(m_activeTheme);
}

bool Backend::saveToTheme(const QString &expectedTheme) {
    refresh();
    if (expectedTheme.isEmpty() || expectedTheme != m_activeTheme) {
        m_notice = tr("The active theme changed. Please try again."); emit changed(); return false;
    }
    auto presets = readObject(m_directory + "/control.json").value("theme_presets").toObject();
    presets.insert(m_activeTheme, QJsonObject{{"arch", m_arch}, {"omarchy", m_omarchy}, {"brightness", m_brightness}});
    if (!save({{"mode", "manual"}, {"arch", m_arch}, {"omarchy", m_omarchy},
               {"brightness", m_brightness}, {"theme_presets", presets}})) return false;
    m_notice = tr("Saved colors for %1.").arg(m_activeTheme); emit changed(); return true;
}

bool Backend::resetTheme(const QString &expectedTheme) {
    refresh();
    if (expectedTheme.isEmpty() || expectedTheme != m_activeTheme) {
        m_notice = tr("The active theme changed. Please try again."); emit changed(); return false;
    }
    auto presets = readObject(m_directory + "/control.json").value("theme_presets").toObject();
    presets.remove(m_activeTheme);
    if (!save({{"mode", "theme"}, {"theme_presets", presets}})) return false;
    m_notice = tr("Using automatic colors for %1.").arg(m_activeTheme); emit changed(); return true;
}

bool Backend::apply() {
    return save({{"mode", "manual"}, {"arch", m_arch}, {"omarchy", m_omarchy}, {"brightness", m_brightness}});
}
bool Backend::useOwnColors() {
    const auto previous = readObject(m_directory + "/control.json");
    if (previous.contains("arch")) m_arch = previous.value("arch").toString();
    if (previous.contains("omarchy")) m_omarchy = previous.value("omarchy").toString();
    if (previous.contains("brightness")) m_brightness = previous.value("brightness").toInt(96);
    return apply();
}
bool Backend::followTheme() { return save({{"mode", "theme"}}); }
bool Backend::turnOff() { return save({{"mode", "off"}}); }

void Backend::refresh() {
    m_deviceUrl = readObject(m_configFile).value("esp32_base_url").toString();
    const auto status = readObject(m_directory + "/status.json");
    QString text;
    const auto age = QDateTime::currentSecsSinceEpoch() - status.value("updated_at").toDouble();
    const QString active = status.value("ok").toBool() && age <= 20 ? status.value("theme").toString() : QString();
    if (active != m_activeTheme) { m_activeTheme = active; m_notice.clear(); }
    if (status.isEmpty() || age > 20) text = tr("Background service is not responding.");
    else if (!status.value("ok").toBool()) text = tr("Connection error: %1").arg(status.value("error").toString());
    else if (status.value("mode").toString() != m_mode) text = tr("Waiting for the sign…");
    else text = tr("Connected · %1").arg(status.value("theme").toString());
    // Refresh theme previews without overwriting a color the user is editing.
    if (!m_dirty && m_mode == "theme" && status.value("ok").toBool() && status.value("mode") == "theme") {
        const auto palette = status.value("palette").toObject();
        auto color = [&](const QString &name, const QString &fallback) {
            const auto rgb = palette.value(name).toArray();
            return rgb.size() == 3 ? QColor(rgb[0].toInt(), rgb[1].toInt(), rgb[2].toInt()).name() : fallback;
        };
        m_arch = color("arch", m_arch); m_omarchy = color("omarchy", m_omarchy);
        m_brightness = status.value("brightness").toInt(96);
    }
    m_status = text; emit changed();
}
