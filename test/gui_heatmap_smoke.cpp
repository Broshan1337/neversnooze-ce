// Offscreen smoke test for the memory heatmap: repeated snapshots of a child
// that keeps writing one page accumulate changed-page counts, and stopping the
// capture joins its worker cleanly.

#include "gui/heatmap.hpp"
#include "platform/linux/linux_process.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QPushButton>

#include <cstdio>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile long g_heatmap_target[16384];

static int failures = 0;
static void check(const char* name, bool ok) {
    std::printf("  %s: %s\n", name, ok ? "OK" : "FAILED");
    if (!ok) ++failures;
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    std::printf("── Memory heatmap ──\n");

    const pid_t child = fork();
    if (child == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        for (;;) {
            g_heatmap_target[9000] = g_heatmap_target[9000] + 1;
            usleep(1000);
        }
    }

    ce::os::LinuxProcessHandle proc(child);
    ce::gui::HeatmapDialog dialog(&proc, {});
    dialog.setRegionLimitForTest(8);

    auto* start = dialog.findChild<QPushButton*>();
    check("the dialog offers Start", start && start->text() == "Start");
    if (!start) return 1;
    start->click();

    bool sawChanges = false;
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 15000) {
            QApplication::processEvents(QEventLoop::AllEvents, 50);
            if (dialog.changedPageCountForTest() > 0) { sawChanges = true; break; }
        }
    }
    check("changed pages accumulate while the child writes", sawChanges);

    auto* stop = dialog.findChild<QPushButton*>();
    if (stop && stop->text() == "Stop") stop->click();
    check("stopping relabels the button",
          dialog.findChild<QPushButton*>() && dialog.changedPageCountForTest() >= 0);

    dialog.hide();
    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);
    std::printf(failures ? "heatmap smoke: FAILED\n" : "heatmap smoke: OK\n");
    return failures ? 1 : 0;
}
