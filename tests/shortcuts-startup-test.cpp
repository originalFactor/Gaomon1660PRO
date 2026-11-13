#include "shortcuts.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class ShortcutsStartupTest : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QByteArray originalPath;
    QByteArray originalConfigHome;
    int readyCount = 0;

    bool enabled() const {
        return QSettings(QStringLiteral("GaomonManagePanel"), QStringLiteral("Shortcuts"))
            .value(QStringLiteral("enabled"), false).toBool();
    }
    QPushButton *toggle(Shortcuts &shortcuts) {
        for (auto *button : shortcuts.findChildren<QPushButton *>())
            if (button->text().contains(QStringLiteral("用快捷键")))
                return button;
        return nullptr;
    }
    void writeLauncher(const QByteArray &script) {
        QFile launcher(directory.filePath(QStringLiteral("pkexec")));
        QVERIFY(launcher.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(launcher.write(script), script.size());
        launcher.close();
        QVERIFY(launcher.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }

private slots:
    void initTestCase() {
        QVERIFY(directory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
        originalConfigHome = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", directory.path().toLocal8Bit());
        originalPath = qgetenv("PATH");
        qputenv("PATH", directory.path().toLocal8Bit() + ':' + originalPath);
    }
    void init() {
        QSettings(QStringLiteral("GaomonManagePanel"), QStringLiteral("Shortcuts")).clear();
        readyCount = 0;
        // 模拟 helper 的就绪通知和通过 stdin 关闭的生命周期，不访问设备或调用授权。
        writeLauncher("#!/bin/sh\nprintf 'ready\\n'\nwhile IFS= read -r line; do :; done\n");
    }
    void successfulStartSurvivesExit() {
        QVERIFY(!enabled());
        {
            Shortcuts shortcuts([this] { ++readyCount; });
            shortcuts.start();
            QTRY_COMPARE(readyCount, 1);
            QVERIFY(enabled());
        }
        QVERIFY(enabled());
        Shortcuts reopened([this] { ++readyCount; });
        reopened.start();
        QTRY_COMPARE(readyCount, 2);
        QVERIFY(enabled());
    }
    void manualStopDisablesRestore() {
        Shortcuts shortcuts([this] { ++readyCount; });
        shortcuts.start();
        QTRY_COMPARE(readyCount, 1);
        auto *button = toggle(shortcuts);
        QVERIFY(button);
        button->click();
        QTRY_VERIFY(!shortcuts.isRunning());
        QVERIFY(!enabled());
    }
    void failedStartDoesNotEnableRestore() {
        writeLauncher("#!/bin/sh\nexit 1\n");
        Shortcuts shortcuts([this] { ++readyCount; });
        auto *process = shortcuts.findChild<QProcess *>();
        QVERIFY(process);
        QSignalSpy finished(process, &QProcess::finished);
        shortcuts.start();
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(readyCount, 0);
        QVERIFY(!enabled());
    }
    void loginAutostartCanBeEnabledAndRemoved() {
        QVERIFY(!StartupSettings::autostartEnabled());
        QString error;
        QVERIFY2(StartupSettings::setAutostart(true, QStringLiteral("/opt/Gaomon Panel/panel"), error), qPrintable(error));
        QVERIFY(StartupSettings::autostartEnabled());
        QFile entry(StartupSettings::autostartPath());
        QVERIFY(entry.open(QIODevice::ReadOnly));
        QVERIFY(entry.readAll().contains("Exec=\"/opt/Gaomon Panel/panel\"\n"));
        entry.close();
        QVERIFY(StartupSettings::setAutostart(false, {}, error));
        QVERIFY(!StartupSettings::autostartEnabled());
        QVERIFY(!QFile::exists(StartupSettings::autostartPath()));
    }
    void autostartEscapesFieldCodesAndRejectsNewlines() {
        QString error;
        QVERIFY(StartupSettings::setAutostart(true, QStringLiteral("/opt/100%/panel"), error));
        QFile entry(StartupSettings::autostartPath());
        QVERIFY(entry.open(QIODevice::ReadOnly));
        QVERIFY(entry.readAll().contains("Exec=\"/opt/100%%/panel\"\n"));
        entry.close();
        QVERIFY(!StartupSettings::setAutostart(true, QStringLiteral("/opt/panel\nHidden=true"), error));
        QVERIFY(StartupSettings::autostartEnabled());
        QVERIFY(StartupSettings::setAutostart(false, {}, error));
    }
    void passwordlessUsesInstalledHelper() {
        QCOMPARE(StartupSettings::helperPath(), QCoreApplication::applicationDirPath() + QStringLiteral("/gaomon-shortcut-helper"));
        QSettings(QStringLiteral("GaomonManagePanel"), QStringLiteral("Shortcuts"))
            .setValue(QStringLiteral("passwordless"), true);
        QCOMPARE(StartupSettings::helperPath(), QString::fromUtf8(GAOMON_INSTALLED_HELPER));
    }
    void cleanupTestCase() {
        qputenv("XDG_CONFIG_HOME", originalConfigHome);
        qputenv("PATH", originalPath);
    }
};

QTEST_MAIN(ShortcutsStartupTest)
#include "shortcuts-startup-test.moc"
