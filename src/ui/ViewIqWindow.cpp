#include "ui/ViewIqWindow.h"

#include "ui/FlowLayout.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <limits>
#include <utility>

namespace {

// Nhịp vẽ giống lớp video trên bản đồ: gói về 400 lần/giây nhưng mắt người chỉ
// cần 25 hình, phần dư giữa hai nhịp vẽ bị gói sau đè lên.
constexpr int kFrameMs = 40;

// Bảng kiểu chung có luật "QWidget { color: ... }" áp cho mọi trạng thái, nên
// điều khiển bị khoá vẫn sáng như bấm được; phải thêm luật :disabled ở đây.
const char *const kDisabledStyle =
    "QRadioButton:disabled, QCheckBox:disabled, QLabel:disabled { color: #5d666f; }";

// Một cụm "Nhãn: [điều khiển]…" — đơn vị gấp dòng của hàng thứ hai.
QWidget *cluster(const QString &caption, const QList<QWidget *> &widgets, QWidget *parent)
{
    auto *w = new QWidget(parent);
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);
    lay->addWidget(new QLabel(caption, w));
    for (QWidget *c : widgets) {
        c->setParent(w);
        lay->addWidget(c);
    }
    return w;
}

QSpinBox *makeSpin(int lo, int hi, int value)
{
    auto *s = new QSpinBox;
    s->setRange(lo, hi);
    s->setValue(value);
    s->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    s->setFixedWidth(76);
    return s;
}

} // namespace

ViewIqWindow::ViewIqWindow(std::shared_ptr<RawIqStore> store, QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint
                          | Qt::WindowMaximizeButtonHint | Qt::WindowStaysOnTopHint)
    , m_store(std::move(store))
{
    setObjectName(QStringLiteral("ViewIqWindow"));
    setWindowTitle(QStringLiteral("ViewIQ - Vẽ cánh sóng"));
    setStyleSheet(QString::fromLatin1(kDisabledStyle));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);
    lay->addWidget(buildControls());
    lay->addWidget(buildPlots(), 1);

    m_timer = new QTimer(this);
    m_timer->setInterval(kFrameMs);
    connect(m_timer, &QTimer::timeout, this, &ViewIqWindow::refresh);

    setDataType(DataBeam);
    pushWindow();

    // Mặc định 800x600, thu nhỏ được còn một nửa theo đặc tả.
    resize(800, 600);
    setMinimumSize(400, 300);
}

