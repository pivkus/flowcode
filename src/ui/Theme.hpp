#pragma once

#include <QWidget>
#include <QPainter>
#include <QColor>
#include <QRect>
#include <QFile>


namespace Sizing {
    constexpr int horizontal_pad = 10;
    constexpr int vertical_pad = 8;
    constexpr int corner_radius = 8;
}

struct Theme {
    QColor background;
    QColor surface;
    QColor border;
    QColor hover;

    QColor text;
    QColor text_secondary;
    QColor text_muted;
    QColor text_bright;

    QColor orange;
    QColor orange_border;
    QColor blue;
    QColor blue_border;
};

inline constexpr Theme theme{
    .background = QColor(34, 34, 34),
    .surface = QColor(39, 39, 39),
    .border = QColor(73, 73, 73),
    .hover = QColor(50, 50, 50),

    .text = QColor(237, 237, 237),
    .text_secondary = QColor(184, 184, 184),
    .text_muted = QColor(171, 171, 171),
    .text_bright = QColor(255, 255, 255),

    .orange = QColor(89, 61, 25),
    .orange_border = QColor(154, 92, 15),
    .blue = QColor(27, 97, 187),
    .blue_border = QColor(57, 138, 235),
};

// SVG assets use currentColor so their tint comes from the theme.
inline QByteArray themedSvg(const QString &path, QColor color){
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QByteArray svg = file.readAll();
    return svg.replace("currentColor", color.name().toUtf8());
}

inline QRect padded_rect(QRect original){
    return original.adjusted(Sizing::horizontal_pad, Sizing::vertical_pad,
                             -Sizing::horizontal_pad, -Sizing::vertical_pad);
}

inline void paintBackground(QPainter& painter, QRect area, QColor bg, QColor border = {}){
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bounds = QRectF(area).adjusted(0.5, 0.5, -0.5, -0.5);

    painter.setPen(border.isValid() ? QPen(border, 1.0) : QPen(Qt::NoPen));
    painter.setBrush(bg);
    painter.drawRoundedRect(bounds, Sizing::corner_radius, Sizing::corner_radius);
}
