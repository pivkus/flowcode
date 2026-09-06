#include "SessionWidget.hpp"

#include <QVBoxLayout>
#include <QScrollBar>
#include <QString>

SessionWidget::SessionWidget(Uuid id, QWidget *parent)
 : QWidget(parent), id(id)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    text_box = new QPlainTextEdit(this);
    text_box->setReadOnly(true);

    input = new QLineEdit(this);
    
    layout->addWidget(text_box, 8);
    layout->addWidget(input, 2);

    setLayout(layout);

    connect(input, &QLineEdit::returnPressed, this, &SessionWidget::submitPrompt);

    reasoning_fmt.setForeground(QColor(128, 128, 128));
    tool_fmt.setForeground(QColor(0, 150, 170));
    prompt_fmt.setForeground(QColor(80, 160, 80));

    error_fmt.setForeground(QColor(200, 60, 60));
}

void SessionWidget::appendStyled(const QString &content, const QTextCharFormat &fmt){

    QScrollBar *v_bar = text_box->verticalScrollBar();
    bool is_at_bottom = (v_bar->value() >= v_bar->maximum() - 10);

    QTextCursor cursor(text_box->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(content, fmt);

    if (is_at_bottom) v_bar->setValue(v_bar->maximum());
}

// TODO: there is a lot of functions that just react slightly differently based on
// the type of text appended - maybe its worth factoring out to a single "append" with a switch case
// on the type - consider
void SessionWidget::appendTokens(const QString &content){
    appendStyled(content, output_fmt);
}

void SessionWidget::appendReasoning(const QString &content){
    appendStyled(content, reasoning_fmt);
}

void SessionWidget::appendToolStarted(const QString &name){
    appendStyled("\n[tool started: " + name + "]\n", tool_fmt);
}

void SessionWidget::appendToolFinished(bool status){
    appendStyled("[tool finished: " + QString::fromStdString(std::format("{}", status)) + "]\n", tool_fmt);
}

void SessionWidget::finishResponse(){
    active_response = false;
}

void SessionWidget::reportError(const QString &content){
    appendStyled("\nERROR: " + content + "\n", error_fmt);
    active_response = false;
}

void SessionWidget::submitPrompt(){
    if (active_response) return;
    active_response = true;

    const QString prompt = input->text();
    appendStyled("\n>> " + prompt + "\n", prompt_fmt);
    emit userPromptSent(id, prompt);
    input->clear();
}

void SessionWidget::renderSession(const TurnVec& history){
    for (const auto& turn : history){
        using enum Turn::Role;
        switch (turn->role){
            case USER: {
                // TODO: this is ugly maybe switch turn to a variant instead of tagged struct
                auto cnt = std::get<std::string>(turn->content);
                appendStyled("\n>> " + QString::fromStdString(cnt) + "\n", prompt_fmt);
                break;
            }
            case ASSISTANT: {
                auto cnt = std::get<AssistantContent>(turn->content);
                // TODO: tool calls
                appendTokens(QString::fromStdString(cnt.text));
                break;
            }
            case SYSTEM: { break; };
            case TOOL: {
                auto cnt = std::get<ToolResultContent>(turn->content);
                appendToolFinished(cnt.ok);
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
        w->appendTokens(content);
    }
}
void SessionStack::routeReasoning(Uuid id, const QString &content){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->appendReasoning(content);
    }
}

void SessionStack::routeToolStarted(Uuid id, const QString &name){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->appendToolStarted(name);
    }
}

void SessionStack::routeToolFinished(Uuid id, bool status){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->appendToolFinished(status);
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