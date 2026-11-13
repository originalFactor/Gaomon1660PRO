#include "pressure-dialog.h"
#include <QEasingCurve>
#include <QSignalSpy>
#include <QtTest>

class PressureTest : public QObject {
    Q_OBJECT
private slots:
    void serialization() {
        auto p = parsePressure(QStringLiteral("0.2,0.5;0.8,0.9;"));
        QVERIFY(p.has_value());
        QCOMPARE(p->first, QPointF(.2, .5));
        auto again = parsePressure(serializePressure(*p));
        QVERIFY(again);
        QCOMPARE(again->second, p->second);
        QVERIFY(!parsePressure(QStringLiteral("0,0;")));
        QVERIFY(!parsePressure(QStringLiteral("bad,0;1,1;")));
        QVERIFY(!parsePressure(QStringLiteral("0,-1;1,1;")));
        QVERIFY(!parsePressure(QStringLiteral("nan,0;1,1;")));
        const auto value = [](QPointF a, QPointF b) {
            QEasingCurve c(QEasingCurve::BezierSpline);
            c.addCubicBezierSegment(a, b, {1, 1});
            return c.valueForProgress(.5);
        };
        QVERIFY(value({0, .65}, {.35, 1}) > .5);
        QVERIFY(value({.65, 0}, {1, .35}) < .5);
        QVERIFY(qAbs(value({0, 0}, {1, 1}) - .5) < .001);
    }
    void dragPoints() {
        PressureCurve curve;
        curve.resize(300, 300);
        curve.show();
        QVERIFY(QTest::qWaitForWindowExposed(&curve));
        QSignalSpy changes(&curve, &PressureCurve::pointsChanged);
        auto r = curve.graphRect();
        QPoint start = r.bottomLeft().toPoint();
        QPoint destination = (r.topLeft() + QPointF(r.width() * .2, r.height() * .4)).toPoint();
        QTest::mousePress(&curve, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(&curve, destination);
        QTest::mouseRelease(&curve, Qt::LeftButton, Qt::NoModifier, destination);
        QVERIFY(changes.count() > 0);
        QVERIFY(qAbs(curve.value().first.x() - .2) < .02);
        QVERIFY(qAbs(curve.value().first.y() - .6) < .02);
        QTest::keyClick(&curve, Qt::Key_2);
        QTest::keyClick(&curve, Qt::Key_Left);
        QVERIFY(curve.value().second.x() < 1);
    }
    void dialogApplyAndDefaults() {
        PressureSettings initial;
        initial.name = QStringLiteral("test");
        initial.curve = QStringLiteral("0,0;1,1;");
        initial.defaultCurve = initial.curve;
        initial.rangeSupported = true;
        initial.minimum = .123456;
        initial.maximum = .987654;
        PressureSettings applied;
        int calls = 0;
        PressureDialog dialog(initial, [&](const PressureSettings &value) {
            applied = value;
            calls++;
            return QString();
        });
        auto *box = dialog.findChild<QDialogButtonBox *>();
        QVERIFY(box);
        auto *apply = box->button(QDialogButtonBox::Apply);
        QVERIFY(apply);
        QPushButton *soft = nullptr;
        for (auto *button : dialog.findChildren<QPushButton *>())
            if (button->text() == QStringLiteral("柔和"))
                soft = button;
        QVERIFY(soft);
        soft->click();
        QCOMPARE(calls, 0);
        apply->click();
        QCOMPARE(calls, 1);
        QVERIFY(applied.curve != initial.curve);
        QCOMPARE(applied.minimum, initial.minimum);
        QCOMPARE(applied.maximum, initial.maximum);
        box->button(QDialogButtonBox::RestoreDefaults)->click();
        QCOMPARE(calls, 1);
        apply->click();
        QCOMPARE(calls, 2);
        QCOMPARE(applied.curve, initial.defaultCurve);
        QCOMPARE(applied.minimum, 0.0);
        QCOMPARE(applied.maximum, 1.0);
    }
    void unknownCurveIsNotOverwritten() {
        PressureSettings initial;
        initial.curve = QStringLiteral("unrecognized");
        initial.defaultCurve = QStringLiteral("0,0;1,1;");
        int calls = 0;
        PressureDialog dialog(initial, [&](const PressureSettings &) {
            calls++;
            return QString();
        });
        auto *box = dialog.findChild<QDialogButtonBox *>();
        QVERIFY(box);
        QVERIFY(!box->button(QDialogButtonBox::Apply)->isEnabled());
        box->button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(box->button(QDialogButtonBox::Apply)->isEnabled());
        QCOMPARE(calls, 0);
    }
};
QTEST_MAIN(PressureTest)
#include "pressure-curve-test.moc"
