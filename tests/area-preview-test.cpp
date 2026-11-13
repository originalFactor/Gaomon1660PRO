#include "area-preview.h"
#include "shortcuts.h"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class PreviewTest : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDir;
private slots:
    void initTestCase() {
        QVERIFY(settingsDir.isValid());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir.path());
    }
    void dragAndResize() {
        AreaPreview view(false);
        view.resize(400, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.setArea(QRectF(.25, .25, .5, .5));
        QSignalSpy changes(&view, &AreaPreview::areaChanged);
        QRectF canvas = view.canvasRect();
        QPoint start =
            (canvas.topLeft() + QPointF(canvas.width() * .5, canvas.height() * .5)).toPoint();
        QPoint moved = start + QPoint(qRound(canvas.width() * .1), qRound(canvas.height() * .1));
        QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(&view, moved);
        QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, moved);
        QVERIFY(changes.count() > 0);
        QVERIFY(qAbs(view.area().x() - .35) < .01);
        QVERIFY(qAbs(view.area().y() - .35) < .01);
        QRectF current = view.area();
        QPoint corner =
            (canvas.topLeft() + QPointF((current.x() + current.width()) * canvas.width(),
                                        (current.y() + current.height()) * canvas.height()))
                .toPoint();
        QPoint far = corner + QPoint(1000, 1000);
        QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, corner);
        QTest::mouseMove(&view, far);
        QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, far);
        QVERIFY(view.area().right() <= 1.00001);
        QVERIFY(view.area().bottom() <= 1.00001);
        QVERIFY(qAbs(view.area().x() - current.x()) < .01);
    }
    void softAreaMapping() {
        QRectF full(0, 0, 1, 1);
        QRectF physical = physicalInputArea(full, true);
        QVERIFY(qAbs(physical.top() - softAreaHeight) < 1e-10);
        QVERIFY(qAbs(physical.bottom() - 1.0) < 1e-10);
        QRectF roundTrip = displayInputArea(physical, true);
        QVERIFY(qAbs(roundTrip.top()) < 1e-10);
        QVERIFY(qAbs(roundTrip.height() - 1.0) < 1e-10);
        const QRectF partial(.1, .2, .6, .4);
        QRectF again = displayInputArea(physicalInputArea(partial, true), true);
        QVERIFY(qAbs(again.x() - partial.x()) < 1e-10);
        QVERIFY(qAbs(again.y() - partial.y()) < 1e-10);
        QVERIFY(qAbs(again.height() - partial.height()) < 1e-10);
        QCOMPARE(physicalInputArea(partial, false), partial);
        QCOMPARE(displayInputArea(partial, false), partial);
        QCOMPARE(displayInputArea(full, true), full);
    }
    void reservedAreaLimitsDragging() {
        AreaPreview view(true);
        view.resize(500, 350);
        view.setReserveSoft(true);
        view.setArea(physicalInputArea(QRectF(.2, .2, .5, .5), true));
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto c = view.canvasRect();
        auto a = view.area();
        QPoint middle = (c.topLeft() + QPointF((a.x() + a.width() / 2) * c.width(),
                                               (a.y() + a.height() / 2) * c.height()))
                            .toPoint();
        QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, middle);
        QTest::mouseMove(&view, middle - QPoint(0, 500));
        QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, middle - QPoint(0, 500));
        QVERIFY(view.area().top() >= softAreaHeight - 1e-10);
        a = view.area();
        QPoint top =
            (c.topLeft() + QPointF((a.x() + a.width() / 2) * c.width(), a.top() * c.height()))
                .toPoint();
        QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, top);
        QTest::mouseMove(&view, top - QPoint(0, 500));
        QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, top - QPoint(0, 500));
        QVERIFY(view.area().top() >= softAreaHeight - 1e-10);
        view.setReserveSoft(false);
        view.setArea(fullArea());
        QCOMPARE(view.area(), fullArea());
    }
    void shortcutRowsFollowSoftOption() {
        Shortcuts dialog([] {});
        auto *table = dialog.findChild<QTableWidget *>();
        QVERIFY(table);
        QCOMPARE(table->item(0, 0)->text(), QStringLiteral("笔侧键 1（靠近笔尖）"));
        QCOMPARE(table->item(1, 0)->text(), QStringLiteral("笔侧键 2"));
        for (int n = 0; n < 6; n++) {
            QCOMPARE(table->item(2 + n * 2, 0)->text(),
                     QStringLiteral("左列侧键 %1").arg(n + 1));
            QCOMPARE(table->item(3 + n * 2, 0)->text(),
                     QStringLiteral("右列侧键 %1").arg(n + 1));
        }
        QCOMPARE(table->item(14, 0)->text(), QStringLiteral("软键 1"));
        QCOMPARE(table->item(29, 0)->text(), QStringLiteral("软键 16"));
        auto *mode = qobject_cast<QComboBox *>(table->cellWidget(0, 1));
        QVERIFY(mode);
        QCOMPARE(mode->count(), 6);
        auto *output = qobject_cast<QStackedWidget *>(table->cellWidget(0, 2));
        QVERIFY(output);
        mode->setCurrentIndex(mode->findData(1));
        QCOMPARE(output->currentIndex(), 1);
        mode->setCurrentIndex(mode->findData(0));
        QCOMPARE(output->currentIndex(), 0);
        QCOMPARE(qobject_cast<QLabel *>(output->currentWidget())->text(),
                 QStringLiteral("B + 笔侧键事件"));
        mode->setCurrentIndex(mode->findData(5));
        QCOMPARE(output->currentIndex(), 3);
        QCOMPARE(qobject_cast<QLabel *>(output->currentWidget())->text(), QStringLiteral("不发送"));
        mode->setCurrentIndex(mode->findData(2));
        QCOMPARE(output->currentIndex(), 2);
        dialog.setSoftVisible(false);
        QVERIFY(table->isRowHidden(14));
        QVERIFY(table->isRowHidden(29));
        QVERIFY(!table->isRowHidden(0));
        QVERIFY(!table->isRowHidden(13));
        dialog.setSoftVisible(true);
        QVERIFY(!table->isRowHidden(14));
        QVERIFY(!table->isRowHidden(29));
    }
    void shortcutPreviewBindings() {
        QSettings cfg(QStringLiteral("GaomonManagePanel"), QStringLiteral("Shortcuts"));
        cfg.clear();
        cfg.setValue(QStringLiteral("shortcut/0"), QStringLiteral("Ctrl+Z"));
        cfg.sync();
        Shortcuts dialog([] {});
        dialog.resize(960, 720);
        dialog.setSoftVisible(true);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *tabs = dialog.findChild<QTabWidget *>();
        QCOMPARE(tabs->currentIndex(), 0);
        for (int i = 0; i < 30; i++) {
            auto *key = dialog.findChild<QPushButton *>(QStringLiteral("shortcutKey%1").arg(i));
            QVERIFY(key);
            QVERIFY(key->isVisible());
            QTest::mouseClick(key, Qt::LeftButton);
            auto *popup = dialog.findChild<QFrame *>(QStringLiteral("bindingPopup"));
            QVERIFY(popup && popup->isVisible());
            QVERIFY(!popup->findChild<QLabel *>(QStringLiteral("defaultBinding"))->text().isEmpty());
            auto *configured = popup->findChild<QLabel *>(QStringLiteral("configuredBinding"));
            if (i == 0) {
                QCOMPARE(popup->findChild<QLabel *>(QStringLiteral("defaultBinding"))->text(),
                         QStringLiteral("默认快捷键：A"));
                QVERIFY(configured->text().contains(QStringLiteral("Ctrl+Z")));
                if (!qEnvironmentVariableIsEmpty("GAOMON_SHORTCUT_SCREENSHOT")) {
                    QPixmap capture = dialog.grab();
                    QPainter painter(&capture);
                    painter.drawPixmap(dialog.mapFromGlobal(popup->pos()), popup->grab());
                    painter.end();
                    capture.save(qEnvironmentVariable("GAOMON_SHORTCUT_SCREENSHOT"));
                }
                popup->findChild<QPushButton *>()->click();
                QCOMPARE(tabs->currentIndex(), 1);
                QCOMPARE(dialog.findChild<QTableWidget *>()->currentRow(), 2);
                tabs->setCurrentIndex(0);
            }
            popup->hide();
        }
        dialog.setSoftVisible(false);
        for (int i = 12; i < 28; i++)
            QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("shortcutKey%1").arg(i))->isVisible());
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("shortcutKey28"))->isVisible());
        dialog.setSoftVisible(true);
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("shortcutKey12"))->isVisible());
        cfg.clear();
    }
    void actionMigrationAndPersistence() {
        QSettings cfg(QStringLiteral("GaomonManagePanel"), QStringLiteral("Shortcuts"));
        cfg.clear();
        cfg.setValue(QStringLiteral("shortcut/0"), QStringLiteral("Ctrl+Z"));
        cfg.setValue(QStringLiteral("shortcut/12"), QStringLiteral("M"));
        cfg.setValue(QStringLiteral("penAction/0"), 3);
        cfg.sync();
        Shortcuts dialog([] {});
        auto *table = dialog.findChild<QTableWidget *>();
        QVERIFY(table);
        auto *hard = qobject_cast<QComboBox *>(table->cellWidget(2, 1));
        QVERIFY(hard);
        QCOMPARE(hard->currentData().toInt(), 1);
        auto *soft = qobject_cast<QComboBox *>(table->cellWidget(14, 1));
        QVERIFY(soft);
        QCOMPARE(soft->currentData().toInt(), 1);
        auto *pen = qobject_cast<QComboBox *>(table->cellWidget(0, 1));
        QVERIFY(pen);
        QCOMPARE(pen->currentData().toInt(), 3);
        auto *other = qobject_cast<QComboBox *>(table->cellWidget(4, 1));
        QVERIFY(other);
        other->setCurrentIndex(other->findData(2));
        auto *output = qobject_cast<QStackedWidget *>(table->cellWidget(4, 2));
        QVERIFY(output);
        QCOMPARE(output->currentIndex(), 2);
        auto *button = qobject_cast<QToolButton *>(output->currentWidget());
        QVERIFY(button);
        button->menu()->actions().at(0)->setChecked(true);
        QPushButton *save = nullptr;
        for (auto *b : dialog.findChildren<QPushButton *>())
            if (b->text() == QStringLiteral("保存绑定"))
                save = b;
        QVERIFY(save);
        save->click();
        cfg.sync();
        QCOMPARE(cfg.value(QStringLiteral("action/0")).toInt(), 1);
        QCOMPARE(cfg.value(QStringLiteral("shortcut/0")).toString(), QStringLiteral("Ctrl+Z"));
        QCOMPARE(cfg.value(QStringLiteral("action/1")).toInt(), 2);
        QCOMPARE(cfg.value(QStringLiteral("mouseMods/1")).toInt(), 1);
        QCOMPARE(cfg.value(QStringLiteral("action/12")).toInt(), 1);
        QCOMPARE(cfg.value(QStringLiteral("action/28")).toInt(), 3);
        Shortcuts reopened([] {});
        auto *table2 = reopened.findChild<QTableWidget *>();
        QCOMPARE(qobject_cast<QComboBox *>(table2->cellWidget(4, 1))->currentData().toInt(), 2);
        auto *hardOutput = qobject_cast<QStackedWidget *>(table2->cellWidget(2, 2));
        QVERIFY(hardOutput);
        auto *hardAction = qobject_cast<QComboBox *>(table2->cellWidget(2, 1));
        QVERIFY(hardAction);
        hardAction->setCurrentIndex(hardAction->findData(0));
        QCOMPARE(qobject_cast<QLabel *>(hardOutput->currentWidget())->text(), QStringLiteral("A"));
        QPushButton *reset = nullptr;
        for (auto *b : reopened.findChildren<QPushButton *>())
            if (b->text() == QStringLiteral("重置所有按键"))
                reset = b;
        QVERIFY(reset);
        reset->click();
        cfg.sync();
        for (int i = 0; i < 30; i++) {
            QCOMPARE(cfg.value(QStringLiteral("action/%1").arg(i)).toInt(), 0);
            QVERIFY(cfg.value(QStringLiteral("shortcut/%1").arg(i)).toString().isEmpty());
            QCOMPARE(cfg.value(QStringLiteral("mouseMods/%1").arg(i)).toInt(), 0);
        }
        Shortcuts resetDialog([] {});
        auto *resetTable = resetDialog.findChild<QTableWidget *>();
        QCOMPARE(qobject_cast<QComboBox *>(resetTable->cellWidget(14, 1))->currentData().toInt(),
                 0);
        auto *softOutput = qobject_cast<QStackedWidget *>(resetTable->cellWidget(14, 2));
        QCOMPARE(qobject_cast<QLabel *>(softOutput->currentWidget())->text(),
                 QStringLiteral("无动作"));
    }
    static QRectF fullArea() {
        return {0, 0, 1, 1};
    }
    void tinySelection() {
        AreaPreview view(true);
        view.resize(400, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.setArea(QRectF(.1, .1, .0001, .0001));
        const QRectF canvas = view.canvasRect();
        QPoint corner =
            (canvas.topLeft() + QPointF(.1 * canvas.width(), .1 * canvas.height())).toPoint();
        QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, corner);
        QTest::mouseMove(&view, corner - QPoint(50, 50));
        QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, corner - QPoint(50, 50));
        QVERIFY(view.area().x() >= 0);
        QVERIFY(view.area().y() >= 0);
        QVERIFY(view.area().width() > 0);
        QVERIFY(view.area().height() > 0);
    }
};
QTEST_MAIN(PreviewTest)
#include "area-preview-test.moc"
