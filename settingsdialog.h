#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QListWidget;
class QStackedWidget;
class QComboBox;
class QLabel;
class QMouseEvent;
QT_END_NAMESPACE

class CustomButton;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override = default;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void settingsApplied();

private slots:
    void apply();
    void accept() override;

private:
    void buildUi();
    void loadFromSettings();
    void saveToSettings() const;
    bool isInTitleBar(const QPoint &globalPos) const;

    QWidget *m_titleBar = nullptr;
    QLabel *m_titleLabel = nullptr;
    CustomButton *m_btnClose = nullptr;
    QPoint m_dragPosition;
    bool m_dragging = false;

    QListWidget *m_nav = nullptr;
    QStackedWidget *m_stack = nullptr;

    // Language
    QComboBox *m_comboAppLanguage = nullptr;
    QComboBox *m_comboSpeechLanguage = nullptr;
};

#endif // SETTINGSDIALOG_H
