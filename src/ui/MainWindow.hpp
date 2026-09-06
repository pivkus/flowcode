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
        void focusSession(Uuid id);
        void onSessionLoaded(Uuid id, TurnVec history);
    private:
        Bridge& bridge;

        SessionStack *stack = nullptr;
        Sidebar *sidebar = nullptr;

        // Id of the session waiting to get loaded from disk to focus it
        std::optional<Uuid> pending_focus;
};