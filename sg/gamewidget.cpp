#include "gamewidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QGridLayout>
#include <QStackedWidget>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QDebug>

GameWidget::GameWidget(QWidget *parent)
    : QWidget(parent), m_currentMonth(QDate::currentDate()), m_currentMood(-1) {
    initUI();
    loadData();
    updateCalendar();
}

GameWidget::~GameWidget() {}

void GameWidget::initUI() {
    setWindowTitle("Mooda 心情日记");
    setFixedSize(550, 650);
    setStyleSheet("background-color: #1e1e2e;");

    m_stackedWidget = new QStackedWidget(this);

    setupCalendarPage();
    setupEditPage();

    m_stackedWidget->addWidget(m_calendarPage);
    m_stackedWidget->addWidget(m_editPage);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_stackedWidget);

    m_stackedWidget->setCurrentIndex(0);

    // 提示条
    m_tipLabel = new QLabel(this);
    m_tipLabel->setAlignment(Qt::AlignCenter);
    m_tipLabel->setStyleSheet("font-size: 13px; color: #a6adc8; margin: 8px;");
    m_tipLabel->hide();

    QVBoxLayout* tipLayout = new QVBoxLayout();
    tipLayout->addWidget(m_tipLabel);
    mainLayout->addLayout(tipLayout);

    m_tipTimer = new QTimer(this);
    connect(m_tipTimer, &QTimer::timeout, this, &GameWidget::hideTip);
}

void GameWidget::setupCalendarPage() {
    m_calendarPage = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_calendarPage);
    layout->setSpacing(12);

    // 标题
    QLabel* title = new QLabel("📔 Mooda");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 28px; font-weight: bold; color: #ffffff; margin: 15px;");
    layout->addWidget(title);

    // 月份切换栏
    QHBoxLayout* monthBar = new QHBoxLayout();

    QPushButton* prevBtn = new QPushButton("◀");
    prevBtn->setFixedSize(40, 32);
    prevBtn->setStyleSheet("background-color: #313244; color: #cdd6f4; font-size: 18px; border-radius: 8px;");
    connect(prevBtn, &QPushButton::clicked, [this]() { onMonthChanged(-1); });

    m_monthLabel = new QLabel();
    m_monthLabel->setAlignment(Qt::AlignCenter);
    m_monthLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #ffffff;");

    QPushButton* nextBtn = new QPushButton("▶");
    nextBtn->setFixedSize(40, 32);
    nextBtn->setStyleSheet("background-color: #313244; color: #cdd6f4; font-size: 18px; border-radius: 8px;");
    connect(nextBtn, &QPushButton::clicked, [this]() { onMonthChanged(1); });

    QPushButton* todayBtn = new QPushButton("今天");
    todayBtn->setFixedSize(60, 32);
    todayBtn->setStyleSheet("background-color: #89b4fa; color: #1e1e2e; font-size: 13px; border-radius: 8px;");
    connect(todayBtn, &QPushButton::clicked, [this]() {
        m_currentMonth = QDate::currentDate();
        updateCalendar();
    });

    monthBar->addWidget(prevBtn);
    monthBar->addStretch();
    monthBar->addWidget(m_monthLabel);
    monthBar->addStretch();
    monthBar->addWidget(nextBtn);
    monthBar->addWidget(todayBtn);
    layout->addLayout(monthBar);

    // 星期标题
    QHBoxLayout* weekLayout = new QHBoxLayout();
    QStringList weekDays = {"一", "二", "三", "四", "五", "六", "日"};
    for (const QString& day : weekDays) {
        QLabel* label = new QLabel(day);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("font-size: 15px; color: #a6adc8; padding: 5px;");
        weekLayout->addWidget(label);
    }
    layout->addLayout(weekLayout);

    // 日历网格
    m_calendarLayout = new QGridLayout();
    m_calendarLayout->setSpacing(8);
    layout->addLayout(m_calendarLayout);

    // 加号按钮（悬浮在右下角）
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_addButton = new QPushButton("+");
    m_addButton->setFixedSize(60, 60);
    m_addButton->setStyleSheet(
        "font-size: 32px;"
        "background-color: #89b4fa;"
        "color: #1e1e2e;"
        "border-radius: 30px;"
        "border: none;"
        "font-weight: bold;"
        );
    connect(m_addButton, &QPushButton::clicked, this, &GameWidget::onAddClicked);
    bottomLayout->addWidget(m_addButton);
    layout->addLayout(bottomLayout);
}

