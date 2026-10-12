#include <QApplication>
#include <QMainWindow>
#include <QPalette>

#include <flowcode/FlowcodeHarness.hpp>
#include <EnvFile.hpp>

#include <Theme.hpp>
#include <Bridge.hpp>
#include <MainWindow.hpp>

int main(int argc, char *argv[]){
    QApplication app(argc, argv);

    QPalette palette = app.palette();
    // Set the default palette colors
    palette.setColor(QPalette::Window, theme.background);
    palette.setColor(QPalette::Base, theme.background);
    palette.setColor(QPalette::WindowText, theme.text);
    palette.setColor(QPalette::Text, theme.text);
    palette.setColor(QPalette::ButtonText, theme.text);
    palette.setColor(QPalette::PlaceholderText, theme.text_muted);
    app.setPalette(palette);

    loadEnvFile();

    Harness harness;
    Bridge bridge(harness);

    MainWindow window(bridge);
    window.show();

    return app.exec();
}
