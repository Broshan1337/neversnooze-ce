// Offscreen smoke test for resilient tables: a JSON table whose record carries
// a pattern resolver rebases to the child's module base after the table loads
// onto an attached process, and the resolver survives a save of the table.

#include "gui/mainwindow.hpp"
#include "core/ct_file.hpp"
#include "platform/linux/linux_process.hpp"

#include <QApplication>
#include <QFile>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
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
    qputenv("XDG_CONFIG_HOME", "/tmp/ns-resilient-config");
    qputenv("XDG_CACHE_HOME", "/tmp/ns-resilient-cache");
    QApplication app(argc, argv);
    app.setOrganizationName("cecore-test");
    app.setApplicationName("gui_resilient_smoke");

    pid_t child = spawnChild();
    std::printf("── Resilient table rebase ──\n");

    uintptr_t moduleBase = 0;
    {
        ce::os::LinuxProcessHandle childHandle(child);
        for (const auto& m : childHandle.modules())
            if (m.name.find("gui_resilient_smoke") != std::string::npos)
                moduleBase = m.base;
    }
    check("the child exposes the test binary module", moduleBase != 0);

    ce::gui::MainWindow main;
    main.attachToPid(child, "child");

    ce::CheatTable table;
    ce::CheatEntry entry;
    entry.id = 1;
    entry.description = "ELF header as int";
    entry.type = ce::ValueType::Int32;
    entry.address = 0x1000;
    entry.resolverPattern = "7F 45 4C 46";
    entry.resolverModule = "gui_resilient_smoke";
    table.entries.push_back(entry);
    const auto path = std::filesystem::temp_directory_path() / "ns_resilient_smoke.json";
    check("table saves to JSON", table.saveJson(path.string()));

    main.loadTableFromPath(QString::fromStdString(path.string()));
    QApplication::processEvents(QEventLoop::AllEvents, 20);

    auto* model = main.findChild<ce::gui::AddressListModel*>();
    bool rebased = model && model->rowCount() == 1 &&
                   model->entries().front().address == moduleBase;
    check("the record rebased to the module base", rebased);

    const auto resolver = model->entries().front().tableFields.value("resolver").toObject();
    bool kept = resolver.value("pattern").toString() == "7F 45 4C 46" &&
                resolver.value("module").toString() == "gui_resilient_smoke";
    check("the resolver is kept for a table save", kept);

    std::filesystem::remove(path);
    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);

    std::printf(failures ? "resilient table smoke: FAILED\n" : "resilient table smoke: OK\n");
    return failures ? 1 : 0;
}
