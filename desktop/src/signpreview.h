#pragma once
#include <QQuickPaintedItem>
#include <QColor>

class SignPreview : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QColor archColor MEMBER m_arch NOTIFY appearanceChanged)
    Q_PROPERTY(QColor omarchyColor MEMBER m_omarchy NOTIFY appearanceChanged)
    Q_PROPERTY(int brightness MEMBER m_brightness NOTIFY appearanceChanged)
    Q_PROPERTY(bool lit MEMBER m_lit NOTIFY appearanceChanged)
public:
    explicit SignPreview(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
signals:
    void appearanceChanged();
private:
    QColor m_arch = QColor("#1793d1"), m_omarchy = QColor("#82fb9c");
    int m_brightness = 96;
    bool m_lit = true;
};
