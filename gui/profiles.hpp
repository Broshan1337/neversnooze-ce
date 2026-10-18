#pragma once

#include <QDialog>
#include <QString>
#include <vector>

class QListWidget;
class QLineEdit;
class QPushButton;

namespace ce::gui {

struct GameProfile {
    QString name;
    QString processMatch;
    QString tablePath;
    bool enabled = true;
};

class ProfileStore {
public:
    static QString filePath();
    std::vector<GameProfile> load() const;
    void save(const std::vector<GameProfile>& profiles) const;
};

class ProfilesDialog : public QDialog {
    Q_OBJECT
public:
    ProfilesDialog(std::vector<GameProfile> profiles, bool watching, QWidget* parent = nullptr);
    const std::vector<GameProfile>& profiles() const { return profiles_; }
    bool watchRequested() const { return watchRequested_; }

    void selectRowForTest(int row);
    QListWidget* listForTest() const { return list_; }

private:
    void updateButtons();
    void applyEditorToProfile();
    void loadEditorFromProfile();
    void addProfile();
    void removeProfile();
    void browseTable();

    std::vector<GameProfile> profiles_;
    bool watchRequested_ = false;
    QListWidget* list_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QLineEdit* processEdit_ = nullptr;
    QLineEdit* tableEdit_ = nullptr;
    QPushButton* removeButton_ = nullptr;
    QPushButton* watchButton_ = nullptr;
};

} // namespace ce::gui
