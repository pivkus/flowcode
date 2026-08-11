#include "SessionWidget.hpp"

#include <QVBoxLayout>
#include <QScrollBar>

SessionWidget::SessionWidget(uint64_t id, QWidget *parent)
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

void SessionWidget::appendTokens(const QString &content){
    appendStyled(content, output_fmt);
}

void SessionWidget::appendReasoning(const QString &content){
    appendStyled(content, reasoning_fmt);
}

void SessionWidget::appendToolStarted(const QString &name){
    appendStyled("\n[tool started: " + name + "]\n", tool_fmt);
}

void SessionWidget::appendToolFinished(const QString &status){
    appendStyled("[tool finished: " + status + "]\n", tool_fmt);
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

SessionManager::SessionManager(Bridge& bridge, QObject *parent)
: QObject(parent), bridge(bridge)
{
    connect(&bridge, &Bridge::tokensReceived, this, &SessionManager::routeTokens);
    connect(&bridge, &Bridge::reasoningReceived, this, &SessionManager::routeReasoning);
    connect(&bridge, &Bridge::toolCallStarted, this, &SessionManager::routeToolStarted);
    connect(&bridge, &Bridge::toolCallFinished, this, &SessionManager::routeToolFinished);
    connect(&bridge, &Bridge::responseFinished, this, &SessionManager::routeFinish);
    connect(&bridge, &Bridge::responseError, this, &SessionManager::routeError);
}

SessionWidget* SessionManager::createSession(uint64_t id){
    SessionWidget *w = new SessionWidget(id);
    sessions.insert(id, w);

    connect(w, &SessionWidget::userPromptSent, &bridge, &Bridge::userPromptSent);
    // Widget will be removed from map automatically
    connect(w, &QObject::destroyed, this, [this, id]{
        sessions.remove(id);
    });

    return w;
}

// TODO: report invalid id
void SessionManager::routeTokens(uint64_t id, const QString &content){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->appendTokens(content);
    }
}
void SessionManager::routeReasoning(uint64_t id, const QString &content){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->appendReasoning(content);
    }
}

void SessionManager::routeToolStarted(uint64_t id, const QString &name){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->appendToolStarted(name);
    }
}

void SessionManager::routeToolFinished(uint64_t id, const QString &status){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->appendToolFinished(status);
    }
}

void SessionManager::routeFinish(uint64_t id){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->finishResponse();
    }
}

void SessionManager::routeError(uint64_t id, const QString &content){
    if (SessionWidget* w = sessions.value(id, nullptr)){
        w->reportError(content);
    }
}