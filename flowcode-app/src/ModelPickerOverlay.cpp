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

class ModelPickerPanel : public QFrame {
    public:
        explicit ModelPickerPanel(QWidget *parent) : QFrame(parent) {}

    protected:
        void paintEvent(QPaintEvent*) override {
            QPainter painter(this);
            paintBackground(painter, rect(), theme.background, theme.border);
        }
};

ModelPickerButton::ModelPickerButton(QWidget *parent) : QToolButton(parent){
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
}

QSize ModelPickerButton::sizeHint() const {
    return {qCeil(QFontMetricsF(font()).horizontalAdvance(text())) + 2 * Sizing::horizontal_pad,
            fontMetrics().height() + 2 * Sizing::vertical_pad};
}

QSize ModelPickerButton::minimumSizeHint() const {
    return {qCeil(QFontMetricsF(font()).horizontalAdvance(QStringLiteral("Select Model")))
                + 2 * Sizing::horizontal_pad,
            sizeHint().height()};
}

void ModelPickerButton::paintEvent(QPaintEvent*){
    QPainter painter(this);
    paintBackground(painter, rect(), isDown() ? theme.hover : theme.surface, theme.border);
    painter.setFont(font());
    painter.setPen(theme.text);

    const QRect text_rect = padded_rect(rect());
    painter.setClipRect(text_rect);
    painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                     fontMetrics().elidedText(text(), Qt::ElideRight, text_rect.width()));
}

ModelPickerOverlay::ModelPickerOverlay(AppSettings &settings, QWidget *parent)
 : QWidget(parent), settings(settings)
{
    panel = new ModelPickerPanel(this);
    panel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    pages = new QStackedWidget(panel);
    key_page = createKeyPage();
    model_page = createModelPage();
    pages->addWidget(key_page);
    pages->addWidget(model_page);

    auto *panel_layout = new QVBoxLayout(panel);
    panel_layout->setContentsMargins(Sizing::horizontal_pad, Sizing::vertical_pad,
                                     Sizing::horizontal_pad, Sizing::vertical_pad);
    panel_layout->addWidget(pages);

    auto *row = new QHBoxLayout;
    row->addWidget(panel, 2);
    row->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->addStretch();
    layout->addLayout(row);

    connect(&settings, &AppSettings::openrouterKeySaved, this, [this]{
        key_field->clear();
        if (isVisible()) showModelSelection();
    });
    setState(State::NoKey);

    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, &ModelPickerOverlay::dismiss);
}

QWidget *ModelPickerOverlay::createKeyPage(){
    auto *page = new QWidget(pages);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel("Connect your Openrouter key", page));

    key_field = new QLineEdit(page);
    key_field->setFrame(false);
    key_field->setStyleSheet(QStringLiteral("QLineEdit { background: %1; }")
        .arg(theme.background.name()));
    key_field->setEchoMode(QLineEdit::Password);
    key_field->setPlaceholderText("Openrouter API key");
    key_field->setAccessibleName("Openrouter API key");
    layout->addWidget(key_field);
    page->setFocusProxy(key_field);

    connect(key_field, &QLineEdit::returnPressed, this, &ModelPickerOverlay::submitKey);
    return page;
}

