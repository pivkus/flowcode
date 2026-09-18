#pragma once
#include <QObject>
#include <QString>
#include <QMetaType>
#include "core/Messages.hpp"
#include <vector>

#include "core/Uuid.hpp"
#include "core/Log.hpp"

class Backend;

// This is needed to use the Uuid type as a data item in qt
Q_DECLARE_METATYPE(Uuid)

class Bridge : public QObject {
    Q_OBJECT
    public:
        Bridge(Backend& backend, QObject *parent = nullptr);
        ~Bridge();

    signals:
        void tokensReceived(Uuid id, const QString &content);
        void reasoningReceived(Uuid id, const QString &content);
        void toolCallStarted(Uuid id, ToolCallId tcid, const QString &name);
        void toolCallFinished(Uuid id, ToolCallId tcid, bool status);
        void responseFinished(Uuid id);
        void responseError(Uuid id, const QString &content);

        void sessionListReceived(std::vector<Uuid> list);
        void sessionLoadReceived(Uuid id, TurnVec history);

    public slots:
        void userPromptSent(Uuid id, const QString &content);
        void sessionCreated(Uuid id);

        void sessionsListRequested();
        void sessionLoadRequested(Uuid id);
    private:
        void drainMessages();

        Backend& backend;
};