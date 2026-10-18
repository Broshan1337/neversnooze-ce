#include "gui/huntwizard.hpp"
#include "gui/mainwindow.hpp"
#include "gui/theme.hpp"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace ce::gui {

HuntWizardDialog::HuntWizardDialog(MainWindow* main, QWidget* parent)
    : QDialog(parent), main_(main) {
    setWindowTitle("Hunt by behavior");
    resize(600, 380);

    auto* root = new QVBoxLayout(this);
    auto* intro = new QLabel("Pick what the value does. The scan controls below are set for each step; "
                              "you run the scans on the main window and the tour advances on its own.");
    intro->setProperty("secondary", true);
    intro->setWordWrap(true);
    root->addWidget(intro);

    categoryCombo_ = new QComboBox;
    categoryCombo_->addItems({
        "I know the value I see right now",
        "I don't know the value, but it changes when I play",
        "It only increases (score, money, XP)",
        "It only decreases (health, ammo, durability)",
        "It's a bar between 0 and 100%",
    });
    root->addWidget(categoryCombo_);

    list_ = new QListWidget;
    root->addWidget(list_, 1);

    hint_ = new QLabel;
    hint_->setWordWrap(true);
    hint_->setProperty("secondary", true);
    root->addWidget(hint_);

    connect(categoryCombo_, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
    rebuild();

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &HuntWizardDialog::poll);
    timer_->start(1000);
    poll();
}

void HuntWizardDialog::rebuild() {
    steps_.clear();
    current_ = 0;
    lastScanCount_ = -1;
    switch (categoryCombo_->currentIndex()) {
        case 0:
            steps_ = {
                {tr("Type the value you see into the Value field and press First Scan.")},
                {tr("Change the value in the game, type the new value, press Next Scan. Repeat while there are many results.")},
                {tr("Select the surviving results and press the red arrow to keep them in the address list. "
                     "Confirm by changing the value in the game and watching the record follow.")},
            };
            break;
        case 1:
            steps_ = {
                {tr("Press First Scan. The comparison is set to \"Unknown initial value\": every address is a candidate.")},
                {tr("Make the value change in the game (you never need to know the number), then press Next Scan.")},
                {tr("Repeat the change-and-scan cycle until a handful of results remain, then keep them in the address list.")},
            };
            break;
        case 2:
            steps_ = {
                {tr("Press First Scan with \"Unknown initial value\".")},
                {tr("Make the value go up in the game (gain score/money), then press Next Scan with \"Increased value\".")},
                {tr("Repeat until few results remain, then keep them in the address list.")},
            };
            break;
        case 3:
            steps_ = {
                {tr("Press First Scan with \"Unknown initial value\".")},
                {tr("Make the value go down in the game (take damage, shoot), then press Next Scan with \"Decreased value\".")},
                {tr("Repeat until few results remain, then keep them in the address list.")},
            };
            break;
        default:
            steps_ = {
                {tr("Press First Scan. The type is set to Float with a 0..100 range: every bar-like float is a candidate.")},
                {tr("Look at the bar's current percentage, switch the comparison to \"Exact Value\", "
                     "type that number and press Next Scan.")},
                {tr("Change the bar in the game and rescan with the new percentage until few results remain, "
                     "then keep them in the address list.")},
            };
            break;
    }
    list_->clear();
    for (const auto& step : steps_)
        list_->addItem(step.instruction);
    applyCurrentStep();
    list_->setCurrentRow(0);
    hint_->setText(steps_.empty() ? QString() : steps_.front().instruction);
}

static void setComboText(QComboBox* combo, const QString& text) {
    const int index = combo->findText(text);
    if (index >= 0) combo->setCurrentIndex(index);
}

void HuntWizardDialog::applyCurrentStep() {
    auto* comparison = main_->findChild<QComboBox*>("scanComparison");
    auto* valueType = main_->findChild<QComboBox*>("scanValueType");
    auto* value = main_->findChild<QLineEdit*>("scanValue");
    auto* upper = main_->findChild<QLineEdit*>("scanValueUpper");
    if (!comparison || !valueType || !value) return;
    const int category = categoryCombo_->currentIndex();
    switch (category) {
        case 0:
            setComboText(comparison, "Exact Value");
            break;
        case 1:
            setComboText(comparison, current_ == 0 ? "Unknown initial value" : "Changed value");
            value->clear();
            break;
        case 2:
            setComboText(comparison, current_ == 0 ? "Unknown initial value" : "Increased value");
            value->clear();
            break;
        case 3:
            setComboText(comparison, current_ == 0 ? "Unknown initial value" : "Decreased value");
            value->clear();
            break;
        default:
            if (current_ == 0) {
                setComboText(valueType, "Float");
                setComboText(comparison, "Value between...");
                value->setText("0");
                if (upper) upper->setText("100");
            } else {
                setComboText(comparison, "Exact Value");
                value->clear();
                value->setFocus();
            }
            break;
    }
}

void HuntWizardDialog::poll() {
    const int scanCount = main_->guidedState().scanCount;
    if (lastScanCount_ < 0) {
        lastScanCount_ = scanCount;
        return;
    }
    if (scanCount > lastScanCount_) {
        lastScanCount_ = scanCount;
        if (current_ < int(steps_.size())) {
            steps_[size_t(current_)].done = true;
            ++current_;
            if (current_ < int(steps_.size())) {
                applyCurrentStep();
                list_->setCurrentRow(current_);
                hint_->setText(steps_[size_t(current_)].instruction);
            } else {
                hint_->setText(tr("Done. Your value should be in the address list."));
            }
        }
    }
    for (int i = 0; i < int(steps_.size()) && i < list_->count(); ++i)
        list_->item(i)->setText(QString(steps_[size_t(i)].done
            ? QString::fromUtf8("\u2713  %1") : QString("%1")).arg(steps_[size_t(i)].instruction));
}

int HuntWizardDialog::doneCountForTest() const {
    int done = 0;
    for (const auto& step : steps_) if (step.done) ++done;
    return done;
}

} // namespace ce::gui
