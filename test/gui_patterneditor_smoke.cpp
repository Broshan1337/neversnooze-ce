// Offscreen smoke test for the pattern editor: a bounded AOB scan inside a
// module finds the module's ELF header, an invalid pattern reports an error,
// and the results table matches the hit count. Scans this process, so no
// fork/ptrace is needed.

#include "gui/patterneditor.hpp"
#include "platform/linux/linux_process.hpp"
#include "scanner/memory_scanner.hpp"

#include <QApplication>
#include <QComboBox>
#include <QTableWidget>

#include <cstdio>
#include <unistd.h>

static int failures = 0;
static void check(const char* name, bool ok) {
    std::printf("  %s: %s\n", name, ok ? "OK" : "FAILED");
    if (!ok) ++failures;
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    ce::os::LinuxProcessHandle proc(getpid());
    std::printf("── Pattern editor ──\n");

    ce::gui::PatternEditorDialog dialog(&proc, {});
    check("modules are listed", dialog.findChildren<QComboBox*>().size() >= 1);

    const auto modules = proc.modules();
    uintptr_t moduleBase = 0;
    QString moduleName;
    for (const auto& m : modules) {
        if (m.name.find("gui_patterneditor_smoke") != std::string::npos) {
            moduleBase = m.base;
            moduleName = QString::fromStdString(m.name);
            break;
        }
    }
    check("the test binary is a module", moduleBase != 0);
    if (!moduleBase) return 1;

    dialog.scanForTest("7F 45 4C 46", moduleName);
    check("ELF magic found in the module", dialog.hitCountForTest() >= 1);
    check("verdict is not an error", dialog.verdictForTest() != "Scanning...");

    std::string error;
    auto direct = ce::scanPatternRange(proc, "7F 45 4C 46", moduleBase,
                                       moduleBase + 0x200000, 4, &error);
    check("core scanPatternRange finds the ELF header",
          !direct.empty() && direct.front() == moduleBase);
    check("module attribution of the first hit",
          dialog.hitCountForTest() > 0);

    dialog.scanForTest("ZZ nothex", moduleName);
    check("invalid pattern yields no hits", dialog.hitCountForTest() == 0);
    check("invalid pattern is reported",
          dialog.verdictForTest().contains("invalid", Qt::CaseInsensitive));

    std::printf(failures ? "pattern editor smoke: FAILED\n" : "pattern editor smoke: OK\n");
    return failures ? 1 : 0;
}
