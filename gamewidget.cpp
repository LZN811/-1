#include "gamewidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QGridLayout>
#include <QStackedWidget>
#include <QScrollArea>
#include <QFrame>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QPixmap>
#include <QTextCursor>
#include <QDebug>
#include <QFont>
#include <QFontDatabase>
#include <QApplication>
#include <QPainter>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QSequentialAnimationGroup>
#include <QPainterPath>
#include <QPaintEvent>
#include <QGraphicsDropShadowEffect>
#include <cmath>

// ── 配色 ─────────────────────────────────────────────────────────────────────
static const QString kBg     = "#EDEAE4";
static const QString kBgCard = "#F5F2EC";
static const QString kDark   = "#2C2C2C";
static const QString kGray   = "#9B9B9B";
static const QString kAccent = "#D4A574";

// 9种心情 blob 颜色
static const char* kBlobColors[9] = {
    "#7BC47B",  // 1 开心  绿
    "#F5C842",  // 2 兴奋  黄
    "#E05A5A",  // 3 愤怒  红
    "#4A7AB5",  // 4 伤心  深蓝
    "#3E3E50",  // 5 难受  深灰
    "#A87CB8",  // 6 尴尬  紫
    "#5BBCCC",  // 7 平静  青
    "#E87840",  // 8 震惊  橙
    "#E87AA0",  // 9 感动  粉
};

static const char* kMonthNamesCN[12] = {
    "一月","二月","三月","四月","五月","六月",
    "七月","八月","九月","十月","十一月","十二月"
};

static const char* kWeekdaysCN[7] = {
    "周一","周二","周三","周四","周五","周六","周日"
};

// ── 工具函数 ─────────────────────────────────────────────────────────────────

// 去白底：亮度取反作为 alpha，白色→全透明，黑色→全不透明
// 让漫画线稿无缝浮于任意背景之上
static QPixmap makeFloating(const QPixmap& src)
{
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QRgb px = line[x];
            int r = qRed(px), g = qGreen(px), b = qBlue(px);
            // 感知亮度，用幂曲线强化深色线条（gamma<1 → 深色更实）
            int brightness = (r * 299 + g * 587 + b * 114) / 1000;
            double t = 1.0 - brightness / 255.0;          // 0=白, 1=黑
            int alpha = static_cast<int>(255.0 * t * t * (3.0 - 2.0 * t)); // smoothstep
            alpha = qMin(255, static_cast<int>(alpha * 1.6)); // 再整体提亮线条
            line[x] = qRgba(r, g, b, qMin(255, alpha));
        }
    }
    return QPixmap::fromImage(img);
}

static QPixmap makeCircular(const QPixmap& src, int size)
{
    // 先把图缩放到正方形（居中裁剪）
    QPixmap scaled = src.scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                 Qt::SmoothTransformation);
    int ox = (scaled.width()  - size) / 2;
    int oy = (scaled.height() - size) / 2;

    QPixmap res(size, size);
    res.fill(Qt::transparent);
    QPainter p(&res);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // 绘制图像
    p.drawPixmap(0, 0, scaled, ox, oy, size, size);

    // 用径向渐变软遮罩（DestinationIn）替代硬裁圆
    // 中心不透明，边缘渐渐淡出 → 无硬边，自然浮于背景
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    QRadialGradient grad(size * 0.5, size * 0.5, size * 0.5);
    grad.setColorAt(0.55, QColor(0, 0, 0, 255));   // 内圈完全不透明
    grad.setColorAt(1.00, QColor(0, 0, 0, 0));     // 外圆边完全透明
    p.fillRect(0, 0, size, size, grad);

    return res;
}

// ── 带横线笔记卡片 ────────────────────────────────────────────────────────────
class RuledCard : public QFrame {
public:
    explicit RuledCard(QWidget* parent = nullptr) : QFrame(parent) {}
protected:
    void paintEvent(QPaintEvent* e) override {
        QFrame::paintEvent(e);
        QPainter p(this);
        p.setPen(QPen(QColor(180, 160, 130, 50)));
        const int lineH = 28, startY = 58;
        for (int y = startY; y < height() - 14; y += lineH)
            p.drawLine(14, y, width() - 14, y);
    }
};

// ─────────────────────────────────────────────────────────────────────────────

GameWidget::GameWidget(QWidget *parent)
    : QWidget(parent),
      m_currentMonth(QDate::currentDate()),
      m_currentMood(-1)
{
    initUI();
    loadData();
    updateEntries();
}

