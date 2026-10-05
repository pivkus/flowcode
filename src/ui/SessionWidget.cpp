#include "SessionWidget.hpp"
#include "ChatInterface.hpp"
#include "ModelPickerOverlay.hpp"
#include "../Bridge.hpp"

#include <QLineEdit>
#include <QScrollArea>
#include <QStackedLayout>
#include <QToolButton>
#include <QVBoxLayout>
#include <QString>
#include <string>
#include <variant>



SessionWidget::SessionWidget(Uuid id, QWidget *parent)
 : QWidget(parent), id(id)
{
    main_layout = new QVBoxLayout(this);

    input_field = new QLineEdit(this);
    chat = new ChatInterface(this);

    chat_area = new QScrollArea(this);
    chat_area->setWidgetResizable(true);
    chat_area->setWidget(chat);

    auto *chat_container = new QWidget(this);

    auto *chat_layout = new QStackedLayout(chat_container);
    chat_layout->setContentsMargins(0, 0, 0, 0);
    chat_layout->setStackingMode(QStackedLayout::StackAll);
    chat_layout->addWidget(chat_area);

    model_picker = new ModelPickerOverlay(chat_container);
    chat_layout->addWidget(model_picker);
    model_picker->hide();

    auto *input_group = new QWidget(this);
    auto *input_layout = new QVBoxLayout(input_group);
    input_layout->setContentsMargins(0, 0, 0, 0);

    auto *model_button = new ModelPickerButton(input_group);
    model_button->setText(model_picker->selected_model);
    input_layout->addWidget(model_button, 0, Qt::AlignLeft);
    input_layout->addWidget(input_field);

    main_layout->addWidget(chat_container, 1);
    main_layout->addWidget(input_group);

    connect(model_button, &QToolButton::clicked, model_picker, &ModelPickerOverlay::openPicker);
    connect(model_picker, &ModelPickerOverlay::modelSelected, model_button, &QToolButton::setText);
    connect(model_picker, &ModelPickerOverlay::dismissed, this, [this]{ input_field->setFocus(); });

    setLayout(main_layout);

    connect(input_field, &QLineEdit::returnPressed, this, &SessionWidget::submitPrompt);

}

void SessionWidget::finishResponse(){
    clearRetryError();
    chat->finishTurn();
    active_response = false;
}

void SessionWidget::reportError(const QString &content){
    setErrorBlock(new ErrorBlock(content, this));
    chat->finishTurn();
    active_response = false;
}

void SessionWidget::reportRetry(const QString &content, std::chrono::steady_clock::time_point deadline){
    setErrorBlock(new ErrorBlock(content, deadline, this));
    active_response = true;
}

void SessionWidget::setErrorBlock(ErrorBlock *block){
    clearError();
    error_block = block;
    main_layout->insertWidget(1, error_block);
}

void SessionWidget::clearError(){
    delete error_block;
    error_block = nullptr;
}

void SessionWidget::clearRetryError(){
    if (error_block && error_block->isRetry()) clearError();
}

void SessionWidget::submitPrompt(){
    if (active_response) return;
    active_response = true;
    clearError();

    const QString prompt = input_field->text();
    chat->appendPrompt(prompt);

    emit userPromptSent(id, prompt, model_picker->selected_model);
    input_field->clear();
}



void SessionWidget::renderSession(const TurnVec& history){
    for (const TurnPtr& turn : history){
        std::visit(overloaded{
            [&](const UserTurn& t){ chat->appendPrompt(QString::fromStdString(t.text)); },
            [&](const AssistantTurn& t){
                for (const auto& block : t.blocks){
                    const auto text = QString::fromStdString(block.text);
                    if (block.kind == AssistantBlock::Kind::OUTPUT) chat->appendText(text);
                    else chat->appendReasoning(text);
                }
                chat->finishTurn();
            }, 
            [&](const ToolResultTurn& t){}, // TODO: tools
            [&](const SystemTurn& t){} 
        }, *turn);
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
    connect(&bridge, &Bridge::responseRetry, this, &SessionStack::routeRetry);
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
        if (!content.isEmpty()) w->clearRetryError();
        w->chat->appendText(content);
    }
}
void SessionStack::routeReasoning(Uuid id, const QString &content){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        if (!content.isEmpty()) w->clearRetryError();
        w->chat->appendReasoning(content);
    }
}

void SessionStack::routeToolStarted(Uuid id, ToolCallId tcid, const QString &name){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->clearRetryError();
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

void SessionStack::routeRetry(Uuid id, const QString &content, std::chrono::steady_clock::time_point deadline){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->reportRetry(content, deadline);
    }
}
