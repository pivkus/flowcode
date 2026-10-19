#pragma once

#include <QString>
#include <QFrame>
#include <QLineEdit>
#include <QToolButton>
#include <QMouseEvent>
#include <QWidget>
#include <QStackedWidget>

class AppSettings;

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
        explicit ModelPickerOverlay(AppSettings &settings, QWidget *parent = nullptr);
        void openPicker();
    signals:
        void dismissed();
    protected:
        void mousePressEvent(QMouseEvent *event) override;
    private:
        void dismiss();
        QWidget *createKeyPage();
        QWidget *createModelPage();
        void showModelSelection();
        void submitKey();
        void submitModel();

        AppSettings &settings;
        QFrame *panel = nullptr;
        QStackedWidget *pages = nullptr;
        QWidget *key_page = nullptr;
        QWidget *model_page = nullptr;
        QLineEdit *key_field = nullptr;
        QLineEdit *model_field = nullptr;
};
