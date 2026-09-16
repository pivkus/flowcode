#include "ChatInterface.hpp"

#include <QLabel>

ToolInfoBlock::ToolInfoBlock(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);

    label = new QLabel(this);
    label->setWordWrap(true);
    layout->addWidget(label);

    setStyleSheet("border: 1px solid pink;"); // DEBUG
    setLayout(layout);
}

void ToolInfoBlock::start(const QString &name, ToolCallId tcid){
    label->setText(label->text() + "\nCalled: " + name);
}
void ToolInfoBlock::finish(ToolCallId tcid, bool status){
    label->setText(label->text() + "\nFinished tool call");
}

ErrorBlock::ErrorBlock(const QString &msg, QWidget *parent) : QWidget(parent){
    auto *layout = new QVBoxLayout(this);

    label = new QLabel(this);
    label->setWordWrap(true);
    label->setText(msg);

    layout->addWidget(label);

    setStyleSheet("border: 1px solid orange;"); // DEBUG
    setLayout(layout);
}

UserpromptBlock::UserpromptBlock(const QString &prompt, QWidget *parent) : QWidget(parent){
    auto *layout = new QVBoxLayout(this);

    label = new QLabel(this);
    label->setWordWrap(true);
    label->setText(prompt);
    layout->addWidget(label);

    setStyleSheet("border: 1px solid green;"); // DEBUG
    setLayout(layout);

}

ReasoningBlock::ReasoningBlock(QWidget *parent) : QWidget(parent){
    auto *layout = new QVBoxLayout(this);

    label = new QLabel(this);
    label->setWordWrap(true);
    layout->addWidget(label);

    setStyleSheet("border: 1px solid blue;"); // DEBUG
    setLayout(layout);
}

void ReasoningBlock::append(const QString &text){
    label->setText(label->text() + text);
}

OutputTextBlock::OutputTextBlock(QWidget *parent) : QWidget(parent){
    auto *layout = new QVBoxLayout(this);

    label = new QLabel(this);
    label->setWordWrap(true);
    layout->addWidget(label);

    setStyleSheet("border: 1px solid red;"); // DEBUG

    setLayout(layout);
}
void OutputTextBlock::append(const QString &text){
    // TODO: this is stupid, use a proper widget like QPlainTextEdit
    // but that is a pain to make resize
    label->setText(label->text() + text);
}

ChatInterface::ChatInterface(QWidget *parent)
 : QWidget(parent)
{
    feed_layout = new QVBoxLayout(this);
    feed_layout->addStretch(1);

    setLayout(feed_layout);
}

void ChatInterface::appendText(const QString &text){
    if (text.isEmpty()) return;
    ensureCurrentBlock<OutputTextBlock>()->append(text);
}

void ChatInterface::appendReasoning(const QString &text){
    if (text.isEmpty()) return;
    ensureCurrentBlock<ReasoningBlock>()->append(text);
}

void ChatInterface::appendPrompt(const QString &prompt){
    addBlock<UserpromptBlock>(prompt);
}

void ChatInterface::appendToolStarted(const QString &name, ToolCallId tcid){
    auto* block = ensureCurrentBlock<ToolInfoBlock>();

    block->start(name, tcid);
    active_tools.emplace(tcid, block);
}

void ChatInterface::appendToolFinished(ToolCallId tcid, bool status){

    auto it = active_tools.find(tcid);
    if (it == active_tools.end()) return;

    if (auto *block = it->second.data()){
        block->finish(tcid, status);
    }

    active_tools.erase(it);
}

void ChatInterface::appendError(const QString &error){
    addBlock<ErrorBlock>(error);
}

void ChatInterface::finishTurn(){
    current_block.clear();
}
