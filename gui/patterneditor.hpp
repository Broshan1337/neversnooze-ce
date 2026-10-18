#pragma once

#include <QDialog>
#include <QByteArray>
#include <functional>
#include <vector>

#include "platform/process_api.hpp"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace ce::gui {

class PatternEditorDialog : public QDialog {
    Q_OBJECT
public:
    PatternEditorDialog(ce::ProcessHandle* proc,
                        std::function<void(uintptr_t)> openAt,
                        QWidget* parent = nullptr);

    void scanForTest(const QString& pattern, const QString& module);
    int hitCountForTest() const;
    QString verdictForTest() const;

private:
    struct Hit { uintptr_t address = 0; QString module; QByteArray preview; };

    void fillModules();
    void runScan();
    void presentHits(std::vector<Hit> hits);
    void exportClipboard();

    ce::ProcessHandle* proc_ = nullptr;
    std::function<void(uintptr_t)> openAt_;
    QLineEdit* patternEdit_ = nullptr;
    QComboBox* moduleCombo_ = nullptr;
    QSpinBox* maxResultsSpin_ = nullptr;
    QPushButton* scanButton_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* verdict_ = nullptr;
    std::vector<ce::ModuleInfo> modules_;
    std::vector<Hit> hits_;
    bool scanning_ = false;
};

} // namespace ce::gui
