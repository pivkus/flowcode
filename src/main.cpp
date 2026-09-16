#include "core/Backend.hpp"
#include "core/EnvFile.hpp"

#include <QApplication>
#include "ui/MainWindow.hpp"
#include "Bridge.hpp"


int main(int argc, char *argv[]){

    QApplication app(argc, argv);

    loadEnvFile();

    Backend backend;
    Bridge bridge(backend);

    MainWindow window(bridge);
    window.show();

    return app.exec();
}