QWidget *ModelPickerOverlay::createModelPage(){
    auto *page = new QWidget(pages);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel("Select a model", page));

    search_field = new QLineEdit(page);
    search_field->setFrame(false);
    search_field->setStyleSheet(QStringLiteral("QLineEdit { background: %1; }")
        .arg(theme.background.name()));
    search_field->setPlaceholderText("Search models…");
    search_field->setAccessibleName("Search models");
    search_field->setClearButtonEnabled(true);
    layout->addWidget(search_field);
    page->setFocusProxy(search_field);

    status_label = new QLabel(page);
    status_label->setWordWrap(true);
    layout->addWidget(status_label);

    model_list = new QListWidget(page);
    model_list->setAccessibleName("Matching models");
    model_list->setFrameShape(QFrame::NoFrame);
    model_list->setMinimumHeight(160);
    model_list->setMaximumHeight(240);
    model_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    model_list->setTextElideMode(Qt::ElideRight);
    model_list->setFont(page->font());
    model_list->setStyleSheet(QStringLiteral(
        "QListWidget { background: %1; color: %2; outline: none; }"
        "QListWidget::item { padding: %3px %4px; border-radius: %6px; }"
        "QListWidget::item:selected { background: %5; color: %2; }")
        .arg(theme.background.name(), theme.text.name())
        .arg(Sizing::vertical_pad).arg(Sizing::horizontal_pad)
        .arg(theme.hover.name()).arg(Sizing::corner_radius));
    layout->addWidget(model_list);

    refresh_button = new QToolButton(page);
    refresh_button->setText("Refresh");
    refresh_button->setCursor(Qt::PointingHandCursor);
    refresh_button->setStyleSheet(QStringLiteral(
        "QToolButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: %4px; padding: %5px %6px; }"
        "QToolButton:hover, QToolButton:pressed { background: %7; }"
        "QToolButton:disabled { color: %8; }")
        .arg(theme.surface.name(), theme.text.name(), theme.border.name())
        .arg(Sizing::corner_radius).arg(Sizing::vertical_pad).arg(Sizing::horizontal_pad)
        .arg(theme.hover.name(), theme.text_muted.name()));
    connect(refresh_button, &QToolButton::clicked, this, &ModelPickerOverlay::refreshModels);
    layout->addWidget(refresh_button, 0, Qt::AlignLeft);

    connect(search_field, &QLineEdit::textChanged, this, &ModelPickerOverlay::filterModels);
    connect(search_field, &QLineEdit::returnPressed, this, &ModelPickerOverlay::submitModel);
    connect(model_list, &QListWidget::itemClicked, this, &ModelPickerOverlay::submitModel);
    connect(model_list, &QListWidget::itemActivated, this, &ModelPickerOverlay::submitModel);

    // Native item activation does not consistently bind Return on every platform.
    for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto *select_model = new QShortcut(QKeySequence(key), model_list);
        select_model->setContext(Qt::WidgetShortcut);
        connect(select_model, &QShortcut::activated, this, &ModelPickerOverlay::submitModel);
    }

    auto *next_model = new QShortcut(QKeySequence(Qt::Key_Down), search_field);
    next_model->setContext(Qt::WidgetShortcut);
    connect(next_model, &QShortcut::activated, this, [this]{
        if (state == State::Loaded && model_list->currentItem()) model_list->setFocus();
    });
    return page;
}

void ModelPickerOverlay::openPicker(){
    if (settings.hasOpenrouterKey()) {
        showModelSelection();
    } else {
        key_field->clear();
        setState(State::NoKey);
    }
    show();
    raise();
    pages->currentWidget()->setFocus();
}

void ModelPickerOverlay::showModelSelection(){
    search_field->clear();
    refreshModels();
    if (isVisible()) search_field->setFocus();
}

void ModelPickerOverlay::setState(State next){
    state = next;
    pages->setCurrentWidget(state == State::NoKey ? key_page : model_page);
    refresh_button->setEnabled(state == State::Failed || state == State::Loaded);
    model_list->setVisible(state == State::Loaded);
    status_label->setVisible(state != State::NoKey);

    if (state != State::Loaded) {
        model_list->clear();
        catalogue.reset();
    }
    switch (state) {
        case State::NoKey:
            status_label->clear();
            break;
        case State::Loading:
            status_label->setText("Loading models…");
            break;
        case State::Failed:
            status_label->setText("Failed to load models. Try refreshing.");
            break;
        case State::Loaded:
            filterModels();
            break;
    }
}

void ModelPickerOverlay::refreshModels(){
    if (request_pending || !settings.hasOpenrouterKey()) return;
    request_pending = true;
    setState(State::Loading);
    emit modelsListRequested();
}

void ModelPickerOverlay::modelListReceived(std::shared_ptr<std::vector<ModelInfo>> models){
    if (!request_pending) return;
    request_pending = false;
    catalogue = std::move(models);
    if (!catalogue) {
        setState(State::Failed);
        return;
    }
    setState(State::Loaded);
}

void ModelPickerOverlay::submitKey(){
    settings.saveOpenrouterKey(key_field->text());
}

void ModelPickerOverlay::filterModels(){
    if (state != State::Loaded || !catalogue) return;

    model_list->clear();
    const QString query = search_field->text().trimmed();
    QListWidgetItem *selected_item = nullptr;
    const QString selected_model = settings.selectedModel();
    for (const auto &model : *catalogue) {
        const QString id = QString::fromStdString(model.id);
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
        if (id == selected_model) selected_item = item;
    }
    model_list->sortItems();
    if (selected_item) {
        model_list->setCurrentItem(selected_item);
        model_list->scrollToItem(selected_item);
    } else if (model_list->count() > 0) {
        model_list->setCurrentRow(0);
    }
    status_label->setVisible(model_list->count() == 0);
    status_label->setText(catalogue->empty() ? "No models available." : "No matching models.");
}

void ModelPickerOverlay::submitModel(){
    if (state != State::Loaded || !settings.hasOpenrouterKey()) return;
    const auto *item = model_list->currentItem();
    if (!item) return;
    settings.selectModel(item->data(Qt::UserRole).toString());
    dismiss();
}

void ModelPickerOverlay::dismiss(){
    key_field->clear();
    hide();
    emit dismissed();
}

void ModelPickerOverlay::mousePressEvent(QMouseEvent *event){
    if (!panel->geometry().contains(event->position().toPoint())) dismiss();
}
