#include "ModelPickerOverlay.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QShortcut>
#include <QVBoxLayout>

#include "Theme.hpp"

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
    return {fontMetrics().horizontalAdvance(text()) + 2 * Sizing::horizontal_pad,
            fontMetrics().height() + 2 * Sizing::vertical_pad};
}

QSize ModelPickerButton::minimumSizeHint() const {
    return {2 * Sizing::horizontal_pad + fontMetrics().maxWidth(), sizeHint().height()};
}

void ModelPickerButton::paintEvent(QPaintEvent*){
    QPainter painter(this);
    paintBackground(painter, rect(), isDown() ? theme.hover : theme.surface, theme.border);
    painter.setFont(font());
    painter.setPen(palette().color(QPalette::WindowText));

    const QRect text_rect = padded_rect(rect());
    painter.setClipRect(text_rect);
    painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                     fontMetrics().elidedText(text(), Qt::ElideRight, text_rect.width()));
}

ModelPickerOverlay::ModelPickerOverlay(QWidget *parent)
 : QWidget(parent)
{
    panel = new ModelPickerPanel(this);

    model_field = new QLineEdit(panel);
    model_field->setFrame(false);
    model_field->setStyleSheet(QStringLiteral("QLineEdit { background: %1; }")
        .arg(theme.background.name()));
    model_field->setPlaceholderText("Model identifier");

    auto *panel_layout = new QVBoxLayout(panel);
    panel_layout->setContentsMargins(Sizing::horizontal_pad, Sizing::vertical_pad,
                                     Sizing::vertical_pad, Sizing::vertical_pad);
    panel_layout->addWidget(model_field);

    auto *row = new QHBoxLayout;
    row->addWidget(panel, 2);
    row->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->addStretch();
    layout->addLayout(row);

    connect(model_field, &QLineEdit::returnPressed, this, [this]{
        const QString model = model_field->text().trimmed();
        if (model.isEmpty()) return;
        selected_model = model;
        emit modelSelected(selected_model);
        dismiss();
    });

    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, &ModelPickerOverlay::dismiss);
}

void ModelPickerOverlay::openPicker(){
    model_field->setText(selected_model);
    show();
    raise();
    model_field->setFocus();
    model_field->selectAll();
}

void ModelPickerOverlay::dismiss(){
    hide();
    emit dismissed();
}

void ModelPickerOverlay::mousePressEvent(QMouseEvent *event){
    if (!panel->geometry().contains(event->position().toPoint())) dismiss();
}