GameWidget::~GameWidget() {}

// GameWidget 本身画全局背景（bg.png）
void GameWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    QPainter p(this);
    static QPixmap bgPix;
    if (bgPix.isNull()) {
        bgPix.load(":/images/bg.png");
        if (bgPix.isNull()) bgPix.load("images/bg.png");
    }
    if (!bgPix.isNull()) {
        QPixmap scaled = bgPix.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                      Qt::SmoothTransformation);
        int x = (scaled.width()  - width())  / 2;
        int y = (scaled.height() - height()) / 2;
        p.drawPixmap(0, 0, scaled, x, y, width(), height());
        // 半透明暖白遮罩，提升文字可读性
        p.fillRect(rect(), QColor(237, 234, 228, 160));
    } else {
        p.fillRect(rect(), QColor(kBg));
    }
}

// ── initUI ───────────────────────────────────────────────────────────────────
void GameWidget::initUI()
{
    setWindowTitle("心情日记");
    setFixedSize(550, 650);

    // 加载手写字体（先从 QRC，再从磁盘）
    int fontId = QFontDatabase::addApplicationFont(":/851LakeusNightWriting-Regular.ttf");
    if (fontId < 0)
        fontId = QFontDatabase::addApplicationFont("851LakeusNightWriting-Regular.ttf");
    m_fontFamily = "SimSun";
    if (fontId >= 0) {
        QStringList fams = QFontDatabase::applicationFontFamilies(fontId);
        if (!fams.isEmpty()) m_fontFamily = fams.first();
    }

    // 全局字体：qApp 级别
    QFont f(m_fontFamily, 12);
    qApp->setFont(f);
    // 只给标签/按钮/输入框设字体，不用 * 避免破坏 QTextEdit 原生渲染
    qApp->setStyleSheet(QString(
        "QLabel       { font-family: '%1'; }"
        "QPushButton  { font-family: '%1'; }"
        "QLineEdit     { font-family: '%1'; }"
        "QTextEdit     { font-family: '%1'; }"
    ).arg(m_fontFamily));

    setWindowTitle("心情日记");

    setAttribute(Qt::WA_StyledBackground, false);

    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->setAttribute(Qt::WA_TranslucentBackground);
    m_stackedWidget->setStyleSheet("QStackedWidget{background:transparent;}");

    setupHomePage();
    setupMoodPage();
    setupRecordPage();
    m_stackedWidget->addWidget(m_homePage);
    m_stackedWidget->addWidget(m_moodPage);
    m_stackedWidget->addWidget(m_recordPage);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_stackedWidget);
    m_stackedWidget->setCurrentIndex(0); // 初始页，不走动画

    // Toast
    m_tipLabel = new QLabel(this);
    m_tipLabel->setAlignment(Qt::AlignCenter);
    m_tipLabel->setStyleSheet(
        "QLabel{font-size:13px;color:#3a3a3a;"
        "background-color:rgba(255,255,255,0.82);"
        "border:1px solid rgba(0,0,0,0.08);"
        "border-radius:16px;padding:7px 20px;}");
    m_tipLabel->hide();
    m_tipTimer = new QTimer(this);
    connect(m_tipTimer, &QTimer::timeout, this, &GameWidget::hideTip);

    // 打字机 timer
    m_typingFull = "和我一起记录今天吧";
    m_typingIdx  = 0;
    m_typingTimer = new QTimer(this);
    m_typingTimer->setInterval(80);
    connect(m_typingTimer, &QTimer::timeout, this, &GameWidget::onTypingTick);

    // ── 页面淡入：用独立遮罩层，不碰 QStackedWidget 本身（避免 QTextEdit 绘制报错）
    m_pageOpacity = new QGraphicsOpacityEffect(this);
    m_pageOpacity->setOpacity(0.0);
    // 遮罩：切页时盖一层白→透明，营造淡入效果
    auto* overlay = new QWidget(this);
    overlay->setObjectName("pageOverlay");
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay->setStyleSheet("background:rgba(237,234,228,1);");
    overlay->setGeometry(0, 0, 550, 650);
    overlay->setGraphicsEffect(m_pageOpacity);
    overlay->hide();
    m_pageAnim = new QPropertyAnimation(m_pageOpacity, "opacity", this);
    m_pageAnim->setDuration(300);
    m_pageAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_pageAnim, &QPropertyAnimation::finished, overlay, &QWidget::hide);

    // ── FAB 脉冲动画（每 3s 轻跳一次）────────────────────────────────────────
    m_fabPulseAnim = nullptr;
}

