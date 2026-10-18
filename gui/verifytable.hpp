#pragma once

#include <QDialog>
#include <vector>

class QLabel;
class QPushButton;
class QTableWidget;

namespace ce::gui {

class MainWindow;

class VerifyTableDialog : public QDialog {
    Q_OBJECT
public:
    explicit VerifyTableDialog(MainWindow* main, QWidget* parent = nullptr);

    void verifyNowForTest() { verify(); }
    int verifiedCountForTest() const { return verified_; }
    int failedCountForTest() const { return failed_; }

private:
    void verify();

    MainWindow* main_;
    QTableWidget* table_ = nullptr;
    QLabel* summary_ = nullptr;
    QPushButton* verifyButton_ = nullptr;
    int verified_ = 0;
    int failed_ = 0;
};

} // namespace ce::gui
