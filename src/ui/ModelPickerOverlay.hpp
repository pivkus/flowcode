#pragma once

#include <QString>
#include <QToolButton>
#include <QWidget>

class QFrame;
class QLineEdit;
class QMouseEvent;

class ModelPickerButton : public QToolButton {
    public:
        explicit ModelPickerButton(QWidget *parent = nullptr);
        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent *event) override;
};

class ModelPickerOverlay : public QWidget {
    Q_OBJECT
    public:
        explicit ModelPickerOverlay(QWidget *parent = nullptr);
        const QString& selectedModel() const { return selected_model; }
        void openPicker();

    signals:
        void modelSelected(const QString &model);
        void dismissed();

    protected:
        void mousePressEvent(QMouseEvent *event) override;

    private:
        void dismiss();

        QString selected_model = "qwen/qwen3.8-27b:free";
        QFrame *panel = nullptr;
        QLineEdit *model_field = nullptr;
};
