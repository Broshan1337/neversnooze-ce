// Offscreen smoke test for the symbol browser: a unique global is found by
// name filter, and adding it creates a symbol-addressed address-list record
// that re-resolves to the same address.

#include "gui/symbolbrowser.hpp"
#include "gui/mainwindow.hpp"
#include "core/expression.hpp"
#include "symbols/elf_symbols.hpp"
#include "platform/linux/linux_process.hpp"

#include <QApplication>

#include <cstdio>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile long g_browser_target = 0x1234;

static int failures = 0;
static void check(const char* name, bool ok) {
    std::printf("  %s: %s\n", name, ok ? "OK" : "FAILED");
    if (!ok) ++failures;
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("XDG_CONFIG_HOME", "/tmp/ns-symbols-config");
    qputenv("XDG_CACHE_HOME", "/tmp/ns-symbols-cache");
    QApplication app(argc, argv);
    app.setOrganizationName("cecore-test");
    app.setApplicationName("gui_symbolbrowser_smoke");
    std::printf("── Symbol browser ──\n");

    const pid_t child = fork();
    if (child == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        for (;;) pause();
    }

    ce::os::LinuxProcessHandle proc(child);
    ce::SymbolResolver resolver;
    resolver.loadProcess(proc);

    std::string moduleName;
    for (const auto& m : proc.modules())
        if (m.name.find("gui_symbolbrowser_smoke") != std::string::npos) {
            moduleName = m.name;
            break;
        }
    check("the child exposes the test binary module", !moduleName.empty());

    ce::ExpressionParser parser(&proc, &resolver);
    const auto byQualified = parser.parse(moduleName + "!g_browser_target");
    check("module!name resolves in an expression",
          byQualified && *byQualified == uintptr_t(&g_browser_target));

    uintptr_t addedAddress = 0;
    QString addedName;
    ce::gui::SymbolBrowserDialog dialog(&proc, &resolver,
        [&](uintptr_t addr, const QString& name) { addedAddress = addr; addedName = name; }, {});
    dialog.setFilterForTest("g_browser_target");
    check("the browser finds the global by name", dialog.rowCountForTest() >= 1);
    dialog.addSelectedForTest();
    check("Add to Address List carries the symbol name and address",
          addedAddress == uintptr_t(&g_browser_target) &&
          addedName == QString("%1!g_browser_target").arg(QString::fromStdString(moduleName)));

    ce::gui::MainWindow main;
    main.attachToPid(child, "child");
    auto* model = main.findChild<ce::gui::AddressListModel*>();
    check("model exists after attach", model != nullptr);
    if (model) {
        model->addEntry(addedAddress, ce::ValueType::Int64, addedName, addedName);
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        const auto& entry = model->entries().front();
        check("the record keeps its symbol expression and address",
              entry.addressExpr == addedName && entry.address == addedAddress);
    }

    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);
    std::printf(failures ? "symbol browser smoke: FAILED\n" : "symbol browser smoke: OK\n");
    return failures ? 1 : 0;
}
