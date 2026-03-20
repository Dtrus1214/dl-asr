#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

QT_BEGIN_NAMESPACE
class QAudioInput;
class QIODevice;
class QTimer;
class QWidget;
class QLabel;
class QPlainTextEdit;
class QVBoxLayout;
class QHBoxLayout;
class QProgressBar;
class QMouseEvent;
class QCloseEvent;
class QShowEvent;
class QHideEvent;
class QSystemTrayIcon;
class QMenu;
class QAction;
class QTranslator;
QT_END_NAMESPACE

class CustomButton;
class AsrEngine;
class SettingsDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
#if defined(Q_OS_WIN)
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif

private slots:
    void showClipboardText();
    void onListenButtonClicked();
    void onAsrStateChanged(int state);
    void onTranscriptionReady(const QString &text);
    void startTranscriptionFromClipboard();
    void onMicAudioReadyRead();
    void toggleWindowVisibility();
    void openSettingsDialog();
    void quitFromTray();
#if defined(Q_OS_WIN)
    void doCopyFromForeground();
    void updateLastForegroundWindow();
#endif

private:
    void onAsrTranscribe();
    void onAsrStop();
    void saveLastRecordingAsWav();
    void setupUiDynamic();
    void setupWindowFrame();
    void setupTrayIcon();
    void loadAndApplySettings();
    void applyAppLanguage(const QString &lang);
    void retranslateUi();
    void registerGlobalHotkey();
    void unregisterGlobalHotkey();
    void simulateCopy();
    bool isInTitleBar(const QPoint &globalPos) const;
    bool startMicrophoneCapture();
    void stopMicrophoneCapture();
    void finalizeCurrentSentence();
    void updateInputLevelFromPcm(const QByteArray &pcm16Mono);
    void sendTextToFocusedWindow(const QString &text);

    QWidget *m_centralWidget = nullptr;
    QWidget *m_titleBar = nullptr;
    QLabel *m_labelTitle = nullptr;
    CustomButton *m_btnClose = nullptr;
    CustomButton *m_btnSettings = nullptr;
    QLabel *m_labelStatus = nullptr;
    QProgressBar *m_inputLevelBar = nullptr;
    float m_inputLevelSmoothed = 0.f;

    AsrEngine *m_asrEngine = nullptr;
    QAudioInput *m_audioInput = nullptr;
    QIODevice *m_audioInputDevice = nullptr;
    bool m_listening = false;
    int m_audioSampleRate = 16000;
    int m_actualAudioSampleRate = 16000;
    QByteArray m_recordedPcm16;
    CustomButton *m_btnListen = nullptr;

    QPoint m_dragPosition;
    bool m_dragging = false;

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_showHideAction = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_quitAction = nullptr;

    QString m_currentAppLanguage;
    QTranslator *m_appTranslator = nullptr;

#if defined(Q_OS_WIN)
    bool m_playAfterCopy = false;
    static const int HOTKEY_ID = 1;
    bool m_hotkeyRegistered = false;
    HWND m_foregroundAtHotkey = 0;
    HWND m_lastKnownForeground = 0;
    QTimer *m_foregroundPollTimer = nullptr;
    HWND m_targetOutputWindow = 0;
#endif
};

#endif // MAINWINDOW_H
