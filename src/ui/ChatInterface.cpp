#include "ChatInterface.hpp"

#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QSizePolicy>
#include <QSvgRenderer>
#include <QTextLayout>
#include <QtMath>

ToolInfoBlock::ToolInfoBlock(QWidget *parent) : QWidget(parent) {
    tool_icon = new QSvgRenderer(QStringLiteral(":/assets/terminal.svg"), this);
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setSizePolicy(policy);
}
void ToolInfoBlock::start(const QString &name, ToolCallId tcid){
    if (state.contains(tcid)) return; // Should not happen

    state[tcid] = Status::IN_PROGRESS;
    named[tcid] = name;
    updateGeometry();
    update();
}
void ToolInfoBlock::finish(ToolCallId tcid, bool status){
    if (!state.contains(tcid)) return;
    state[tcid] = status ? Status::SUCCEEDED : Status::FAILED;
    update();
}
QSize ToolInfoBlock::sizeHint() const {
    QFont prefix_font(font());
    prefix_font.setWeight(QFont::DemiBold);
    const int line_height = qMax(fontMetrics().height(), QFontMetrics(prefix_font).height());
    return {0, 2*vertical_pad + static_cast<int>(state.size()) * line_height};
}
QSize ToolInfoBlock::minimumSizeHint() const {
    return {2 * horizontal_pad + fontMetrics().maxWidth(),
            2 * vertical_pad + fontMetrics().height()};
}
void ToolInfoBlock::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    const QRect text_rect = rect().adjusted(horizontal_pad, vertical_pad,
                                            -horizontal_pad, -vertical_pad);
    painter.setClipRect(text_rect);

    QFont prefix_font(font());
    prefix_font.setWeight(QFont::DemiBold);
    const QFontMetrics prefix_metrics(prefix_font);

    const int line_height = qMax(fontMetrics().height(), prefix_metrics.height());
    const int icon_size = prefix_metrics.height();
    int y = text_rect.top();
    for (const auto &[tcid, status] : state) {
        const auto name = named.find(tcid);
        if (name == named.end()) continue;

        QString prefix;
        switch (status) {
            case Status::IN_PROGRESS: prefix = QStringLiteral("Using "); break;
            case Status::SUCCEEDED:   prefix = QStringLiteral("Used "); break;
            case Status::FAILED:      prefix = QStringLiteral("Failed "); break;
        }
        const int prefix_width = prefix_metrics.horizontalAdvance(prefix);
        tool_icon->render(&painter, QRectF(text_rect.left(),
                                          y + (line_height - icon_size) / 2.0,
                                          icon_size, icon_size));
        const int prefix_x = text_rect.left() + icon_size + icon_gap;
        painter.setFont(prefix_font);
        painter.setPen(QColor("#ededed"));
        painter.drawText(QRect(prefix_x, y, prefix_width, line_height),
                         Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, prefix);

        painter.setFont(font());
        painter.setPen(QColor("#b8b8b8"));
        const int name_x = prefix_x + prefix_width;
        painter.drawText(QRect(name_x, y,
                               qMax(0, text_rect.right() - name_x + 1), line_height),
                         Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                         name->second);
        y += line_height;
    }
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
    const QRect text_rect(0, 0, qMax(1, width - 2*horizontal_pad), 0);
    const QRect bounds = fontMetrics().boundingRect(text_rect, text_flags, errmsg);
    return qMax(fontMetrics().height(), bounds.height()) + 2*vertical_pad;
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
    painter.drawText(text_rect, text_flags, errmsg);
}


UserpromptBlock::UserpromptBlock(const QString &prompt, QWidget *parent)
: QWidget(parent), prompt_text(prompt)
{
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
}
QSize UserpromptBlock::sizeHint() const {
    return {0, heightForWidth(width())};
}
QSize UserpromptBlock::minimumSizeHint() const {
    return {2 * horizontal_pad + fontMetrics().maxWidth(),
            2 * vertical_pad + fontMetrics().height()};
}
int UserpromptBlock::heightForWidth(int width) const {
    const int bubble_width = qMax(1, width * bubble_width_percent / 100);
    const QRect text_rect(0, 0, qMax(1, bubble_width - 2 * horizontal_pad), 0);
    const QRect bounds = fontMetrics().boundingRect(text_rect, text_flags, prompt_text);
    return qMax(fontMetrics().height(), bounds.height()) + 2 * vertical_pad;
}
void UserpromptBlock::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int bubble_width = width() * bubble_width_percent / 100;
    const QRect bubble_rect(width() - bubble_width, 0, bubble_width, height());
    painter.setPen(QPen(QColor(70, 155, 255, 180), 1.0));
    painter.setBrush(QColor(25, 110, 220, 210));
    painter.drawRoundedRect(QRectF(bubble_rect).adjusted(0.5, 0.5, -0.5, -0.5),
                            corner_radius, corner_radius);

    const QRect text_rect = bubble_rect.adjusted(horizontal_pad, vertical_pad,
                                                 -horizontal_pad, -vertical_pad);
    painter.setPen(Qt::white);
    painter.setClipRect(text_rect);
    painter.drawText(text_rect, text_flags, prompt_text);
}


ReasoningBlock::ReasoningBlock(QWidget *parent)
: QWidget(parent), bg_color(52, 52, 52, 64)
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
    painter.setPen(QPen(QColor(255, 255, 255, 40), 0));
    painter.setBrush(bg_color);

    // Inset by half the stroke width to keep the outline inside the widget.
    painter.drawRoundedRect(QRectF(bg_rect).adjusted(0.5, 0.5, -0.5, -0.5),
                            corner_radius, corner_radius);

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
