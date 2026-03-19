#include "settingsdialog.h"

#include "custombutton.h"

#include <QListWidget>
#include <QStackedWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QSettings>
#include <QMouseEvent>

static constexpr const char *kSettingsGroup = "settings";
static constexpr const char *kAppLanguageKey = "appLanguage";
static constexpr const char *kSpeechLanguageKey = "speechLanguage";
static constexpr const char *kLegacyAsrLanguageKey = "AsrLanguage";

static const int SETTINGS_TITLE_BAR_HEIGHT = 42;

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setModal(true);
    resize(520, 360);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);

    buildUi();
    loadFromSettings();
}

bool SettingsDialog::isInTitleBar(const QPoint &globalPos) const
{
    if (!m_titleBar)
        return false;
    const QPoint local = m_titleBar->mapFromGlobal(globalPos);
    return QRect(QPoint(0, 0), m_titleBar->size()).contains(local);
}

void SettingsDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isInTitleBar(event->globalPos())) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        m_dragging = true;
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void SettingsDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void SettingsDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}

void SettingsDialog::buildUi()
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(0);

    QWidget *frame = new QWidget(this);
    frame->setObjectName(QStringLiteral("dialogFrame"));
    outer->addWidget(frame);

    QVBoxLayout *root = new QVBoxLayout(frame);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_titleBar = new QWidget(frame);
    m_titleBar->setObjectName(QStringLiteral("settingsTitleBar"));
    m_titleBar->setFixedHeight(SETTINGS_TITLE_BAR_HEIGHT);
    m_titleBar->setCursor(Qt::SizeAllCursor);

    QHBoxLayout *titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(14, 0, 10, 0);
    titleLayout->setSpacing(8);
    m_titleLabel = new QLabel(tr("Settings"), m_titleBar);
    m_titleLabel->setObjectName(QStringLiteral("settingsTitleLabel"));
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch(1);

    m_btnClose = new CustomButton(CustomButton::TitleBar, m_titleBar);
    m_btnClose->setObjectName(QStringLiteral("settingsBtnClose"));
    m_btnClose->setFixedSize(28, 28);
    m_btnClose->setText(QString());
    m_btnClose->setIconPath(QStringLiteral(":/icons/close.svg"));
    m_btnClose->setToolTip(tr("Close window"));
    titleLayout->addWidget(m_btnClose, 0, Qt::AlignVCenter);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);
    root->addWidget(m_titleBar);

    QWidget *contentRoot = new QWidget(frame);
    contentRoot->setObjectName(QStringLiteral("dialogContent"));
    QVBoxLayout *contentLayout = new QVBoxLayout(contentRoot);
    contentLayout->setContentsMargins(14, 12, 14, 14);
    contentLayout->setSpacing(12);
    root->addWidget(contentRoot, 1);

    setStyleSheet(QStringLiteral(R"(
        QDialog { background-color: transparent; font-size: 13px; }
        QWidget#dialogFrame { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #f3f9ff); border: 1px solid #d0e4ff; border-radius: 14px; }
        QWidget#settingsTitleBar { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e4f3ff, stop:1 #c0ddff); border-top-left-radius: 14px; border-top-right-radius: 14px; }
        QLabel#settingsTitleLabel { color: #1f3b5e; font-weight: 600; }
        QListWidget#settingsNav { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e4f3ff, stop:1 #c0ddff); border: 1px solid #d0e4ff; border-radius: 12px; padding: 6px; outline: 0; }
        QListWidget#settingsNav::item { color: #1f3b5e; padding: 12px 10px; margin: 2px 0px; border-radius: 10px; }
        QListWidget#settingsNav::item:selected { background: #ffffff; border: 1px solid #bfe0ff; color: #1f3b5e; font-weight: 600; }
        QStackedWidget { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f9fcff, stop:1 #e4f1ff); border: 1px solid #d0e4ff; border-radius: 12px; }
        QLabel { color: #1f3b5e; }
        QComboBox { background: #ffffff; color: #123456; border: 1px solid #d0e4ff; border-radius: 8px; padding: 6px 10px; min-height: 20px; }
        QComboBox:focus { border: 1px solid #63a9ff; }
        QPushButton { background: #ffffff; border: 1px solid #d0e4ff; border-radius: 10px; padding: 9px 14px; color: #1f3b5e; font-weight: 600; }
    )"));

    QHBoxLayout *center = new QHBoxLayout();
    center->setContentsMargins(0, 0, 0, 0);
    center->setSpacing(10);
    m_nav = new QListWidget(this);
    m_nav->setObjectName(QStringLiteral("settingsNav"));
    m_nav->setSelectionMode(QAbstractItemView::SingleSelection);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nav->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_nav->setFixedWidth(165);
    m_stack = new QStackedWidget(this);
    center->addWidget(m_nav);
    center->addWidget(m_stack, 1);
    contentLayout->addLayout(center, 1);

    connect(m_nav, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    auto addPage = [this](QWidget *page, const QString &title) {
        m_stack->addWidget(page);
        m_nav->addItem(title);
    };

    {
        QWidget *page = new QWidget(m_stack);
        QVBoxLayout *v = new QVBoxLayout(page);
        v->setContentsMargins(12, 12, 12, 12);
        v->setSpacing(10);
        QLabel *hint = new QLabel(tr("App language requires translation files (.qm)."), page);
        hint->setWordWrap(true);
        v->addWidget(hint);
        QFormLayout *form = new QFormLayout();
        m_comboAppLanguage = new QComboBox(page);
        m_comboAppLanguage->addItem(tr("System default"), QString());
        m_comboAppLanguage->addItem(tr("English"), QStringLiteral("en"));
        m_comboAppLanguage->addItem(tr("Japanese"), QStringLiteral("ja"));
        m_comboAppLanguage->addItem(tr("Chinese (Simplified)"), QStringLiteral("zh_CN"));
        form->addRow(tr("Application"), m_comboAppLanguage);
        m_comboSpeechLanguage = new QComboBox(page);
        m_comboSpeechLanguage->addItem(tr("Auto / model default"), QString());
        m_comboSpeechLanguage->addItem(tr("English"), QStringLiteral("en"));
        m_comboSpeechLanguage->addItem(tr("Japanese"), QStringLiteral("ja"));
        m_comboSpeechLanguage->addItem(tr("Chinese (Simplified)"), QStringLiteral("zh_CN"));
        form->addRow(tr("Speech"), m_comboSpeechLanguage);
        v->addLayout(form);
        v->addStretch(1);
        addPage(page, tr("Language"));
    }

    if (m_nav->count() > 0)
        m_nav->setCurrentRow(0);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    contentLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &SettingsDialog::apply);
}

void SettingsDialog::loadFromSettings()
{
    QSettings s;
    s.beginGroup(QLatin1String(kSettingsGroup));

    const QString appLang = s.value(QLatin1String(kAppLanguageKey), QString()).toString();
    int appIdx = m_comboAppLanguage ? m_comboAppLanguage->findData(appLang) : -1;
    if (m_comboAppLanguage) m_comboAppLanguage->setCurrentIndex(appIdx < 0 ? 0 : appIdx);

    QString speechLang = s.value(QLatin1String(kSpeechLanguageKey), QString()).toString();
    if (speechLang.isEmpty())
        speechLang = s.value(QLatin1String(kLegacyAsrLanguageKey), QString()).toString();
    int speechIdx = m_comboSpeechLanguage ? m_comboSpeechLanguage->findData(speechLang) : -1;
    if (m_comboSpeechLanguage) m_comboSpeechLanguage->setCurrentIndex(speechIdx < 0 ? 0 : speechIdx);

    s.endGroup();
}

void SettingsDialog::saveToSettings() const
{
    QSettings s;
    s.beginGroup(QLatin1String(kSettingsGroup));

    if (m_comboAppLanguage)
        s.setValue(QLatin1String(kAppLanguageKey), m_comboAppLanguage->currentData().toString());
    if (m_comboSpeechLanguage)
        s.setValue(QLatin1String(kSpeechLanguageKey), m_comboSpeechLanguage->currentData().toString());

    s.endGroup();
    s.sync();
}

void SettingsDialog::apply()
{
    saveToSettings();
    emit settingsApplied();
}

void SettingsDialog::accept()
{
    apply();
    QDialog::accept();
}
