#include "signpreview.h"
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QLinearGradient>
#include <cmath>

SignPreview::SignPreview(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this, &SignPreview::appearanceChanged, this, [this] { update(); });
}

void SignPreview::paint(QPainter *p) {
    if (width() <= 0 || height() <= 0) return;
    p->setRenderHint(QPainter::Antialiasing);
    // The normalized face follows the handmade sign: Arch above a pixel wordmark.
    const qreal scale = qMin(width() / 620.0, height() / 650.0);
    p->translate((width() - 620 * scale) / 2, (height() - 650 * scale) / 2);
    p->scale(scale, scale);
    QPainterPath arch;
    arch.moveTo(310, 92);
    arch.cubicTo(304, 92, 303, 99, 299, 108);
    arch.lineTo(232, 250); arch.lineTo(261, 272); arch.lineTo(221, 260);
    arch.lineTo(121, 456);
    arch.quadTo(174, 419, 255, 407);
    arch.cubicTo(246, 371, 265, 320, 294, 320);
    arch.cubicTo(325, 320, 347, 369, 338, 406);
    arch.quadTo(414, 417, 487, 456);
    arch.lineTo(438, 358); arch.lineTo(408, 343); arch.lineTo(431, 345);
    arch.lineTo(322, 108); arch.quadTo(315, 92, 310, 92);
    arch.closeSubpath();

    // A small pixel alphabet keeps the physical Omarchy lettering recognisable.
    const char *letters[] = {
        "01110100011000110001100011000101110", // O
        "10001110111010110101100011000110001", // M
        "01110100011000111111100011000110001", // A
        "11110100011000111110101001001010001", // R
        "01111100001000010000100001000001111", // C
        "10001100011000111111100011000110001", // H
        "10001100010101000100001000010000100"  // Y
    };
    QPainterPath word;
    constexpr qreal pixel = 10;
    for (int letter = 0; letter < 7; ++letter)
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 5; ++x)
                if (letters[letter][y * 5 + x] == '1')
                    word.addRect(105 + letter * 60 + x * pixel, 478 + y * pixel, pixel, pixel);
    word = word.simplified();
    QPainterPath plaque;
    plaque.moveTo(88, 464); plaque.lineTo(145, 464); plaque.lineTo(145, 458);
    plaque.lineTo(477, 458); plaque.lineTo(477, 464); plaque.lineTo(532, 464);
    plaque.lineTo(532, 474); plaque.lineTo(542, 474); plaque.lineTo(542, 554);
    plaque.lineTo(529, 554); plaque.lineTo(529, 564); plaque.lineTo(323, 564);
    plaque.lineTo(323, 571); plaque.lineTo(283, 571); plaque.lineTo(283, 564);
    plaque.lineTo(89, 564); plaque.lineTo(89, 554); plaque.lineTo(78, 554);
    plaque.lineTo(78, 475); plaque.lineTo(88, 475); plaque.closeSubpath();

    const qreal power = m_lit ? std::sqrt(qBound(0, m_brightness, 255) / 255.0) : 0;
    auto halo = [&](const QPainterPath &path, QColor color) {
        if (!color.isValid() || power == 0) return;
        // Broad translucent strokes mimic light spilling onto the wall. Drawing
        // these before the opaque face leaves the illumination behind the sign.
        p->setBrush(Qt::NoBrush);
        for (int spread = 100; spread >= 4; spread -= 4) {
            QColor glow = color;
            glow.setAlphaF(power * .065 * (1.0 - spread / 125.0));
            p->setPen(QPen(glow, spread, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->drawPath(path);
        }
        color.setAlphaF(.65 * power);
        p->setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPath(path);
    };
    halo(arch, m_arch);
    halo(plaque, m_omarchy);
    halo(word, m_omarchy);

    auto face = [&](const QPainterPath &path) {
        QLinearGradient faceGradient(0, 100, 0, 560);
        faceGradient.setColorAt(0, QColor("#282624"));
        faceGradient.setColorAt(1, QColor("#121416"));
        p->setPen(QPen(QColor("#65543c"), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(faceGradient); p->drawPath(path);
    };
    face(arch); face(plaque);
    p->setPen(QPen(QColor("#6d5b3b"), 2));
    p->setBrush(QColor("#1c2927")); p->drawPath(word);

    // A deterministic fine texture suggests the moss-filled wooden front without
    // embedding the user's photographs or tinting the whole face like a screen.
    p->save(); p->setClipPath(arch);
    p->setPen(Qt::NoPen);
    for (int y = 100; y < 460; y += 4)
        for (int x = 120; x < 490; x += 4) {
            const unsigned hash = unsigned(x * 73856093u) ^ unsigned(y * 19349663u);
            QColor fleck(72 + hash % 18, 83 + hash % 20, 100 + hash % 23, 38);
            p->setBrush(fleck); p->drawEllipse(QPointF(x + hash % 3, y), .8, 1.7);
        }
    p->restore();
    p->save(); p->setClipPath(word);
    for (int y = 478; y < 548; y += 3)
        for (int x = 105; x < 515; x += 3) {
            const unsigned hash = unsigned(x * 73856093u) ^ unsigned(y * 19349663u);
            p->setPen(QColor(82, 113 + hash % 30, 105, 70)); p->drawPoint(x, y);
        }
    p->restore();
}
