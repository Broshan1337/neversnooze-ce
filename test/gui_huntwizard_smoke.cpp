// Offscreen smoke test for the hunt-by-behavior wizard: the wizard configures
// the scan controls per behavior, and a real hunt against a spawned child
// (known value, change it from outside, rescan) advances the wizard's steps.

#include "gui/mainwindow.hpp"
#include "gui/huntwizard.hpp"
#include "platform/linux/linux_process.hpp"

#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QPushButton>

#include <cstdio>
#include <functional>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile int32_t g_hunt_target = 100;

static int failures = 0;
static void check(const char* name, bool ok) {
    std::printf("  %s: %s\n", name, ok ? "OK" : "FAILED");
    if (!ok) ++failures;
}

static bool waitFor(const std::function<bool()>& condition, int ms) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < ms)
        QApplication::processEvents(QEventLoop::AllEvents, 20);
    return condition();
}

static QPushButton* button(QWidget* parent, const QString& text) {
    for (auto* b : parent->findChildren<QPushButton*>())
        if (b->text().startsWith(text)) return b;
    return nullptr;
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("XDG_CONFIG_HOME", "/tmp/ns-hunt-config");
    qputenv("XDG_CACHE_HOME", "/tmp/ns-hunt-cache");
    QApplication app(argc, argv);
    app.setOrganizationName("cecore-test");
    app.setApplicationName("gui_huntwizard_smoke");
    std::printf("── Hunt by behavior ──\n");

    const pid_t child = fork();
    if (child == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        for (;;) pause();
    }

    ce::gui::MainWindow main;
    main.attachToPid(child, "child");
    ce::gui::HuntWizardDialog wizard(&main);
    wizard.show();

    auto* comparison = main.findChild<QComboBox*>("scanComparison");
    auto* valueType = main.findChild<QComboBox*>("scanValueType");
    check("the main window exposes its scan controls",
          comparison && valueType);

    wizard.pollNowForTest();
    check("exact-value behavior selects Exact Value",
          comparison && comparison->currentText() == "Exact Value");

    auto* value = main.findChild<QLineEdit*>("scanValue");
    auto* first = button(&main, "First Scan");
    auto* next = button(&main, "Next Scan");
    check("scan buttons are reachable", value && first && next);
    if (!value || !first || !next) return 1;

    value->setText("100");
    first->click();
    check("the hunt advances past the first scan",
          waitFor([&] { wizard.pollNowForTest(); return wizard.doneCountForTest() >= 1; }, 15000));

    ce::os::LinuxProcessHandle writer(child);
    const int32_t changed = 50;
    (void)writer.write(reinterpret_cast<uintptr_t>(&g_hunt_target),
                       const_cast<int32_t*>(&changed), sizeof changed);
    value->setText("50");
    next->click();
    check("the hunt advances after the value changed and rescanned",
          waitFor([&] { wizard.pollNowForTest(); return wizard.doneCountForTest() >= 2; }, 15000));

    auto* decreasedCombo = main.findChild<QComboBox*>("scanComparison");
    wizard.findChild<QComboBox*>()->setCurrentIndex(3);
    wizard.pollNowForTest();
    check("decreasing behavior selects Unknown then Decreased",
          decreasedCombo && decreasedCombo->currentText().contains("Unknown"));

    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);
    std::printf(failures ? "hunt wizard smoke: FAILED\n" : "hunt wizard smoke: OK\n");
    return failures ? 1 : 0;
}