QGroupBox *ViewIqWindow::buildControls()
{
    auto *g = new QGroupBox(QStringLiteral("Điều khiển"), this);
    auto *v = new QVBoxLayout(g);
    v->setContentsMargins(8, 6, 8, 8);
    v->setSpacing(6);

    // Dòng 1: Start/Stop, loại dữ liệu và phương vị của gói mới nhất.
    auto *row1 = new QHBoxLayout;
    row1->setSpacing(24);
    m_runBox = new QCheckBox(QStringLiteral("Start/Stop"), g);
    m_typeLabel = new QLabel(QStringLiteral("Loại dữ liệu: —"), g);
    m_azmLabel = new QLabel(QStringLiteral("Azm: —"), g);
    // Giữ bề rộng theo chữ dài nhất để nhãn "Azm" không nhảy qua lại mỗi khi
    // loại dữ liệu đổi.
    m_typeLabel->setMinimumWidth(fontMetrics().horizontalAdvance(QStringLiteral("Loại dữ liệu: CS F2")) + 4);
    m_azmLabel->setMinimumWidth(fontMetrics().horizontalAdvance(QStringLiteral("Azm: 359.99°")) + 4);
    row1->addWidget(m_runBox);
    row1->addWidget(m_typeLabel);
    row1->addWidget(m_azmLabel);
    row1->addStretch(1);
    v->addLayout(row1);

    // Dòng 2: gấp dòng khi cửa sổ hẹp.
    auto *rbViewIq = new QRadioButton(QStringLiteral("ViewIQ"));
    auto *rbBeam = new QRadioButton(QStringLiteral("Vẽ CS"));
    auto *rbDb = new QRadioButton(QStringLiteral("dB"));
    auto *rbAmp = new QRadioButton(QStringLiteral("Amp"));
    m_dataGroup = new QButtonGroup(this);
    m_dataGroup->addButton(rbViewIq, DataViewIq);
    m_dataGroup->addButton(rbBeam, DataBeam);
    rbBeam->setChecked(true);
    m_viewGroup = new QButtonGroup(this);
    m_viewGroup->addButton(rbDb, 0);
    m_viewGroup->addButton(rbAmp, 1);
    rbDb->setChecked(true);

    m_meanSpin = makeSpin(RawIq::kMeanMin, RawIq::kMeanMax, RawIq::kMeanDefault);
    m_startSpin = makeSpin(0, RawIq::beamStartMax(RawIq::kMeanDefault), RawIq::kStartDefault);

    auto *row2 = new QWidget(g);
    auto *flow = new FlowLayout(row2);
    m_dataCluster = cluster(QStringLiteral("DataType:"), {rbViewIq, rbBeam}, row2);
    m_viewCluster = cluster(QStringLiteral("ViewType:"), {rbDb, rbAmp}, row2);
    m_meanCluster = cluster(QStringLiteral("MeanWords:"), {m_meanSpin}, row2);
    flow->addWidget(m_dataCluster);
    flow->addWidget(m_viewCluster);
    flow->addWidget(m_meanCluster);
    flow->addWidget(cluster(QStringLiteral("StartWord:"), {m_startSpin}, row2));
    v->addWidget(row2);

    connect(m_runBox, &QCheckBox::toggled, this, &ViewIqWindow::setRunning);
    connect(m_dataGroup, &QButtonGroup::idClicked, this, &ViewIqWindow::setDataType);
    connect(m_viewGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_dB = (id == 0);
        // dB và Amp chênh nhau hàng trăm lần: giữ thang cũ thì một trong hai
        // chế độ bẹp dí xuống đáy trong vài giây đầu.
        m_beamRange.reset();
        redrawBeam();
    });
    connect(m_meanSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        applyStartRange();
        pushWindow();
    });
    connect(m_startSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ViewIqWindow::pushWindow);
    return g;
}

QGroupBox *ViewIqWindow::buildPlots()
{
    m_plotGroup = new QGroupBox(QStringLiteral("Vẽ CS"), this);
    auto *v = new QVBoxLayout(m_plotGroup);
    v->setContentsMargins(6, 6, 6, 6);

    // Hai panel ẩn/hiện theo DataType: panel 1 là ViewIQ, panel 2 là cặp đồ
    // thị cánh sóng chia đôi bằng thanh kéo.
    m_stack = new QStackedWidget(m_plotGroup);
    m_scope = new IqScopeView(m_stack);

    auto *split = new QSplitter(Qt::Horizontal, m_stack);
    split->setChildrenCollapsible(false);
    m_polar = new BeamPolarView(split);
    m_beamScope = new BeamScopeView(split);
    split->addWidget(m_polar);
    split->addWidget(m_beamScope);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);

    m_stack->insertWidget(DataViewIq, m_scope);
    m_stack->insertWidget(DataBeam, split);
    v->addWidget(m_stack);
    return m_plotGroup;
}

void ViewIqWindow::closeEvent(QCloseEvent *event)
{
    // Đóng cửa sổ là dừng hẳn: luồng nhận thôi tính, mở lại thì Start/Stop về
    // mặc định (bỏ chọn) như đặc tả.
    m_runBox->setChecked(false);
    QWidget::closeEvent(event);
}

void ViewIqWindow::setRunning(bool running)
{
    m_store->setRunning(running);
    // Lúc chạy thì DataType tự bám theo IQType của gói, nên khoá lại cho khỏi
    // tưởng chọn tay được.
    m_dataCluster->setEnabled(!running);
    if (running) {
        m_beamRange.reset();
        m_timer->start();
    } else {
        // Dừng là đứng hình để soi: đồ thị giữ nguyên, di chuột vẫn xem được.
        m_timer->stop();
    }
}