// ── setupHomePage ─────────────────────────────────────────────────────────────
void GameWidget::setupHomePage()
{
    m_homePage = new QWidget();
    m_homePage->setAttribute(Qt::WA_TranslucentBackground);
    m_homePage->setStyleSheet("background:transparent;");

    QVBoxLayout* vbox = new QVBoxLayout(m_homePage);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // 顶部：年 + 月导航
    QWidget* hdr = new QWidget();
    hdr->setFixedHeight(88);
    hdr->setAttribute(Qt::WA_TranslucentBackground);
    hdr->setStyleSheet("background:transparent;");
    QVBoxLayout* hdrV = new QVBoxLayout(hdr);
    hdrV->setContentsMargins(24, 14, 24, 4);
    hdrV->setSpacing(4);

    m_yearLabel = new QLabel();
    m_yearLabel->setAlignment(Qt::AlignCenter);
    m_yearLabel->setStyleSheet(
        "QLabel{font-size:12px;color:"+kGray+";background:transparent;}");
    hdrV->addWidget(m_yearLabel);

    QHBoxLayout* mRow = new QHBoxLayout();
    const QString navSS =
        "QPushButton{font-size:22px;color:"+kGray+";background:transparent;border:none;}"
        "QPushButton:hover{color:"+kDark+";}";

    QPushButton* prevBtn = new QPushButton("‹");
    prevBtn->setFixedSize(28, 28);
    prevBtn->setStyleSheet(navSS);
    connect(prevBtn, &QPushButton::clicked, [this](){ onMonthChanged(-1); });

    m_monthLabel = new QLabel();
    m_monthLabel->setAlignment(Qt::AlignCenter);
    // 蓝色高亮背景，和 mooda 一模一样
    m_monthLabel->setStyleSheet(
        "QLabel{font-size:20px;font-weight:normal;color:"+kDark+";"
        "background:transparent;padding:0 8px;}");

    QPushButton* nextBtn = new QPushButton("›");
    nextBtn->setFixedSize(28, 28);
    nextBtn->setStyleSheet(navSS);
    connect(nextBtn, &QPushButton::clicked, [this](){ onMonthChanged(1); });

    mRow->addWidget(prevBtn); mRow->addStretch();
    mRow->addWidget(m_monthLabel); mRow->addStretch();
    mRow->addWidget(nextBtn);
    hdrV->addLayout(mRow);
    vbox->addWidget(hdr);

    // 记录气泡滚动区
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setAttribute(Qt::WA_TranslucentBackground);
    scroll->setStyleSheet(
        "QScrollArea{background:transparent;border:none;}"
        "QScrollBar:vertical{background:transparent;width:4px;border:none;}"
        "QScrollBar::handle:vertical{background:rgba(120,110,100,0.28);border-radius:2px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");
    scroll->viewport()->setAttribute(Qt::WA_TranslucentBackground);
    scroll->viewport()->setStyleSheet("background:transparent;");

    m_entriesContainer = new QWidget();
    m_entriesContainer->setAttribute(Qt::WA_TranslucentBackground);
    m_entriesContainer->setStyleSheet("background:transparent;");
    m_entriesLayout = new QGridLayout(m_entriesContainer);
    m_entriesLayout->setSpacing(8);
    m_entriesLayout->setContentsMargins(18, 10, 18, 10);
    for (int c = 0; c < 7; ++c)
        m_entriesLayout->setColumnStretch(c, 1);
    scroll->setWidget(m_entriesContainer);
    vbox->addWidget(scroll, 1);

    // 底部留出 FAB 空间
    QWidget* sp = new QWidget();
    sp->setFixedHeight(72);
    sp->setAttribute(Qt::WA_TranslucentBackground);
    sp->setStyleSheet("background:transparent;");
    vbox->addWidget(sp);

    // ── FAB：优先用赵颖头像图片，没有则用"+"按钮 ─────────────────────────
    m_fabBtn = new QPushButton(m_homePage);
    QPixmap avatarPix(":/images/avatar.png");
    if (avatarPix.isNull()) avatarPix.load("images/avatar.png");
    if (!avatarPix.isNull()) {
        m_fabBtn->setFixedSize(70, 70);
        m_fabBtn->setIcon(QIcon(makeCircular(makeFloating(avatarPix), 70)));
        m_fabBtn->setIconSize(QSize(70, 70));
        m_fabBtn->setStyleSheet(
            "QPushButton{border-radius:35px;background:transparent;border:none;}"
            "QPushButton:pressed{background-color:rgba(0,0,0,0.08);border-radius:35px;}");
        m_fabBtn->move((550 - 70) / 2, 650 - 78);
    } else {
        // fallback
        m_fabBtn->setText("+");
        m_fabBtn->setFixedSize(58, 58);
        m_fabBtn->setStyleSheet(
            "QPushButton{font-size:24px;font-weight:300;color:white;"
            "background-color:#3E3E4E;border-radius:29px;border:none;}"
            "QPushButton:hover{background-color:#4E4E60;}"
            "QPushButton:pressed{background-color:#2E2E3C;}");
        m_fabBtn->move((550 - 58) / 2, 650 - 70);
    }
    connect(m_fabBtn, &QPushButton::clicked, this, &GameWidget::onFabClicked);
    m_fabBtn->raise();

    // FAB 脉冲动画：每 3s 轻轻放大再缩回（用 geometry 实现）
    // 用 QTimer 驱动，避免依赖 Qt Widgets Animation 在透明窗口的限制
    QTimer* pulseTimer = new QTimer(this);
    pulseTimer->setInterval(3000);
    connect(pulseTimer, &QTimer::timeout, this, [this]() {
        QRect orig = m_fabBtn->geometry();
        int cx = orig.center().x(), cy = orig.center().y();
        // 放大 10%
        auto* grow = new QPropertyAnimation(m_fabBtn, "geometry", this);
        grow->setDuration(180);
        grow->setEasingCurve(QEasingCurve::OutCubic);
        int big = static_cast<int>(orig.width() * 1.12);
        grow->setStartValue(orig);
        grow->setEndValue(QRect(cx - big/2, cy - big/2, big, big));
        // 缩回
        auto* shrink = new QPropertyAnimation(m_fabBtn, "geometry", this);
        shrink->setDuration(180);
        shrink->setEasingCurve(QEasingCurve::InCubic);
        shrink->setStartValue(QRect(cx - big/2, cy - big/2, big, big));
        shrink->setEndValue(orig);
        auto* seq = new QSequentialAnimationGroup(this);
        seq->addAnimation(grow);
        seq->addAnimation(shrink);
        seq->start(QAbstractAnimation::DeleteWhenStopped);
    });
    pulseTimer->start();
}

