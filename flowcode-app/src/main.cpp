#include <QApplication>
#include <QMainWindow>
#include <QPalette>

#include <flowcode/FlowcodeHarness.hpp>

#include <Paths.hpp>
#include <Theme.hpp>
#include <Bridge.hpp>
#include <MainWindow.hpp>
#include <AppSettings.hpp>

int main(int argc, char *argv[]){
    QApplication app(argc, argv);
    app.setOrganizationName("flowcode");
    app.setApplicationName("flowcode");

    QPalette palette = app.palette();
    // Set the default palette colors
    palette.setColor(QPalette::Window, theme.background);
    palette.setColor(QPalette::Base, theme.background);
    palette.setColor(QPalette::WindowText, theme.text);
    palette.setColor(QPalette::Text, theme.text);
    palette.setColor(QPalette::ButtonText, theme.text);
    palette.setColor(QPalette::PlaceholderText, theme.text_muted);
    app.setPalette(palette);

    Harness harness(HarnessConfig {
            paths::sessionsDir(),
            FLOWCODE_SYSTEM_PROMPT_PATH
    });

    if (!harness.init()) return 1;

    Bridge bridge(harness);
    AppSettings settings;
    QObject::connect(&settings, &AppSettings::openrouterKeySaved, &bridge, &Bridge::setOpenrouterKey);
    bridge.setOpenrouterKey(settings.openrouterKey());

    MainWindow window(bridge, settings);
    window.show();

    return app.exec();
}
