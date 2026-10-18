// Offscreen smoke test for the overlay HUD: pinned address-list records are
// pushed to the overlay by the status tick, unpinned ones are not, and the
// overlay paints with entries without errors.

#include "gui/mainwindow.hpp"
#include "gui/overlay.hpp"

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

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("XDG_CONFIG_HOME", "/tmp/ns-overlay-config");
    qputenv("XDG_CACHE_HOME", "/tmp/ns-overlay-cache");
    QApplication app(argc, argv);
    app.setOrganizationName("cecore-test");
    app.setApplicationName("gui_overlay_smoke");
    std::printf("── Overlay HUD ──\n");

    const pid_t child = fork();
    if (child == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        for (;;) pause();
    }

    ce::gui::MainWindow main;
    main.attachToPid(child, "child");
    auto* model = main.findChild<ce::gui::AddressListModel*>();
    check("model present after attach", model != nullptr);
    if (!model) return 1;
    const int id = model->addEntry(0x1000, ce::ValueType::Int32, "Pinned health");

    auto* overlay = main.ensureOverlayHud();
    check("the overlay window opened", overlay != nullptr);
    if (!overlay) return 1;

    overlay->setEntries({{"Gold", "123"}});
    check("entries reach the overlay", overlay->entryCountForTest() == 1);

    model->setEntryPinned(0, true);
    check("pinning marks the record", model->entries().front().pinned);

    {
        QElapsedTimer timer;
        timer.start();
        while (overlay->entryCountForTest() != 1 && timer.elapsed() < 3000)
            QApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    check("the pinned record reaches the overlay after a tick",
          overlay->entryCountForTest() == 1);

    model->setEntryPinned(0, false);
    check("unpinning clears the record", !model->entries().front().pinned);

    overlay->grab();
    check("the overlay paints with entries", true);

    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);
    (void)id;
    std::printf(failures ? "overlay smoke: FAILED\n" : "overlay smoke: OK\n");
    return failures ? 1 : 0;
}