// ── setupMoodPage ─────────────────────────────────────────────────────────────
// 9个表情围成一圈，赵颖头像在中央，和mooda"表情排在周围"一样
void GameWidget::setupMoodPage()
{
    m_moodPage = new QWidget();
    m_moodPage->setAttribute(Qt::WA_TranslucentBackground);
    m_moodPage->setStyleSheet("background:transparent;");

    QVBoxLayout* vbox = new QVBoxLayout(m_moodPage);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // 返回按钮
    QWidget* topBar = new QWidget();
    topBar->setFixedHeight(48);
    topBar->setAttribute(Qt::WA_TranslucentBackground);
    topBar->setStyleSheet("background:transparent;");
    QHBoxLayout* tbL = new QHBoxLayout(topBar);
    tbL->setContentsMargins(16, 10, 16, 10);
    QPushButton* back = new QPushButton("← 返回");
    back->setFixedSize(72, 28);
    back->setStyleSheet(
        "QPushButton{font-size:12px;color:"+kGray+";background:transparent;border:none;}"
        "QPushButton:hover{color:"+kDark+";}");
    connect(back, &QPushButton::clicked, this, &GameWidget::onCancelClicked);
    tbL->addWidget(back); tbL->addStretch();
    vbox->addWidget(topBar);

    vbox->addStretch(1);

    // ── 聊天泡头像 + 打字机文字 ──────────────────────────────────────────────
    {
        QHBoxLayout* chatRow = new QHBoxLayout();
        chatRow->setContentsMargins(24, 0, 24, 0);
        chatRow->setSpacing(10);

        // 头像（圆形，来自 avatar.png）
        QLabel* avt = new QLabel();
        int avtSz = 48;
        QPixmap avtPx;
        avtPx.load(":/images/avatar.png");
        if (avtPx.isNull()) avtPx.load("images/avatar.png");
        if (!avtPx.isNull())
            avt->setPixmap(makeCircular(makeFloating(avtPx), avtSz));
        avt->setFixedSize(avtSz, avtSz);
        avt->setAttribute(Qt::WA_TranslucentBackground);
        chatRow->addWidget(avt, 0, Qt::AlignBottom);

        // 聊天气泡
        m_bubbleLabel = new QLabel();
        m_bubbleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        m_bubbleLabel->setWordWrap(false);
        m_bubbleLabel->setStyleSheet(
            "QLabel{font-size:15px;color:#2c2c2c;"
            "background:rgba(220,216,208,0.72);"
            "border:1px solid rgba(0,0,0,0.06);"
            "border-radius:14px;"
            "padding:8px 14px;}");
        m_bubbleLabel->setMinimumWidth(80);
        m_bubbleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        chatRow->addWidget(m_bubbleLabel, 0, Qt::AlignVCenter);
        chatRow->addStretch();

        vbox->addLayout(chatRow);
    }

    vbox->addSpacing(14);

    // ── 圆环布局：9个表情围成一圈，中央是赵颖头像 ───────────────────────────
    // 圆环容器，固定大小，绝对定位子控件
    QWidget* ring = new QWidget();
    const int ringSize = 340;
    ring->setFixedSize(ringSize, ringSize);
    ring->setAttribute(Qt::WA_TranslucentBackground);
    ring->setStyleSheet("background:transparent;");

    const int cx = ringSize / 2;
    const int cy = ringSize / 2;
    const int radius = 128;   // 圆环半径
    const int btnSize = 66;   // 表情按钮大小（圆，无色环）

    // 9个赵颖表情图均匀排成一圈
    for (int i = 0; i < 9; ++i) {
        int moodId = i + 1;
        double angle = -M_PI / 2.0 + i * (2.0 * M_PI / 9.0);
        double cosA = std::cos(angle), sinA = std::sin(angle);

        int bx = cx + static_cast<int>(radius * cosA) - btnSize / 2;
        int by = cy + static_cast<int>(radius * sinA) - btnSize / 2;

        QPushButton* btn = new QPushButton(ring);
        btn->setFixedSize(btnSize, btnSize);

        QPixmap face = makeCircular(getMoodPixmap(moodId, btnSize), btnSize);
        btn->setIcon(QIcon(face));
        btn->setIconSize(QSize(btnSize, btnSize));
        btn->setStyleSheet(
            QString("QPushButton{border-radius:%1px;background:transparent;border:none;}"
                    "QPushButton:pressed{background-color:rgba(0,0,0,0.10);border-radius:%1px;}")
                .arg(btnSize / 2));
        btn->move(bx, by);

        // 存 moodId，用具名 slot onMoodBtnClicked() 取出 —— 彻底可靠
        btn->setProperty("moodId", moodId);
        connect(btn, &QPushButton::clicked, this, &GameWidget::onMoodBtnClicked);

        m_moodButtons.append(btn);
    }

    // 居中放置圆环
    QHBoxLayout* ringWrap = new QHBoxLayout();
    ringWrap->addStretch();
    ringWrap->addWidget(ring);
    ringWrap->addStretch();
    vbox->addLayout(ringWrap);

    vbox->addStretch(2);
}

