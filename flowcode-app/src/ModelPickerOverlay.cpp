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
        showModelSelection();
    });
    connect(&settings, &AppSettings::selectedModelChanged, this, [this](const QString &model){
        model_field->setText(model);
    });

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

    model_field = new QLineEdit(page);
    model_field->setFrame(false);
    model_field->setStyleSheet(QStringLiteral("QLineEdit { background: %1; }")
        .arg(theme.background.name()));
    model_field->setPlaceholderText("provider/model-name");
    model_field->setAccessibleName("Model identifier");
    layout->addWidget(model_field);
    page->setFocusProxy(model_field);

    connect(model_field, &QLineEdit::returnPressed, this, &ModelPickerOverlay::submitModel);
    return page;
}

void ModelPickerOverlay::openPicker(){
    if (settings.hasOpenrouterKey()) {
        showModelSelection();
    } else {
        key_field->clear();
        pages->setCurrentWidget(key_page);
    }
    show();
    raise();
    pages->currentWidget()->setFocus();
}

void ModelPickerOverlay::showModelSelection(){
    model_field->setText(settings.selectedModel());
    model_field->selectAll();
    pages->setCurrentWidget(model_page);
    if (isVisible()) model_field->setFocus();
}

void ModelPickerOverlay::submitKey(){
    settings.saveOpenrouterKey(key_field->text());
}

void ModelPickerOverlay::submitModel(){
    const QString model = model_field->text().trimmed();
    if (model.isEmpty() || !settings.hasOpenrouterKey()) return;
    settings.selectModel(model);
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
