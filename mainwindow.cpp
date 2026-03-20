#include "mainwindow.h"
#include "custombutton.h"
#include "settingsdialog.h"
#include "asr/asrengine.h"

#include <QMouseEvent>
#include <QClipboard>
#include <QApplication>
#include <QAudioFormat>
#include <QAudioInput>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QCloseEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QStyle>
#include <QEvent>
#include <QIcon>
#include <QSettings>
#include <QTranslator>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QProgressBar>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
#include <QGuiApplication>
#include <QWindow>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtGui/qnativeinterface.h>
#endif
#include <xcb/xcb.h>
#include <xcb/xtest.h>
#include <xcb/xcb_keysyms.h>
/* Keysym values (avoid X11/keysymdef.h dependency) */
#ifndef XK_Control_L
#define XK_Control_L 0xffe3
#endif
#ifndef XK_c
#define XK_c         0x0063
#endif
#endif

static const int TITLE_BAR_HEIGHT = 40;
static const int WINDOW_RADIUS = 14;
static const int CONTENT_PADDING = 14;

namespace {

/** Peak sample magnitude in [0, 1] for mono s16le PCM. */
float pcm16PeakLinear(const QByteArray &pcm16Mono)
{
    const int sampleCount = pcm16Mono.size() / static_cast<int>(sizeof(qint16));
    if (sampleCount <= 0)
        return 0.f;
    const qint16 *p = reinterpret_cast<const qint16 *>(pcm16Mono.constData());
    int peak = 0;
    for (int i = 0; i < sampleCount; ++i) {
        const int v = qAbs(static_cast<int>(p[i]));
        if (v > peak)
            peak = v;
    }
    return static_cast<float>(peak) / 32768.0f;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUiDynamic();
    setupWindowFrame();
    connect(m_btnClose, &QPushButton::clicked, this, &QWidget::hide);

    m_asrEngine = new AsrEngine(this);
    loadAndApplySettings();
    m_asrEngine->initialize();
    connect(m_asrEngine, &AsrEngine::stateChanged, this, &MainWindow::onAsrStateChanged);
    connect(m_asrEngine, &AsrEngine::transcriptionReady, this, &MainWindow::onTranscriptionReady);
    connect(m_btnListen, &QPushButton::clicked, this, &MainWindow::onListenButtonClicked);
    onAsrStateChanged(m_asrEngine->state());

    setupTrayIcon();
    registerGlobalHotkey();
#if defined(Q_OS_WIN)
    m_foregroundPollTimer = new QTimer(this);
    connect(m_foregroundPollTimer, &QTimer::timeout, this, &MainWindow::updateLastForegroundWindow);
    m_foregroundPollTimer->start(300);
#endif
}

MainWindow::~MainWindow()
{
    stopMicrophoneCapture();
    unregisterGlobalHotkey();
}

void MainWindow::applyAppLanguage(const QString &lang)
{
    const QString normalized = lang.trimmed();
    if (m_currentAppLanguage == normalized)
        return;

    if (!m_appTranslator) {
        QObject *obj = qApp->property("_CrystalAsr_translator").value<QObject *>();
        m_appTranslator = qobject_cast<QTranslator *>(obj);
    }
    if (!m_appTranslator) {
        m_currentAppLanguage = normalized;
        retranslateUi();
        return;
    }

    qApp->removeTranslator(m_appTranslator);

    bool loaded = false;
    if (!normalized.isEmpty()) {
        const QString base = QStringLiteral("CrystalAsr_%1").arg(normalized);
        const QString appDir = QCoreApplication::applicationDirPath();
        const QString diskPath = appDir + QStringLiteral("/i18n");
        loaded = (m_appTranslator->load(base, diskPath) || m_appTranslator->load(QStringLiteral(":/i18n/") + base));
    }

    if (loaded)
        qApp->installTranslator(m_appTranslator);

    m_currentAppLanguage = normalized;
    retranslateUi();
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("CrystalAsr"));
    if (m_labelTitle)
        m_labelTitle->setText(tr("Transcription"));
    if (m_btnSettings)
        m_btnSettings->setToolTip(tr("Settings"));
    if (m_trayIcon)
        m_trayIcon->setToolTip(tr("CrystalAsr - Speech assistant"));

