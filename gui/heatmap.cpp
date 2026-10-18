#include "gui/heatmap.hpp"
#include "gui/theme.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>

namespace ce::gui {

class HeatmapCanvas : public QWidget {
    Q_OBJECT
public:
    HeatmapCanvas(QWidget* parent = nullptr) : QWidget(parent) {
        setMouseTracking(true);
        setMinimumSize(360, 160);
    }

    void setRows(const std::vector<HeatmapRow>* rows) { rows_ = rows; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(24, 24, 32));
        if (!rows_ || rows_->empty()) return;
        const int rowHeight = 14;
        const int rowGap = 4;
        const int topPad = 6;
        const int left = 178;
        const int maxRows = std::max(1, (height() - topPad) / (rowHeight + rowGap));
        painter.setFont(monospaceFont(-2));
        for (int i = 0; i < int(rows_->size()) && i < maxRows; ++i) {
            const auto& row = (*rows_)[size_t(i)];
            const int y = topPad + i * (rowHeight + rowGap);
            painter.setPen(QColor(160, 160, 175));
            QString name = row.name.isEmpty()
                ? QString("0x%1").arg(row.base, 0, 16) : row.name;
            if (name.size() > 22) name = name.left(19) + "...";
            painter.drawText(6, y + rowHeight - 3, name);
            const int usable = width() - left - 6;
            if (usable <= 0 || row.pageCounts.empty()) continue;
            const double pagesPerX = double(row.pageCounts.size()) / usable;
            for (int x = 0; x < usable; ++x) {
                const size_t first = size_t(x * pagesPerX);
                const size_t last = std::min(row.pageCounts.size() - 1,
                                            size_t((x + 1) * pagesPerX));
                uint32_t count = 0;
                for (size_t p = first; p <= last; ++p)
                    count = std::max(count, row.pageCounts[p]);
                if (!count) continue;
                const int alpha = std::min(255, 90 + int(count) * 30);
                painter.fillRect(left + x, y, 1, rowHeight, QColor(150, 127, 238, alpha));
            }
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        const uintptr_t address = addressAt(event->pos());
        setToolTip(address ? QString("0x%1").arg(address, 0, 16) : QString());
    }

    void mousePressEvent(QMouseEvent* event) override {
        const uintptr_t address = addressAt(event->pos());
        if (address) emit activated(address);
    }

signals:
    void activated(uintptr_t address);

private:
    uintptr_t addressAt(const QPoint& pos) const {
        if (!rows_ || rows_->empty()) return 0;
        const int rowHeight = 14, rowGap = 4, topPad = 6, left = 178;
        const int row = (pos.y() - topPad) / (rowHeight + rowGap);
        if (row < 0 || row >= int(rows_->size())) return 0;
        const auto& r = (*rows_)[size_t(row)];
        const int usable = width() - left - 6;
        if (pos.x() < left || usable <= 0 || r.pageCounts.empty()) return 0;
        const size_t page = size_t((pos.x() - left) * double(r.pageCounts.size()) / usable);
        if (page >= r.pageCounts.size()) return 0;
        return r.base + page * 4096;
    }

