#pragma once
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QTranslator>

class Translator : public QTranslator {
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY changed)
public:
    explicit Translator(QObject *parent = nullptr);
    QString language() const { return m_language; }
    QString translate(const char *, const char *source, const char *, int) const override;
signals:
    void changed();
private:
    void reload();
    QString m_language = "en";
    QJsonObject m_strings;
    QFileSystemWatcher m_watcher;
};
