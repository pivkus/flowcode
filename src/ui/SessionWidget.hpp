#pragma once

#include <QWidget>
#include <QHash>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QTextCharFormat>

#include "../Bridge.hpp"

// TODO: allow queuing a user_prompt even if there there are tokens comming in (active_response == true)
class SessionWidget : public QWidget {
    Q_OBJECT
    public:
        SessionWidget(uint64_t id, QWidget *parent = nullptr);
        void appendTokens(const QString &content);
        void appendReasoning(const QString &content);
        void appendToolStarted(const QString &name);
        void appendToolFinished(const QString &status);
        void finishResponse();
        void reportError(const QString &content);
    signals:
        void userPromptSent(uint64_t id, const QString &content);
    private slots:
        void submitPrompt();
    private:
        void appendStyled(const QString &content, const QTextCharFormat &fmt);

        uint64_t id;
        bool active_response = false;
        QLineEdit *input = nullptr;
        QPlainTextEdit *text_box = nullptr;

        QTextCharFormat output_fmt;
        QTextCharFormat reasoning_fmt;
        QTextCharFormat tool_fmt;
        QTextCharFormat prompt_fmt;
        QTextCharFormat error_fmt;
};

class SessionManager : public QObject {
    Q_OBJECT
    public:
        SessionManager(Bridge& bridge, QObject *parent = nullptr);
        SessionWidget* createSession(uint64_t id);
    
    private slots:
        void routeTokens(uint64_t id, const QString &content);
        void routeReasoning(uint64_t id, const QString &content);
        void routeToolStarted(uint64_t id, const QString &name);
        void routeToolFinished(uint64_t id, const QString &status);
        void routeError(uint64_t id, const QString &content);
        void routeFinish(uint64_t id);
    private:
        QHash<uint64_t, SessionWidget*> sessions;
        Bridge& bridge;
};