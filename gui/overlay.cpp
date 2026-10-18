#include "gui/overlay.hpp"

#include <QApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QTimer>

namespace ce::gui {

OverlayWindow::OverlayWindow(QWidget* parent) : QWidget(parent) {
    setWindowTitle("Neversnooze CE Overlay");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint | Qt::BypassWindowManagerHint |
        Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);

    if (auto* screen = QGuiApplication::primaryScreen())
        setGeometry(screen->geometry());

    // No repaint loop: the setters repaint when something changes. Redrawing
    // a full-screen translucent window 60 times a second cost the game's
    // compositor for nothing, and its count was shown as the game's FPS.
}

void OverlayWindow::setStatusText(const QString& text) {
    statusText_ = text;
    update();
}

void OverlayWindow::setEntries(const QVector<QPair<QString, QString>>& entries) {
    entries_ = entries;
    update();
}

void OverlayWindow::setOsdEnabled(bool enabled) {
    osdEnabled_ = enabled;
    update();
}

void OverlayWindow::setCrosshairEnabled(bool enabled) {
    crosshairEnabled_ = enabled;
    update();
}

void OverlayWindow::paintEvent(QPaintEvent*) {

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (osdEnabled_) {
        QFont font = painter.font();
        font.setPointSize(11);
        font.setBold(true);
        painter.setFont(font);

        QStringList lines;
        lines << QStringLiteral("Neversnooze CE");
        lines << statusText_;

        const QFontMetrics metrics(font);
        int width = 0;
        for (const auto& line : lines)
            width = std::max(width, metrics.horizontalAdvance(line));

        QRect panel(18, 18, width + 24, metrics.height() * lines.size() + 18);
        painter.setPen(QColor(17, 17, 27, 210));
        painter.setBrush(QColor(17, 17, 27, 160));
        painter.drawRoundedRect(panel, 6, 6);

        painter.setPen(QColor(205, 214, 244));
        int y = panel.top() + 10 + metrics.ascent();
        for (const auto& line : lines) {
            painter.drawText(panel.left() + 12, y, line);
            y += metrics.height();
        }

        if (!entries_.isEmpty()) {
            int entriesWidth = 0;
            for (const auto& entry : entries_)
                entriesWidth = std::max(entriesWidth, metrics.horizontalAdvance(
                    entry.first + "  " + entry.second));
            QRect entriesPanel(18, panel.bottom() + 8,
                               entriesWidth + 24, metrics.height() * entries_.size() + 18);
            painter.setPen(QColor(17, 17, 27, 210));
            painter.setBrush(QColor(17, 17, 27, 160));
            painter.drawRoundedRect(entriesPanel, 6, 6);
            int ey = entriesPanel.top() + 10 + metrics.ascent();
            for (const auto& entry : entries_) {
                painter.setPen(QColor(205, 214, 244));
                painter.drawText(entriesPanel.left() + 12, ey, entry.first);
                painter.setPen(QColor(150, 127, 238));
                painter.drawText(entriesPanel.left() + 12 + metrics.horizontalAdvance(entry.first) +
                                     metrics.horizontalAdvance("  "), ey, entry.second);
                ey += metrics.height();
            }
        }
    }

    if (crosshairEnabled_) {
        const QPoint center(width() / 2, height() / 2);
        const int gap = 5;
        const int length = 18;

        QPen shadow(QColor(17, 17, 27, 180), 4, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(shadow);
        painter.drawLine(center.x() - length, center.y(), center.x() - gap, center.y());
        painter.drawLine(center.x() + gap, center.y(), center.x() + length, center.y());
        painter.drawLine(center.x(), center.y() - length, center.x(), center.y() - gap);
        painter.drawLine(center.x(), center.y() + gap, center.x(), center.y() + length);

        QPen line(QColor(166, 227, 161), 2, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(line);
        painter.drawLine(center.x() - length, center.y(), center.x() - gap, center.y());
        painter.drawLine(center.x() + gap, center.y(), center.x() + length, center.y());
        painter.drawLine(center.x(), center.y() - length, center.x(), center.y() - gap);
        painter.drawLine(center.x(), center.y() + gap, center.x(), center.y() + length);
    }
}

} // namespace ce::gui