    const std::vector<HeatmapRow>* rows_ = nullptr;
};

HeatmapDialog::HeatmapDialog(ce::ProcessHandle* proc,
                             std::function<void(uintptr_t)> openAt,
                             QWidget* parent)
    : QDialog(parent), proc_(proc), openAt_(std::move(openAt)) {
    setWindowTitle("Memory heatmap");
    resize(680, 420);

    auto* root = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout;
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    intervalSpin_ = new QSpinBox;
    intervalSpin_->setRange(500, 60000);
    intervalSpin_->setSingleStep(500);
    intervalSpin_->setValue(2000);
    form->addRow("Interval (ms):", intervalSpin_);
    controls->addLayout(form);
    startButton_ = new QPushButton("Start");
    startButton_->setProperty("primary", true);
    controls->addWidget(startButton_);
    auto* clearButton = new QPushButton("Clear");
    controls->addWidget(clearButton);
    controls->addStretch(1);
    root->addLayout(controls);

    canvas_ = new HeatmapCanvas(this);
    canvas_->setRows(&rows_);
    connect(canvas_, &HeatmapCanvas::activated, this, [this](uintptr_t address) {
        if (openAt_) openAt_(address);
    });
    root->addWidget(canvas_, 1);

    statusLabel_ = new QLabel("Repeatedly snapshots the writable memory and highlights the pages that change. Click a cell to open it in the Memory Viewer.");
    statusLabel_->setProperty("secondary", true);
    statusLabel_->setWordWrap(true);
    root->addWidget(statusLabel_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, [this]() {
        stop_ = true;
        if (worker_.joinable()) worker_.join();
        accept();
    });
    root->addWidget(buttons);

    if (!proc_) {
        startButton_->setEnabled(false);
        statusLabel_->setText("Open a process first.");
    }

    connect(startButton_, &QPushButton::clicked, this, &HeatmapDialog::onStartStop);
    connect(clearButton, &QPushButton::clicked, this, &HeatmapDialog::clear);
}

HeatmapDialog::~HeatmapDialog() {
    stop_ = true;
    if (worker_.joinable()) worker_.join();
}

void HeatmapDialog::onStartStop() {
    if (!stop_) {
        stop_ = true;
        if (worker_.joinable()) worker_.join();
        startButton_->setText("Start");
        statusLabel_->setText(QString("Stopped. %1 snapshot pair(s), %2 changed pages.")
                                  .arg(snapshotCount_).arg(changedPages_));
        return;
    }
    if (!proc_) return;
    stop_ = false;
    startButton_->setText("Stop");
    statusLabel_->setText("Capturing...");
    auto* proc = proc_;
    const int interval = intervalSpin_->value();
    auto& stop = stop_;
    worker_ = std::thread([this, proc, interval, &stop]() {
        std::optional<ce::Snapshot> baseline;
        while (!stop) {
            auto capture = ce::Snapshot::capture(*proc, 256 * 1024 * 1024);
            if (baseline) {
                auto pages = baseline->pageDiff(capture);
                QMetaObject::invokeMethod(this, [this, pages]() {
                    accumulate(pages);
                }, Qt::QueuedConnection);
            }
            baseline.emplace(std::move(capture));
            for (int waited = 0; !stop && waited < interval; waited += 100)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
}

void HeatmapDialog::accumulate(std::vector<ce::Snapshot::PageDiff> pages) {
    ++snapshotCount_;
    changedPages_ += int(pages.size());
    for (const auto& page : pages) {
        bool found = false;
        for (auto& row : rows_) {
            if (page.base >= row.base && page.base < row.base + row.size) {
                const size_t index = size_t((page.base - row.base) / 4096);
                if (index < row.pageCounts.size())
                    row.pageCounts[index] = std::min<uint32_t>(row.pageCounts[index] + 1, 255);
                found = true;
                break;
            }
        }
        if (!found && int(rows_.size()) < regionLimit_) {
            uintptr_t regionBase = page.base & ~uintptr_t{4095};
            uint64_t regionSize = 4096;
            if (auto region = proc_->queryRegion(page.base)) {
                regionBase = region->base;
                regionSize = region->size;
            }
            HeatmapRow row;
            row.base = regionBase;
            row.size = regionSize;
            row.pageCounts.assign(size_t((regionSize + 4095) / 4096), 0);
            for (const auto& m : proc_->modules())
                if (row.base >= m.base && row.base < m.base + m.size)
                    row.name = QString::fromStdString(m.name);
            const size_t index = size_t((page.base - row.base) / 4096);
            if (index < row.pageCounts.size()) row.pageCounts[index] = 1;
            rows_.push_back(std::move(row));
        }
    }
    std::sort(rows_.begin(), rows_.end(), [](const HeatmapRow& a, const HeatmapRow& b) {
        uint64_t aTotal = 0, bTotal = 0;
        for (uint32_t c : a.pageCounts) aTotal += c;
        for (uint32_t c : b.pageCounts) bTotal += c;
        if (aTotal != bTotal) return aTotal > bTotal;
        return a.base < b.base;
    });
    canvas_->update();
    statusLabel_->setText(QString("%1 snapshot pair(s), %2 changed pages. Click a cell to open it in the Memory Viewer.")
                              .arg(snapshotCount_).arg(changedPages_));
}

void HeatmapDialog::clear() {
    rows_.clear();
    changedPages_ = 0;
    snapshotCount_ = 0;
    canvas_->update();
    statusLabel_->setText("Cleared.");
}

int HeatmapDialog::changedPageCountForTest() const { return changedPages_; }

} // namespace ce::gui

#include "heatmap.moc"
