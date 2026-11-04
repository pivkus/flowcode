#include <AppSettings.hpp>

AppSettings::AppSettings(QObject *parent) : QObject(parent) {}

QString AppSettings::selectedModel() const {
    return settings.value("model/selected").toString().trimmed();
}

QString AppSettings::openrouterKey() const {
    return settings.value("openrouter/key").toString().trimmed();
}

bool AppSettings::hasOpenrouterKey() const {
    return !openrouterKey().isEmpty();
}

void AppSettings::selectModel(const QString &model) {
    const QString value = model.trimmed();
    if (!hasOpenrouterKey() || value.isEmpty() || value == selectedModel()) return;
    settings.setValue("model/selected", value);
    emit selectedModelChanged(value);
}

void AppSettings::saveOpenrouterKey(const QString &key) {
    const QString value = key.trimmed();
    if (hasOpenrouterKey() || value.isEmpty()) return;
    settings.setValue("openrouter/key", value);
    emit openrouterKeySaved(value);
}

void AppSettings::modelListReceived(ModelListPtr models){
    model_request_pending = false;
    model_list = std::move(models);
    emit modelListUpdated(model_list);
}

ModelListPtr AppSettings::getModelList(bool force){
    if (!model_request_pending && (force || !model_list) && hasOpenrouterKey()) {
        model_request_pending = true;
        model_list.reset();
        emit modelsListRequested(force);
    }
    return model_list;
}
