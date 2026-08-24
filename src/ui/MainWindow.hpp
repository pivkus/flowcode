#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QHBoxLayout>

#include "../Bridge.hpp"
#include "SessionWidget.hpp"

class MainWindow : public QWidget {
    Q_OBJECT
    public:
        explicit MainWindow(Bridge& bridge, QWidget *parent = nullptr);
    signals:
        void sessionCreated(Uuid id);
    private slots:
        void newSession();
    private:
        SessionManager manager;
        QHBoxLayout *layout = nullptr;
};