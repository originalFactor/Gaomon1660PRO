#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QIcon>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSignalBlocker>
#include <QTabletEvent>
#include <QTextStream>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QTimer>
#include "shortcuts.h"
#include "area-preview.h"
#include "pressure-dialog.h"
#include <algorithm>

static const QString service = QStringLiteral("org.kde.KWin");
static const QString iface = QStringLiteral("org.kde.KWin.InputDevice");
static const QString props = QStringLiteral("org.freedesktop.DBus.Properties");
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

class Window : public QWidget {
    QComboBox *devices = new QComboBox, *outputs = new QComboBox;
    QCheckBox *workspace = new QCheckBox(QStringLiteral("映射到整个桌面（所有显示器）")),
              *left = new QCheckBox(QStringLiteral("左手模式")),
              *reserveSoft = new QCheckBox(QStringLiteral("预留软快捷键区域"));
    QDoubleSpinBox *fields[8];
    AreaPreview *previews[2]{};
    QLabel *status = new QLabel;
    TestPad *test = new TestPad;
    QString path;
    Shortcuts *shortcutDialog = nullptr;
    bool appliedSoft = false;
    QVariant get(const QString &key, QString *error = nullptr) const {
        QDBusInterface d(service, path, props, QDBusConnection::sessionBus());
        QDBusReply<QDBusVariant> r = d.call(QStringLiteral("Get"), iface, key);
        if (!r.isValid()) {
            if (error)
                *error = r.error().message();
            return {};
        }
        return r.value().variant();
    }
    bool set(const QString &key, const QVariant &value, QString &error) {
        QDBusInterface d(service, path, props, QDBusConnection::sessionBus());
        auto r =
            d.call(QStringLiteral("Set"), iface, key, QVariant::fromValue(QDBusVariant(value)));
        if (r.type() == QDBusMessage::ErrorMessage) {
            error = key + QStringLiteral(": ") + r.errorMessage();
            return false;
        }
        return true;
    }
    static Area area(const QVariant &v) {
        if (v.canConvert<Area>())
            return v.value<Area>();
        return qdbus_cast<Area>(v.value<QDBusArgument>());
    }
    Area readArea(int base) const {
        return {fields[base]->value() / 100, fields[base + 1]->value() / 100,
                fields[base + 2]->value() / 100, fields[base + 3]->value() / 100};
    }
    static QRectF rectOf(const Area &a) {
        return {a.x, a.y, a.width, a.height};
    }
    static Area areaOf(const QRectF &r) {
        return {r.x(), r.y(), r.width(), r.height()};
    }
    void updatePreview(int base) {
        if (!previews[base / 4])
            return;
        Area a = readArea(base);
        const double x = std::clamp(a.x, 0.0, 0.9999), y = std::clamp(a.y, 0.0, 0.9999);
        QRectF display(x, y, std::clamp(a.width, 0.0001, 1.0 - x),
                       std::clamp(a.height, 0.0001, 1.0 - y));
        previews[base / 4]->setArea(base == 0 ? physicalInputArea(display, reserveSoft->isChecked())
                                              : display);
    }
    void showArea(int base, const Area &a) {
        const double v[] = {a.x, a.y, a.width, a.height};
        for (int i = 0; i < 4; i++) {
            QSignalBlocker block(fields[base + i]);
            fields[base + i]->setValue(v[i] * 100);
        }
        updatePreview(base);
    }
    void updateOutputPreview() {
        if (!previews[1])
            return;
        auto screens = QGuiApplication::screens();
        if (screens.isEmpty())
            return;
        QRect bounds;
        if (workspace->isChecked()) {
            for (auto *screen : screens)
                bounds = bounds.isNull() ? screen->geometry() : bounds.united(screen->geometry());
        } else {
            QScreen *selected = nullptr;
            for (auto *screen : screens)
                if (screen->name() == outputs->currentData().toString())
                    selected = screen;
            if (!selected)
                selected = QGuiApplication::primaryScreen();
            if (!selected)
                return;
            bounds = selected->geometry();
            screens = {selected};
        }
        if (bounds.width() <= 0 || bounds.height() <= 0)
            return;
        QList<QRectF> rectangles;
        for (auto *screen : screens) {
            const QRect r = screen->geometry();
            rectangles << QRectF(double(r.x() - bounds.x()) / bounds.width(),
                                 double(r.y() - bounds.y()) / bounds.height(),
                                 double(r.width()) / bounds.width(),
                                 double(r.height()) / bounds.height());
        }
        previews[1]->setReference(bounds.size(), rectangles);
    }
    QGroupBox *areaBox(const QString &title, int base) {
        auto *box = new QGroupBox(title);
        auto *layout = new QVBoxLayout(box);
        previews[base / 4] = new AreaPreview(base == 0);
        layout->addWidget(previews[base / 4], 1);
        auto *hint = new QLabel(QStringLiteral("拖动矩形移动 · 拖动边缘或角点调整大小"));
        hint->setWordWrap(true);
        layout->addWidget(hint);
        auto *grid = new QGridLayout;
        const QString labels[] = {QStringLiteral("左边距"), QStringLiteral("上边距"),
                                  QStringLiteral("宽度"), QStringLiteral("高度")};
        for (int i = 0; i < 4; i++) {
            fields[base + i] = new QDoubleSpinBox;
            fields[base + i]->setRange(i < 2 ? 0 : 0.01, 100);
            fields[base + i]->setDecimals(2);
            fields[base + i]->setSuffix(QStringLiteral(" %"));
            fields[base + i]->setSingleStep(1);
            grid->addWidget(new QLabel(labels[i]), i / 2, (i % 2) * 2);
            grid->addWidget(fields[base + i], i / 2, (i % 2) * 2 + 1);
            connect(fields[base + i], &QDoubleSpinBox::valueChanged, this,
                    [this, base] { updatePreview(base); });
        }
        layout->addLayout(grid);
        connect(previews[base / 4], &AreaPreview::areaChanged, this, [this, base](const QRectF &r) {
            showArea(base, areaOf(base == 0 ? displayInputArea(r, reserveSoft->isChecked()) : r));
        });
        auto *reset = new QPushButton(QStringLiteral("恢复整个区域"));
        layout->addWidget(reset);
        connect(reset, &QPushButton::clicked, this, [this, base] { showArea(base, Area{}); });
        return box;
    }
    void refreshDevices() {
        QString old = devices->currentData().toString();
        QSignalBlocker b(devices);
        devices->clear();
        QDBusInterface manager(service, QStringLiteral("/org/kde/KWin/InputDevice"), props,
                               QDBusConnection::sessionBus());
        QDBusReply<QDBusVariant> reply =
            manager.call(QStringLiteral("Get"), QStringLiteral("org.kde.KWin.InputDeviceManager"),
                         QStringLiteral("devicesSysNames"));
        if (!reply.isValid()) {
            path.clear();
            status->setText(QStringLiteral("无法连接 KWin：%1").arg(reply.error().message()));
            return;
        }
        const QStringList names = reply.value().variant().toStringList();
        for (const QString &name : names) {
            path = QStringLiteral("/org/kde/KWin/InputDevice/") + name;
            if (!get(QStringLiteral("tabletTool")).toBool() ||
                get(QStringLiteral("isVirtual")).toBool())
                continue;
            devices->addItem(get(QStringLiteral("name")).toString(), path);
        }
        int i = devices->findData(old);
        if (i >= 0)
            devices->setCurrentIndex(i);
        if (devices->count() == 0) {
            path.clear();
            status->setText(QStringLiteral("未找到物理数位笔。请连接数位板并点击刷新。"));
            return;
        }
        load();
    }
    void load() {
        path = devices->currentData().toString();
        if (path.isEmpty())
            return;
        QString err;
        QVariant name = get(QStringLiteral("outputName"), &err);
        if (!err.isEmpty()) {
            status->setText(QStringLiteral("读取失败：") + err);
            return;
        }
        {
            QSignalBlocker b(outputs);
            outputs->clear();
            outputs->addItem(QStringLiteral("自动选择"), QString());
            for (QScreen *s : QGuiApplication::screens())
                outputs->addItem(QStringLiteral("%1 (%2 × %3)")
                                     .arg(s->name())
                                     .arg(s->geometry().width())
                                     .arg(s->geometry().height()),
                                 s->name());
            int i = outputs->findData(name.toString());
            if (i < 0 && !name.toString().isEmpty()) {
                outputs->addItem(name.toString() + QStringLiteral("（未连接）"), name);
                i = outputs->count() - 1;
            }
            outputs->setCurrentIndex(std::max(0, i));
        }
        appliedSoft = QSettings(QStringLiteral("GaomonManagePanel"), QStringLiteral("Mapping"))
                          .value(QStringLiteral("reserveSoft"), false)
                          .toBool();
        {
            QSignalBlocker block(reserveSoft);
            reserveSoft->setChecked(appliedSoft);
        }
        previews[0]->setReserveSoft(appliedSoft);
        if (shortcutDialog) {
            shortcutDialog->setSoftVisible(appliedSoft);
            shortcutDialog->setAppliedSoft(appliedSoft);
        }
        workspace->setChecked(get(QStringLiteral("mapToWorkspace")).toBool());
        left->setChecked(get(QStringLiteral("leftHanded")).toBool());
        left->setEnabled(get(QStringLiteral("supportsLeftHanded")).toBool());
        showArea(0, areaOf(displayInputArea(rectOf(area(get(QStringLiteral("inputArea")))),
                                            appliedSoft)));
        showArea(4, area(get(QStringLiteral("outputArea"))));
        for (int i = 0; i < 4; i++)
            fields[i]->setEnabled(get(QStringLiteral("supportsInputArea")).toBool());
        for (int i = 4; i < 8; i++)
            fields[i]->setEnabled(get(QStringLiteral("supportsOutputArea")).toBool());
        previews[0]->setEnabled(fields[0]->isEnabled());
        previews[1]->setEnabled(fields[4]->isEnabled());
        updateOutputPreview();
        outputs->setEnabled(!workspace->isChecked());
        status->setText(QStringLiteral("已读取 KWin 设置。更改后点击“应用”。"));
    }
    bool syncVirtual() {
        const QString original = devices->currentData().toString();
        if (original.isEmpty())
            return false;
        path = original;
        const QStringList keys = {
            QStringLiteral("outputName"),       QStringLiteral("mapToWorkspace"),
            QStringLiteral("inputArea"),        QStringLiteral("outputArea"),
            QStringLiteral("leftHanded"),       QStringLiteral("pressureCurve"),
            QStringLiteral("pressureRangeMin"), QStringLiteral("pressureRangeMax")};
        QList<QVariant> values;
        for (const auto &key : keys)
            values << get(key);
        QDBusInterface manager(service, QStringLiteral("/org/kde/KWin/InputDevice"), props,
                               QDBusConnection::sessionBus());
        QDBusReply<QDBusVariant> r =
            manager.call(QStringLiteral("Get"), QStringLiteral("org.kde.KWin.InputDeviceManager"),
                         QStringLiteral("devicesSysNames"));
        if (!r.isValid()) {
            path = original;
            return false;
        }
        bool found = false;
        for (const auto &name : r.value().variant().toStringList()) {
            path = QStringLiteral("/org/kde/KWin/InputDevice/") + name;
            if (get(QStringLiteral("name")).toString() !=
                QStringLiteral("GaomonManagePanel Virtual Pen"))
                continue;
            found = true;
            QString err;
            for (int i = 0; i < keys.size(); i++) {
                QVariant value = values[i];
                if (!value.isValid() && i >= 5)
                    continue;
                if (i == 2 || i == 3)
                    value = QVariant::fromValue(area(value));
                if (!set(keys[i], value, err)) {
                    status->setText(QStringLiteral("虚拟笔映射失败：") + err);
                    found = false;
                    break;
                }
            }
            break;
        }
        path = original;
        return found;
    }
    void syncVirtualLater(int attempts = 15) {
        if (syncVirtual())
            return;
        if (attempts > 0)
            QTimer::singleShot(400, this, [this, attempts] { syncVirtualLater(attempts - 1); });
        else {
            if (shortcutDialog)
                shortcutDialog->stop();
            status->setText(QStringLiteral("未能配置虚拟笔映射，已停用快捷键；请检查连接。"));
        }
    }
    void openPressure() {
        path = devices->currentData().toString();
        if (path.isEmpty())
            return;
        QString err;
        const auto curve = get(QStringLiteral("pressureCurve"), &err);
        if (!err.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("无法读取压感设置"), err);
            return;
        }
        PressureSettings settings;
        settings.name = devices->currentText();
        settings.curve = curve.toString();
        settings.defaultCurve = get(QStringLiteral("defaultPressureCurve")).toString();
        settings.rangeSupported = get(QStringLiteral("supportsPressureRange")).toBool();
        if (settings.rangeSupported) {
            settings.minimum = get(QStringLiteral("pressureRangeMin")).toDouble();
            settings.maximum = get(QStringLiteral("pressureRangeMax")).toDouble();
            settings.defaultMinimum = get(QStringLiteral("defaultPressureRangeMin")).toDouble();
            settings.defaultMaximum = get(QStringLiteral("defaultPressureRangeMax")).toDouble();
        }
        PressureDialog dialog(
            settings,
            [this](const PressureSettings &next) {
                QString error;
                if (!set(QStringLiteral("pressureCurve"), next.curve, error))
                    return error;
                if (next.rangeSupported) {
                    const double currentMax = get(QStringLiteral("pressureRangeMax")).toDouble();
                    const QString first = next.minimum >= currentMax
                                              ? QStringLiteral("pressureRangeMax")
                                              : QStringLiteral("pressureRangeMin");
                    const QString second = first == QStringLiteral("pressureRangeMax")
                                               ? QStringLiteral("pressureRangeMin")
                                               : QStringLiteral("pressureRangeMax");
                    if (!set(first,
                             first.endsWith(QStringLiteral("Max")) ? next.maximum : next.minimum,
                             error))
                        return error;
                    if (!set(second,
                             second.endsWith(QStringLiteral("Max")) ? next.maximum : next.minimum,
                             error))
                        return error;
                }
                if (shortcutDialog && shortcutDialog->isRunning() && !syncVirtual())
                    return QStringLiteral("物理笔已更新，但虚拟笔同步失败。请重新启用快捷键。");
                return QString();
            },
            this);
        dialog.exec();
    }
    void apply() {
        if (path.isEmpty())
            return;
        Area in = readArea(0), out = readArea(4);
        auto valid = [](const Area &a) {
            return a.width > 0 && a.height > 0 && a.x + a.width <= 1.000001 &&
                   a.y + a.height <= 1.000001;
        };
        if (!valid(in) || !valid(out)) {
            QMessageBox::warning(this, QStringLiteral("区域无效"),
                                 QStringLiteral("边距加宽度或高度不能超过 100%。"));
            return;
        }
        QStringList errors;
        QString e;
        auto put = [&](const QString &k, const QVariant &v) {
            e.clear();
            if (!set(k, v, e))
                errors << e;
        };
        if (fields[0]->isEnabled())
            put(QStringLiteral("inputArea"), QVariant::fromValue(areaOf(physicalInputArea(
                                                 rectOf(in), reserveSoft->isChecked()))));
        if (fields[4]->isEnabled())
            put(QStringLiteral("outputArea"), QVariant::fromValue(out));
        if (left->isEnabled())
            put(QStringLiteral("leftHanded"), left->isChecked());
        put(QStringLiteral("outputName"), outputs->currentData().toString());
        put(QStringLiteral("mapToWorkspace"), workspace->isChecked());
        if (!errors.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("部分设置未能应用"), errors.join('\n'));
            load();
            return;
        }
        QSettings(QStringLiteral("GaomonManagePanel"), QStringLiteral("Mapping"))
            .setValue(QStringLiteral("reserveSoft"), reserveSoft->isChecked());
        load();
        syncVirtual();
        status->setText(QStringLiteral("已应用并重新读取 KWin 设置。"));
    }

