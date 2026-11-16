#include <AppSettings.hpp>

AppSettings::AppSettings(QObject *parent) : QObject(parent) {}

QString AppSettings::selectedModel() const {
    return settings.value("model/selected").toString();
}

QString AppSettings::openrouterKey() const {
    return settings.value("openrouter/key").toString();
}

QString AppSettings::selectedEffort() const {
    return settings.value("model/effort").toString();
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

void AppSettings::selectEffort(const QString &effort){
    if (selectedModel().isEmpty()) return;
    const QString value = effort.trimmed();
    settings.setValue("model/effort", value);
    emit selectedEffortChanged(value);
}
