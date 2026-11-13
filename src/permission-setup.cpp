#include "startup-paths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <cstdio>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

static bool trustedPath(const QString &path) {
    auto trustedParents = [](QString current) {
        for (;;) {
            struct stat info{};
            if (::stat(QFile::encodeName(current).constData(), &info) != 0 || info.st_uid != 0
                || (info.st_mode & (S_IWGRP | S_IWOTH)))
                return false;
            if (current == QStringLiteral("/"))
                return true;
            current = QFileInfo(current).absolutePath();
        }
    };
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return !canonical.isEmpty() && trustedParents(path) && trustedParents(canonical);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() != 2 || (args[1] != QStringLiteral("--enable") && args[1] != QStringLiteral("--disable"))) {
        fprintf(stderr, "usage: gaomon-permission-setup --enable|--disable\n");
        return 1;
    }
    bool valid = false;
    const uint uid = qEnvironmentVariable("PKEXEC_UID").toUInt(&valid);
    const auto *user = valid && uid != 0 ? getpwuid(uid) : nullptr;
    if (geteuid() != 0 || !user) {
        fprintf(stderr, "run through pkexec as a non-root user\n");
        return 1;
    }
    const QString rulesDirectory = QStringLiteral("/etc/polkit-1/rules.d");
    if (!QFileInfo(rulesDirectory).isDir() || !trustedPath(rulesDirectory)) {
        fprintf(stderr, "polkit rules directory must be owned and writable only by root\n");
        return 1;
    }
    const QString path = rulesDirectory + QStringLiteral("/49-gaomon-shortcuts-%1.rules").arg(uid);
    if (args[1] == QStringLiteral("--disable")) {
        if (!QFile::exists(path) || QFile::remove(path))
            return 0;
        fprintf(stderr, "cannot remove permission rule\n");
        return 1;
    }
    const QString helper = QString::fromUtf8(GAOMON_INSTALLED_HELPER);
    if (!QFileInfo(helper).isFile() || !QFileInfo(helper).isExecutable() || !trustedPath(helper)) {
        fprintf(stderr, "install the helper in the configured root-owned location first\n");
        return 1;
    }
    auto jsString = [](const QString &text) {
        const QByteArray json = QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);
        return json.mid(1, json.size() - 2);
    };
    const QByteArray rule = "// Managed by gaomon-permission-setup.\n"
        "polkit.addRule(function(action, subject) {\n"
        "    if (action.id === 'org.freedesktop.policykit.exec'\n"
        "        && action.lookup('program') === " + jsString(helper) + "\n"
        "        && subject.user === " + jsString(QString::fromLocal8Bit(user->pw_name)) + "\n"
        "        && subject.local && subject.active) {\n"
        "        return polkit.Result.YES;\n"
        "    }\n"
        "});\n";
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther)
        || file.write(rule) != rule.size() || !file.commit()) {
        fprintf(stderr, "cannot save permission rule\n");
        return 1;
    }
    return 0;
}
