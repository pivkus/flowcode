#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QHBoxLayout>

#include "../Bridge.hpp"
#include "Session.hpp"

class MainWindow : public QWidget {
    Q_OBJECT
    public:
        explicit MainWindow(Bridge& bridge, QWidget *parent = nullptr);
    private:
        SessionManager manager;
        QHBoxLayout *layout = nullptr;
};