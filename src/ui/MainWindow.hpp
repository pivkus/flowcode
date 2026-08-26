#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QHBoxLayout>

#include "../Bridge.hpp"
#include "SessionWidget.hpp"
#include "Sidebar.hpp"

class MainWindow : public QWidget {
    Q_OBJECT
    public:
        explicit MainWindow(Bridge& bridge, QWidget *parent = nullptr);
    signals:
        void sessionCreated(Uuid id);
    private slots:
        void newSession();
    private:
        // This blank widget is be displayed as an empty session when "New session" is pressed
        // or on app startup as the default focus - may not have a id until backend sends it
        SessionWidget *blank = nullptr;

        SessionManager manager;
        Sidebar *sidebar = nullptr;
        QHBoxLayout *layout = nullptr;
};