// ── setupRecordPage ───────────────────────────────────────────────────────────
void GameWidget::setupRecordPage()
{
    m_recordPage = new QWidget();
    m_recordPage->setObjectName("recordPage");
    m_recordPage->setStyleSheet("#recordPage{background-color:"+kBg+";}");

    QVBoxLayout* vbox = new QVBoxLayout(m_recordPage);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // 顶栏：✕ | 年月 | ✓
    QWidget* topBar = new QWidget();
    topBar->setFixedHeight(50);
    topBar->setStyleSheet("background:transparent;");
    QHBoxLayout* tbL = new QHBoxLayout(topBar);
    tbL->setContentsMargins(16, 8, 16, 8);

    m_cancelBtn = new QPushButton("✕");
    m_cancelBtn->setFixedSize(34, 34);
    m_cancelBtn->setStyleSheet(
        "QPushButton{font-size:15px;color:"+kGray+";background:transparent;border:none;}"
        "QPushButton:hover{color:"+kDark+";}");
    connect(m_cancelBtn, &QPushButton::clicked, this, &GameWidget::onCancelClicked);

    m_recTopDateLabel = new QLabel();
    m_recTopDateLabel->setAlignment(Qt::AlignCenter);
    m_recTopDateLabel->setStyleSheet(
        "QLabel{font-size:14px;color:"+kGray+";background:transparent;}");

    m_saveBtn = new QPushButton("✓");
    m_saveBtn->setFixedSize(34, 34);
    m_saveBtn->setStyleSheet(
        "QPushButton{font-size:18px;font-weight:bold;color:"+kDark+";"
        "background:transparent;border:none;}"
        "QPushButton:hover{color:#555;}"
        "QPushButton:pressed{color:"+kAccent+";}");
    connect(m_saveBtn, &QPushButton::clicked, this, &GameWidget::onSaveClicked);

    tbL->addWidget(m_cancelBtn); tbL->addStretch();
    tbL->addWidget(m_recTopDateLabel); tbL->addStretch();
    tbL->addWidget(m_saveBtn);
    vbox->addWidget(topBar);

    // 笔记本卡片
    QHBoxLayout* cardWrap = new QHBoxLayout();
    cardWrap->setContentsMargins(20, 6, 20, 6);

    RuledCard* card = new RuledCard();
    card->setFixedHeight(390);
    // 不用 QGraphicsDropShadowEffect（会导致 QTextEdit 文字消失）
    // 改用 border + 轻微 margin 模拟卡片感
    card->setStyleSheet(
        "RuledCard{background-color:"+kBgCard+";"
        "border:1px solid rgba(160,140,110,0.35);"
        "border-bottom:2px solid rgba(160,140,110,0.25);"
        "border-right:2px solid rgba(160,140,110,0.25);"
        "border-radius:10px;}");

    QVBoxLayout* cardV = new QVBoxLayout(card);
    cardV->setContentsMargins(22, 16, 22, 16);
    cardV->setSpacing(6);

    // 日期行
    QHBoxLayout* dateRow = new QHBoxLayout();
    dateRow->setSpacing(0);
    m_recDayNum = new QLabel();
    m_recDayNum->setStyleSheet(
        "QLabel{font-size:30px;font-weight:bold;color:"+kDark+";"
        "text-decoration:underline;background:transparent;}");
    m_recWeekday = new QLabel();
    m_recWeekday->setStyleSheet(
        "QLabel{font-size:14px;color:"+kDark+";"
        "background:transparent;padding-left:6px;padding-top:12px;}");
    dateRow->addWidget(m_recDayNum);
    dateRow->addWidget(m_recWeekday);
    dateRow->addStretch();
    cardV->addLayout(dateRow);

    // 大心情图
    m_recMoodImg = new QLabel();
    m_recMoodImg->setAlignment(Qt::AlignCenter);
    m_recMoodImg->setFixedHeight(120);
    m_recMoodImg->setStyleSheet("background:transparent;");
    cardV->addWidget(m_recMoodImg);

    // 可编辑心情名称（蓝色高亮）
    m_recMoodNameEdit = new QLineEdit();
    m_recMoodNameEdit->setAlignment(Qt::AlignCenter);
    m_recMoodNameEdit->setMaximumWidth(130);
    m_recMoodNameEdit->setFixedHeight(32);
    m_recMoodNameEdit->setStyleSheet(
        "QLineEdit{font-size:15px;font-weight:bold;color:"+kDark+";"
        "background:transparent;"
        "border:none;padding:0 10px;}");
    cardV->addWidget(m_recMoodNameEdit, 0, Qt::AlignCenter);

    QFrame* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame{color:rgba(180,160,130,0.28);}");
    cardV->addWidget(sep);

    m_recTextEdit = new QTextEdit();
    m_recTextEdit->setPlaceholderText("写下今天的心情...");
    m_recTextEdit->setStyleSheet(
        "QTextEdit{"
        "font-size:14px;"
        "color:"+kDark+";"
        "background:transparent;"
        "border:none;"
        "selection-background-color:rgba(180,160,130,0.35);"
        "selection-color:"+kDark+";"
        "}");
    cardV->addWidget(m_recTextEdit, 1);

    cardWrap->addWidget(card);
    vbox->addLayout(cardWrap);
    vbox->addStretch();
}