    if (m_showHideAction)
        m_showHideAction->setText(isVisible() ? tr("Hide CrystalAsr") : tr("Show CrystalAsr"));
    if (m_settingsAction)
        m_settingsAction->setText(tr("Settings..."));
    if (m_quitAction)
        m_quitAction->setText(tr("Quit"));
    if (m_inputLevelBar)
        m_inputLevelBar->setToolTip(tr("Microphone level (while listening)"));
    if (m_asrEngine)
        onAsrStateChanged(m_asrEngine->state());

}

void MainWindow::setupUiDynamic()
{
    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName("centralWidget");
    setCentralWidget(m_centralWidget);

    QVBoxLayout *rootLayout = new QVBoxLayout(m_centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ---- Title bar (draggable region) ----
    m_titleBar = new QWidget(m_centralWidget);
    m_titleBar->setObjectName("titleBar");
    m_titleBar->setFixedHeight(TITLE_BAR_HEIGHT);
    m_titleBar->setCursor(Qt::SizeAllCursor);

    QHBoxLayout *titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(CONTENT_PADDING, 0, 8, 0);
    titleLayout->setSpacing(8);

    m_labelTitle = new QLabel(tr("Transcription"), m_titleBar);
    m_labelTitle->setObjectName("labelTitle");
    titleLayout->addWidget(m_labelTitle);

    titleLayout->addStretch();

    m_btnSettings = new CustomButton(CustomButton::TitleBar, m_titleBar);
    m_btnSettings->setObjectName("btnSettings");
    m_btnSettings->setText(QString());
    m_btnSettings->setFixedSize(28, 28);
    m_btnSettings->setIconPath(QStringLiteral(":/icons/settings.svg"));
    m_btnSettings->setToolTip(tr("Settings"));
    titleLayout->addWidget(m_btnSettings, 0, Qt::AlignVCenter);
    connect(m_btnSettings, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);

    m_btnClose = new CustomButton(CustomButton::TitleBar, m_titleBar);
    m_btnClose->setObjectName("btnClose");
    m_btnClose->setText(QString());
    m_btnClose->setFixedSize(28, 28);
    m_btnClose->setIconPath(QStringLiteral(":/icons/close.svg"));
    titleLayout->addWidget(m_btnClose, 0, Qt::AlignVCenter);

    rootLayout->addWidget(m_titleBar);

    // ---- Content ----
    QWidget *content = new QWidget(m_centralWidget);
    content->setObjectName("content");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(CONTENT_PADDING, 12, CONTENT_PADDING, CONTENT_PADDING);
    contentLayout->setSpacing(10);

    QHBoxLayout *asrLayout = new QHBoxLayout();
    asrLayout->setSpacing(8);
    m_btnListen = new CustomButton(CustomButton::Primary, content);
    m_btnListen->setObjectName("btnListen");
    m_btnListen->setFixedSize(32, 32);
    m_btnListen->setIconPath(QStringLiteral(":/icons/microphone.svg"));
    m_btnListen->setToolTip(tr("Start listening"));
    asrLayout->addWidget(m_btnListen);

    m_inputLevelBar = new QProgressBar(content);
    m_inputLevelBar->setObjectName(QStringLiteral("micLevelBar"));
    m_inputLevelBar->setRange(0, 100);
    m_inputLevelBar->setValue(0);
    m_inputLevelBar->setTextVisible(false);
    m_inputLevelBar->setFixedHeight(10);
    m_inputLevelBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_inputLevelBar->setVisible(false);
    m_inputLevelBar->setToolTip(tr("Microphone level (while listening)"));
    asrLayout->addWidget(m_inputLevelBar, 1);

    contentLayout->addLayout(asrLayout);

    m_labelStatus = new QLabel(tr("Idle"), content);
    m_labelStatus->setObjectName("labelStatus");
    m_labelStatus->setWordWrap(true);
    contentLayout->addWidget(m_labelStatus);

    rootLayout->addWidget(content);
}

void MainWindow::setupWindowFrame()
{
    setWindowTitle(tr("CrystalAsr"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground); // so drop shadow is visible

    // Crystal-style palette: light, airy blues and white
    const char *sheet = R"(
        QMainWindow {
            background-color: transparent;
        }
        QWidget#centralWidget {
            background-color: #ffffff;
            border: 1px solid #d0e4ff;
            border-radius: %1px;
        }
        QWidget#titleBar {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                              stop:0 #e4f3ff,
                                              stop:1 #c0ddff);
            border-top-left-radius: %1px;
            border-top-right-radius: %1px;
        }
        QWidget#content {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                              stop:0 #f9fcff,
                                              stop:1 #e4f1ff);
            border-bottom-left-radius: %1px;
            border-bottom-right-radius: %1px;
        }
        QLabel#labelTitle {
            color: #1f3b5e;
            font-size: 13px;
            font-weight: 600;
        }
        QLabel#labelStatus {
            color: #4d6580;
            font-size: 11px;
        }
        QProgressBar#micLevelBar {
            border: 1px solid #d0e4ff;
            border-radius: 5px;
            background-color: #ffffff;
            min-height: 10px;
            max-height: 10px;
        }
        QProgressBar#micLevelBar::chunk {
            border-radius: 4px;
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #a8dcff, stop:0.55 #4aa3ff, stop:1 #2b7fd4);
        }
        QPlainTextEdit#textSelected {
            background-color: #ffffff;
            color: #123456;
            border: 1px solid #d0e4ff;
            border-radius: 6px;
            padding: 8px;
            font-size: 12px;
            selection-background-color: #c0ddff;
        }
    )";
    setStyleSheet(QString::fromUtf8(sheet).arg(WINDOW_RADIUS));

    // No window mask: rounded shape is drawn by Qt with antialiasing (soft edges).
    // Shadow removed to avoid a dark rectangular area; content has transparent corners.
    clearMask();
}

