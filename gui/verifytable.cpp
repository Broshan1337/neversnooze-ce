#include "gui/verifytable.hpp"
#include "gui/mainwindow.hpp"
#include "gui/theme.hpp"
#include "core/expression.hpp"
#include "core/value_io.hpp"
#include "scanner/memory_scanner.hpp"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace ce::gui {

VerifyTableDialog::VerifyTableDialog(MainWindow* main, QWidget* parent)
    : QDialog(parent), main_(main) {
    setWindowTitle("Verify table");
    resize(720, 420);

    auto* root = new QVBoxLayout(this);
    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({"Description", "Address", "Value", "Status"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setProperty("monospace", true);
    styleDataTable(table_);
    setEmptyHint(table_, "No address-list records to verify.");
    root->addWidget(table_, 1);

    summary_ = new QLabel("Checks every record against the attached process: its expression or "
                          "pattern must resolve, and its memory must be readable. A green table "
                          "survives a game restart; red rows tell you what to fix before sharing it.");
    summary_->setProperty("secondary", true);
    summary_->setWordWrap(true);
    root->addWidget(summary_);

    auto* row = new QHBoxLayout;
    verifyButton_ = new QPushButton("Re-verify");
    verifyButton_->setProperty("primary", true);
    row->addWidget(verifyButton_);
    row->addStretch(1);
    root->addLayout(row);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    root->addWidget(buttons);

    connect(verifyButton_, &QPushButton::clicked, this, [this]() { verify(); });
    verify();
}

void VerifyTableDialog::verify() {
    verified_ = 0;
    failed_ = 0;
    table_->setRowCount(0);
    const auto& entries = main_->addressListModel()->entries();
    if (entries.empty()) {
        summary_->setText("No address-list records to verify.");
        return;
    }
    table_->setRowCount(int(entries.size()));
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        uintptr_t address = entry.address;
        QString status;
        if (entry.isGroup) {
            status = "group";
        } else if (!main_->processForVerify()) {
            status = "no process attached";
        } else {
            ce::ExpressionParser parser(main_->processForVerify(), main_->symbolResolverForVerify());
            if (!entry.addressExpr.isEmpty()) {
                const auto resolved = parser.parse(entry.addressExpr.toStdString());
                if (!resolved) {
                    status = QString("expression does not resolve: %1").arg(entry.addressExpr);
                } else {
                    address = *resolved;
                }
            }
            const auto resolver = entry.tableFields.value("resolver").toObject();
            const QString pattern = resolver.value("pattern").toString();
            if (status.isEmpty() && !pattern.isEmpty()) {
                uintptr_t resolved = 0;
                std::string error;
                if (!ce::resolvePatternAddress(*main_->processForVerify(), pattern.toStdString(),
                                               resolver.value("module").toString().toStdString(),
                                               (int64_t)resolver.value("offset").toDouble(),
                                               resolved, &error)) {
                    status = QString("pattern failed: %1").arg(QString::fromStdString(error));
                } else {
                    address = resolved;
                }
            }
            if (status.isEmpty()) {
                const auto value = ce::readTypedValue(*main_->processForVerify(), address, entry.type);
                if (!value)
                    status = QString("unreadable: %1").arg(QString::fromStdString(value.error()));
                else
                    status = "ok";
            }
        }
        if (status == "ok") ++verified_; else ++failed_;
        table_->setItem(int(i), 0, new QTableWidgetItem(entry.description));
        table_->setItem(int(i), 1, new QTableWidgetItem(
            QString("0x%1").arg(entry.addressExpr.isEmpty()
                ? entry.address : address, 0, 16)));
        table_->setItem(int(i), 2, new QTableWidgetItem(entry.currentValue));
        auto* statusItem = new QTableWidgetItem(status);
        statusItem->setForeground(status == "ok" ? QColor(150, 220, 150)
                                                  : status == "group" ? QColor(170, 170, 184)
                                                                      : QColor(240, 120, 120));
        table_->setItem(int(i), 3, statusItem);
    }
    summary_->setText(QString("%1 of %2 records verified against the process; %3 failed.")
                          .arg(verified_).arg(entries.size()).arg(failed_));
}

} // namespace ce::gui
