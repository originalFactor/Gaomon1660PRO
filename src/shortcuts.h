#pragma once
#include "soft-area.h"
#include "startup-settings.h"
#include "shortcut-preview.h"
#include <QDialog>
#include <QComboBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QMenu>
#include <QProcess>
#include <QTableWidget>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QSet>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QTabWidget>
#include <QScreen>
#include <QShortcut>
#include <QCoreApplication>
#include <linux/input-event-codes.h>
#include <functional>

class Shortcuts : public QDialog {
    ShortcutPreview *preview = nullptr;
    QTabWidget *tabs = new QTabWidget;
    QFrame *bindingPopup = nullptr;
    QTableWidget *table = new QTableWidget(30, 3);
    QProcess *helper = new QProcess(this);
    QLabel *status = new QLabel;
    QPushButton *toggle = new QPushButton(QStringLiteral("启用快捷键"));
    QKeySequenceEdit *edits[30];
    QComboBox *actions[30];
    QStackedWidget *outputs[30];
    QToolButton *mouseMods[30];
    QAction *mouseModActions[30][4];
    const int source[12] = {KEY_A, KEY_KPPLUS,  KEY_RIGHTBRACE, KEY_SPACE, KEY_S, KEY_I,
                            KEY_C, KEY_KPMINUS, KEY_LEFTBRACE,  KEY_TAB,   KEY_E, KEY_Z};
    const int sourceMods[12] = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 5};
    std::function<void()> readyCallback;
    QByteArray pending;
    bool appliedSoft = false;
    bool enableRequested = false;
    // 板侧键按编号把左右两列排到相邻两行，其余按键保持原有顺序。
    static int tableRow(int i) {
        if (i < 12)
            return 2 + (i % 6) * 2 + i / 6;
        return i < 28 ? i + 2 : i - 28;
    }
    QString keyLabel(int i) const {
        return table->item(tableRow(i), 0)->text();
    }
    QString defaultBinding(int i) const {
        return qobject_cast<QLabel *>(outputs[i]->widget(0))->text();
    }
    QString configuredBinding(int i) const {
        const int mode = actions[i]->currentData().toInt();
        if (mode == 0)
            return QStringLiteral("默认 · ") + defaultBinding(i);
        if (mode == 1)
            return edits[i]->keySequence().isEmpty()
                       ? QStringLiteral("键盘快捷键（尚未设置）")
                       : edits[i]->keySequence().toString(QKeySequence::NativeText);
        if (mode == 5)
            return QStringLiteral("禁用（不发送）");
        return (mouseModMask(i) ? mouseMods[i]->text() + QStringLiteral(" + ") : QString())
               + actions[i]->currentText();
    }
    void showBinding(int i, QWidget *anchor) {
        if (bindingPopup)
            delete bindingPopup;
        bindingPopup = new QFrame(this, Qt::Popup);
        bindingPopup->setObjectName(QStringLiteral("bindingPopup"));
        bindingPopup->setFrameShape(QFrame::StyledPanel);
        auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), bindingPopup);
        connect(escape, &QShortcut::activated, bindingPopup, &QWidget::hide);
        auto *layout = new QVBoxLayout(bindingPopup);
        auto *title = new QLabel(keyLabel(i));
        auto font = title->font();
        font.setBold(true);
        title->setFont(font);
        layout->addWidget(title);
        auto *defaults = new QLabel(QStringLiteral("默认快捷键：%1").arg(defaultBinding(i)));
        defaults->setObjectName(QStringLiteral("defaultBinding"));
        layout->addWidget(defaults);
        auto *configured = new QLabel(QStringLiteral("当前配置：%1").arg(configuredBinding(i)));
        configured->setObjectName(QStringLiteral("configuredBinding"));
        layout->addWidget(configured);
        layout->addWidget(new QLabel(QStringLiteral("修改后点击“保存绑定”生效。")));
        auto *edit = new QPushButton(QStringLiteral("配置此按键…"));
        layout->addWidget(edit);
        connect(edit, &QPushButton::clicked, this, [this, i] {
            bindingPopup->hide();
            tabs->setCurrentWidget(table);
            const int row = tableRow(i);
            table->selectRow(row);
            table->scrollToItem(table->item(row, 0));
            actions[i]->setFocus();
        });
        bindingPopup->adjustSize();
        QPoint position = anchor->mapToGlobal(QPoint(anchor->width() + 6, 0));
        const QRect available = anchor->screen()->availableGeometry();
        position.setX(qMax(available.left(), qMin(position.x(), available.right() - bindingPopup->width() + 1)));
        position.setY(qMax(available.top(), qMin(position.y(), available.bottom() - bindingPopup->height() + 1)));
        bindingPopup->move(position);
        bindingPopup->show();
    }
    static int linuxKey(int key) {
        static const int letters[] = {KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
                                      KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
                                      KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z};
        static const int digits[] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
                                     KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};
        if (key >= Qt::Key_A && key <= Qt::Key_Z)
            return letters[key - Qt::Key_A];
        if (key >= Qt::Key_0 && key <= Qt::Key_9)
            return digits[key - Qt::Key_0];
        if (key >= Qt::Key_F1 && key <= Qt::Key_F10)
            return KEY_F1 + key - Qt::Key_F1;
        if (key == Qt::Key_F11)
            return KEY_F11;
        if (key == Qt::Key_F12)
            return KEY_F12;
        switch (key) {
        case Qt::Key_Space:
            return KEY_SPACE;
        case Qt::Key_Tab:
            return KEY_TAB;
        case Qt::Key_Backspace:
            return KEY_BACKSPACE;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            return KEY_ENTER;
        case Qt::Key_Escape:
            return KEY_ESC;
        case Qt::Key_Delete:
            return KEY_DELETE;
        case Qt::Key_Insert:
            return KEY_INSERT;
        case Qt::Key_Home:
            return KEY_HOME;
        case Qt::Key_End:
            return KEY_END;
        case Qt::Key_PageUp:
            return KEY_PAGEUP;
        case Qt::Key_PageDown:
            return KEY_PAGEDOWN;
        case Qt::Key_Left:
            return KEY_LEFT;
        case Qt::Key_Right:
            return KEY_RIGHT;
        case Qt::Key_Up:
            return KEY_UP;
        case Qt::Key_Down:
            return KEY_DOWN;
        case Qt::Key_Minus:
            return KEY_MINUS;
        case Qt::Key_Equal:
            return KEY_EQUAL;
        case Qt::Key_BracketLeft:
            return KEY_LEFTBRACE;
        case Qt::Key_BracketRight:
            return KEY_RIGHTBRACE;
        case Qt::Key_Semicolon:
            return KEY_SEMICOLON;
        case Qt::Key_Apostrophe:
            return KEY_APOSTROPHE;
        case Qt::Key_Comma:
            return KEY_COMMA;
        case Qt::Key_Period:
            return KEY_DOT;
        case Qt::Key_Slash:
            return KEY_SLASH;
        case Qt::Key_Backslash:
            return KEY_BACKSLASH;
        default:
            return 0;
        }
    }
    static int savedMode(const QSettings &cfg, int i) {
        const auto key = QStringLiteral("action/%1").arg(i);
        if (cfg.contains(key))
            return cfg.value(key).toInt();
        if (i >= 28)
            return cfg.value(QStringLiteral("penAction/%1").arg(i - 28), 0).toInt();
        return cfg.value(QStringLiteral("shortcut/%1").arg(i)).toString().isEmpty() ? 0 : 1;
    }
    int mouseModMask(int i) const {
        int mask = 0;
        for (int m = 0; m < 4; m++)
            if (mouseModActions[i][m]->isChecked())
                mask |= 1 << m;
        return mask;
    }
    void refreshAction(int i) {
        const int mode = actions[i]->currentData().toInt();
        outputs[i]->setCurrentIndex(mode == 1 ? 1
                                              : (mode >= 2 && mode <= 4 ? 2 : (mode == 5 ? 3 : 0)));
        QStringList names;
        const QString labels[] = {QStringLiteral("Ctrl"), QStringLiteral("Shift"),
                                  QStringLiteral("Alt"), QStringLiteral("Meta")};
        for (int m = 0; m < 4; m++)
            if (mouseModActions[i][m]->isChecked())
                names << labels[m];
        mouseMods[i]->setText(names.isEmpty() ? QStringLiteral("修饰键：无")
                                              : names.join(QStringLiteral(" + ")));
        if (preview)
            preview->setBindingDescription(i, keyLabel(i), configuredBinding(i));
    }
    bool saveAndSend(bool persist = true) {
        QSettings cfg(QStringLiteral("GaomonManagePanel"), QStringLiteral("Shortcuts"));
        QJsonArray hard, soft, pen;
        QStringList errors;
        int modes[30], masks[30];
        QKeySequence sequences[30];
        for (int i = 0; i < 30; i++) {
            modes[i] = persist ? actions[i]->currentData().toInt() : savedMode(cfg, i);
            masks[i] = persist ? mouseModMask(i)
                               : cfg.value(QStringLiteral("mouseMods/%1").arg(i), 0).toInt();
            sequences[i] = persist ? edits[i]->keySequence()
                                   : QKeySequence::fromString(
                                         cfg.value(QStringLiteral("shortcut/%1").arg(i)).toString(),
                                         QKeySequence::PortableText);
            if (modes[i] == 0)
                continue;
            QJsonArray mods;
            QJsonObject b;
            if (modes[i] == 1) {
                if (sequences[i].isEmpty() || !linuxKey(int(sequences[i][0].key()))) {
                    errors << table->item(tableRow(i), 0)->text();
                    continue;
                }
                const auto combo = sequences[i][0];
                const auto m = combo.keyboardModifiers();
                if (m & Qt::ControlModifier)
                    mods.append(KEY_LEFTCTRL);
                if (m & Qt::ShiftModifier)
                    mods.append(KEY_LEFTSHIFT);
                if (m & Qt::AltModifier)
                    mods.append(KEY_LEFTALT);
                if (m & Qt::MetaModifier)
                    mods.append(KEY_LEFTMETA);
                b.insert(QStringLiteral("kind"), QStringLiteral("shortcut"));
                b.insert(QStringLiteral("key"), linuxKey(int(combo.key())));
            } else if (modes[i] >= 2 && modes[i] <= 4) {
                const int keys[] = {KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_LEFTALT, KEY_LEFTMETA};
                for (int m = 0; m < 4; m++)
                    if (masks[i] & (1 << m))
                        mods.append(keys[m]);
                b.insert(QStringLiteral("kind"), modes[i] == 2   ? QStringLiteral("left")
                                                 : modes[i] == 3 ? QStringLiteral("right")
                                                                 : QStringLiteral("middle"));
            } else if (modes[i] == 5)
                b.insert(QStringLiteral("kind"), QStringLiteral("disabled"));
            else {
                errors << table->item(tableRow(i), 0)->text();
                continue;
            }
            b.insert(QStringLiteral("modifiers"), mods);
            if (i < 12) {
                b.insert(QStringLiteral("source"), source[i]);
                b.insert(QStringLiteral("sourceMods"), sourceMods[i]);
                hard.append(b);
            } else if (i < 28) {
                b.insert(QStringLiteral("source"), i - 12);
                soft.append(b);
            } else {
                b.insert(QStringLiteral("source"), i == 28 ? BTN_STYLUS : BTN_STYLUS2);
                pen.append(b);
            }
        }
        if (persist && !errors.isEmpty()) {
            status->setText(QStringLiteral("以下按键缺少快捷键或包含不支持的按键：%1")
                                .arg(errors.join(QStringLiteral("、"))));
            return false;
        }
        if (persist)
            for (int i = 0; i < 30; i++) {
                cfg.setValue(QStringLiteral("action/%1").arg(i), modes[i]);
                cfg.setValue(QStringLiteral("shortcut/%1").arg(i),
                             sequences[i].toString(QKeySequence::PortableText));
                cfg.setValue(QStringLiteral("mouseMods/%1").arg(i), masks[i]);
            }
        if (persist) {
            cfg.sync();
            if (cfg.status() != QSettings::NoError) {
                status->setText(QStringLiteral("无法保存按键设置，请检查配置文件权限。"));
                return false;
            }
        }
        QJsonObject config{{"hard", hard},
                           {"soft", soft},
                           {"pen", pen},
                           {"height", softAreaHeight},
                           {"softEnabled", appliedSoft}};
        if (helper->state() == QProcess::Running)
            helper->write(QJsonDocument(config).toJson(QJsonDocument::Compact) + '\n');
        if (!persist)
            return true;
        status->setText(helper->state() == QProcess::Running
                            ? (appliedSoft ? QStringLiteral("绑定已保存并发送；软快捷键已启用。 ")
                                           : QStringLiteral("绑定已保存并发送；软快捷键未启用。"))
                            : QStringLiteral("绑定已保存；启用后生效。"));
        return true;
    }
    void readOutput() {
        pending += helper->readAllStandardOutput();
        while (pending.contains('\n')) {
            int n = pending.indexOf('\n');
            QByteArray line = pending.left(n);
            pending.remove(0, n + 1);
            if (line == "ready" && enableRequested) {
                QSettings(QStringLiteral("GaomonManagePanel"), QStringLiteral("Shortcuts"))
                    .setValue(QStringLiteral("enabled"), true);
                toggle->setText(QStringLiteral("停用快捷键"));
                saveAndSend(false);
                readyCallback();
            }
        }
    }

