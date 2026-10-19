#pragma once
#include <optional>

#include <QWidget>

#include <Bridge.hpp>

class SessionStack;
class Sidebar;
class AppSettings;

class MainWindow : public QWidget {
    Q_OBJECT
    public:
        explicit MainWindow(Bridge& bridge, AppSettings &settings, QWidget *parent = nullptr);
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
