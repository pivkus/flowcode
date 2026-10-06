#include "core/Backend.hpp"
#include "core/EnvFile.hpp"

#include <QApplication>
#include "ui/MainWindow.hpp"
#include "Bridge.hpp"

#include "ui/Theme.hpp"

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

    Backend backend;
    Bridge bridge(backend);

    MainWindow window(bridge);
    window.show();

    return app.exec();
}