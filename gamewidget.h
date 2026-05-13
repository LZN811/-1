#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include <QWidget>
#include <QMap>
#include <QDate>
#include <QVector>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QSequentialAnimationGroup>

class QPushButton;
class QLabel;
class QLineEdit;
class QTextEdit;
class QGridLayout;
class QStackedWidget;
class QTimer;
class QScrollArea;

class GameWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameWidget(QWidget *parent = nullptr);
    ~GameWidget();

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onFabClicked();
    void onMoodBtnClicked();
    void onMoodSelected(int moodId);
    void onSaveClicked();
    void onMonthChanged(int offset);
    void onCancelClicked();
    void hideTip();
    void onTypingTick();
    void onEntryClicked();   // 点击首页已有日记

private:
    struct DiaryEntry {
        int     mood;
        QString text;
        QString moodName;
    };
    QMap<QDate, DiaryEntry> m_diaryData;

    QStackedWidget* m_stackedWidget;
    QWidget*        m_homePage;
    QWidget*        m_moodPage;
    QWidget*        m_recordPage;

    // 首页
    QLabel*      m_yearLabel;
    QLabel*      m_monthLabel;
    QWidget*     m_entriesContainer;
    QGridLayout* m_entriesLayout;
    QPushButton* m_fabBtn;

    // 选心情页
    QVector<QPushButton*> m_moodButtons;

    // 写日记页
    QLabel*    m_recTopDateLabel;
    QLabel*    m_recDayNum;
    QLabel*    m_recWeekday;
    QLabel*    m_recMoodImg;
    QLineEdit* m_recMoodNameEdit;
    QTextEdit* m_recTextEdit;
    QPushButton* m_saveBtn;
    QPushButton* m_cancelBtn;

    // Toast
    QLabel*  m_tipLabel;
    QTimer*  m_tipTimer;

    // 打字机聊天泡
    QLabel*  m_bubbleLabel;
    QTimer*  m_typingTimer;
    QString  m_typingFull;
    int      m_typingIdx;

    // 动画
    QGraphicsOpacityEffect* m_pageOpacity;   // 页面淡入
    QPropertyAnimation*     m_pageAnim;
    QPropertyAnimation*     m_fabPulseAnim;  // FAB 脉冲

    QString m_fontFamily;   // 运行时字体名

    QDate m_currentMonth;
    int   m_currentMood;

    void initUI();
    void setupHomePage();
    void setupMoodPage();
    void setupRecordPage();
    void updateEntries();
    void loadData();
    void saveData();
    void showTip(const QString& msg);
    void switchPage(int idx);        // 带淡入动画的页面切换

    QPixmap getMoodPixmap(int mood, int size = 40) const;
    QString getMoodColor(int mood) const;
    QString getMoodName(int mood) const;
};

#endif
