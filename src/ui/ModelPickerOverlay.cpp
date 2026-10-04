#include "ModelPickerOverlay.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QShortcut>
#include <QVBoxLayout>

namespace {
constexpr int horizontal_pad = 10;
constexpr int vertical_pad = 8;
constexpr int corner_radius = 8;

void paintBackground(QPainter& painter, const QWidget& widget, bool focused = false){
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds = QRectF(widget.rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    // Keep chat content from showing through the picker panel.
    painter.setPen(Qt::NoPen);
    painter.setBrush(widget.palette().color(QPalette::Window));
    painter.drawRoundedRect(bounds, corner_radius, corner_radius);

    QColor border = widget.palette().color(QPalette::WindowText);
    border.setAlpha(focused ? 100 : 40);
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(QColor(52, 52, 52, 64));
    painter.drawRoundedRect(bounds, corner_radius, corner_radius);
}

class ModelPickerPanel : public QFrame {
    public:
        explicit ModelPickerPanel(QWidget *parent) : QFrame(parent) {}

    protected:
        void paintEvent(QPaintEvent*) override {
            QPainter painter(this);
            paintBackground(painter, *this);
        }
};
}

ModelPickerButton::ModelPickerButton(QWidget *parent) : QToolButton(parent){
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
}

QSize ModelPickerButton::sizeHint() const {
    return {fontMetrics().horizontalAdvance(text()) + 2 * horizontal_pad,
            fontMetrics().height() + 2 * vertical_pad};
}

QSize ModelPickerButton::minimumSizeHint() const {
    return {2 * horizontal_pad + fontMetrics().maxWidth(), sizeHint().height()};
}

void ModelPickerButton::paintEvent(QPaintEvent*){
    QPainter painter(this);
    paintBackground(painter, *this, hasFocus() || isDown());
    painter.setFont(font());
    painter.setPen(palette().color(QPalette::WindowText));
    const QRect text_rect = rect().adjusted(horizontal_pad, vertical_pad,
                                            -horizontal_pad, -vertical_pad);
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
    model_field->setStyleSheet("QLineEdit { background: transparent; }");
    model_field->setPlaceholderText("Model identifier");

    auto *panel_layout = new QVBoxLayout(panel);
    panel_layout->setContentsMargins(horizontal_pad, vertical_pad, horizontal_pad, vertical_pad);
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
