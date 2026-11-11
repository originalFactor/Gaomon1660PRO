#pragma once
#include "pressure-curve.h"
#include "pressure-test.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <functional>

struct PressureSettings {
    QString name, curve, defaultCurve;
    double minimum = 0, maximum = 1, defaultMinimum = 0, defaultMaximum = 1;
    bool rangeSupported = false;
};
class PressureDialog : public QDialog {
    PressureSettings initial;
    std::function<QString(const PressureSettings &)> commit;
    PressureCurve *graph = new PressureCurve;
    QDoubleSpinBox *minimum = new QDoubleSpinBox, *maximum = new QDoubleSpinBox;
    QLabel *status = new QLabel;
    QPushButton *applyButton = nullptr;
    bool curveValid = true, rangeEdited = false;
    void setPoints(const PressurePoints &points) {
        graph->setValue(points);
        curveValid = true;
        applyButton->setEnabled(true);
        status->setText(QStringLiteral("已更改曲线，点击“应用”后用笔测试。"));
    }
    void apply() {
        if (!curveValid)
            return;
        PressureSettings next = initial;
        next.curve = serializePressure(graph->value());
        if (rangeEdited) {
            next.minimum = minimum->value() / 100;
            next.maximum = maximum->value() / 100;
        }
        if (next.rangeSupported && next.minimum >= next.maximum) {
            status->setText(QStringLiteral("最小力度必须小于最大力度。"));
            return;
        }
        const QString error = commit(next);
        if (!error.isEmpty()) {
            status->setText(QStringLiteral("应用失败：") + error);
            return;
        }
        initial = next;
        rangeEdited = false;
        status->setText(QStringLiteral("压感设置已应用。请在右侧区域用笔测试。"));
    }

public:
    PressureDialog(const PressureSettings &settings,
                   std::function<QString(const PressureSettings &)> writer,
                   QWidget *parent = nullptr)
        : QDialog(parent), initial(settings), commit(std::move(writer)) {
        setWindowTitle(QStringLiteral("笔尖压感曲线"));
        resize(720, 440);
        auto *layout = new QVBoxLayout(this);
        auto *description =
            new QLabel(QStringLiteral("%1 · "
                                      "拖动两个圆点调整曲线，越靠左上越灵敏。图中横轴为笔尖力度，纵"
                                      "轴为应用收到的压力。改动在点击“应用”后生效。")
                           .arg(settings.name));
        description->setWordWrap(true);
        layout->addWidget(description);
        auto *content = new QHBoxLayout;
        auto *left = new QVBoxLayout;
        auto *right = new QVBoxLayout;
        left->addWidget(graph, 1);
        auto *presets = new QHBoxLayout;
        auto *soft = new QPushButton(QStringLiteral("柔和")),
             *normal = new QPushButton(QStringLiteral("线性")),
             *firm = new QPushButton(QStringLiteral("偏硬"));
        presets->addWidget(soft);
        presets->addWidget(normal);
        presets->addWidget(firm);
        left->addLayout(presets);
        auto *testBox = new QGroupBox(QStringLiteral("压感测试"));
        auto *testLayout = new QVBoxLayout(testBox);
        auto *test = new TestPad;
        test->setMinimumSize(250, 190);
        testLayout->addWidget(test, 1);
        auto *clear = new QPushButton(QStringLiteral("清除"));
        testLayout->addWidget(clear, 0, Qt::AlignRight);
        right->addWidget(testBox, 1);
        auto *rangeBox = new QGroupBox(QStringLiteral("有效力度范围"));
        auto *range = new QFormLayout(rangeBox);
        for (auto *spin : {minimum, maximum}) {
            spin->setRange(0, 100);
            spin->setDecimals(3);
            spin->setSuffix(QStringLiteral(" %"));
            spin->setSingleStep(1);
        }
        minimum->setValue(settings.minimum * 100);
        maximum->setValue(settings.maximum * 100);
        minimum->setEnabled(settings.rangeSupported);
        maximum->setEnabled(settings.rangeSupported);
        range->addRow(QStringLiteral("最低力度"), minimum);
        range->addRow(QStringLiteral("达到最大压力"), maximum);
        if (!settings.rangeSupported) {
            auto *note = new QLabel(QStringLiteral("当前设备暂不支持力度范围设置；曲线仍可使用。"));
            note->setWordWrap(true);
            range->addRow(note);
        }
        right->addWidget(rangeBox);
        content->addLayout(left, 1);
        content->addLayout(right, 1);
        layout->addLayout(content, 1);
        status->setWordWrap(true);
        layout->addWidget(status);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::RestoreDefaults |
                                             QDialogButtonBox::Apply | QDialogButtonBox::Close);
        applyButton = buttons->button(QDialogButtonBox::Apply);
        applyButton->setText(QStringLiteral("应用"));
        buttons->button(QDialogButtonBox::RestoreDefaults)->setText(QStringLiteral("恢复默认"));
        buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
        layout->addWidget(buttons);
        const auto parsed = parsePressure(settings.curve);
        if (parsed)
            graph->setValue(*parsed);
        else {
            curveValid = false;
            applyButton->setEnabled(false);
            status->setText(QStringLiteral("当前曲线格式无法识别；可调整控制点或恢复默认后应用。"));
        }
        connect(minimum, &QDoubleSpinBox::valueChanged, this, [this] { rangeEdited = true; });
        connect(maximum, &QDoubleSpinBox::valueChanged, this, [this] { rangeEdited = true; });
        connect(graph, &PressureCurve::pointsChanged, this, [this] {
            curveValid = true;
            applyButton->setEnabled(true);
            status->setText(QStringLiteral("已更改曲线，点击“应用”后用笔测试。"));
        });
        connect(soft, &QPushButton::clicked, this, [this] { setPoints({{0, 0.65}, {0.35, 1}}); });
        connect(normal, &QPushButton::clicked, this, [this] { setPoints({{0, 0}, {1, 1}}); });
        connect(firm, &QPushButton::clicked, this, [this] { setPoints({{0.65, 0}, {1, 0.35}}); });
        connect(clear, &QPushButton::clicked, test, &TestPad::clear);
        connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
                [this] {
                    setPoints(parsePressure(initial.defaultCurve).value_or(PressurePoints{}));
                    minimum->setValue(initial.defaultMinimum * 100);
                    maximum->setValue(initial.defaultMaximum * 100);
                    rangeEdited = true;
                });
        connect(applyButton, &QPushButton::clicked, this, [this] { apply(); });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
};
