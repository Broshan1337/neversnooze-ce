#pragma once

#include <QDialog>
#include <QString>
#include <vector>

class QComboBox;
class QLabel;
class QListWidget;
class QTimer;

namespace ce::gui {

class MainWindow;

class HuntWizardDialog : public QDialog {
    Q_OBJECT
public:
    explicit HuntWizardDialog(MainWindow* main, QWidget* parent = nullptr);

    void pollNowForTest() { poll(); }
    int doneCountForTest() const;
    int stepCountForTest() const { return int(steps_.size()); }

private:
    struct Step {
        QString instruction;
        bool done = false;
    };

    void rebuild();
    void applyCurrentStep();
    void poll();

    MainWindow* main_;
    QComboBox* categoryCombo_ = nullptr;
    QListWidget* list_ = nullptr;
    QLabel* hint_ = nullptr;
    QTimer* timer_ = nullptr;
    std::vector<Step> steps_;
    int current_ = 0;
    int lastScanCount_ = -1;
};

} // namespace ce::gui