public:
    int diagnoseVirtual() {
        return syncVirtual() ? 0 : 1;
    }
    int diagnose() const {
        QTextStream out(stdout);
        out << "platform=" << QGuiApplication::platformName() << "\n";
        out << "devices=" << devices->count() << "\n";
        out << "device=" << devices->currentText() << "\n";
        out << "output=" << outputs->currentData().toString() << "\n";
        out << "input=" << fields[0]->value() << "," << fields[1]->value() << ","
            << fields[2]->value() << "," << fields[3]->value() << "\n";
        out << "status=" << status->text() << "\n";
        return path.isEmpty() ? 1 : 0;
    }
    Window() {
        setWindowTitle(QStringLiteral("数位板管理面板"));
        resize(860, 800);
        auto *root = new QVBoxLayout(this);
        auto *top = new QHBoxLayout;
        top->addWidget(new QLabel(QStringLiteral("数位笔")));
        top->addWidget(devices, 1);
        auto *refresh = new QPushButton(QStringLiteral("刷新设备"));
        top->addWidget(refresh);
        root->addLayout(top);
        auto *screenBox = new QGroupBox(QStringLiteral("屏幕映射"));
        auto *form = new QFormLayout(screenBox);
        form->addRow(QStringLiteral("显示器"), outputs);
        form->addRow(workspace);
        form->addRow(left);
        root->addWidget(screenBox);
        auto *areas = new QHBoxLayout;
        auto *inputBox = areaBox(QStringLiteral("数位板有效区域"), 0);
        reserveSoft->setToolTip(
            QStringLiteral("勾选后，100% 表示软键下方的完整绘图区。更改后点击应用。"));
        static_cast<QVBoxLayout *>(inputBox->layout())->addWidget(reserveSoft);
        connect(reserveSoft, &QCheckBox::toggled, this, [this](bool enabled) {
            QRectF physical = physicalInputArea(rectOf(readArea(0)), !enabled);
            previews[0]->setReserveSoft(enabled);
            showArea(0, areaOf(displayInputArea(physical, enabled)));
            if (shortcutDialog)
                shortcutDialog->setSoftVisible(enabled);
            status->setText(QStringLiteral("预留软键区域已更改，点击“应用”后生效。"));
        });
        areas->addWidget(inputBox);
        areas->addWidget(areaBox(QStringLiteral("屏幕目标区域"), 4));
        root->addLayout(areas);
        auto *testBox = new QGroupBox(QStringLiteral("压感测试"));
        auto *tl = new QVBoxLayout(testBox);
        tl->addWidget(test);
        auto *clear = new QPushButton(QStringLiteral("清除"));
        tl->addWidget(clear, 0, Qt::AlignRight);
        root->addWidget(testBox, 1);
        status->setWordWrap(true);
        root->addWidget(status);
        auto *actions = new QHBoxLayout;
        auto *pressure = new QPushButton(QStringLiteral("压感曲线…"));
        actions->addWidget(pressure);
        auto *shortcuts = new QPushButton(QStringLiteral("笔与数位板按键…"));
        actions->addWidget(shortcuts);
        actions->addStretch();
        auto *reload = new QPushButton(QStringLiteral("放弃更改"));
        auto *applyButton = new QPushButton(QStringLiteral("应用"));
        actions->addWidget(reload);
        actions->addWidget(applyButton);
        root->addLayout(actions);
        connect(pressure, &QPushButton::clicked, this, [this] { openPressure(); });
        connect(shortcuts, &QPushButton::clicked, this, [this] {
            if (!shortcutDialog) {
                shortcutDialog = new Shortcuts([this] { syncVirtualLater(); }, this);
                shortcutDialog->setAppliedSoft(appliedSoft);
            }
            shortcutDialog->setSoftVisible(reserveSoft->isChecked());
            shortcutDialog->show();
            shortcutDialog->raise();
            shortcutDialog->activateWindow();
        });
        connect(refresh, &QPushButton::clicked, this, [this] { refreshDevices(); });
        connect(devices, &QComboBox::currentIndexChanged, this, [this] { load(); });
        connect(reload, &QPushButton::clicked, this, [this] { load(); });
        connect(applyButton, &QPushButton::clicked, this, [this] { apply(); });
        connect(clear, &QPushButton::clicked, test, [this] { test->clear(); });
        connect(workspace, &QCheckBox::toggled, outputs, &QWidget::setDisabled);
        connect(workspace, &QCheckBox::toggled, this, [this] { updateOutputPreview(); });
        connect(outputs, &QComboBox::currentIndexChanged, this, [this] { updateOutputPreview(); });
        auto watchScreen = [this](QScreen *screen) {
            connect(screen, &QScreen::geometryChanged, this, [this] { updateOutputPreview(); });
        };
        for (QScreen *screen : QGuiApplication::screens())
            watchScreen(screen);
        connect(qApp, &QGuiApplication::screenAdded, this, [this, watchScreen](QScreen *screen) {
            watchScreen(screen);
            updateOutputPreview();
        });
        connect(qApp, &QGuiApplication::screenRemoved, this, [this] { updateOutputPreview(); });
        refreshDevices();
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Gaomon Manage Panel"));
    app.setDesktopFileName(QStringLiteral("gaomon-manage-panel"));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/gaomon-manage-panel.png")));
    qDBusRegisterMetaType<Area>();
    Window window;
    if (app.arguments().contains(QStringLiteral("--diagnose")))
        return window.diagnose();
    if (app.arguments().contains(QStringLiteral("--sync-virtual")))
        return window.diagnoseVirtual();
    window.show();
    return app.exec();
}