// ── updateEntries ─────────────────────────────────────────────────────────────
void GameWidget::updateEntries()
{
    while (m_entriesLayout->count() > 0) {
        QLayoutItem* it = m_entriesLayout->takeAt(0);
        if (it->widget()) delete it->widget();
        delete it;
    }

    m_yearLabel->setText(m_currentMonth.toString("yyyy年"));
    m_monthLabel->setText(kMonthNamesCN[m_currentMonth.month() - 1]);

    QDate first(m_currentMonth.year(), m_currentMonth.month(), 1);
    int startCol    = first.dayOfWeek() - 1;
    int daysInMonth = m_currentMonth.daysInMonth();
    int row = 0, col = startCol;

    for (int i = 0; i < startCol; ++i) {
        auto* ph = new QWidget();
        ph->setFixedSize(70, 70);
        ph->setAttribute(Qt::WA_TranslucentBackground);
        ph->setStyleSheet("background:transparent;");
        m_entriesLayout->addWidget(ph, 0, i);
    }

    for (int day = 1; day <= daysInMonth; ++day) {
        QDate d(m_currentMonth.year(), m_currentMonth.month(), day);

        if (m_diaryData.contains(d)) {
            int id = m_diaryData[d].mood;
            // 用 QPushButton 让日记可点击查看/续写
            QPushButton* btn = new QPushButton();
            btn->setFixedSize(70, 70);
            btn->setIcon(QIcon(makeCircular(getMoodPixmap(id, 70), 70)));
            btn->setIconSize(QSize(70, 70));
            btn->setToolTip(getMoodName(id) + " · " + d.toString("M月d日"));
            btn->setStyleSheet(
                "QPushButton{border-radius:35px;background:transparent;border:none;}"
                "QPushButton:hover{background:rgba(0,0,0,0.06);border-radius:35px;}"
                "QPushButton:pressed{background:rgba(0,0,0,0.12);border-radius:35px;}");
            btn->setProperty("entryDate", d);
            connect(btn, &QPushButton::clicked, this, &GameWidget::onEntryClicked);
            m_entriesLayout->addWidget(btn, row, col, Qt::AlignCenter);
        }
        if (++col >= 7) { col = 0; ++row; }
    }
}

