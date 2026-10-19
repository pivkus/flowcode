#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

class AppSettings : public QObject {
    Q_OBJECT
public:
    explicit AppSettings(QObject *parent = nullptr);

    QString selectedModel() const;
    QString openrouterKey() const;
    bool hasOpenrouterKey() const;

    void selectModel(const QString &model);
    void saveOpenrouterKey(const QString &key);

signals:
    void selectedModelChanged(const QString &model);
    void openrouterKeySaved(const QString &key);

private:
    QSettings settings;
};