public:
    explicit Shortcuts(std::function<void()> onReady, QWidget *parent = nullptr)
        : QDialog(parent), readyCallback(std::move(onReady)) {
        setWindowTitle(QStringLiteral("1060PRO 快捷键"));
        resize(960, 720);
        auto *layout = new QVBoxLayout(this);
        auto *info =
            new QLabel(QStringLiteral("点击预览中的按键，查看默认快捷键和当前配置。板侧键按左、右两列"
                                      "从上到下编号，配置表中同编号的左右键相邻排列；笔侧键 1 靠近笔尖。"
                                      "启用时需要授权访问本数位板。"));
        info->setWordWrap(true);
        layout->addWidget(info);
        table->setHorizontalHeaderLabels(
            {QStringLiteral("按键"), QStringLiteral("动作"), QStringLiteral("快捷键 / 修饰键")});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->verticalHeader()->hide();
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        QSettings cfg(QStringLiteral("GaomonManagePanel"), QStringLiteral("Shortcuts"));
        for (int i = 0; i < 30; i++) {
            const int row = tableRow(i);
            QString label = i < 12 ? QStringLiteral("%1列侧键 %2")
                                         .arg(i < 6 ? QStringLiteral("左") : QStringLiteral("右"))
                                         .arg(i % 6 + 1)
                            : i < 28 ? QStringLiteral("软键 %1").arg(i - 11)
                                     : (i == 28 ? QStringLiteral("笔侧键 1（靠近笔尖）")
                                                : QStringLiteral("笔侧键 2"));
            table->setItem(row, 0, new QTableWidgetItem(label));
            table->item(row, 0)->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            actions[i] = new QComboBox;
            actions[i]->addItem(i < 12   ? QStringLiteral("设备默认")
                                : i < 28 ? QStringLiteral("无动作")
                                         : QStringLiteral("默认笔按键"),
                                0);
            actions[i]->addItem(QStringLiteral("键盘快捷键"), 1);
            actions[i]->addItem(QStringLiteral("鼠标左键"), 2);
            actions[i]->addItem(QStringLiteral("鼠标右键"), 3);
            actions[i]->addItem(QStringLiteral("鼠标中键"), 4);
            actions[i]->addItem(QStringLiteral("禁用"), 5);
            actions[i]->setCurrentIndex(qMax(0, actions[i]->findData(savedMode(cfg, i))));
            table->setCellWidget(row, 1, actions[i]);
            outputs[i] = new QStackedWidget;
            static const QString defaults[] = {
                QStringLiteral("A"),        QStringLiteral("Ctrl + 小键盘 +"),
                QStringLiteral("]"),        QStringLiteral("空格"),
                QStringLiteral("Ctrl + S"), QStringLiteral("I"),
                QStringLiteral("C"),        QStringLiteral("Ctrl + 小键盘 −"),
                QStringLiteral("["),        QStringLiteral("Tab"),
                QStringLiteral("Ctrl + E"), QStringLiteral("Ctrl + Alt + Z")};
            const QString defaultOutput = i < 12    ? defaults[i]
                                          : i < 28  ? QStringLiteral("无动作")
                                          : i == 28 ? QStringLiteral("B + 笔侧键事件")
                                                    : QStringLiteral("E");
            auto *defaultLabel = new QLabel(defaultOutput);
            defaultLabel->setAlignment(Qt::AlignCenter);
            if (i >= 28)
                defaultLabel->setToolTip(
                    QStringLiteral("这是本设备实测的原始输出；笔侧键事件如何响应取决于当前应用。"));
            outputs[i]->addWidget(defaultLabel);
            edits[i] = new QKeySequenceEdit;
            edits[i]->setMaximumSequenceLength(1);
            edits[i]->setKeySequence(
                QKeySequence::fromString(cfg.value(QStringLiteral("shortcut/%1").arg(i)).toString(),
                                         QKeySequence::PortableText));
            connect(edits[i], &QKeySequenceEdit::keySequenceChanged, this,
                    [this, i] { if (preview) preview->setBindingDescription(i, keyLabel(i), configuredBinding(i)); });
            outputs[i]->addWidget(edits[i]);
            mouseMods[i] = new QToolButton;
            mouseMods[i]->setPopupMode(QToolButton::InstantPopup);
            auto *menu = new QMenu(mouseMods[i]);
            const QString names[] = {QStringLiteral("Ctrl"), QStringLiteral("Shift"),
                                     QStringLiteral("Alt"), QStringLiteral("Meta")};
            const int mask = cfg.value(QStringLiteral("mouseMods/%1").arg(i), 0).toInt();
            for (int m = 0; m < 4; m++) {
                mouseModActions[i][m] = menu->addAction(names[m]);
                mouseModActions[i][m]->setCheckable(true);
                mouseModActions[i][m]->setChecked(mask & (1 << m));
                connect(mouseModActions[i][m], &QAction::toggled, this,
                        [this, i] { refreshAction(i); });
            }
            mouseMods[i]->setMenu(menu);
            outputs[i]->addWidget(mouseMods[i]);
            auto *disabledLabel = new QLabel(QStringLiteral("不发送"));
            disabledLabel->setAlignment(Qt::AlignCenter);
            outputs[i]->addWidget(disabledLabel);
            table->setCellWidget(row, 2, outputs[i]);
            connect(actions[i], &QComboBox::currentIndexChanged, this,
                    [this, i] { refreshAction(i); });
            refreshAction(i);
        }
        preview = new ShortcutPreview([this](int i, QWidget *anchor) { showBinding(i, anchor); });
        tabs->addTab(preview, QStringLiteral("按键预览"));
        tabs->addTab(table, QStringLiteral("按键配置"));
        for (int i = 0; i < 30; i++)
            preview->setBindingDescription(i, keyLabel(i), configuredBinding(i));
        layout->addWidget(tabs);
        auto *buttons = new QHBoxLayout;
        auto *reset = new QPushButton(QStringLiteral("重置所有按键"));
        auto *save = new QPushButton(QStringLiteral("保存绑定"));
        buttons->addWidget(toggle);
        buttons->addWidget(reset);
        buttons->addWidget(save);
        layout->addLayout(buttons);
        status->setWordWrap(true);
        layout->addWidget(status);
        connect(toggle, &QPushButton::clicked, this, [this] {
            if (helper->state() != QProcess::NotRunning) {
                QSettings(QStringLiteral("GaomonManagePanel"), QStringLiteral("Shortcuts"))
                    .setValue(QStringLiteral("enabled"), false);
                stop();
                return;
            }
            start();
        });
        connect(save, &QPushButton::clicked, this, [this] { saveAndSend(); });
        connect(reset, &QPushButton::clicked, this, [this] {
            for (int i = 0; i < 30; i++) {
                actions[i]->setCurrentIndex(actions[i]->findData(0));
                edits[i]->clear();
                for (auto *item : mouseModActions[i])
                    item->setChecked(false);
                refreshAction(i);
            }
            if (saveAndSend())
                status->setText(helper->state() == QProcess::Running
                                    ? QStringLiteral("所有按键已重置并保存；按住的按键释放后生效。")
                                    : QStringLiteral("所有按键已重置并保存。"));
        });
        connect(helper, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
            status->setText(QStringLiteral("启动失败：") + helper->errorString());
            toggle->setText(QStringLiteral("启用快捷键"));
        });
        connect(helper, &QProcess::readyReadStandardOutput, this, [this] { readOutput(); });
        connect(helper, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
            toggle->setText(QStringLiteral("启用快捷键"));
            QString err = QString::fromLocal8Bit(helper->readAllStandardError()).trimmed();
            status->setText(code == 0 ? QStringLiteral("快捷键已停用。")
                                      : QStringLiteral("快捷键已停止：%1")
                                            .arg(err.isEmpty() ? QString::number(code) : err));
        });
        status->setText(QStringLiteral("设置在保存后保留；启用后，下次打开面板会自动启动快捷键。可在主面板设置登录自启动和免密授权。"));
    }
    void start() {
        if (helper->state() != QProcess::NotRunning)
            return;
        enableRequested = true;
        pending.clear();
        helper->start(QStringLiteral("pkexec"), {StartupSettings::helperPath()});
        status->setText(QStringLiteral("等待授权并启动…"));
    }
    void setSoftVisible(bool enabled) {
        if (bindingPopup)
            bindingPopup->hide();
        preview->setSoftVisible(enabled);
        for (int i = 14; i < 30; i++)
            table->setRowHidden(i, !enabled);
    }
    void setAppliedSoft(bool enabled) {
        appliedSoft = enabled;
        if (helper->state() == QProcess::Running)
            saveAndSend(false);
    }
    bool isRunning() const {
        return helper->state() == QProcess::Running;
    }
    void stop() {
        enableRequested = false;
        if (helper->state() != QProcess::NotRunning)
            helper->closeWriteChannel();
    }
    ~Shortcuts() {
        if (helper->state() != QProcess::NotRunning) {
            helper->closeWriteChannel();
            if (!helper->waitForFinished(1000)) {
                helper->kill();
                helper->waitForFinished(1000);
            }
        }
    }
};
