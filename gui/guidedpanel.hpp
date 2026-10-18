#pragma once

#include <QDockWidget>
#include <vector>

class QLabel;
class QListWidget;
class QTimer;

namespace ce::gui {

class MainWindow;

class GuidedTourPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit GuidedTourPanel(MainWindow* main, QWidget* parent = nullptr);
    bool isStepDoneForTest(int step) const;
    int doneCountForTest() const;
    void pollNowForTest() { poll(); }
    void resetNowForTest() { reset(); }

private:
    struct Step {
        QString title;
        QString hint;
        bool done = false;
    };
    void poll();
    void rebuildList();
    void reset();

    MainWindow* main_;
    QListWidget* list_ = nullptr;
    QLabel* hint_ = nullptr;
    QTimer* timer_ = nullptr;
    std::vector<Step> steps_;
    int current_ = 0;
};

} // namespace ce::gui
