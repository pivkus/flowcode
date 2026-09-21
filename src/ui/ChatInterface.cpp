#include "ChatInterface.hpp"

#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QSizePolicy>
#include <QSvgRenderer>
#include <QTextLayout>
#include <QtMath>

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

ErrorBlock::ErrorBlock(const QString &msg, QWidget *parent)
: QWidget(parent), errmsg(msg)
{
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
}
QSize ErrorBlock::sizeHint() const {
    return {0, heightForWidth(width())};
}
QSize ErrorBlock::minimumSizeHint() const {
    return {2 * horizontal_pad + fontMetrics().maxWidth(),
            2 * vertical_pad + fontMetrics().height()};
}
int ErrorBlock::heightForWidth(int width) const {
    return layoutText(qMax(1, width - 2 * horizontal_pad)) + 2 * vertical_pad;
}
int ErrorBlock::layoutText(int width, QPainter *painter) const {
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    qreal height = 0;
    // Use the same layout for measurement and painting, preserving empty lines.
    const QString normalized = QString(errmsg).replace("\r\n", "\n").replace('\r', '\n');
    for (const auto &paragraph : normalized.split('\n')) {
        if (paragraph.isEmpty()) {
            height += fontMetrics().height();
            continue;
        }
        QTextLayout layout(paragraph, font());
        layout.setTextOption(option);
        layout.beginLayout();
        while (true) {
            auto line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(width);
            line.setPosition(QPointF(0, height));
            height += line.height();
        }
        layout.endLayout();
        if (painter) layout.draw(painter, QPointF(horizontal_pad, vertical_pad));
    }
    return qCeil(height);
}
void ErrorBlock::paintEvent(QPaintEvent *event){
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    auto bg_rect = rect();
    painter.setPen(QPen(QColor(255, 140, 0, 100), 1.0));
    painter.setBrush(QColor(255, 140, 0, 64));

    // Inset by half the stroke width to keep the outline inside the widget.
    painter.drawRoundedRect(QRectF(bg_rect).adjusted(0.5, 0.5, -0.5, -0.5),
                            corner_radius, corner_radius);

    const auto text_rect = bg_rect.adjusted(horizontal_pad, vertical_pad,
                                           -horizontal_pad, -vertical_pad);
    painter.setPen(Qt::white);
    painter.setClipRect(text_rect);
    layoutText(qMax(1, text_rect.width()), &painter);
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

ReasoningBlock::ReasoningBlock(QWidget *parent)
: QWidget(parent), bg_color("#343434")
{
    thinking_icon = new QSvgRenderer(QStringLiteral(":/assets/lightbulb.svg"), this);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}
QSize ReasoningBlock::sizeHint() const {
    QFont prefix_font(font());
    prefix_font.setWeight(QFont::DemiBold);
    const int height = qMax(fontMetrics().height(), QFontMetrics(prefix_font).height())
        + 2 * vertical_pad;
    return {0, height};
}
void ReasoningBlock::append(const QString &text){
    reasoning_text += text;
    update();
}
void ReasoningBlock::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QFont prefix_font(font());
    prefix_font.setWeight(QFont::DemiBold);
    QFont content_font(font());
    content_font.setWeight(QFont::Normal);
    const QFontMetrics prefix_metrics(prefix_font);
    const QFontMetrics content_metrics(content_font);

    auto bg_rect = rect();
    painter.setPen(Qt::NoPen); // Draw no outline
    painter.setBrush(bg_color);
    painter.drawRoundedRect(bg_rect, corner_radius, corner_radius);

    const QString prefix_text = "Thinking";
    const int prefix_width = prefix_metrics.horizontalAdvance(prefix_text);

    auto text_rect = bg_rect.adjusted(horizontal_pad, vertical_pad,
                                     -horizontal_pad, -vertical_pad);
    // Scale with the label font and keep the icon outside the stream's fade.
    const int icon_size = prefix_metrics.height();
    const QRectF icon_rect(text_rect.left(),
                           text_rect.top() + (text_rect.height() - icon_size) / 2.0,
                           icon_size, icon_size);
    thinking_icon->render(&painter, icon_rect);
    text_rect.adjust(icon_size + text_gap / 2, 0, 0, 0);
    auto text_flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine;
    painter.setFont(prefix_font);
    painter.setPen(QColor("#ededed"));
    painter.drawText(text_rect, text_flags, prefix_text);

    auto content_rect = text_rect.adjusted(prefix_width + text_gap, 0, 0, 0);
    if (content_rect.width() <= 0) return;

    int content_width = content_metrics.size(Qt::TextSingleLine, reasoning_text).width();
    const bool overflowing = content_width > content_rect.width();
    // Keep the newest tokens at the right edge without inserting an ellipsis.
    auto stream_rect = content_rect;
    if (overflowing) stream_rect.setLeft(content_rect.right() - content_width + 1);

    painter.save();
    painter.setClipRect(content_rect);
    painter.setFont(content_font);
    painter.setPen(QColor("#ababab"));
    painter.drawText(stream_rect, text_flags, reasoning_text);

    if (overflowing) {
        const int fade_width = qMin(48, content_rect.width());
        QLinearGradient fade(content_rect.left(), 0, content_rect.left() + fade_width, 0);
        fade.setColorAt(0, bg_color);
        QColor transparent_bg = bg_color;
        transparent_bg.setAlpha(0);
        fade.setColorAt(1, transparent_bg);
        painter.fillRect(QRect(content_rect.topLeft(), QSize(fade_width, content_rect.height())), fade);
    }
    painter.restore();
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