// ── 槽函数 ────────────────────────────────────────────────────────────────────
void GameWidget::onFabClicked()
{
    // 每次进入选心情页，重新播放打字机动画
    m_typingIdx = 0;
    m_bubbleLabel->setText("");
    m_typingTimer->start();
    switchPage(1);
}

// 圆环按钮点击 — 用 sender() 从 property 取 moodId，彻底可靠
void GameWidget::onMoodBtnClicked()
{
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    int id = btn->property("moodId").toInt();
    if (id < 1 || id > 9) return;
    m_typingTimer->stop();
    onMoodSelected(id);
}

// 打字机动画：每 tick 追加一个字符
void GameWidget::onTypingTick()
{
    if (m_typingIdx >= m_typingFull.length()) {
        m_typingTimer->stop();
        return;
    }
    m_bubbleLabel->setText(m_typingFull.left(++m_typingIdx));
}

void GameWidget::onMoodSelected(int moodId)
{
    m_currentMood = moodId;
    QDate today = QDate::currentDate();

    m_recTopDateLabel->setText(today.toString("yyyy年M月"));
    m_recDayNum->setText(QString::number(today.day()));
    m_recWeekday->setText(kWeekdaysCN[today.dayOfWeek() - 1]);

    // 记录页大图也纯圆形，无彩色背景
    m_recMoodImg->setPixmap(makeCircular(getMoodPixmap(moodId, 110), 110));

    // 始终用当前选中心情的名称（不用旧存档覆盖）
    m_recMoodNameEdit->setText(getMoodName(moodId));

    // 每次选新心情都清空，不把旧存档内容带进来
    m_recTextEdit->clear();

    switchPage(2);
    m_recTextEdit->setFocus();
}

void GameWidget::onSaveClicked()
{
    QDate today = QDate::currentDate();
    DiaryEntry e;
    e.mood     = m_currentMood;
    e.text     = m_recTextEdit->toPlainText();
    e.moodName = m_recMoodNameEdit->text().trimmed();
    if (e.moodName.isEmpty()) e.moodName = getMoodName(m_currentMood);
    m_diaryData[today] = e;
    saveData();
    updateEntries();
    showTip("已保存 ✦");
    switchPage(0);
    m_currentMood = -1;
}

void GameWidget::onMonthChanged(int offset)
{
    m_currentMonth = m_currentMonth.addMonths(offset);
    updateEntries();
}

void GameWidget::onCancelClicked()
{
    switchPage(0);
    m_currentMood = -1;
}

