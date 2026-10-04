#pragma once

#include <QWidget>
#include <QHash>
#include <QStackedWidget>

#include "../core/Messages.hpp"
#include "../core/Uuid.hpp"

class Bridge;
class ChatInterface;
class QLineEdit;
class QScrollArea;
class QVBoxLayout;
class ErrorBlock;

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

class SessionWidget : public QWidget {
    Q_OBJECT
    public:
        SessionWidget(Uuid id, QWidget *parent = nullptr);

        void renderSession(const TurnVec& history);

        void finishResponse();
        void reportError(const QString &content);
        void reportRetry(const QString &content, std::chrono::steady_clock::time_point deadline);
        void clearRetryError();

        ChatInterface *chat = nullptr;
    signals:
        void userPromptSent(Uuid id, const QString &content, const QString &model);
    private slots:
        void submitPrompt();

    private:
        Uuid id;
        bool active_response = false;

        // QString selected_model = "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning:free";
        QString selected_model = "qwen/qwen3.8-27b:free";

        void setErrorBlock(ErrorBlock *block);
        void clearError();
        QVBoxLayout *main_layout = nullptr;
        ErrorBlock *error_block = nullptr;

        QLineEdit *input_field = nullptr;
        QScrollArea *chat_area = nullptr;
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
        void routeToolStarted(Uuid id, ToolCallId tcid, const QString &name);
        void routeToolFinished(Uuid id, ToolCallId tcid, bool status);
        void routeError(Uuid id, const QString &content);
        void routeRetry(Uuid id, const QString &content, std::chrono::steady_clock::time_point deadline);
        void routeFinish(Uuid id);
    private:
        // Adds the Uuid to SessionWidget mapping to QStackedWidget
        QHash<Uuid, SessionWidget*> sessions;

        Bridge& bridge;
};
