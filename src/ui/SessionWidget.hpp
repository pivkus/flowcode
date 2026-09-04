#pragma once

#include <QWidget>
#include <QHash>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QTextCharFormat>

#include "../Bridge.hpp"
#include "../core/Uuid.hpp"

// TODO: allow queuing a user_prompt even if there there are tokens comming in (active_response == true)
class SessionWidget : public QWidget {
    Q_OBJECT
    public:
        SessionWidget(Uuid id, QWidget *parent = nullptr);
        void appendTokens(const QString &content);
        void appendReasoning(const QString &content);
        void appendToolStarted(const QString &name);
        void appendToolFinished(bool status);
        void finishResponse();
        void reportError(const QString &content);
    signals:
        void userPromptSent(Uuid id, const QString &content);
    private slots:
        void submitPrompt();
    private:
        void appendStyled(const QString &content, const QTextCharFormat &fmt);

        Uuid id;
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
        SessionWidget* createSession(Uuid id);
    
    private slots:
        void routeTokens(Uuid id, const QString &content);
        void routeReasoning(Uuid id, const QString &content);
        void routeToolStarted(Uuid id, const QString &name);
        void routeToolFinished(Uuid id, bool status);
        void routeError(Uuid id, const QString &content);
        void routeFinish(Uuid id);
    private:
        QHash<Uuid, SessionWidget*> sessions;
        Bridge& bridge;
};