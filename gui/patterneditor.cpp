#include "gui/patterneditor.hpp"

#include "gui/theme.hpp"
#include "scanner/memory_scanner.hpp"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <thread>

namespace ce::gui {

PatternEditorDialog::PatternEditorDialog(ce::ProcessHandle* proc,
                                         std::function<void(uintptr_t)> openAt,
                                         QWidget* parent)
    : QDialog(parent), proc_(proc), openAt_(std::move(openAt)) {
    setWindowTitle("Pattern editor");
    resize(760, 480);

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    patternEdit_ = new QLineEdit;
    patternEdit_->setFont(monospaceFont());
    patternEdit_->setPlaceholderText("48 8B ?? 05 7F 45 4C 46");
    patternEdit_->setMinimumWidth(320);
    form->addRow("Pattern:", patternEdit_);

    moduleCombo_ = new QComboBox;
    form->addRow("Search in:", moduleCombo_);

    maxResultsSpin_ = new QSpinBox;
    maxResultsSpin_->setRange(10, 100000);
    maxResultsSpin_->setSingleStep(10);
    maxResultsSpin_->setValue(1000);
    form->addRow("Max results:", maxResultsSpin_);

    root->addLayout(form);

    auto* buttonRow = new QHBoxLayout;
    scanButton_ = new QPushButton("Scan");
    scanButton_->setProperty("primary", true);
    buttonRow->addWidget(scanButton_);
    auto* copyLua = new QPushButton("Copy Lua scan");
    auto* copyBytes = new QPushButton("Copy C bytes + mask");
    buttonRow->addWidget(copyLua);
    buttonRow->addWidget(copyBytes);
    buttonRow->addStretch(1);
    root->addLayout(buttonRow);

    table_ = new QTableWidget(0, 3, this);
    table_->setHorizontalHeaderLabels({"Address", "Module", "Preview"});
    table_->setProperty("monospace", true);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    setEmptyHint(table_, "No matches yet");
    root->addWidget(table_, 1);

    verdict_ = new QLabel;
    verdict_->setProperty("secondary", true);
    root->addWidget(verdict_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    root->addWidget(buttons);

    fillModules();
    if (!proc_) {
        scanButton_->setEnabled(false);
        verdict_->setText("Open a process first.");
    }

    connect(scanButton_, &QPushButton::clicked, this, &PatternEditorDialog::runScan);
    connect(copyLua, &QPushButton::clicked, this, &PatternEditorDialog::exportClipboard);
    connect(copyBytes, &QPushButton::clicked, this, &PatternEditorDialog::exportClipboard);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row < static_cast<int>(hits_.size()) && openAt_)
            openAt_(hits_[size_t(row)].address);
    });
}

void PatternEditorDialog::fillModules() {
    moduleCombo_->clear();
    moduleCombo_->addItem("All readable regions", 0);
    modules_.clear();
    if (proc_) {
        modules_ = proc_->modules();
        for (size_t i = 0; i < modules_.size(); ++i)
            moduleCombo_->addItem(QString::fromStdString(modules_[i].name), int(i + 1));
    }
}

