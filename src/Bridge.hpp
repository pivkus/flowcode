#pragma once
#include <QObject>
#include <QString>
#include <QMetaObject>
#include "core/Backend.hpp"

#include "core/Uuid.hpp"

class Bridge : public QObject {
    Q_OBJECT
    public:
        Bridge(Backend& backend, QObject *parent = nullptr);
        ~Bridge();

    signals:
        void tokensReceived(Uuid id, const QString &content);
        void reasoningReceived(Uuid id, const QString &content);
        void toolCallStarted(Uuid id, const QString &name);
        void toolCallFinished(Uuid id, const QString &status);
        void responseFinished(Uuid id);
        void responseError(Uuid id, const QString &content);

    public slots:
        void userPromptSent(Uuid id, const QString &content);
        void sessionCreated(Uuid id);
    private:
        void drainMessages();

        Backend& backend;
};