#pragma once
#include "startup-paths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace StartupSettings {
inline QString autostartPath() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/autostart/gaomon-manage-panel.desktop");
}
inline bool autostartEnabled() {
    QSettings entry(autostartPath(), QSettings::IniFormat);
    entry.beginGroup(QStringLiteral("Desktop Entry"));
    return QFile::exists(autostartPath()) && !entry.value(QStringLiteral("Hidden"), false).toBool();
}
inline bool setAutostart(bool enabled, const QString &executable, QString &error) {
    const QString path = autostartPath();
    if (!enabled) {
        if (!QFile::exists(path) || QFile::remove(path))
            return true;
        error = QStringLiteral("无法移除登录自启动文件：%1").arg(path);
        return false;
    }
    if (executable.contains('\n') || executable.contains('\r')) {
        error = QStringLiteral("程序路径包含不支持的换行符。");
        return false;
    }
    // Desktop Entry 的字符串转义先于 Exec 参数转义，百分号需避免被当作字段代码。
    QString command = executable;
    command.replace(QStringLiteral("\\"), QStringLiteral("\\\\\\\\"));
    command.replace(QStringLiteral("\""), QStringLiteral("\\\\\""));
    command.replace(QStringLiteral("`"), QStringLiteral("\\\\`"));
    command.replace(QStringLiteral("$"), QStringLiteral("\\\\$"));
    command.replace(QStringLiteral("%"), QStringLiteral("%%"));
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        error = QStringLiteral("无法创建登录自启动目录。");
        return false;
    }
    QSaveFile file(path);
    const QByteArray data = (QStringLiteral("[Desktop Entry]\nType=Application\nName=Gaomon Manage Panel\nExec=\"")
        + command + QStringLiteral("\"\nIcon=gaomon-manage-panel\nTerminal=false\n")).toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        error = QStringLiteral("无法保存登录自启动设置：%1").arg(file.errorString());
        return false;
    }
    return true;
}
inline bool passwordlessEnabled() {
    return QSettings(QStringLiteral("GaomonManagePanel"), QStringLiteral("Shortcuts"))
        .value(QStringLiteral("passwordless"), false).toBool();
}
inline QString helperPath() {
    return passwordlessEnabled() ? QString::fromUtf8(GAOMON_INSTALLED_HELPER)
                                : QCoreApplication::applicationDirPath() + QStringLiteral("/gaomon-shortcut-helper");
}
}
