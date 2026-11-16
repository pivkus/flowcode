#include <QFrame>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QShortcut>
#include <QVBoxLayout>
#include <QtMath>

#include <ModelPickerOverlay.hpp>
#include <AppSettings.hpp>
#include <QLabel>
#include <Theme.hpp>

// QStackedWidget normally reserves space for its largest (even hidden) page.
class ModelPickerPages : public QStackedWidget {
    public:
        explicit ModelPickerPages(QWidget *parent) : QStackedWidget(parent) {
            layout()->setSizeConstraint(QLayout::SetNoConstraint);
            connect(this, &QStackedWidget::currentChanged, this, [this]{
                // Set QSizePolicy::Ignored for inactiv pages
                for (int i = 0; i < count(); ++i) {
                    const auto policy = i == currentIndex()
                        ? QSizePolicy::Preferred : QSizePolicy::Ignored;
                    widget(i)->setSizePolicy(policy, policy);
                }
                updateGeometry();
            });
        }

        QSize sizeHint() const override {
            return currentWidget() ? currentWidget()->sizeHint() : QStackedWidget::sizeHint();
        }

        QSize minimumSizeHint() const override {
            return currentWidget() ? currentWidget()->minimumSizeHint()
                                   : QStackedWidget::minimumSizeHint();
        }

};

class ModelPickerPanel : public QFrame {
    public:
        explicit ModelPickerPanel(QWidget *parent) : QFrame(parent) {
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        }

    protected:
        void paintEvent(QPaintEvent*) override {
            QPainter painter(this);
            paintBackground(painter, rect(), theme.background, theme.border);
        }
};

ChatControlButton::ChatControlButton(const QString &label, QWidget *parent)
 : QToolButton(parent), minimum_label(label)
{
    setText(label);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
}

QSize ChatControlButton::sizeHint() const {
    return {qCeil(QFontMetricsF(font()).horizontalAdvance(text())) + 2 * Sizing::horizontal_pad,
            fontMetrics().height() + 2 * Sizing::vertical_pad};
}

QSize ChatControlButton::minimumSizeHint() const {
    return {qCeil(QFontMetricsF(font()).horizontalAdvance(minimum_label))
                + 2 * Sizing::horizontal_pad,
            sizeHint().height()};
}

void ChatControlButton::paintEvent(QPaintEvent*){
    QPainter painter(this);
    paintBackground(painter, rect(), isDown() ? theme.hover : theme.surface, theme.border);
    painter.setFont(font());
    painter.setPen(theme.text);

    const QRect text_rect = padded_rect(rect());
    painter.setClipRect(text_rect);
    painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                     fontMetrics().elidedText(text(), Qt::ElideRight, text_rect.width()));
}


ChatOptionsOverlay::ChatOptionsOverlay(AppSettings &settings, QWidget *parent)
    : settings(settings), QWidget(parent)
{
    panel = new ModelPickerPanel(this);
    pages = new ModelPickerPages(panel);

    auto *panel_layout = new QVBoxLayout(panel);
    panel_layout->addWidget(pages);

    auto *overlay_layout = new QVBoxLayout(this);
    overlay_layout->addWidget(panel, 0, Qt::AlignBottom | Qt::AlignLeft);

    pages->addWidget(createKeyPage());
    pages->addWidget(createLoadingPage());
    pages->addWidget(createFailedPage());
    pages->addWidget(createModelPage());
    pages->addWidget(createEffortPage());

    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated,
            this, &ChatOptionsOverlay::dismiss);

    catalogue_pending = true;
}

void ChatOptionsOverlay::mousePressEvent(QMouseEvent *event){
    if (!panel->geometry().contains(event->position().toPoint())) dismiss();
}

