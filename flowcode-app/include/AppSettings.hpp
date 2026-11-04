#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

#include <flowcode/Messages.hpp>

class AppSettings : public QObject {
    Q_OBJECT
public:
    explicit AppSettings(QObject *parent = nullptr);

    QString selectedModel() const;
    QString openrouterKey() const;
    bool hasOpenrouterKey() const;

    void selectModel(const QString &model);
    void saveOpenrouterKey(const QString &key);

    ModelListPtr getModelList(bool force = false);
signals:
    void selectedModelChanged(const QString &model);
    void openrouterKeySaved(const QString &key);

    void modelListUpdated(ModelListPtr models);
    void modelsListRequested(bool force);
public slots:
    void modelListReceived(ModelListPtr models);

private:
    QSettings settings;
    ModelListPtr model_list;
    bool model_request_pending = false;
};
