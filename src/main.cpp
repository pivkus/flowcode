#include <print>
#include <optional>
#include <string>
#include <iostream>
#include <cstdint>
#include <chrono>
#include <thread>

#include <poll.h>
#include <unistd.h>

#include "core/Backend.hpp"
#include "core/EnvFile.hpp"

#include <QApplication>
#include <QWidget>
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