void ChatOptionsOverlay::toggleModelPicker(){
    if (!isVisible() || target != Target::Model) open(Target::Model);
    else dismiss();
}
void ChatOptionsOverlay::toggleEffortPicker(){
    if (!isVisible() || target != Target::Effort) open(Target::Effort);
    else dismiss();
}
// Set requested target, finds appropriate page, shows it
void ChatOptionsOverlay::open(Target requested){
    target = requested;
    resolvePage();
    if (page == Page::Model) filterModels(search_field->text());

    show();
    raise();
    pages->currentWidget()->setFocus(Qt::OtherFocusReason);
}
void ChatOptionsOverlay::setPage(Page next){
    page = next;
    pages->setCurrentWidget(pages->widget(page));
    if (isVisible()) pages->currentWidget()->setFocus(Qt::OtherFocusReason);
}
void ChatOptionsOverlay::resolvePage(){
    if (!settings.hasOpenrouterKey()) setPage(Page::Key);
    else if (catalogue_pending) setPage(Page::Loading);
    else if (!catalogue) setPage(Page::Failed);
    else if (target == Target::Model) setPage(Page::Model);
    else if (settings.selectedModel().isEmpty()) setPage(Page::Model);
    else setPage(Page::Effort);
}
void ChatOptionsOverlay::modelListReceived(ModelListPtr models){
    catalogue = std::move(models);
    catalogue_pending = false;
    resolvePage();
}
void ChatOptionsOverlay::refreshModels(bool force){
    emit modelsListRequested(force);
    catalogue_pending = true;
    resolvePage();
}
void ChatOptionsOverlay::submitKey(const QString& key){
    settings.saveOpenrouterKey(key);
    resolvePage();
}
void ChatOptionsOverlay::submitModel(const QString& id){
    if (!settings.hasOpenrouterKey()) return;
    // Look up model info from catalogue
    auto it = catalogue->find(id.toStdString());
    if (it == catalogue->end()) return;
    const ModelInfo& model = it->second;

    bool changed = id != settings.selectedModel();

    // Select a sensible default for reasoning effort so
    // that it is not uninitialized
    std::string reasoning;

    if (!model.supports_reasoning) reasoning = "off";
    else if (!model.supported_efforts.empty()) 
        reasoning = model.supported_efforts.front();
    else reasoning = "on";



    settings.selectModel(id);
    if (changed) settings.selectEffort(QString::fromStdString(reasoning));
    
    if (target == Target::Model) dismiss();
    else resolvePage();
}
void ChatOptionsOverlay::submitEffort(const QString& effort){
    settings.selectEffort(effort);
    dismiss();
}
void ChatOptionsOverlay::dismiss(){
    hide();
    emit dismissed();
}

QWidget* ChatOptionsOverlay::createKeyPage(){
    auto *page = new QWidget(pages);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel("Connect your OpenRouter key", page));
 
    auto *field = new QLineEdit(page);
    field->setFrame(false);
    field->setStyleSheet(QStringLiteral("QLineEdit { background: %1; }")
        .arg(theme.background.name()));
    field->setEchoMode(QLineEdit::Password);
    field->setPlaceholderText("OpenRouter API key");
    field->setAccessibleName("OpenRouter API key");
    layout->addWidget(field);
    page->setFocusProxy(field);
    connect(field, &QLineEdit::returnPressed, this, [this, field]{
        submitKey(field->text());
    });
    connect(&settings, &AppSettings::openrouterKeySaved, field, &QLineEdit::clear);
    connect(this, &ChatOptionsOverlay::dismissed, field, &QLineEdit::clear);
    return page;
}

QWidget* ChatOptionsOverlay::createLoadingPage(){
    auto *page = new QWidget(pages);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel("Loading models…", page));
    return page;
}

QWidget* ChatOptionsOverlay::createFailedPage(){
    auto *page = new QWidget(pages);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *message = new QLabel("Failed to load models. Try refreshing.", page);
    message->setWordWrap(true);
    layout->addWidget(message);
    auto *refresh = new ChatControlButton("Refresh", page);
    layout->addWidget(refresh, 0, Qt::AlignLeft);
    page->setFocusProxy(refresh);
    connect(refresh, &QToolButton::clicked, this, [this]{ refreshModels(true); });
    return page;
}

