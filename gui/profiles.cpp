#include "gui/profiles.hpp"

#include "gui/theme.hpp"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QFile>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace ce::gui {

QString ProfileStore::filePath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/profiles.json";
}

std::vector<GameProfile> ProfileStore::load() const {
    std::vector<GameProfile> out;
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) return out;
    const auto doc = QJsonDocument::fromJson(file.readAll());
    for (const auto& value : doc.array()) {
        const auto obj = value.toObject();
        GameProfile profile;
        profile.name = obj.value("name").toString();
        profile.processMatch = obj.value("process").toString();
        profile.tablePath = obj.value("table").toString();
        profile.enabled = obj.value("enabled").toBool(true);
        if (!profile.processMatch.isEmpty()) out.push_back(std::move(profile));
    }
    return out;
}

void ProfileStore::save(const std::vector<GameProfile>& profiles) const {
    QJsonArray array;
    for (const auto& profile : profiles) {
        QJsonObject obj;
        obj["name"] = profile.name;
        obj["process"] = profile.processMatch;
        obj["table"] = profile.tablePath;
        obj["enabled"] = profile.enabled;
        array.append(obj);
    }
    QDir().mkpath(QFileInfo(filePath()).absolutePath());
    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

ProfilesDialog::ProfilesDialog(std::vector<GameProfile> profiles, bool watching, QWidget* parent)
    : QDialog(parent), profiles_(std::move(profiles)), watchRequested_(watching) {
    setWindowTitle("Game profiles");
    resize(560, 420);

    auto* root = new QVBoxLayout(this);
    auto* intro = new QLabel("A profile attaches to a process as soon as one with a matching name starts, and loads its cheat table.");
    intro->setProperty("secondary", true);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* body = new QHBoxLayout;
    list_ = new QListWidget;
    body->addWidget(list_, 1);

    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    nameEdit_ = new QLineEdit;
    form->addRow("Name:", nameEdit_);
    processEdit_ = new QLineEdit;
    processEdit_->setFont(monospaceFont());
    processEdit_->setPlaceholderText("tf_linux64");
    form->addRow("Process name:", processEdit_);
    auto* tableRow = new QHBoxLayout;
    tableEdit_ = new QLineEdit;
    tableEdit_->setMinimumWidth(180);
    tableRow->addWidget(tableEdit_, 1);
    auto* browse = new QPushButton("Browse...");
    tableRow->addWidget(browse);
    form->addRow("Cheat table:", tableRow);
    body->addLayout(form);
    root->addLayout(body, 1);

    auto* buttonRow = new QHBoxLayout;
    auto* addButton = new QPushButton("Add");
    removeButton_ = new QPushButton("Remove");
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(removeButton_);
    buttonRow->addStretch(1);
    watchButton_ = new QPushButton(watching ? "Stop watching" : "Watch for these games");
    watchButton_->setProperty("primary", true);
    buttonRow->addWidget(watchButton_);
    root->addLayout(buttonRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    root->addWidget(buttons);

    int row = 0;
    for (const auto& profile : profiles_) {
        list_->addItem(profile.name.isEmpty() ? profile.processMatch : profile.name);
        list_->item(row++)->setCheckState(profile.enabled ? Qt::Checked : Qt::Unchecked);
    }
    if (!profiles_.empty()) list_->setCurrentRow(0);
    loadEditorFromProfile();
    updateButtons();

    connect(list_, &QListWidget::currentRowChanged, this, [this](int) {
        applyEditorToProfile();
        loadEditorFromProfile();
    });
    connect(list_, &QListWidget::itemChanged, this, [this](QListWidgetItem*) { updateButtons(); });
    for (auto* edit : {nameEdit_, processEdit_, tableEdit_})
        connect(edit, &QLineEdit::editingFinished, this, &ProfilesDialog::applyEditorToProfile);
    connect(addButton, &QPushButton::clicked, this, &ProfilesDialog::addProfile);
    connect(removeButton_, &QPushButton::clicked, this, &ProfilesDialog::removeProfile);
    connect(browse, &QPushButton::clicked, this, &ProfilesDialog::browseTable);
    connect(watchButton_, &QPushButton::clicked, this, [this]() {
        applyEditorToProfile();
        watchRequested_ = !watchRequested_;
        watchButton_->setText(watchRequested_ ? "Stop watching" : "Watch for these games");
        accept();
    });
}

void ProfilesDialog::applyEditorToProfile() {
    const int row = list_->currentRow();
    if (row < 0 || row >= int(profiles_.size())) return;
    auto& profile = profiles_[size_t(row)];
    profile.name = nameEdit_->text().trimmed();
    profile.processMatch = processEdit_->text().trimmed();
    profile.tablePath = tableEdit_->text().trimmed();
    profile.enabled = list_->item(row)->checkState() == Qt::Checked;
    list_->item(row)->setText(profile.name.isEmpty() ? profile.processMatch : profile.name);
}

void ProfilesDialog::loadEditorFromProfile() {
    const int row = list_->currentRow();
    if (row < 0 || row >= int(profiles_.size())) {
        nameEdit_->clear();
        processEdit_->clear();
        tableEdit_->clear();
        nameEdit_->setEnabled(false);
        processEdit_->setEnabled(false);
        tableEdit_->setEnabled(false);
        return;
    }
    const auto& profile = profiles_[size_t(row)];
    nameEdit_->setEnabled(true);
    processEdit_->setEnabled(true);
    tableEdit_->setEnabled(true);
    nameEdit_->setText(profile.name);
    processEdit_->setText(profile.processMatch);
    tableEdit_->setText(profile.tablePath);
}

void ProfilesDialog::addProfile() {
    applyEditorToProfile();
    GameProfile profile;
    profile.name = "New profile";
    profiles_.push_back(profile);
    list_->addItem(profile.name);
    list_->item(list_->count() - 1)->setCheckState(Qt::Checked);
    list_->setCurrentRow(list_->count() - 1);
    loadEditorFromProfile();
    updateButtons();
}

void ProfilesDialog::removeProfile() {
    const int row = list_->currentRow();
    if (row < 0 || row >= int(profiles_.size())) return;
    profiles_.erase(profiles_.begin() + row);
    delete list_->takeItem(row);
    if (row < list_->count()) list_->setCurrentRow(row);
    else if (list_->count() > 0) list_->setCurrentRow(list_->count() - 1);
    loadEditorFromProfile();
    updateButtons();
}

void ProfilesDialog::browseTable() {
    const QString path = QFileDialog::getOpenFileName(this, "Choose the cheat table",
        QString(), "Cheat tables (*.ct *.CT *.json);;All files (*)");
    if (!path.isEmpty()) tableEdit_->setText(path);
}

void ProfilesDialog::updateButtons() {
    const bool hasSelection = list_->currentRow() >= 0;
    removeButton_->setEnabled(hasSelection);
    bool anyEnabled = false;
    for (const auto& profile : profiles_)
        if (profile.enabled && !profile.processMatch.isEmpty()) anyEnabled = true;
    watchButton_->setEnabled(anyEnabled);
}

void ProfilesDialog::selectRowForTest(int row) {
    if (row >= 0 && row < list_->count()) list_->setCurrentRow(row);
}

} // namespace ce::gui
