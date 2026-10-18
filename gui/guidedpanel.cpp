#include "gui/guidedpanel.hpp"
#include "gui/mainwindow.hpp"
#include "gui/theme.hpp"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace ce::gui {

GuidedTourPanel::GuidedTourPanel(MainWindow* main, QWidget* parent)
    : QDockWidget("Guided tour", parent), main_(main) {
    steps_ = {
        {tr("Open a process"),
         tr("File > Open Process... (Ctrl+O) picks the program to examine. Neversnooze CE reads its memory from outside; nothing is modified yet.")},
        {tr("Run a First Scan"),
         tr("Type the value you see in the game (health, gold, ...) and press First Scan. Every address currently holding that value is kept.")},
        {tr("Narrow the results"),
         tr("Change the value in the game, type the new one, press Next Scan. Repeat until only a few addresses remain.")},
        {tr("Keep an address"),
         tr("Select results and press the red arrow (or double-click a row) to add them to the address list below.")},
        {tr("Freeze a value"),
         tr("Tick the Active box of an address-list record: its value is written back every moment, so the game cannot change it.")},
        {tr("Open the Memory Viewer"),
         tr("Tools > Memory Viewer... (Ctrl+M) shows memory and disassembly around any address.")},
        {tr("Find what writes a value"),
         tr("Right-click an address-list record > Find what writes to this address: every writing instruction is logged with its registers.")},
        {tr("Save the table"),
         tr("File > Save (Ctrl+S) writes the address list to a .CT table you can reload any time; records with a pattern resolver even survive game updates.")},
    };

    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);

    list_ = new QListWidget;
    layout->addWidget(list_, 1);

    hint_ = new QLabel;
    hint_->setWordWrap(true);
    hint_->setProperty("secondary", true);
    layout->addWidget(hint_);

    auto* resetButton = new QPushButton("Restart the tour");
    connect(resetButton, &QPushButton::clicked, this, &GuidedTourPanel::reset);
    layout->addWidget(resetButton);

    setWidget(body);
    rebuildList();

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &GuidedTourPanel::poll);
    timer_->start(1000);
    poll();
}

void GuidedTourPanel::poll() {
    if (!main_) return;
    const auto state = main_->guidedState();
    const bool checks[] = {
        state.processAttached,
        state.scanCount >= 1,
        state.scanCount >= 2 || state.resultCount >= 1,
        state.entryCount >= 1,
        state.frozenCount >= 1,
        state.memoryViewCount >= 1,
        state.codeFinderCount >= 1,
        state.tableSaved,
    };
    bool changed = false;
    for (size_t i = 0; i < steps_.size() && i < sizeof(checks) / sizeof(checks[0]); ++i)
        if (!steps_[i].done && checks[i]) { steps_[i].done = true; changed = true; }
    while (current_ < int(steps_.size()) - 1 && steps_[size_t(current_)].done) ++current_;
    if (changed) rebuildList();
    else if (auto* item = list_->item(current_)) item->setSelected(true);
    hint_->setText(current_ < int(steps_.size()) ? steps_[size_t(current_)].hint : tr("Tour complete."));
}

void GuidedTourPanel::rebuildList() {
    list_->clear();
    for (size_t i = 0; i < steps_.size(); ++i) {
        auto* item = new QListWidgetItem(steps_[i].done
            ? QString::fromUtf8("\u2713  %1").arg(steps_[i].title)
            : steps_[i].title);
        item->setFlags((item->flags() & ~Qt::ItemIsUserCheckable) | Qt::ItemIsEnabled);
        list_->addItem(item);
    }
    if (current_ < list_->count()) list_->setCurrentRow(current_);
}

void GuidedTourPanel::reset() {
    for (auto& step : steps_) step.done = false;
    current_ = 0;
    rebuildList();
}

bool GuidedTourPanel::isStepDoneForTest(int step) const {
    return step >= 0 && step < int(steps_.size()) && steps_[size_t(step)].done;
}

int GuidedTourPanel::doneCountForTest() const {
    int done = 0;
    for (const auto& step : steps_) if (step.done) ++done;
    return done;
}

} // namespace ce::gui