QWidget* ChatOptionsOverlay::createModelPage(){
    auto *page = new QWidget(pages);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel("Select a model", page));

    search_field = new QLineEdit(page);
    search_field->setStyleSheet(QStringLiteral("QLineEdit { background: %1; }")
        .arg(theme.background.name()));
    search_field->setPlaceholderText("Search models…");
    search_field->setAccessibleName("Search models");
    search_field->setClearButtonEnabled(true);
    layout->addWidget(search_field);
    page->setFocusProxy(search_field);

    model_list = new QListWidget(page);
    model_list->setAccessibleName("Matching models");
    model_list->setFrameShape(QFrame::NoFrame);

    model_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    model_list->setTextElideMode(Qt::ElideRight);
    model_list->setStyleSheet(QStringLiteral(
        "QListWidget { background: %1; color: %2; outline: none; }"
        "QListWidget::item { padding: %3px %4px; border-radius: %6px; }"
        "QListWidget::item:selected { background: %5; color: %2; }")
        .arg(theme.background.name(), theme.text.name())
        .arg(Sizing::vertical_pad).arg(Sizing::horizontal_pad)
        .arg(theme.hover.name()).arg(Sizing::corner_radius));
    layout->addWidget(model_list);

    auto *refresh = new ChatControlButton("Refresh", page);
    layout->addWidget(refresh, 0, Qt::AlignLeft);
    connect(refresh, &QToolButton::clicked, this, [this]{ refreshModels(true); });
    connect(search_field, &QLineEdit::textChanged, this, &ChatOptionsOverlay::filterModels);
    connect(this, &ChatOptionsOverlay::dismissed, search_field, &QLineEdit::clear);
    connect(pages, &QStackedWidget::currentChanged, page, 
        [this, page]{
            if (pages->currentWidget() == page) {
                filterModels(search_field->text());
            }
    });
    connect(&settings, &AppSettings::selectedModelChanged,
            page, [this]{ filterModels(search_field->text()); });

    const auto select = [this]{
        const auto *item = model_list->currentItem();
        if (item) submitModel(item->data(Qt::UserRole).toString());
    };
    connect(search_field, &QLineEdit::returnPressed, this, select);
    connect(model_list, &QListWidget::itemClicked, this, select);
    connect(model_list, &QListWidget::itemActivated, this, select);
    for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto *shortcut = new QShortcut(QKeySequence(key), model_list);
        shortcut->setContext(Qt::WidgetShortcut);
        connect(shortcut, &QShortcut::activated, this, select);
    }
    auto *down = new QShortcut(QKeySequence(Qt::Key_Down), search_field);
    down->setContext(Qt::WidgetShortcut);
    connect(down, &QShortcut::activated, this, [this]{
        if (model_list->currentItem()) model_list->setFocus(Qt::ShortcutFocusReason);
    });
    filterModels(search_field->text());
    return page;
}