void ViewIqWindow::setDataType(int type)
{
    m_dataType = (type == DataViewIq) ? DataViewIq : DataBeam;
    if (QAbstractButton *b = m_dataGroup->button(m_dataType); b && !b->isChecked())
        b->setChecked(true);
    const bool beam = (m_dataType == DataBeam);
    m_plotGroup->setTitle(beam ? QStringLiteral("Vẽ CS") : QStringLiteral("ViewIQ"));
    m_stack->setCurrentIndex(m_dataType);
    m_viewCluster->setEnabled(beam);
    m_meanCluster->setEnabled(beam);
    applyStartRange();
}

void ViewIqWindow::applyStartRange()
{
    // QSpinBox tự kẹp giá trị đang có về dải mới và phát valueChanged, nên
    // luồng nhận cũng nhận ngay StartWord đã kẹp.
    m_startSpin->setMaximum(m_dataType == DataViewIq ? RawIq::kViewStartMax
                                                     : RawIq::beamStartMax(m_meanSpin->value()));
}

void ViewIqWindow::pushWindow()
{
    m_store->setWindow(m_startSpin->value(), m_meanSpin->value());
}

void ViewIqWindow::refresh()
{
    if (!m_store->update(&m_snap))
        return;

    const QString name = RawIq::typeName(m_snap.type);
    m_typeLabel->setText(QStringLiteral("Loại dữ liệu: %1")
                             .arg(name.isEmpty() ? QStringLiteral("—") : name));
    m_azmLabel->setText(name.isEmpty()
                            ? QStringLiteral("Azm: —")
                            : QStringLiteral("Azm: %1°").arg(RawIq::azimuthDeg(m_snap.azm4096), 0, 'f', 2));

    if (m_runBox->isChecked() && RawIq::isValidType(m_snap.type)) {
        const int want = RawIq::isBeamType(m_snap.type) ? DataBeam : DataViewIq;
        if (want != m_dataType)
            setDataType(want);
    }

    if (m_snap.viewSeq != m_drawnView) {
        m_drawnView = m_snap.viewSeq;
        if (m_snap.iq1.size() == size_t(RawIq::kViewPoints))
            m_scope->setTrace(m_snap.viewStart, m_snap.iq1, m_snap.iq2);
        else
            m_scope->clearTrace();
    }
    if (m_snap.beamSeq != m_drawnBeam) {
        m_drawnBeam = m_snap.beamSeq;
        if (m_snap.beamType != m_drawnBeamType) {
            m_drawnBeamType = m_snap.beamType;
            m_beamRange.reset();
        }
        redrawBeam();
    }
}

void ViewIqWindow::redrawBeam()
{
    // Kho giữ trung bình cộng thô; đổi sang dB ở đây nên bấm dB/Amp là đổi
    // ngay cả đồ thị đã tích luỹ, không phải chờ ăng ten quay thêm một vòng.
    const size_t n = m_snap.sum.size();
    m_beam.sum.resize(n);
    m_beam.sub.resize(n);
    double lo = std::numeric_limits<double>::infinity();
    double hi = -lo;
    for (size_t i = 0; i < n; ++i) {
        const double s = m_dB ? RawIq::toDb(m_snap.sum[i]) : m_snap.sum[i];
        const double d = m_dB ? RawIq::toDb(m_snap.sub[i]) : m_snap.sub[i];
        m_beam.sum[i] = s;
        m_beam.sub[i] = d;
        for (const double x : {s, d}) {
            if (!std::isnan(x)) {
                lo = qMin(lo, x);
                hi = qMax(hi, x);
            }
        }
    }
    m_beam.sweep = m_snap.beamLast;
    if (lo <= hi) {
        // Biên độ tuyến tính lấy 0 làm gốc; dB thì bám theo dữ liệu, nếu không
        // cánh sóng 40..90 dB bị nén vào nửa trên của đồ thị.
        if (!m_dB)
            lo = qMin(lo, 0.0);
        m_beamRange.feed(lo, hi, true);
    } else {
        m_beamRange.reset();
    }
    m_beam.valid = m_beamRange.valid();
    m_beam.lo = m_beamRange.lo();
    m_beam.hi = m_beamRange.hi();

    m_polar->setTrace(m_beam);
    m_beamScope->setTrace(m_beam);
}