void PatternEditorDialog::runScan() {
    if (!proc_ || scanning_) return;
    const QString pattern = patternEdit_->text().trimmed();
    if (pattern.isEmpty()) {
        QMessageBox::warning(this, "Pattern editor", "Enter an AOB pattern (e.g. \"7F 45 ?? 46\").");
        return;
    }
    const int moduleIndex = moduleCombo_->currentData().toInt();
    uintptr_t start = 0, stop = UINTPTR_MAX;
    QString moduleFilter;
    if (moduleIndex > 0) {
        const auto& m = modules_[size_t(moduleIndex - 1)];
        start = m.base;
        stop = m.base + m.size - 1;
        moduleFilter = QString::fromStdString(m.name);
    }
    const size_t cap = size_t(maxResultsSpin_->value());
    scanning_ = true;
    scanButton_->setEnabled(false);
    verdict_->setText("Scanning...");
    std::thread([this, pattern, start, stop, cap, moduleFilter]() {
        std::string error;
        std::vector<Hit> hits;
        auto addresses = scanPatternRange(*proc_, pattern.toStdString(), start, stop, cap, &error);
        auto allModules = moduleFilter.isEmpty() ? proc_->modules() : std::vector<ce::ModuleInfo>{};
        for (uintptr_t addr : addresses) {
            Hit hit;
            hit.address = addr;
            if (!moduleFilter.isEmpty()) {
                hit.module = moduleFilter;
            } else {
                for (const auto& m : allModules)
                    if (addr >= m.base && addr < m.base + m.size)
                        hit.module = QString::fromStdString(m.name);
            }
            uint8_t preview[32] = {};
            auto read = proc_->read(addr, preview, sizeof preview);
            hit.preview = QByteArray(reinterpret_cast<char*>(preview),
                                     int(read ? *read : 0));
            hits.push_back(std::move(hit));
        }
        QString verdict = error.empty()
            ? (hits.empty() ? QString("No matches.")
                            : hits.size() == 1 ? QString("Unique match.")
                                               : QString("%1 matches (not unique).").arg(hits.size()))
            : QString::fromStdString(error);
        QMetaObject::invokeMethod(this, [this, hits, verdict]() {
            presentHits(hits);
            verdict_->setText(verdict);
        }, Qt::QueuedConnection);
    }).detach();
}

void PatternEditorDialog::presentHits(std::vector<Hit> hits) {
    scanning_ = false;
    scanButton_->setEnabled(proc_ != nullptr);
    hits_ = std::move(hits);
    table_->setRowCount(int(hits_.size()));
    for (size_t i = 0; i < hits_.size(); ++i) {
        const auto& hit = hits_[i];
        auto* addressItem = new QTableWidgetItem("0x" + QString::number(hit.address, 16));
        addressItem->setData(Qt::UserRole, qulonglong(hit.address));
        table_->setItem(int(i), 0, addressItem);
        table_->setItem(int(i), 1, new QTableWidgetItem(hit.module));
        QString preview;
        for (char byte : hit.preview)
            preview += QString::number(uint8_t(byte), 16).rightJustified(2, '0') + ' ';
        table_->setItem(int(i), 2, new QTableWidgetItem(preview.trimmed()));
    }
    table_->scrollToTop();
}

void PatternEditorDialog::exportClipboard() {
    const auto* senderButton = qobject_cast<const QPushButton*>(QObject::sender());
    const QString pattern = patternEdit_->text().trimmed();
    std::vector<uint8_t> bytes, mask;
    if (!parseAobPattern(pattern.toStdString(), bytes, mask)) {
        QMessageBox::warning(this, "Pattern editor", "Enter a valid AOB pattern first.");
        return;
    }
    QString out;
    if (senderButton && senderButton->text() == "Copy Lua scan") {
        out = "AOBScan(\"" + pattern + "\")";
    } else {
        QString byteArray, maskArray;
        for (size_t i = 0; i < bytes.size(); ++i) {
            byteArray += QString("0x%1, ").arg(bytes[i], 2, 16, QChar('0'));
            maskArray += QString("0x%1, ").arg(mask[i], 2, 16, QChar('0'));
        }
        out = "static const uint8_t bytes[] = { " + byteArray + "};\n"
            + "static const uint8_t mask[]  = { " + maskArray + "};";
    }
    QApplication::clipboard()->setText(out);
}

void PatternEditorDialog::scanForTest(const QString& pattern, const QString& module) {
    if (!module.isEmpty()) {
        const int index = moduleCombo_->findText(module);
        if (index >= 0) moduleCombo_->setCurrentIndex(index);
    }
    patternEdit_->setText(pattern);
    runScan();
    QElapsedTimer timer;
    timer.start();
    while ((scanning_ || verdict_->text() == "Scanning...") && timer.elapsed() < 30000)
        QApplication::processEvents(QEventLoop::AllEvents, 20);
}

int PatternEditorDialog::hitCountForTest() const { return int(hits_.size()); }

QString PatternEditorDialog::verdictForTest() const { return verdict_->text(); }

} // namespace ce::gui
