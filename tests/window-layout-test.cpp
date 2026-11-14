#define main gaomonApplicationMain
#include "main.cpp"
#undef main
#include <QScrollBar>
#include <QTemporaryDir>
#include <QtTest>

class WindowLayoutTest : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDir;
private slots:
    void initTestCase() {
        QVERIFY(settingsDir.isValid());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir.path());
    }
    void constrainedHeight_data() {
        QTest::addColumn<int>("height");
        QTest::newRow("screenshot") << 840;
        QTest::newRow("small-screen") << 600;
        QTest::newRow("large-screen") << 1100;
    }
    void constrainedHeight() {
        QFETCH(int, height);
        Window window;
        // Model a window manager constraining the window to the screen size.
        window.setFixedSize(860, height);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto previews = window.findChildren<AreaPreview *>();
        QCOMPARE(previews.size(), 2);
        for (auto *preview : previews) {
            auto *box = preview->parentWidget();
            QVERIFY(box->rect().contains(preview->geometry()));
            for (auto *control : box->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
                if (control != preview && control->isVisible())
                    QVERIFY2(!preview->geometry().intersects(control->geometry()),
                             control->metaObject()->className());
            }
        }
        auto *scroll = window.findChild<QScrollArea *>();
        QVERIFY(scroll);
        if (height == 600)
            QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        for (auto *button : window.findChildren<QPushButton *>()) {
            if (button->text() == QStringLiteral("应用")) {
                QVERIFY(window.rect().contains(QRect(button->mapTo(&window, QPoint()), button->size())));
                QVERIFY(!scroll->isAncestorOf(button));
            }
        }
        if (height == 840 && !qEnvironmentVariableIsEmpty("GAOMON_LAYOUT_SCREENSHOT")) {
            scroll->verticalScrollBar()->setValue(0);
            QVERIFY(window.grab().save(qEnvironmentVariable("GAOMON_LAYOUT_SCREENSHOT")));
        }
    }
};
QTEST_MAIN(WindowLayoutTest)
#include "window-layout-test.moc"