void GameWidget::setupEditPage() {
    m_editPage = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_editPage);
    layout->setSpacing(20);
    layout->setContentsMargins(20, 30, 20, 30);

    // 标题
    QLabel* title = new QLabel("今天心情怎么样？");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #ffffff;");
    layout->addWidget(title);

    // 表情转盘 3x3
    QGridLayout* moodGrid = new QGridLayout();
    moodGrid->setSpacing(15);
    moodGrid->setHorizontalSpacing(20);

    QVector<QPair<int, QString>> moods = {
        {1, "😊 开心"}, {2, "😌 平静"}, {3, "😐 一般"},
        {4, "😢 难过"}, {5, "😠 愤怒"}, {6, "🥰 甜蜜"},
        {7, "😎 酷"},   {8, "🥱 困倦"}, {9, "🤯 炸裂"}
    };

    for (int i = 0; i < moods.size(); i++) {
        int moodId = moods[i].first;
        QString text = moods[i].second;

        QPushButton* btn = new QPushButton(text);
        btn->setFixedSize(110, 70);
        btn->setStyleSheet(
            "font-size: 18px;"
            "background-color: #313244;"
            "color: #cdd6f4;"
            "border-radius: 15px;"
            );
        connect(btn, &QPushButton::clicked, [this, moodId]() { onMoodSelected(moodId); });
        moodGrid->addWidget(btn, i / 3, i % 3);
        m_moodButtons.append(btn);
    }
    layout->addLayout(moodGrid);

    // 当前心情提示
    m_moodHintLabel = new QLabel("未选择心情");
    m_moodHintLabel->setAlignment(Qt::AlignCenter);
    m_moodHintLabel->setStyleSheet("font-size: 14px; color: #a6adc8; margin: 5px;");
    layout->addWidget(m_moodHintLabel);

    // 日记输入框
    m_textEdit = new QTextEdit();
    m_textEdit->setPlaceholderText("写下今天的心情...（可不写）");
    m_textEdit->setMaximumHeight(120);
    m_textEdit->setStyleSheet(
        "font-size: 16px;"
        "background-color: #313244;"
        "color: #cdd6f4;"
        "border-radius: 10px;"
        "padding: 10px;"
        "border: none;"
        );
    layout->addWidget(m_textEdit);

    // 按钮行
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(20);

    m_cancelBtn = new QPushButton("取消");
    m_saveBtn = new QPushButton("完成");

    m_cancelBtn->setFixedHeight(45);
    m_saveBtn->setFixedHeight(45);
    m_cancelBtn->setStyleSheet(
        "font-size: 16px;"
        "background-color: #45475a;"
        "color: #cdd6f4;"
        "border-radius: 12px;"
        "padding: 8px;"
        );
    m_saveBtn->setStyleSheet(
        "font-size: 16px;"
        "background-color: #89b4fa;"
        "color: #1e1e2e;"
        "border-radius: 12px;"
        "padding: 8px;"
        "font-weight: bold;"
        );

    connect(m_cancelBtn, &QPushButton::clicked, this, &GameWidget::onCancelClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &GameWidget::onSaveClicked);

    btnLayout->addStretch();
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    layout->addStretch();
}

void GameWidget::updateCalendar() {
    // 清空调色板
    while (m_calendarLayout->count() > 0) {
        QLayoutItem* item = m_calendarLayout->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }

    m_monthLabel->setText(m_currentMonth.toString("yyyy年 MM月"));

    QDate firstDay(m_currentMonth.year(), m_currentMonth.month(), 1);
    int startWeekday = firstDay.dayOfWeek() - 1;
    if (startWeekday < 0) startWeekday = 6;

    int daysInMonth = m_currentMonth.daysInMonth();

    int row = 0, col = 0;

    // 空白格子
    for (int i = 0; i < startWeekday; i++) {
        QLabel* empty = new QLabel("");
        empty->setFixedSize(68, 68);
        empty->setStyleSheet("background-color: transparent;");
        m_calendarLayout->addWidget(empty, row, col);
        col++;
        if (col >= 7) { col = 0; row++; }
    }

    // 日期格子
    for (int day = 1; day <= daysInMonth; day++) {
        QDate currentDate(m_currentMonth.year(), m_currentMonth.month(), day);

        QPushButton* dayBtn = new QPushButton();
        dayBtn->setFixedSize(68, 68);

        QString moodEmoji = "";
        int mood = -1;
        if (m_diaryData.contains(currentDate)) {
            mood = m_diaryData[currentDate].mood;
            moodEmoji = getMoodEmoji(mood);
        }

        if (!moodEmoji.isEmpty()) {
            dayBtn->setText(QString::number(day) + "\n" + moodEmoji);
        } else {
            dayBtn->setText(QString::number(day));
        }

        QString bgColor = "#313244";
        if (currentDate == QDate::currentDate()) {
            bgColor = "#89b4fa";
        } else if (mood != -1) {
            bgColor = getMoodColor(mood);
        }

        dayBtn->setStyleSheet(QString(
                                  "font-size: 14px;"
                                  "background-color: %1;"
                                  "color: #ffffff;"
                                  "border-radius: 12px;"
                                  "padding: 5px;"
                                  ).arg(bgColor));

        dayBtn->setEnabled(false);
        m_calendarLayout->addWidget(dayBtn, row, col);

        col++;
        if (col >= 7) { col = 0; row++; }
    }
}

