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
#include <cstddef>
#include <flowcode/Messages.hpp>


class AppSettings;

class ChatControlButton : public QToolButton {
    public:
        explicit ChatControlButton(const QString &label, QWidget *parent = nullptr);
        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent *event) override;

    private:
        QString minimum_label;
};

class ChatOptionsOverlay : public QWidget {
    Q_OBJECT
    public:
        explicit ChatOptionsOverlay(
            AppSettings &settings, QWidget *parent = nullptr);
        void toggleModelPicker();
        void toggleEffortPicker();
    signals:
        void dismissed();
        void modelsListRequested(bool force = false);
    protected:
        void mousePressEvent(QMouseEvent *event) override;
    public slots:
        void modelListReceived(ModelListPtr models);
        void refreshModels(bool force);
    private:
        AppSettings &settings;
        ModelListPtr catalogue = nullptr;
        bool catalogue_pending = true;
        
        enum class Target { Model, Effort };
        enum Page { Key, Loading, Failed, Model, Effort };

        void open(Target target);
        void dismiss();
        void resolvePage();
        void setPage(Page page);

        void submitKey(const QString& key);
        void submitModel(const QString& id);
        void submitEffort(const QString& effort);
        void refreshModels();
        void filterModels(const QString& query);

        QWidget *createKeyPage();
        QWidget *createLoadingPage();
        QWidget *createFailedPage();
        QWidget *createModelPage();
        QWidget *createEffortPage();
        
        Target target = Target::Model;
        Page page = Page::Key;

        QStackedWidget *pages = nullptr;
        QFrame *panel = nullptr;
        QListWidget *model_list = nullptr;
        QLineEdit *search_field = nullptr;
        // QListWidget *model_list = nullptr;
        // QLabel *model_status = nullptr;
};
