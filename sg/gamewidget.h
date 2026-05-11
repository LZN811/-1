#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include <QWidget>
#include <QMap>
#include <QDate>
#include <QVector>

class QPushButton;
class QLabel;
class QTextEdit;
class QGridLayout;
class QStackedWidget;
class QTimer;

class GameWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameWidget(QWidget *parent = nullptr);
    ~GameWidget();

private slots:
    void onMoodSelected(int moodId);
    void onSaveClicked();
    void onMonthChanged(int offset);
    void onAddClicked();
    void onCancelClicked();
    void hideTip();

private:
    // 数据结构
    struct DiaryEntry {
        int mood;
        QString text;
    };
    QMap<QDate, DiaryEntry> m_diaryData;

    // UI组件 - 主页（日历页）
    QStackedWidget* m_stackedWidget;
    QWidget* m_calendarPage;
    QWidget* m_editPage;

    QLabel* m_monthLabel;
    QGridLayout* m_calendarLayout;
    QPushButton* m_addButton;

    // UI组件 - 编辑页
    QVector<QPushButton*> m_moodButtons;
    QLabel* m_moodHintLabel;
    QTextEdit* m_textEdit;
    QPushButton* m_saveBtn;
    QPushButton* m_cancelBtn;
    QLabel* m_tipLabel;
    QTimer* m_tipTimer;

    QDate m_currentMonth;
    int m_currentMood;  // 1-9

    // 函数
    void initUI();
    void setupCalendarPage();
    void setupEditPage();
    void loadData();
    void saveData();
    void updateCalendar();
    void showTip(const QString& msg);

    // 辅助函数
    QString getMoodEmoji(int mood) const;
    QString getMoodColor(int mood) const;
    QString getMoodName(int mood) const;
};

#endif