void GameWidget::onEntryClicked()
{
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QDate d = btn->property("entryDate").toDate();
    if (!d.isValid() || !m_diaryData.contains(d)) return;

    const DiaryEntry& e = m_diaryData[d];
    m_currentMood = e.mood;

    // 填入该天的日期、心情图、心情名、日记内容
    m_recTopDateLabel->setText(d.toString("yyyy年M月"));
    m_recDayNum->setText(QString::number(d.day()));
    m_recWeekday->setText(kWeekdaysCN[d.dayOfWeek() - 1]);
    m_recMoodImg->setPixmap(makeCircular(getMoodPixmap(e.mood, 110), 110));
    m_recMoodNameEdit->setText(e.moodName.isEmpty() ? getMoodName(e.mood) : e.moodName);
    m_recTextEdit->setText(e.text);   // 加载已有日记内容

    switchPage(2);
    m_recTextEdit->setFocus();
    // 光标移到末尾，方便续写
    QTextCursor cursor = m_recTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_recTextEdit->setTextCursor(cursor);
}

void GameWidget::showTip(const QString& msg)
{
    m_tipLabel->setText(msg);
    m_tipLabel->adjustSize();
    int w = qMax(220, m_tipLabel->sizeHint().width() + 48);
    m_tipLabel->setFixedWidth(w);
    m_tipLabel->move((width() - w) / 2, height() - 72);
    m_tipLabel->show();
    m_tipLabel->raise();
    m_tipTimer->start(2000);
}

void GameWidget::hideTip()
{
    m_tipLabel->hide();
    m_tipTimer->stop();
}

// 带淡入动画的页面切换（遮罩层从不透明→透明，不影响 QTextEdit）
void GameWidget::switchPage(int idx)
{
    m_stackedWidget->setCurrentIndex(idx);
    // 找到遮罩层并播放淡出动画
    QWidget* overlay = findChild<QWidget*>("pageOverlay");
    if (overlay) {
        if (m_pageAnim->state() == QAbstractAnimation::Running)
            m_pageAnim->stop();
        m_pageOpacity->setOpacity(1.0);
        overlay->raise();
        overlay->show();
        m_pageAnim->setStartValue(1.0);
        m_pageAnim->setEndValue(0.0);
        m_pageAnim->start();
    }
}

// ── 数据读写 ──────────────────────────────────────────────────────────────────
void GameWidget::loadData()
{
    QFile file("mooda_data.txt");
    if (!file.open(QIODevice::ReadOnly)) return;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QStringList p = in.readLine().split("|");
        if (p.size() >= 2) {
            QDate d = QDate::fromString(p[0], "yyyy-MM-dd");
            if (d.isValid()) {
                DiaryEntry e;
                e.mood     = p[1].toInt();
                e.text     = p.size() >= 3 ? p[2] : "";
                e.moodName = p.size() >= 4 ? p[3] : getMoodName(e.mood);
                m_diaryData[d] = e;
            }
        }
    }
    file.close();
}

void GameWidget::saveData()
{
    QFile file("mooda_data.txt");
    if (!file.open(QIODevice::WriteOnly)) return;
    QTextStream out(&file);
    for (auto it = m_diaryData.cbegin(); it != m_diaryData.cend(); ++it)
        out << it.key().toString("yyyy-MM-dd") << "|"
            << it.value().mood     << "|"
            << it.value().text     << "|"
            << it.value().moodName << "\n";
    file.close();
}

// ── 资源辅助 ──────────────────────────────────────────────────────────────────
QPixmap GameWidget::getMoodPixmap(int mood, int size) const
{
    QPixmap px(QString(":/images/mood%1.png").arg(mood));
    if (px.isNull()) px.load(QString("images/mood%1.png").arg(mood));
    if (px.isNull()) { px = QPixmap(size, size); px.fill(Qt::lightGray); return px; }
    // 去白底后再缩放，线稿无缝浮于背景
    return makeFloating(px).scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QString GameWidget::getMoodColor(int mood) const
{
    if (mood >= 1 && mood <= 9) return QString(kBlobColors[mood - 1]);
    return "#E0D8D0";
}

QString GameWidget::getMoodName(int mood) const
{
    // 顺时针从12点出发：1=狂喜 2=愉快 3=得意 4=平静 5=甜蜜
    //                   6=疲惫 7=崩溃 8=震惊 9=愤怒
    switch (mood) {
    case 1: return "雀跃";
    case 2: return "愉快";
    case 3: return "得意";
    case 4: return "平静";
    case 5: return "甜蜜";
    case 6: return "疲惫";
    case 7: return "崩溃";
    case 8: return "震惊";
    case 9: return "愤怒";
    default: return "平静";
    }
}
