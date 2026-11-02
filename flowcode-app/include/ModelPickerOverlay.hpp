#pragma once

#include <QString>
#include <QFrame>
#include <QLineEdit>
#include <QToolButton>
#include <QMouseEvent>
#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QListWidget>
#include <flowcode/Messages.hpp>


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
        void togglePicker();
    public slots:
        void modelListReceived(ModelListPtr models);
        void refreshModels(bool force);
    signals:
        void dismissed();
        void modelsListRequested(bool force=false);
    protected:
        void mousePressEvent(QMouseEvent *event) override;
    private:
        void dismiss();
        QWidget *createKeyPage();
        QWidget *createModelPage();
        void showModelSelection();
        void submitKey();
        void submitModel();
        void filterModels();

        enum class State { NoKey, Loading, Failed, Loaded };
        void setState(State next);
        State state = State::NoKey;

        AppSettings &settings;
        bool request_pending = false;
        ModelListPtr catalogue;
        QLabel *status_label = nullptr;
        QListWidget *model_list = nullptr;
        QToolButton *refresh_button = nullptr;
        QFrame *panel = nullptr;
        QStackedWidget *pages = nullptr;
        QWidget *key_page = nullptr;
        QWidget *model_page = nullptr;
        QLineEdit *key_field = nullptr;
        QLineEdit *search_field = nullptr;
};