bool MainWindow::isInTitleBar(const QPoint &globalPos) const
{
    if (!m_titleBar || !m_centralWidget)
        return false;
    QPoint local = m_centralWidget->mapFromGlobal(globalPos);
    return m_titleBar->geometry().contains(local);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isInTitleBar(event->globalPos())) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        m_dragging = true;
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    } else {
        m_dragging = false;
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::setupTrayIcon()
{
    m_trayMenu = new QMenu(this);
    m_showHideAction = m_trayMenu->addAction(tr("Show CrystalAsr"), this, &MainWindow::toggleWindowVisibility);
    m_settingsAction = m_trayMenu->addAction(tr("Settings..."), this, &MainWindow::openSettingsDialog);
    m_quitAction = m_trayMenu->addAction(tr("Quit"), this, &MainWindow::quitFromTray);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->setIcon(QIcon(QStringLiteral(":/icons/app.svg")));
    m_trayIcon->setToolTip(tr("CrystalAsr - Speech assistant"));
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger)
            toggleWindowVisibility();
    });
    m_trayIcon->show();
}

void MainWindow::toggleWindowVisibility()
{
    if (isVisible()) {
        hide();
        m_showHideAction->setText(tr("Show CrystalAsr"));
    } else {
        show();
        raise();
        activateWindow();
        m_showHideAction->setText(tr("Hide CrystalAsr"));
    }
}

void MainWindow::quitFromTray()
{
    m_trayIcon->hide();
    qApp->quit();
}

void MainWindow::openSettingsDialog()
{
    SettingsDialog dlg(this);
    connect(&dlg, &SettingsDialog::settingsApplied, this, &MainWindow::loadAndApplySettings);
    dlg.exec();
}

void MainWindow::loadAndApplySettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("settings"));

    const QString appLang = s.value(QStringLiteral("appLanguage"), QString()).toString();
    const QString speechLanguage = s.value(QStringLiteral("speechLanguage"), QString()).toString().trimmed();
    if (m_asrEngine)
        m_asrEngine->setLanguage(speechLanguage);

    s.endGroup();

    applyAppLanguage(appLang);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    hide(); // minimize to tray; quit only via tray menu
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_showHideAction)
        m_showHideAction->setText(tr("Hide CrystalAsr"));
}

void MainWindow::hideEvent(QHideEvent *event)
{
    QMainWindow::hideEvent(event);
    if (m_showHideAction)
        m_showHideAction->setText(tr("Show CrystalAsr"));
}

