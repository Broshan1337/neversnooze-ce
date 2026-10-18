// Offscreen smoke test for the guided tour: steps complete in order as the
// real app state changes (attach, scan, address list, memory viewer), and the
// done set only grows.

#include "gui/mainwindow.hpp"
#include "gui/guidedpanel.hpp"
#include "core/ct_file.hpp"
#include "platform/linux/linux_process.hpp"

#include <QApplication>
#include <QElapsedTimer>

#include <cstdio>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures = 0;
static void check(const char* name, bool ok) {
    std::printf("  %s: %s\n", name, ok ? "OK" : "FAILED");
    if (!ok) ++failures;
}

static pid_t spawnChild() {
    pid_t child = fork();
    if (child == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        for (;;) pause();
    }
    return child;
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("XDG_CONFIG_HOME", "/tmp/ns-guided-config");
    qputenv("XDG_CACHE_HOME", "/tmp/ns-guided-cache");
    QApplication app(argc, argv);
    app.setOrganizationName("cecore-test");
    app.setApplicationName("gui_guided_smoke");
    std::printf("── Guided tour ──\n");

    const pid_t child = spawnChild();
    ce::gui::MainWindow main;
    ce::gui::GuidedTourPanel panel(&main);
    panel.show();

    int doneBefore = panel.doneCountForTest();
    main.attachToPid(child, "child");
    panel.pollNowForTest();
    check("step 1 completes on attach",
          panel.isStepDoneForTest(0) && panel.doneCountForTest() > doneBefore);
    doneBefore = panel.doneCountForTest();

    auto* model = main.findChild<ce::gui::AddressListModel*>();
    check("step 3 is not done without a kept address", model && !panel.isStepDoneForTest(3));
    if (model) {
        model->addEntry(0x1000, ce::ValueType::Int32, "Tour record");
        panel.pollNowForTest();
        check("step 4 completes when an address is kept",
              panel.isStepDoneForTest(3) && panel.doneCountForTest() > doneBefore);
    }
    doneBefore = panel.doneCountForTest();

    main.openMemoryView(0x1000);
    panel.pollNowForTest();
    check("step 6 completes when the memory viewer opens",
          panel.isStepDoneForTest(5) && panel.doneCountForTest() > doneBefore);

    panel.resetNowForTest();
    check("reset clears the progress",
          panel.doneCountForTest() == 0 && !panel.isStepDoneForTest(0));
    panel.pollNowForTest();
    check("state-based steps re-complete after a reset",
          panel.isStepDoneForTest(0) && panel.isStepDoneForTest(3) && panel.isStepDoneForTest(5));

    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);
    std::printf(failures ? "guided tour smoke: FAILED\n" : "guided tour smoke: OK\n");
    return failures ? 1 : 0;
}
