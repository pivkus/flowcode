#pragma once

#include <QWidget>
#include <QHash>
#include <QPlainTextEdit>
#include <QLineEdit>

#include "../Bridge.hpp"

// TODO: allow queuing a user_prompt even if there there are tokens comming in (active_response == true)
class SessionWidget : public QWidget {
    Q_OBJECT
    public:
        SessionWidget(uint64_t id, QWidget *parent = nullptr);
        void appendTokens(const QString &content);
        void finishResponse();
        void reportError(const QString &content);
    signals:
        void userPromptSent(uint64_t id, const QString &content);
    private slots:
        void submitPrompt();
    private:
        uint64_t id;
        bool active_response = false;
        QLineEdit *input = nullptr;
        QPlainTextEdit *text_box = nullptr;
};

class SessionManager : public QObject {
    Q_OBJECT
    public:
        SessionManager(Bridge& bridge, QObject *parent = nullptr);
        SessionWidget* createSession(uint64_t id);
    
    private slots:
        void routeTokens(uint64_t id, const QString &content);
        void routeError(uint64_t id, const QString &content);
        void routeFinish(uint64_t id);
    private:
        QHash<uint64_t, SessionWidget*> sessions;
        Bridge& bridge;
};