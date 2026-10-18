#pragma once

#include <QDialog>
#include <QString>
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

#include "scanner/snapshot.hpp"

class QLabel;
class QPushButton;
class QSpinBox;

namespace ce {
class ProcessHandle;
}

namespace ce::gui {

struct HeatmapRow {
    uint64_t base = 0;
    uint64_t size = 0;
    QString name;
    std::vector<uint32_t> pageCounts;
};

class HeatmapCanvas;

class HeatmapDialog : public QDialog {
    Q_OBJECT
public:
    HeatmapDialog(ce::ProcessHandle* proc,
                  std::function<void(uintptr_t)> openAt,
                  QWidget* parent = nullptr);
    ~HeatmapDialog() override;

    int changedPageCountForTest() const;
    void setRegionLimitForTest(int limit) { regionLimit_ = limit; }

private:
    void onStartStop();
    void clear();
    void accumulate(std::vector<ce::Snapshot::PageDiff> pages);

    ce::ProcessHandle* proc_ = nullptr;
    std::function<void(uintptr_t)> openAt_;
    QSpinBox* intervalSpin_ = nullptr;
    QPushButton* startButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    HeatmapCanvas* canvas_ = nullptr;
    std::vector<HeatmapRow> rows_;
    int regionLimit_ = 24;
    std::atomic<bool> stop_{true};
    std::thread worker_;
    int snapshotCount_ = 0;
    int changedPages_ = 0;
};

} // namespace ce::gui
