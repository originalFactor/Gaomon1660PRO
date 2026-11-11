#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QEventLoop>
#include <QProcess>
#include <QTimer>
#include <QtTest>

struct Area {
    double x = 0, y = 0, width = 1, height = 1;
};
Q_DECLARE_METATYPE(Area)
QDBusArgument &operator<<(QDBusArgument &a, const Area &r) {
    a.beginStructure();
    a << r.x << r.y << r.width << r.height;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, Area &r) {
    a.beginStructure();
    a >> r.x >> r.y >> r.width >> r.height;
    a.endStructure();
    return a;
}

class FakeManager : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.InputDeviceManager")
    Q_PROPERTY(QStringList devicesSysNames READ devicesSysNames)
public:
    QStringList devicesSysNames() const {
        return {QStringLiteral("event1"), QStringLiteral("event2")};
    }
};
class FakeDevice : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.InputDevice")
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(bool isVirtual MEMBER isVirtual)
    Q_PROPERTY(bool tabletTool MEMBER tabletTool)
    Q_PROPERTY(bool supportsLeftHanded MEMBER supportsLeftHanded)
    Q_PROPERTY(bool supportsInputArea MEMBER supportsInputArea)
    Q_PROPERTY(bool supportsOutputArea MEMBER supportsOutputArea)
    Q_PROPERTY(QString outputName MEMBER outputName)
    Q_PROPERTY(bool mapToWorkspace MEMBER mapToWorkspace)
    Q_PROPERTY(Area inputArea MEMBER inputArea)
    Q_PROPERTY(Area outputArea MEMBER outputArea)
    Q_PROPERTY(bool leftHanded MEMBER leftHanded)
    Q_PROPERTY(QString pressureCurve MEMBER pressureCurve)
    Q_PROPERTY(double pressureRangeMin MEMBER pressureRangeMin)
    Q_PROPERTY(double pressureRangeMax MEMBER pressureRangeMax)
public:
    QString name, outputName = QStringLiteral("HDMI-A-1"),
                  pressureCurve = QStringLiteral("0,0;1,1;");
    bool isVirtual = false, tabletTool = true, supportsLeftHanded = true, supportsInputArea = true,
         supportsOutputArea = true, mapToWorkspace = false, leftHanded = false;
    Area inputArea, outputArea;
    double pressureRangeMin = 0, pressureRangeMax = 1;
};
// Qt's MEMBER property comparison requires an equality operator.
bool operator==(const Area &a, const Area &b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}
bool operator!=(const Area &a, const Area &b) {
    return !(a == b);
}

class PressureSyncTest : public QObject {
    Q_OBJECT
private slots:
    void copiesPressureToVirtual() {
        qDBusRegisterMetaType<Area>();
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        QVERIFY(bus.registerService(QStringLiteral("org.kde.KWin")));
        FakeManager manager;
        FakeDevice real, virtualPen;
        real.name = QStringLiteral("GAOMON 1060PRO Pen");
        real.pressureCurve = QStringLiteral("0.1,0.7;0.5,0.9;");
        real.pressureRangeMin = .12;
        real.pressureRangeMax = .84;
        virtualPen.name = QStringLiteral("GaomonManagePanel Virtual Pen");
        virtualPen.isVirtual = true;
        QVERIFY(bus.registerObject(QStringLiteral("/org/kde/KWin/InputDevice"), &manager,
                                   QDBusConnection::ExportAllProperties));
        QVERIFY(bus.registerObject(QStringLiteral("/org/kde/KWin/InputDevice/event1"), &real,
                                   QDBusConnection::ExportAllProperties));
        QVERIFY(bus.registerObject(QStringLiteral("/org/kde/KWin/InputDevice/event2"), &virtualPen,
                                   QDBusConnection::ExportAllProperties));
        QProcess app;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&app, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &loop,
                &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        app.start(qEnvironmentVariable("GAOMON_TEST_APP"), {QStringLiteral("--sync-virtual")});
        timer.start(10000);
        loop.exec();
        if (app.state() != QProcess::NotRunning) {
            app.kill();
            app.waitForFinished();
            QFAIL("sync process timed out");
        }
        QVERIFY2(app.exitCode() == 0, app.readAllStandardError().constData());
        QCOMPARE(virtualPen.pressureCurve, real.pressureCurve);
        QCOMPARE(virtualPen.pressureRangeMin, real.pressureRangeMin);
        QCOMPARE(virtualPen.pressureRangeMax, real.pressureRangeMax);
    }
};
QTEST_GUILESS_MAIN(PressureSyncTest)
#include "pressure-sync-test.moc"