#if defined(Q_OS_WIN)
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == static_cast<WPARAM>(HOTKEY_ID)) {
            m_playAfterCopy = false;
            m_foregroundAtHotkey = GetForegroundWindow();
            QTimer::singleShot(20, this, &MainWindow::doCopyFromForeground);
            *result = 0;
            return true;
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::doCopyFromForeground()
{
    HWND us = reinterpret_cast<HWND>(winId());
    if (m_foregroundAtHotkey && m_foregroundAtHotkey != us && IsWindow(m_foregroundAtHotkey)) {
        DWORD targetThread = GetWindowThreadProcessId(m_foregroundAtHotkey, NULL);
        DWORD ourThread = GetCurrentThreadId();
        if (targetThread != ourThread) {
            AttachThreadInput(ourThread, targetThread, TRUE);
            SetForegroundWindow(m_foregroundAtHotkey);
            AttachThreadInput(ourThread, targetThread, FALSE);
        } else {
            SetForegroundWindow(m_foregroundAtHotkey);
        }
        Sleep(60);
    }
    m_foregroundAtHotkey = 0;
    simulateCopy();
    if (m_playAfterCopy) {
        QTimer::singleShot(150, this, &MainWindow::startTranscriptionFromClipboard);
    } else {
        QTimer::singleShot(150, this, &MainWindow::showClipboardText);
    }
}
#endif

#if defined(Q_OS_WIN)
void MainWindow::updateLastForegroundWindow()
{
    HWND fg = GetForegroundWindow();
    HWND us = reinterpret_cast<HWND>(winId());
    if (fg && fg != us && IsWindow(fg))
        m_lastKnownForeground = fg;
}
#endif

void MainWindow::showClipboardText()
{
    QString text = QApplication::clipboard()->text(QClipboard::Clipboard).trimmed();
    int n = text.length();
    if (!m_labelStatus)
        return;
    if (text.isEmpty())
        m_labelStatus->setText(tr("Last: 0 characters (no selection copied?)"));
    else
        m_labelStatus->setText(tr("Last: %1 character(s) from clipboard").arg(n));
}

void MainWindow::onAsrTranscribe()
{
    if (!m_asrEngine || !m_asrEngine->isAvailable())
        return;

    if (m_listening)
        return;

#if defined(Q_OS_WIN)
    HWND current = reinterpret_cast<HWND>(winId());
    HWND fg = GetForegroundWindow();
    if (fg == current && m_lastKnownForeground && IsWindow(m_lastKnownForeground))
        m_targetOutputWindow = m_lastKnownForeground;
    else
        m_targetOutputWindow = fg;
#endif

    if (!startMicrophoneCapture() && m_labelStatus)
        m_labelStatus->setText(tr("Microphone is not available."));
}

void MainWindow::onAsrStop()
{
    if (m_asrEngine && m_asrEngine->isAvailable())
        m_asrEngine->flushAudio();
    stopMicrophoneCapture();
    if (m_asrEngine && m_asrEngine->isAvailable()) {
        m_asrEngine->stop();
    }
    saveLastRecordingAsWav();
    if (m_labelStatus)
        m_labelStatus->setText(tr("Listening stopped."));
}

void MainWindow::onListenButtonClicked()
{
    if (m_listening) {
        onAsrStop();
    } else {
        onAsrTranscribe();
    }
}

void MainWindow::startTranscriptionFromClipboard()
{
    if (!m_asrEngine || !m_asrEngine->isAvailable())
        return;

    QString text = QApplication::clipboard()->text(QClipboard::Clipboard).trimmed();
    if (text.isEmpty())
        return;

    if (m_labelStatus)
        m_labelStatus->setText(tr("Transcribing clipboard text..."));
    m_asrEngine->transcribe(text);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);
    Q_UNUSED(event);
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onAsrStateChanged(int state)
{
    if (!m_btnListen)
        return;
    bool available = m_asrEngine && m_asrEngine->isAvailable();
    bool processing = (state == AsrEngine::Processing);
    const bool busy = processing || m_listening;
    m_btnListen->setIconPath(busy ? QStringLiteral(":/icons/stop.svg")
                                  : QStringLiteral(":/icons/microphone.svg"));
    m_btnListen->setToolTip(busy ? tr("Stop listening") : tr("Start listening"));
    m_btnListen->setEnabled(available);
}

void MainWindow::onTranscriptionReady(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (m_labelStatus) {
        if (trimmed.isEmpty())
            m_labelStatus->setText(tr("No transcript produced."));
        else
            m_labelStatus->setText(tr("Transcript: %1").arg(trimmed));
    }
    if (!trimmed.isEmpty())
        sendTextToFocusedWindow(trimmed + QStringLiteral(" "));
}

void MainWindow::registerGlobalHotkey()
{
#if defined(Q_OS_WIN)
    if (m_hotkeyRegistered) return;
    HWND hwnd = reinterpret_cast<HWND>(winId());
    m_hotkeyRegistered = RegisterHotKey(hwnd, HOTKEY_ID, MOD_CONTROL | MOD_SHIFT, 0x53);
#endif
}

void MainWindow::unregisterGlobalHotkey()
{
#if defined(Q_OS_WIN)
    if (!m_hotkeyRegistered) return;
    UnregisterHotKey(reinterpret_cast<HWND>(winId()), HOTKEY_ID);
    m_hotkeyRegistered = false;
#endif
}

void MainWindow::simulateCopy()
{
#if defined(Q_OS_WIN)
    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 0x43;
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 0x43;
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    xcb_connection_t *conn = nullptr;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (auto *x11 = qApp->nativeInterface<QNativeInterface::QX11Application>())
        conn = x11->connection();
#else
    if (QWindow *win = windowHandle())
        conn = static_cast<xcb_connection_t *>(qApp->platformNativeInterface()->nativeResourceForWindow("connection", win));
#endif
    if (!conn) return;
    xcb_key_symbols_t *syms = xcb_key_symbols_alloc(conn);
    if (!syms) return;
    xcb_keycode_t *ctrl_keycodes = xcb_key_symbols_get_keycode(syms, XK_Control_L);
    xcb_keycode_t *c_keycodes = xcb_key_symbols_get_keycode(syms, XK_c);
    if (!ctrl_keycodes || !c_keycodes) {
        free(ctrl_keycodes);
        free(c_keycodes);
        xcb_key_symbols_free(syms);
        return;
    }
    xcb_keycode_t ctrl_kc = ctrl_keycodes[0];
    xcb_keycode_t c_kc = c_keycodes[0];
    free(ctrl_keycodes);
    free(c_keycodes);
    xcb_key_symbols_free(syms);

    /* Ctrl down, C down, C up, Ctrl up (XTest sends to focused window) */
    xcb_test_fake_input(conn, XCB_KEY_PRESS, ctrl_kc, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    xcb_test_fake_input(conn, XCB_KEY_PRESS, c_kc, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    xcb_test_fake_input(conn, XCB_KEY_RELEASE, c_kc, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    xcb_test_fake_input(conn, XCB_KEY_RELEASE, ctrl_kc, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    xcb_flush(conn);
#endif
}

bool MainWindow::startMicrophoneCapture()
{
    stopMicrophoneCapture();

    QAudioFormat format;
    format.setSampleRate(m_audioSampleRate);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec(QStringLiteral("audio/pcm"));
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    m_audioInput = new QAudioInput(format, this);
    m_audioInputDevice = m_audioInput->start();
    if (!m_audioInputDevice) {
        delete m_audioInput;
        m_audioInput = nullptr;
        return false;
    }

    m_actualAudioSampleRate = m_audioInput->format().sampleRate();
    m_recordedPcm16.clear();

    connect(m_audioInputDevice, &QIODevice::readyRead, this, &MainWindow::onMicAudioReadyRead);
    m_listening = true;

    m_inputLevelSmoothed = 0.f;
    if (m_inputLevelBar) {
        m_inputLevelBar->setValue(0);
        m_inputLevelBar->setVisible(true);
    }

    if (m_labelStatus)
        m_labelStatus->setText(tr("Listening... Speak now."));
    return true;
}

void MainWindow::stopMicrophoneCapture()
{
    m_listening = false;
    m_inputLevelSmoothed = 0.f;
    if (m_inputLevelBar) {
        m_inputLevelBar->setValue(0);
        m_inputLevelBar->setVisible(false);
    }
    if (m_audioInput) {
        m_audioInput->stop();
        m_audioInput->deleteLater();
        m_audioInput = nullptr;
    }
    m_audioInputDevice = nullptr;
}

void MainWindow::onMicAudioReadyRead()
{
    if (!m_audioInputDevice || !m_listening)
        return;

    const QByteArray chunk = m_audioInputDevice->readAll();
    if (chunk.isEmpty())
        return;
    // Store raw PCM so you can inspect what VAD/ASR saw (written as WAV on stop).
    m_recordedPcm16.append(chunk);
    updateInputLevelFromPcm(chunk);
    if (m_asrEngine)
        m_asrEngine->acceptAudioChunkPcm16(chunk, m_audioSampleRate);
}

void MainWindow::finalizeCurrentSentence()
{
    // Segmentation is now handled by AsrEngine VAD.
}

void MainWindow::updateInputLevelFromPcm(const QByteArray &pcm16Mono)
{
    if (!m_inputLevelBar || !m_listening)
        return;

    const float peakLinear = pcm16PeakLinear(pcm16Mono);
    // Slightly compress dynamic range so speech is easier to read on a short bar.
    const float instant = std::sqrt(peakLinear);

    // Fast attack, slow release — stable meter without sluggish response.
    constexpr float attackBlend = 0.55f;
    constexpr float releaseBlend = 0.88f;
    if (instant > m_inputLevelSmoothed)
        m_inputLevelSmoothed = m_inputLevelSmoothed * (1.f - attackBlend) + instant * attackBlend;
    else
        m_inputLevelSmoothed = m_inputLevelSmoothed * releaseBlend + instant * (1.f - releaseBlend);

    const int display = qBound(0, static_cast<int>(m_inputLevelSmoothed * 100.f + 0.5f), 100);
    m_inputLevelBar->setValue(display);
}

void MainWindow::saveLastRecordingAsWav()
{
    if (m_recordedPcm16.isEmpty())
        return;

    const QString dirPath = QCoreApplication::applicationDirPath() + QStringLiteral("/recordings");
    QDir().mkpath(dirPath);

    const QString fileName = QStringLiteral("recording_%1.wav")
                                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString filePath = dirPath + QStringLiteral("/") + fileName;

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly))
        return;

    // WAV header for PCM 16-bit mono, little-endian.
    const quint32 sampleRate = static_cast<quint32>(m_actualAudioSampleRate > 0 ? m_actualAudioSampleRate : m_audioSampleRate);
    const quint16 numChannels = 1;
    const quint16 bitsPerSample = 16;
    const quint32 byteRate = sampleRate * numChannels * bitsPerSample / 8;
    const quint16 blockAlign = numChannels * bitsPerSample / 8;
    const quint32 dataSize = static_cast<quint32>(m_recordedPcm16.size());

    auto writeLE16 = [&](quint16 v) {
        const char b[2] = { static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF) };
        f.write(b, 2);
    };
    auto writeLE32 = [&](quint32 v) {
        const char b[4] = {
            static_cast<char>(v & 0xFF),
            static_cast<char>((v >> 8) & 0xFF),
            static_cast<char>((v >> 16) & 0xFF),
            static_cast<char>((v >> 24) & 0xFF),
        };
        f.write(b, 4);
    };

    // RIFF descriptor
    f.write("RIFF", 4);
    writeLE32(36 + dataSize); // file size minus 8 bytes
    f.write("WAVE", 4);

    // fmt sub-chunk
    f.write("fmt ", 4);
    writeLE32(16);            // PCM fmt chunk size
    writeLE16(1);             // Audio format: 1 = PCM
    writeLE16(numChannels);
    writeLE32(sampleRate);
    writeLE32(byteRate);
    writeLE16(blockAlign);
    writeLE16(bitsPerSample);

    // data sub-chunk
    f.write("data", 4);
    writeLE32(dataSize);
    f.write(m_recordedPcm16.constData(), static_cast<qint64>(m_recordedPcm16.size()));

    if (m_labelStatus)
        m_labelStatus->setText(tr("Listening stopped. Saved: %1").arg(filePath));
}

void MainWindow::sendTextToFocusedWindow(const QString &text)
{
    if (text.trimmed().isEmpty())
        return;

#if defined(Q_OS_WIN)
    HWND us = reinterpret_cast<HWND>(winId());
    HWND target = (m_targetOutputWindow && IsWindow(m_targetOutputWindow)) ? m_targetOutputWindow : m_lastKnownForeground;
    if (!target || target == us || !IsWindow(target))
        return;

    const QString previousClipboard = QApplication::clipboard()->text(QClipboard::Clipboard);
    QApplication::clipboard()->setText(text, QClipboard::Clipboard);

    DWORD targetThread = GetWindowThreadProcessId(target, NULL);
    DWORD ourThread = GetCurrentThreadId();
    if (targetThread != ourThread) {
        AttachThreadInput(ourThread, targetThread, TRUE);
        SetForegroundWindow(target);
        AttachThreadInput(ourThread, targetThread, FALSE);
    } else {
        SetForegroundWindow(target);
    }
    Sleep(60);

    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));

    QTimer::singleShot(60, this, [previousClipboard]() {
        QApplication::clipboard()->setText(previousClipboard, QClipboard::Clipboard);
    });
#else
    Q_UNUSED(text);
#endif
}
