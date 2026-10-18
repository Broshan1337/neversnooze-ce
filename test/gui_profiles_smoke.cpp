// Offscreen smoke test for game profiles: the store round-trips through
// profiles.json, the dialog edits profiles, and a watcher auto-attaches to a
// spawned child with a matching comm and loads the profile's table.

#include "gui/mainwindow.hpp"
#include "gui/profiles.hpp"
#include "core/ct_file.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QListWidget>

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
    qputenv("XDG_CONFIG_HOME", "/tmp/ns-profiles-config");
    qputenv("XDG_CACHE_HOME", "/tmp/ns-profiles-cache");
    QApplication app(argc, argv);
    app.setOrganizationName("cecore-test");
    app.setApplicationName("gui_profiles_smoke");
    std::printf("── Game profiles ──\n");

    {
        ce::gui::GameProfile one, two;
        one.name = "TF2";
        one.processMatch = "tf_linux64";
        one.tablePath = "/tmp/a.ct";
        two.name = "";
        two.processMatch = "hl2_linux";
        two.enabled = false;
        ce::gui::ProfileStore store;
        store.save({one, two});
        auto loaded = store.load();
        check("store round-trips two profiles",
              loaded.size() == 2 &&
              loaded[0].name == "TF2" && loaded[0].processMatch == "tf_linux64" &&
              loaded[0].tablePath == "/tmp/a.ct" && loaded[0].enabled &&
              loaded[1].processMatch == "hl2_linux" && !loaded[1].enabled);
    }

    {
        ce::gui::GameProfile profile;
        profile.name = "Watched";
        profile.processMatch = "ns_profwatch";
        ce::gui::ProfilesDialog dialog({profile}, false, nullptr);
        check("dialog lists the profile", dialog.listForTest()->count() == 1);
        dialog.selectRowForTest(0);
        dialog.profiles();
        check("dialog returns the profiles unchanged",
              dialog.profiles().size() == 1 && dialog.profiles().front().processMatch == "ns_profwatch");
    }

    ce::CheatTable table;
    ce::CheatEntry entry;
    entry.id = 1;
    entry.description = "Profile record";
    entry.type = ce::ValueType::Int32;
    entry.address = 0x1000;
    table.entries.push_back(entry);
    const auto tablePath = "/tmp/ns_profiles_smoke_table.json";
    check("profile table saves", table.saveJson(tablePath));

    {
        ce::gui::GameProfile profile;
        profile.name = "Auto";
        profile.processMatch = "ns_profwatch";
        profile.tablePath = QString::fromStdString(tablePath);
        ce::gui::ProfileStore store;
        store.save({profile});
    }

    ce::gui::MainWindow main;
    check("main window loaded the stored profile",
          main.gameProfilesForTest().size() == 1);
    main.startProfileWatchers();

    const pid_t child = fork();
    if (child == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        prctl(PR_SET_NAME, "ns_profwatch_t");
        for (;;) pause();
    }

    bool attached = false;
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 10000) {
            QApplication::processEvents(QEventLoop::AllEvents, 20);
            if (main.currentPidForTest() == child) { attached = true; break; }
        }
    }
    check("the watcher auto-attached to the matching child", attached);

    auto* model = main.findChild<ce::gui::AddressListModel*>();
    check("the profile's table loaded with the auto-attach",
          model && model->rowCount() == 1);

    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);
    std::remove(tablePath);

    std::printf(failures ? "game profiles smoke: FAILED\n" : "game profiles smoke: OK\n");
    return failures ? 1 : 0;
}
