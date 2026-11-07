#include "translator.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGuiApplication>
#include <QLocale>

Translator::Translator(QObject *parent) : QTranslator(parent) {
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &Translator::reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &Translator::reload);
    reload();
}

QString Translator::translate(const char *, const char *source, const char *, int) const {
    return m_strings.value(QString::fromUtf8(source)).toString();
}

void Translator::reload() {
    const QString path = QDir::homePath() + "/.config/omarchy/language";
    const auto watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty()) m_watcher.removePaths(watched);
    const QString directory = QFileInfo(path).absolutePath();
    if (QFileInfo::exists(directory)) m_watcher.addPath(directory);
    if (QFileInfo::exists(path)) m_watcher.addPath(path);
    QFile languageFile(path);
    QString language = "en";
    if (languageFile.open(QIODevice::ReadOnly)) language = QString::fromUtf8(languageFile.readLine()).trimmed();
    if (language.isEmpty()) language = "en";
    QJsonObject strings;
    if (language.startsWith("ko")) {
        QFile catalog(":/i18n/ko.json");
        if (catalog.open(QIODevice::ReadOnly)) strings = QJsonDocument::fromJson(catalog.readAll()).object();
    }
    if (language == m_language && strings == m_strings) return;
    m_language = language; m_strings = strings;
    const QLocale locale(language);
    QLocale::setDefault(locale);
    QGuiApplication::setLayoutDirection(locale.textDirection());
    emit changed();
}
