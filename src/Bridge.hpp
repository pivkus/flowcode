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
        void reasoningReceived(uint64_t id, const QString &content);
        void toolCallStarted(uint64_t id, const QString &name);
        void toolCallFinished(uint64_t id, const QString &status);
        void responseFinished(uint64_t id);
        void responseError(uint64_t id, const QString &content);

    public slots:
        void userPromptSent(uint64_t id, const QString &content);
        void sessionCreated(uint64_t id);
    private:
        void drainMessages();

        Backend& backend;
};