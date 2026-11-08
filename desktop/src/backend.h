#pragma once
#include <QObject>
#include <QColor>
#include <QTimer>
#include <QProcess>
#include <QJsonObject>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString arch READ arch WRITE setArch NOTIFY changed)
    Q_PROPERTY(QString omarchy READ omarchy WRITE setOmarchy NOTIFY changed)
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY changed)
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString activeTheme READ activeTheme NOTIFY changed)
    Q_PROPERTY(bool hasThemePreset READ hasThemePreset NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
public:
    explicit Backend(QString directory = {}, QObject *parent = nullptr, QString pickerExecutable = {});
    ~Backend() override;
    Q_INVOKABLE bool pickScreenColor(bool archTarget);
    QString arch() const { return m_arch; }
    QString omarchy() const { return m_omarchy; }
    int brightness() const { return m_brightness; }
    QString mode() const { return m_mode; }
    QString status() const { return m_status; }
    QString activeTheme() const { return m_activeTheme; }
    bool hasThemePreset() const;
    QString notice() const { return m_notice; }
    bool dirty() const { return m_dirty; }
    void setArch(const QString &value);
    void setOmarchy(const QString &value);
    void setBrightness(int value);
    Q_INVOKABLE bool apply();
    Q_INVOKABLE bool saveToTheme(const QString &expectedTheme);
    Q_INVOKABLE bool resetTheme(const QString &expectedTheme);
    Q_INVOKABLE bool followTheme();
    Q_INVOKABLE bool useOwnColors();
    Q_INVOKABLE bool turnOff();
    Q_INVOKABLE void refresh();
    bool save(const QJsonObject &control);
    static bool validColor(const QString &value);
    QString directory() const { return m_directory; }
signals:
    void changed();
    void screenPickFinished();
private:
    void load();
    void finishScreenPick(const QString &message);
    QProcess m_picker;
    QTimer m_pickerTimeout;
    QString m_pickerExecutable;
    bool m_picking = false, m_pickArch = true;
    QString m_directory, m_arch = "#0000ff", m_omarchy = "#00ff00";
    QString m_activeTheme, m_notice;
    QString m_mode = "theme", m_status;
    int m_brightness = 96;
    bool m_dirty = false;
    QTimer m_poll;
};
