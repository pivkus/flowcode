#include "Session.hpp"

#include <QVBoxLayout>

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

    connect(input, &QLineEdit::returnPressed, this, &SessionWidget::promptSubmitted);

}

void SessionWidget::appendTokens(const QString &content){
    text_box->insertPlainText(content);
}

void SessionWidget::finishResponse(){
    active_response = false;
}

void SessionWidget::reportError(const QString &content){
    text_box->insertPlainText("ERROR: " + content);
    active_response = false;
}

void SessionWidget::promptSubmitted(){
    if (active_response) return;
    active_response = true;

    const QString prompt = input->text();
    text_box->appendPlainText(">> " + prompt + "\n");
    emit userPromptSent(id, prompt);
    input->clear();
}

SessionManager::SessionManager(Bridge& bridge, QObject *parent)
: QObject(parent), bridge(bridge)
{
    connect(&bridge, &Bridge::tokensReceived, this, &SessionManager::routeTokens);
    connect(&bridge, &Bridge::responseFinished, this, &SessionManager::routeFinish);
    connect(&bridge, &Bridge::responseError, this, &SessionManager::routeError);
}

SessionWidget* SessionManager::createSession(uint64_t id){
    SessionWidget *w = new SessionWidget(id);
    sessions.insert(id, w);

    connect(w, &SessionWidget::userPromptSent, &bridge, &Bridge::userPrompt);
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