QWidget* ChatOptionsOverlay::createEffortPage(){
    auto *page = new QWidget(pages);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel("Select reasoning effort", page));

    // Displays the (lack of) reasoning capabilities of selected model
    auto *status = new QLabel(page);
    status->setWordWrap(true);
    layout->addWidget(status);

    auto *efforts = new QListWidget(page);
    efforts->setAccessibleName("Reasoning effort levels");
    efforts->setFrameShape(QFrame::NoFrame);
    efforts->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    efforts->setTextElideMode(Qt::ElideRight);
    efforts->setStyleSheet(QStringLiteral(
        "QListWidget { background: %1; color: %2; outline: none; }"
        "QListWidget::item { padding: %3px %4px; border-radius: %6px; }"
        "QListWidget::item:selected { background: %5; color: %2; }")
        .arg(theme.background.name(), theme.text.name())
        .arg(Sizing::vertical_pad).arg(Sizing::horizontal_pad)
        .arg(theme.hover.name()).arg(Sizing::corner_radius));
    layout->addWidget(efforts);
    page->setFocusProxy(efforts);

    auto renderEfforts = [this, efforts, status](){
        Q_ASSERT(catalogue);
        efforts->clear();
        // Note: here I am also assuming catalogue is loaded
        // and the model is indeed selected, the widget state should
        // guarantee it, maybe assert
        auto model = catalogue->find(settings.selectedModel().toStdString());
        // Note: if the selected model not in catalogue (is it stale), the widget
        // should force a change of model i think (or react differently that giving up)
        if (model == catalogue->end()){
            status->setText("Reasoning information not available");
        } else if (!model->second.supports_reasoning){
            status->setText("This model does not support reasoning");
        } else if (model->second.supported_efforts.empty()){
            auto *on = new QListWidgetItem("On", efforts);
            on->setData(Qt::UserRole, QStringLiteral("on"));
            efforts->setCurrentItem(on);
            if (!model->second.reasoning_mandatory) {
                // Some models might not allow reasoning off
                auto *off = new QListWidgetItem("Off", efforts);
                off->setData(Qt::UserRole, QStringLiteral("off"));
                if (settings.selectedEffort() == "off") efforts->setCurrentItem(off);
            }
        } else {
            for (const auto &effort : model->second.supported_efforts) {
                const auto value = QString::fromStdString(effort);
                auto *item = new QListWidgetItem(value, efforts);
                item->setData(Qt::UserRole, value);
                if (value == settings.selectedEffort()) efforts->setCurrentItem(item);
            } 
        }

        status->setVisible(efforts->count() == 0);
        efforts->setVisible(efforts->count() > 0);

        if (efforts->count() > 0) {
            efforts->setFixedHeight(qMin(240, efforts->sizeHintForRow(0) * efforts->count()));
        }
        // Based on the visibility of status/efforts, the sizing might be different
        pages->updateGeometry();
    };

    // resolvePage guarantees that the page won't be made visible unless it has
    // all the data it needs (catalogue)  
    auto renderIfVisible = [this, page, renderEfforts]{
        if (pages->currentWidget() == page) renderEfforts();
    };

    connect(pages, &QStackedWidget::currentChanged, page, renderIfVisible);
    // Note: Do we need to render every time here? what if page is not efforts
    connect(&settings, &AppSettings::selectedModelChanged, page, renderIfVisible);
    connect(&settings, &AppSettings::selectedEffortChanged, page, renderIfVisible);

    const auto select = [this, efforts]{
        const auto *item = efforts->currentItem();
        if (item) submitEffort(item->data(Qt::UserRole).toString());
    };
    connect(efforts, &QListWidget::itemClicked, this, select);
    connect(efforts, &QListWidget::itemActivated, this, select);
    for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto *shortcut = new QShortcut(QKeySequence(key), efforts);
        shortcut->setContext(Qt::WidgetShortcut);
        connect(shortcut, &QShortcut::activated, this, select);
    }
    // No effort list rendering at construction.
    return page;
}

void ChatOptionsOverlay::filterModels(const QString& query){
    model_list->clear();

    QListWidgetItem *selected_item = nullptr;
    if (catalogue) {
        for (const auto &[model_id, model] : *catalogue) {
            const QString id = QString::fromStdString(model_id);
            if (!id.contains(query, Qt::CaseInsensitive)) continue;
    
            QString label = id;
            const auto separator = id.indexOf('/');
            if (separator > 0) {
                QString provider = id.left(separator);
                provider[0] = provider[0].toUpper();
                label = provider + QStringLiteral("   ") + id.mid(separator + 1);
            }
            auto *item = new QListWidgetItem(label, model_list);
            item->setData(Qt::UserRole, id);
            item->setToolTip(id);
            if (id == settings.selectedModel()) selected_item = item;
        }
    
    }
    model_list->sortItems();
    if (selected_item) {
        model_list->setCurrentItem(selected_item);
        model_list->scrollToItem(selected_item);
    } else if (model_list->count() > 0) {
        model_list->setCurrentRow(0);
    }
}
