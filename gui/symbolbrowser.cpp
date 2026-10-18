#include "gui/symbolbrowser.hpp"
#include "gui/theme.hpp"
#include "symbols/elf_symbols.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace ce::gui {

SymbolBrowserDialog::SymbolBrowserDialog(ce::ProcessHandle* proc, ce::SymbolResolver* resolver,
                                         std::function<void(uintptr_t, const QString&)> addToList,
                                         std::function<void(uintptr_t)> openAt,
                                         QWidget* parent)
    : QDialog(parent), proc_(proc), resolver_(resolver),
      addToList_(std::move(addToList)), openAt_(std::move(openAt)) {
    setWindowTitle("Symbol browser");
    resize(780, 520);

    auto* root = new QVBoxLayout(this);

    auto* controls = new QHBoxLayout;
    moduleCombo_ = new QComboBox;
    moduleCombo_->setMinimumWidth(220);
    filterEdit_ = new QLineEdit;
    filterEdit_->setFont(monospaceFont());
    filterEdit_->setPlaceholderText("Filter by name (e.g. health, m_iAmmo, g_Game)");
    filterEdit_->setMinimumWidth(280);
    controls->addWidget(new QLabel("Module:"));
    controls->addWidget(moduleCombo_);
    controls->addWidget(filterEdit_, 1);
    root->addLayout(controls);

    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({"Name", "Module", "Address", "Size"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setProperty("monospace", true);
    styleDataTable(table_);
    setEmptyHint(table_, "No symbols match the filter.");
    root->addWidget(table_, 1);

    auto* buttonRow = new QHBoxLayout;
    auto* addButton = new QPushButton("Add to Address List");
    addButton->setProperty("primary", true);
    auto* openButton = new QPushButton("Open in Memory Viewer");
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(openButton);
    buttonRow->addStretch(1);
    root->addLayout(buttonRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    root->addWidget(buttons);

    connect(addButton, &QPushButton::clicked, this, &SymbolBrowserDialog::addSelected);
    connect(openButton, &QPushButton::clicked, this, [this]() {
        const int row = table_->currentRow();
        if (row >= 0 && row < int(rows_.size()) && openAt_)
            openAt_(rows_[size_t(row)].address);
    });
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row >= 0 && row < int(rows_.size()))
            addSelectedAt(row);
    });
    connect(filterEdit_, &QLineEdit::textChanged, this, [this]() { refresh(); });
    connect(moduleCombo_, &QComboBox::currentIndexChanged, this, [this]() { refresh(); });

    if (!proc_ || !resolver_) {
        setEnabled(false);
        filterEdit_->setPlaceholderText("Open a process first.");
    } else {
        moduleCombo_->addItem("All modules", QString());
        for (const auto& m : proc_->modules())
            moduleCombo_->addItem(QString::fromStdString(m.name),
                                  QString::fromStdString(m.name));
        refresh();
    }
}

void SymbolBrowserDialog::addSelectedAt(int row) {
    if (row < 0 || row >= int(rows_.size()) || !addToList_) return;
    const auto& row_ = rows_[size_t(row)];
    addToList_(row_.address, row_.module.isEmpty() ? row_.name
        : QString("%1!%2").arg(row_.module, row_.name));
}

void SymbolBrowserDialog::addSelected() {
    const int row = table_->currentRow();
    if (row >= 0 && row < int(rows_.size()))
        addSelectedAt(row);
}

void SymbolBrowserDialog::refresh() {
    rows_.clear();
    table_->setRowCount(0);
    if (!proc_ || !resolver_) return;
    const QString module = moduleCombo_->currentData().toString();
    const QString filter = filterEdit_->text().trimmed();
    if (filter.size() < 2) {
        setEmptyHint(table_, "Type at least two letters to search the process's symbols.");
        return;
    }
    for (const auto& symbol : resolver_->symbols()) {
        const QString name = QString::fromStdString(symbol.name);
        if (!name.contains(filter, Qt::CaseInsensitive)) continue;
        if (!module.isEmpty() &&
            !QString::fromStdString(symbol.module).contains(module, Qt::CaseInsensitive))
            continue;
        Row row;
        row.address = symbol.address;
        row.name = name;
        row.module = QString::fromStdString(symbol.module);
        row.size = symbol.size;
        rows_.push_back(row);
        if (rows_.size() >= 5000) break;
    }
    std::sort(rows_.begin(), rows_.end(), [](const Row& a, const Row& b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    table_->setRowCount(int(rows_.size()));
    for (size_t i = 0; i < rows_.size(); ++i) {
        const auto& row = rows_[i];
        table_->setItem(int(i), 0, new QTableWidgetItem(row.name));
        table_->setItem(int(i), 1, new QTableWidgetItem(row.module));
        table_->setItem(int(i), 2, new QTableWidgetItem("0x" + QString::number(row.address, 16)));
        table_->setItem(int(i), 3, new QTableWidgetItem(QString::number(row.size)));
    }
    setEmptyHint(table_, rows_.empty() ? "No symbols match the filter." : QString());
    table_->scrollToTop();
}

void SymbolBrowserDialog::setFilterForTest(const QString& text) {
    filterEdit_->setText(text);
    refresh();
}

int SymbolBrowserDialog::rowCountForTest() const { return int(rows_.size()); }

void SymbolBrowserDialog::addSelectedForTest() {
    if (table_->rowCount() > 0) {
        table_->selectRow(0);
        addSelected();
    }
}

} // namespace ce::gui
