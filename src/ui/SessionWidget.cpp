#include "SessionWidget.hpp"
#include "ChatInterface.hpp"
#include "../Bridge.hpp"

#include <QLineEdit>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QString>
#include <string>
#include <variant>

SessionWidget::SessionWidget(Uuid id, QWidget *parent)
 : QWidget(parent), id(id)
{
    QVBoxLayout *main_layout = new QVBoxLayout(this);

    input_field = new QLineEdit(this);
    chat = new ChatInterface(this);

    chat_area = new QScrollArea(this);
    chat_area->setWidgetResizable(true);
    chat_area->setWidget(chat);

    main_layout->addWidget(chat_area, 8);
    main_layout->addWidget(input_field, 2);

    setLayout(main_layout);

    connect(input_field, &QLineEdit::returnPressed, this, &SessionWidget::submitPrompt);

}

void SessionWidget::finishResponse(){
    chat->finishTurn();
    active_response = false;
}

void SessionWidget::reportError(const QString &content){
    chat->appendError(content);
    active_response = false;
}

void SessionWidget::submitPrompt(){
    if (active_response) return;
    active_response = true;

    const QString prompt = input_field->text();
    chat->appendPrompt(prompt);

    emit userPromptSent(id, prompt);
    input_field->clear();
}

void SessionWidget::renderSession(const TurnVec& history){
    for (const auto& turn : history){
        using enum Turn::Role;
        switch (turn->role){
            case USER: {
                // TODO: this is ugly maybe switch turn to a variant instead of tagged struct
                auto cnt = std::get<std::string>(turn->content);
                chat->appendPrompt(QString::fromStdString(cnt));
                break;
            }
            case ASSISTANT: {
                auto cnt = std::get<AssistantContent>(turn->content);
                // TODO: tool calls
                chat->appendText(QString::fromStdString(cnt.text));
                break;
            }
            case SYSTEM: { break; };
            case TOOL: {
                auto cnt = std::get<ToolResultContent>(turn->content);
                // appendToolFinished(cnt.ok);
                break;
            };
        }
    }
}

SessionStack::SessionStack(Bridge& bridge, QWidget *parent)
: QStackedWidget(parent), bridge(bridge)
{
    connect(&bridge, &Bridge::tokensReceived, this, &SessionStack::routeTokens);
    connect(&bridge, &Bridge::reasoningReceived, this, &SessionStack::routeReasoning);
    connect(&bridge, &Bridge::toolCallStarted, this, &SessionStack::routeToolStarted);
    connect(&bridge, &Bridge::toolCallFinished, this, &SessionStack::routeToolFinished);
    connect(&bridge, &Bridge::responseFinished, this, &SessionStack::routeFinish);
    connect(&bridge, &Bridge::responseError, this, &SessionStack::routeError);
}

SessionWidget* SessionStack::create(Uuid id, const TurnVec& history){
    SessionWidget *w = new SessionWidget(id);
    sessions.insert(id, w);

    addWidget(w); // Adds it to the internal QStackedWidget list

    connect(w, &SessionWidget::userPromptSent, &bridge, &Bridge::userPromptSent);
    return w;
}

SessionWidget* SessionStack::get(Uuid id){
    return sessions.value(id, nullptr);
}


// TODO: report invalid id
void SessionStack::routeTokens(Uuid id, const QString &content){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->chat->appendText(content);
    }
}
void SessionStack::routeReasoning(Uuid id, const QString &content){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->chat->appendReasoning(content);
    }
}

void SessionStack::routeToolStarted(Uuid id, ToolCallId tcid, const QString &name){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->chat->appendToolStarted(name, tcid);
    }
}

void SessionStack::routeToolFinished(Uuid id, ToolCallId tcid, bool status){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->chat->appendToolFinished(tcid, status);
    }
}

void SessionStack::routeFinish(Uuid id){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->finishResponse();
    }
}

void SessionStack::routeError(Uuid id, const QString &content){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->reportError(content);
    }
}
