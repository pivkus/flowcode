#pragma once
#include <QObject>
#include <QString>
#include <QMetaObject>
#include "core/Backend.hpp"



class Bridge : public QObject {
    Q_OBJECT
    public:
        Bridge(Backend& backend, QObject *parent = nullptr);
        ~Bridge();

    signals:
        void tokensReceived(uint64_t id, const QString &content);
        void responseFinished(uint64_t id);
        void responseError(uint64_t id, const QString &content);

    public slots:
        void userPrompt(uint64_t id, const QString &content);
    private:
        void drainMessages();

        Backend& backend;
};