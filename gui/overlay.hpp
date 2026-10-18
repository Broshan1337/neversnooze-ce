#pragma once

#include <QElapsedTimer>
#include <QPair>
#include <QString>
#include <QVector>
#include <QWidget>

namespace ce::gui {

class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    explicit OverlayWindow(QWidget* parent = nullptr);

    void setStatusText(const QString& text);
    void setEntries(const QVector<QPair<QString, QString>>& entries);
    void setOsdEnabled(bool enabled);
    void setCrosshairEnabled(bool enabled);
    bool osdEnabled() const { return osdEnabled_; }
    bool crosshairEnabled() const { return crosshairEnabled_; }
    int entryCountForTest() const { return entries_.size(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString statusText_ = "No process";
    QVector<QPair<QString, QString>> entries_;
    bool osdEnabled_ = true;
    bool crosshairEnabled_ = true;
};

} // namespace ce::gui
