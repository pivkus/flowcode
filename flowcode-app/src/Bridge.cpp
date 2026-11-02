#include <Bridge.hpp>
#include <variant>


template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

Bridge::Bridge(Harness& harness, QObject *parent)
 : QObject(parent), harness(harness) 
{
    harness.setMessageCallback([this](){
        QMetaObject::invokeMethod(this, &Bridge::processMessages, Qt::QueuedConnection);
    });
}

void Bridge::setOpenrouterKey(const QString &key) {
    harness.config.setOpenrouterKey(key.isEmpty()
        ? std::nullopt : std::optional<std::string>(key.toStdString()));
}

Bridge::~Bridge(){
    // Bridge is destroyed before Harness (declared after it in main), so the coordinator
    // thread is still running — unregister the callback so it can't call into a dead Harness
    harness.setMessageCallback(nullptr);
}

void Bridge::userPromptSent(Uuid id, const QString &content, const QString &model)
{
    harness.pushMessage(PromptSubmission {
        .sid = id,
        .prompt = content.toStdString(),
        .model = model.toStdString()
    });
}

void Bridge::sessionCreated(Uuid id)
{
    harness.pushMessage(SessionCreation { .sid = id });
}

void Bridge::sessionsListRequested()
{
    harness.pushMessage(ListSessionsReq {});
}

void Bridge::modelsListRequested()
{
    harness.pushMessage(ListModelsReq {});
}

void Bridge::sessionLoadRequested(Uuid id)
{
    harness.pushMessage(LoadSessionReq { .sid = id });
}

void Bridge::processMessages(){
    while (auto msg = harness.popMessage()){
        std::visit(overloaded{
                [&](OutputTokensDelta& ot){ emit tokensReceived(ot.sid, QString::fromStdString(ot.delta)); },
                [&](ReasoningTokensDelta& rt){ emit reasoningReceived(rt.sid, QString::fromStdString(rt.delta)); },
                [&](EmitToolStarted& ts){ emit toolCallStarted(ts.sid, ts.tcid ,QString::fromStdString(ts.name)); },
                [&](EmitToolResult& tr){ emit toolCallFinished(tr.sid, tr.tcid, tr.ok); },
                [&](SessionError& e){ emit responseError(e.sid, QString::fromStdString(e.msg)); },
                [&](SessionRetry& r){ emit responseRetry(r.sid, QString::fromStdString(r.msg), r.deadline); },
                [&](TurnFinished& tf){ emit responseFinished(tf.sid); },
                [&](SessionNamed& sn){ emit sessionNamed(sn.sid, QString::fromStdString(sn.name)); },
                [&](ListSessionsRes& ls){ emit sessionListReceived(std::move(ls.uuids), std::move(ls.names)); },
                [&](LoadSessionRes& ls){ emit sessionLoadReceived(ls.sid, std::move(ls.history)); },
                [&](ListModelsRes& ls){ emit modelListReceived(std::move(ls.models)); },
        }, *msg);
    }
}