void GameWidget::onMoodSelected(int moodId) {
    m_currentMood = moodId;

    for (int i = 0; i < m_moodButtons.size(); i++) {
        QString style = (i + 1 == moodId) ?
                            "font-size: 18px; background-color: #89b4fa; color: #1e1e2e; border-radius: 15px;" :
                            "font-size: 18px; background-color: #313244; color: #cdd6f4; border-radius: 15px;";
        m_moodButtons[i]->setStyleSheet(style);
    }

    m_moodHintLabel->setText(QString("当前心情：%1 %2")
                                 .arg(getMoodName(m_currentMood))
                                 .arg(getMoodEmoji(m_currentMood)));
}

void GameWidget::onSaveClicked() {
    if (m_currentMood == -1) {
        showTip("请先选择一个心情～");
        return;
    }

    QDate today = QDate::currentDate();
    DiaryEntry entry;
    entry.mood = m_currentMood;
    entry.text = m_textEdit->toPlainText();

    m_diaryData[today] = entry;
    saveData();
    updateCalendar();
    showTip("✅ 已保存今天的心情");

    m_stackedWidget->setCurrentIndex(0);
}

void GameWidget::onMonthChanged(int offset) {
    m_currentMonth = m_currentMonth.addMonths(offset);
    updateCalendar();
}

void GameWidget::onAddClicked() {
    // 重置编辑页
    m_currentMood = -1;
    m_textEdit->clear();
    for (auto btn : m_moodButtons) {
        btn->setStyleSheet("font-size: 18px; background-color: #313244; color: #cdd6f4; border-radius: 15px;");
    }
    m_moodHintLabel->setText("未选择心情");

    // 加载今天的已有记录
    QDate today = QDate::currentDate();
    if (m_diaryData.contains(today)) {
        m_currentMood = m_diaryData[today].mood;
        m_textEdit->setText(m_diaryData[today].text);
        if (m_currentMood >= 1 && m_currentMood <= m_moodButtons.size()) {
            m_moodButtons[m_currentMood - 1]->setStyleSheet(
                "font-size: 18px; background-color: #89b4fa; color: #1e1e2e; border-radius: 15px;");
            m_moodHintLabel->setText(QString("当前心情：%1 %2")
                                         .arg(getMoodName(m_currentMood))
                                         .arg(getMoodEmoji(m_currentMood)));
        }
    }

    m_stackedWidget->setCurrentIndex(1);
}

void GameWidget::onCancelClicked() {
    m_stackedWidget->setCurrentIndex(0);
}

void GameWidget::showTip(const QString& msg) {
    m_tipLabel->setText(msg);
    m_tipLabel->show();
    m_tipTimer->start(2000);
}

void GameWidget::hideTip() {
    m_tipLabel->hide();
    m_tipTimer->stop();
}

void GameWidget::loadData() {
    QFile file("mooda_data.txt");
    if (!file.open(QIODevice::ReadOnly)) return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList parts = line.split("|");
        if (parts.size() >= 2) {
            QDate date = QDate::fromString(parts[0], "yyyy-MM-dd");
            if (date.isValid()) {
                DiaryEntry entry;
                entry.mood = parts[1].toInt();
                entry.text = (parts.size() >= 3) ? parts[2] : "";
                m_diaryData[date] = entry;
            }
        }
    }
    file.close();
}

void GameWidget::saveData() {
    QFile file("mooda_data.txt");
    if (!file.open(QIODevice::WriteOnly)) return;

    QTextStream out(&file);
    for (auto it = m_diaryData.begin(); it != m_diaryData.end(); ++it) {
        out << it.key().toString("yyyy-MM-dd") << "|"
            << it.value().mood << "|"
            << it.value().text << "\n";
    }
    file.close();
}

QString GameWidget::getMoodEmoji(int mood) const {
    switch(mood) {
    case 1: return "😊";
    case 2: return "😌";
    case 3: return "😐";
    case 4: return "😢";
    case 5: return "😠";
    case 6: return "🥰";
    case 7: return "😎";
    case 8: return "🥱";
    case 9: return "🤯";
    default: return "😐";
    }
}

QString GameWidget::getMoodColor(int mood) const {
    switch(mood) {
    case 1: return "#a6e3a1";
    case 2: return "#89b4fa";
    case 3: return "#a6adc8";
    case 4: return "#f9e2af";
    case 5: return "#f38ba8";
    case 6: return "#f5c2e7";
    case 7: return "#94e2d5";
    case 8: return "#cba6f7";
    case 9: return "#fab387";
    default: return "#313244";
    }
}

QString GameWidget::getMoodName(int mood) const {
    switch(mood) {
    case 1: return "开心";
    case 2: return "平静";
    case 3: return "一般";
    case 4: return "难过";
    case 5: return "愤怒";
    case 6: return "甜蜜";
    case 7: return "酷";
    case 8: return "困倦";
    case 9: return "炸裂";
    default: return "一般";
    }
}