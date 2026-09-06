#pragma once

#include <QWidget>
#include <QHash>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QTextCharFormat>
#include <QStackedWidget>

#include "../Bridge.hpp"
#include "../core/Uuid.hpp"

// TODO: allow queuing a user_prompt even if there there are tokens comming in (active_response == true)
class SessionWidget : public QWidget {
    Q_OBJECT
    public:
        SessionWidget(Uuid id, QWidget *parent = nullptr);

        void renderSession(const TurnVec& history);

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


class SessionStack : public QStackedWidget {
    Q_OBJECT
    public:
        SessionStack(Bridge& bridge, QWidget *parent = nullptr);

        SessionWidget* create(Uuid id, const TurnVec& history = {});
        SessionWidget* get(Uuid id);


    private slots:
        void routeTokens(Uuid id, const QString &content);
        void routeReasoning(Uuid id, const QString &content);
        void routeToolStarted(Uuid id, const QString &name);
        void routeToolFinished(Uuid id, bool status);
        void routeError(Uuid id, const QString &content);
        void routeFinish(Uuid id);
    private:
        // Adds the Uuid to SessionWidget mapping to QStackedWidget
        QHash<Uuid, SessionWidget*> sessions;
    
        Bridge& bridge;
};