// Offscreen smoke test for Verify table: the example tutorial table loads onto
// a spawned cetutorial process, every record verifies green, and a record whose
// address is broken turns red.

#include "gui/mainwindow.hpp"
#include "gui/verifytable.hpp"
#include "platform/linux/linux_process.hpp"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>

#include <cstdio>
#include <functional>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

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

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("XDG_CONFIG_HOME", "/tmp/ns-verify-config");
    qputenv("XDG_CACHE_HOME", "/tmp/ns-verify-cache");
    QApplication app(argc, argv);
    app.setOrganizationName("cecore-test");
    app.setApplicationName("gui_verifytable_smoke");
    std::printf("── Verify table ──\n");

    const auto tutorialPath = QDir(QCoreApplication::applicationDirPath())
                                  .absoluteFilePath("cetutorial");
    pid_t child = fork();
    if (child == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        execl(tutorialPath.toLocal8Bit().constData(), "cetutorial", nullptr);
        _exit(1);
    }
    usleep(300000);

    ce::gui::MainWindow main;
    main.attachToPid(child, "cetutorial");
    const auto tablePath = QDir(QCoreApplication::applicationDirPath())
        .filePath("../tables/examples/cetutorial.json");
    check("the example table exists", QFileInfo::exists(tablePath));
    if (!QFileInfo::exists(tablePath)) return 1;
    main.loadTableFromPath(tablePath);

    auto* model = main.findChild<ce::gui::AddressListModel*>();
    check("the table loaded three records", model && model->rowCount() == 3);
    QApplication::processEvents(QEventLoop::AllEvents, 50);

    ce::gui::VerifyTableDialog dialog(&main);
    dialog.verifyNowForTest();
    check("all records verify green", dialog.failedCountForTest() == 0);
    check("three records checked", dialog.verifiedCountForTest() == 3);

    if (model) {
        model->setEntryAddress(0, 0x1);
        dialog.verifyNowForTest();
        check("a broken address turns red", dialog.failedCountForTest() == 1);
    }

    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);
    std::printf(failures ? "verify table smoke: FAILED\n" : "verify table smoke: OK\n");
    return failures ? 1 : 0;
}
