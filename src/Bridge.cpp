#include "Bridge.hpp"


Bridge::Bridge(Backend& backend, QObject *parent) 
: QObject(parent), backend(backend)
{
    backend.response_queue.setCallback([this]{
        QMetaObject::invokeMethod(this, &Bridge::drainMessages, Qt::QueuedConnection);
    });
}

Bridge::~Bridge(){
    // Bridge is destroyed before Backend (declared after it in main), so the network
    // thread is still running — unregister the callback so it can't call into a dead Bridge
    backend.response_queue.setCallback(nullptr);
}

void Bridge::drainMessages(){
    while (auto msg = backend.response_queue.dequeue()){
        // TODO: turn this into a switch case
        if (msg->kind == ResponseMessage::Kind::RESPONSE_END){
            emit responseFinished(msg->id);
        } else if (msg->kind == ResponseMessage::Kind::RESPONSE_ERROR){
            emit responseError(msg->id, QString::fromStdString(msg->content));
        } else {
            emit tokensReceived(msg->id, QString::fromStdString(msg->content));
        }
    }
}

void Bridge::userPrompt(uint64_t id, const QString &content){
    RequestMessage resp {
        .id = id,
        .kind = RequestMessage::Kind::USER_PROMPT,
        .content = content.toStdString()
    };

    backend.request_queue.enqueue(std::move(resp));
}