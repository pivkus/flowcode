
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>
#include <QSvgRenderer>
#include <QTimer>

#include <ChatInterface.hpp>
#include <Theme.hpp>

ToolInfoBlock::ToolInfoBlock(QWidget *parent) : QWidget(parent) {
    tool_icon = new QSvgRenderer(themedSvg(QStringLiteral(":/assets/terminal.svg"), theme.text), this);
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setSizePolicy(policy);
    setMouseTracking(true);
}
void ToolInfoBlock::start(const QString &name, ToolCallId tcid){


    tools.push_back({tcid, name, Status::IN_PROGRESS});
    if (tools.size() == collapse_threshold + 1 && underMouse()){
        setSummaryHovered(summaryRect().contains(mapFromGlobal(QCursor::pos())));
    }
    updateGeometry();
    update();
}
void ToolInfoBlock::finish(ToolCallId tcid, bool status){
    for (auto &tool : tools){
        if (tool.id != tcid) continue;
        tool.status = status ? Status::SUCCEEDED : Status::FAILED;
        update();
        return;
    }
}
bool ToolInfoBlock::hasSummary() const {
    return tools.size() > collapse_threshold;
}
int ToolInfoBlock::rowHeight() const {
    QFont prefix_font(font());
    prefix_font.setWeight(QFont::DemiBold);
    return qMax(fontMetrics().height(), QFontMetrics(prefix_font).height())
        + 2 * Sizing::vertical_pad;
}
QRect ToolInfoBlock::summaryRect() const {
    return {Sizing::horizontal_pad, 0, width() - 2 * Sizing::horizontal_pad, rowHeight()};
}
std::pair<QString, QString> ToolInfoBlock::summaryText() const {
    int running = 0;
    int failed = 0;
    for (const auto &tool : tools){
        if (tool.status == Status::IN_PROGRESS) ++running;
        if (tool.status == Status::FAILED) ++failed;
    }
    QString summary = QStringLiteral("%1 tools").arg(tools.size());
    if (running > 0) summary += QStringLiteral(" · %1 running").arg(running);
    if (failed > 0) summary += QStringLiteral(" · %1 failed").arg(failed);
    return {running == 0 ? QStringLiteral("Used ") : QString(), summary};
}
void ToolInfoBlock::setSummaryHovered(bool hovered){
    summary_hovered = hovered;
    if (hovered) setCursor(Qt::PointingHandCursor);
    else unsetCursor();
    update(summaryRect());
}
void ToolInfoBlock::mouseMoveEvent(QMouseEvent *event){
    setSummaryHovered(hasSummary() && summaryRect().contains(event->position().toPoint()));
    QWidget::mouseMoveEvent(event);
}
void ToolInfoBlock::leaveEvent(QEvent *event){
    setSummaryHovered(false);
    QWidget::leaveEvent(event);
}
void ToolInfoBlock::mousePressEvent(QMouseEvent *event){
    if (event->button() == Qt::LeftButton && hasSummary()
        && summaryRect().contains(event->position().toPoint())){
        expanded = !expanded;
        updateGeometry();
        update();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

QSize ToolInfoBlock::sizeHint() const {
    int rows = static_cast<int>(tools.size());
    if (hasSummary()) rows = expanded ? rows + 1 : 1;
    return {0, rows * rowHeight()};
}
QSize ToolInfoBlock::minimumSizeHint() const {
    return {2 * Sizing::horizontal_pad + fontMetrics().maxWidth(),
            sizeHint().height()};
}
void ToolInfoBlock::paintEvent(QPaintEvent *event) {
    QPainter painter(this);

    const QRect rows_rect = rect().adjusted(Sizing::horizontal_pad, 0, -Sizing::horizontal_pad, 0);
    painter.setClipRect(rows_rect);

    QFont prefix_font(font());
    prefix_font.setWeight(QFont::DemiBold);
    const QFontMetrics prefix_metrics(prefix_font);

    const int row_height = rowHeight();
    const int icon_size = prefix_metrics.height();

    const auto drawText = [&](QRect area, const QString &prefix, const QString &detail){
        const auto flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine;
        if (!prefix.isEmpty()){
            const int prefix_width = prefix_metrics.horizontalAdvance(prefix);
            painter.setFont(prefix_font);
            painter.setPen(theme.text);
            painter.drawText(QRect(area.topLeft(), QSize(prefix_width, area.height())), flags, prefix);
            area.adjust(prefix_width, 0, 0, 0);
        }
        if (area.width() <= 0) return;
        painter.setFont(font());
        painter.setPen(theme.text_secondary);
        painter.drawText(area, flags, detail);
    };

    int y = rows_rect.top();
    if (hasSummary()){
        const QRect header = summaryRect();
        if (summary_hovered) paintBackground(painter, header, theme.hover);
        const auto [prefix, detail] = summaryText();
        drawText(header.adjusted(icon_gap, 0, -icon_gap, 0), prefix, detail);
        if (!expanded) return;
        y += row_height;
    }
    for (const auto &tool : tools) {
        QString prefix;
        switch (tool.status) {
            case Status::IN_PROGRESS: prefix = QStringLiteral("Using "); break;
            case Status::SUCCEEDED:   prefix = QStringLiteral("Used "); break;
            case Status::FAILED:      prefix = QStringLiteral("Failed "); break;
        }
        tool_icon->render(&painter, QRectF(rows_rect.left(),
                                          y + (row_height - icon_size) / 2.0,
                                          icon_size, icon_size));
        const QRect row(rows_rect.left(), y, rows_rect.width(), row_height);
        drawText(row.adjusted(icon_size + icon_gap, 0, 0, 0), prefix, tool.name);
        y += row_height;
    }
}



ErrorBlock::ErrorBlock(const QString &msg, QWidget *parent)
: QWidget(parent), errmsg(msg)
{
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
}
ErrorBlock::ErrorBlock(const QString &msg, std::chrono::steady_clock::time_point deadline, QWidget *parent)
: ErrorBlock(msg, parent)
{
    retry_timer = new QTimer(this);
    const auto update_countdown = [this, deadline]{
        const auto remaining = deadline - std::chrono::steady_clock::now();
        const auto seconds = std::chrono::ceil<std::chrono::seconds>(remaining).count();
        const QString status = seconds > 0
            ? QStringLiteral("Retrying in %1 s").arg(seconds)
            : QStringLiteral("Retrying…");
        if (status != retry_status){
            retry_status = status;
            updateGeometry();
            update();
        }
        if (seconds <= 0) retry_timer->stop();
    };
    connect(retry_timer, &QTimer::timeout, this, update_countdown);
    retry_timer->start(100);
    update_countdown();
}
QSize ErrorBlock::sizeHint() const {
    return {0, heightForWidth(width())};
}
QSize ErrorBlock::minimumSizeHint() const {
    return {2 * Sizing::horizontal_pad + fontMetrics().maxWidth(),
            2 * Sizing::vertical_pad + fontMetrics().height()};
}
int ErrorBlock::heightForWidth(int width) const {
    const QRect text_rect(0, 0, qMax(1, width - 2*Sizing::horizontal_pad), 0);
    const QRect bounds = fontMetrics().boundingRect(text_rect, text_flags, displayText());
    return qMax(fontMetrics().height(), bounds.height()) + 2*Sizing::vertical_pad;
}
QString ErrorBlock::displayText() const {
    return retry_status.isEmpty() ? errmsg : errmsg + '\n' + retry_status;
}
void ErrorBlock::paintEvent(QPaintEvent *event){
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    paintBackground(painter, rect(), theme.orange, theme.orange_border);
    const auto text_rect = padded_rect(rect());

    painter.setPen(theme.text_bright);
    painter.setClipRect(text_rect);
    painter.drawText(text_rect, text_flags, displayText());
}

// Known bug: QLabel interprets html-like text automatically, when a model emits
// <img src=""> for example it tries to open it
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
    return {2 * Sizing::horizontal_pad + fontMetrics().maxWidth(),
            2 * Sizing::vertical_pad   + fontMetrics().height()};
}
int UserpromptBlock::heightForWidth(int width) const {
    const int bubble_width = qMax(1, width * bubble_width_percent / 100);
    const QRect text_rect(0, 0, qMax(1, bubble_width - 2*Sizing::horizontal_pad), 0);

    const QRect bounds = fontMetrics().boundingRect(text_rect, text_flags, prompt_text);
    return qMax(fontMetrics().height(), bounds.height()) + 2 * Sizing::vertical_pad;
}
void UserpromptBlock::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int bubble_width = width() * bubble_width_percent / 100;

    const QRect bubble_rect(width() - bubble_width, 0, bubble_width, height());
    paintBackground(painter, bubble_rect, theme.blue, theme.blue_border);

    const QRect text_rect = padded_rect(bubble_rect);
    painter.setPen(theme.text_bright);
    painter.setClipRect(text_rect);
    painter.drawText(text_rect, text_flags, prompt_text);
}


