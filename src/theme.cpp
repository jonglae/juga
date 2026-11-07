#include "theme.h"
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
Theme::Theme(QObject *parent) : QObject(parent) {
    m_file = QDir::homePath() + "/.local/state/omarchy/current/theme/colors.toml";
    m_timer.setSingleShot(true); m_timer.setInterval(100);
    connect(&m_timer, &QTimer::timeout, this, &Theme::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this]{ m_timer.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this]{ m_timer.start(); });
    reload();
}
void Theme::reload() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty()) m_watcher.removePaths(paths);
    QString ancestor = QFileInfo(m_file).absolutePath();
    while (!QFile::exists(ancestor) && ancestor != "/") ancestor = QFileInfo(ancestor).absolutePath();
    if (QFile::exists(ancestor)) m_watcher.addPath(ancestor);
    QFile f(m_file);
    if (f.open(QIODevice::ReadOnly)) {
        auto match = QRegularExpression(R"(^\s*accent\s*=\s*["'](#[0-9a-fA-F]{6})["'])", QRegularExpression::MultilineOption).match(QString::fromUtf8(f.readAll()));
        if (match.hasMatch()) m_accent = match.captured(1);
        if (QFile::exists(m_file)) m_watcher.addPath(m_file);
    }
    QProcess p; p.start("hyprctl", {"-j", "getoption", "decoration:rounding"});
    if (p.waitForFinished(300)) {
        const auto obj = QJsonDocument::fromJson(p.readAllStandardOutput()).object();
        if (obj.value("int").isDouble()) m_radius = qMax(0, obj.value("int").toInt());
    }
    emit changed();
}
