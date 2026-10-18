#pragma once

#include <QDialog>
#include <functional>
#include <vector>

class QComboBox;
class QLineEdit;
class QTableWidget;

namespace ce {
class ProcessHandle;
class SymbolResolver;
}

namespace ce::gui {

class SymbolBrowserDialog : public QDialog {
    Q_OBJECT
public:
    SymbolBrowserDialog(ce::ProcessHandle* proc, ce::SymbolResolver* resolver,
                        std::function<void(uintptr_t, const QString&)> addToList,
                        std::function<void(uintptr_t)> openAt,
                        QWidget* parent = nullptr);

    void setFilterForTest(const QString& text);
    int rowCountForTest() const;
    void addSelectedForTest();

private:
    void refresh();
    void addSelected();

    ce::ProcessHandle* proc_ = nullptr;
    ce::SymbolResolver* resolver_ = nullptr;
    std::function<void(uintptr_t, const QString&)> addToList_;
    std::function<void(uintptr_t)> openAt_;
    QComboBox* moduleCombo_ = nullptr;
    QLineEdit* filterEdit_ = nullptr;
    QTableWidget* table_ = nullptr;
    void addSelectedAt(int row);
    struct Row { uintptr_t address; QString name; QString module; size_t size = 0; };
    std::vector<Row> rows_;
};

} // namespace ce::gui