ReasoningBlock::ReasoningBlock(QWidget *parent)
: QWidget(parent)
{
    thinking_icon = new QSvgRenderer(themedSvg(QStringLiteral(":/assets/lightbulb.svg"), theme.text), this);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}
QSize ReasoningBlock::sizeHint() const {
    QFont prefix_font(font());
    prefix_font.setWeight(QFont::DemiBold);
    const int height = qMax(fontMetrics().height(), QFontMetrics(prefix_font).height())
        + 2*Sizing::vertical_pad;
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

    paintBackground(painter, rect(), theme.surface, theme.border);

    const QString prefix_text = "Thinking";
    const int prefix_width = prefix_metrics.horizontalAdvance(prefix_text);

    QRect text_rect = padded_rect(rect());
    // Scale with the label font and keep the icon outside the stream's fade.
    const int icon_size = prefix_metrics.height();
    const QRectF icon_rect(text_rect.left(),
                           text_rect.top() + (text_rect.height() - icon_size) / 2.0,
                           icon_size, icon_size);
    thinking_icon->render(&painter, icon_rect);
    text_rect.adjust(icon_size + text_gap / 2, 0, 0, 0);
    auto text_flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine;
    painter.setFont(prefix_font);
    painter.setPen(theme.text);
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
    painter.setPen(theme.text_muted);
    painter.drawText(stream_rect, text_flags, reasoning_text);

    if (overflowing) {
        const int fade_width = qMin(48, content_rect.width());
        QLinearGradient fade(content_rect.left(), 0, content_rect.left() + fade_width, 0);
        fade.setColorAt(0, theme.surface);
        QColor transparent_bg = theme.surface;
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
    auto policy = label->sizePolicy();
    policy.setHorizontalPolicy(QSizePolicy::Ignored);
    label->setSizePolicy(policy);
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
    if (state != State::Active || text.isEmpty()) return;
    current_reasoning.clear();
    current_tools.clear();

    if (!current_output) current_output = addBlock<OutputTextBlock>();
    current_output->append(text);
}

void ChatInterface::appendReasoning(const QString &text){
    if (state != State::Active || text.isEmpty()) return;
    current_output.clear();

    if (!current_reasoning){
        current_reasoning = addBlock<ReasoningBlock>();
        if (current_tools){
            // Reasoning stays above tools even when the first event was a tool.
            feed_layout->removeWidget(current_reasoning);
            feed_layout->insertWidget(feed_layout->indexOf(current_tools), current_reasoning);
        }
    }
    current_reasoning->append(text);
}

void ChatInterface::appendPrompt(const QString &prompt){
    current_reasoning.clear();
    current_tools.clear();
    current_output.clear();

    addBlock<UserpromptBlock>(prompt);
    state = State::Active;
}

void ChatInterface::appendToolStarted(const QString &name, ToolCallId tcid){
    if (state != State::Active || active_tools.contains(tcid)) return;
    current_output.clear();
    
    if (!current_tools) current_tools = addBlock<ToolInfoBlock>();
    current_tools->start(name, tcid);
    active_tools.emplace(tcid, current_tools);
}

void ChatInterface::appendToolFinished(ToolCallId tcid, bool status){

    auto it = active_tools.find(tcid);
    if (it == active_tools.end()) return;

    if (auto *block = it->second.data()){
        block->finish(tcid, status);
    }

    active_tools.erase(it);
}

void ChatInterface::finishTurn(){
    current_reasoning.clear();
    current_tools.clear();
    current_output.clear();
    state = State::Passive;
}
