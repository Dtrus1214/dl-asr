# Offline ASR experiments use sherpa-onnx C API (in-process).
# Set SHERPA_ONNX_DIR to sherpa-onnx install dir (e.g. C:/sherpa-onnx-install) to auto-add include/lib.
QT       += core gui widgets multimedia concurrent svg

SHERPA_ONNX_DIR = C:/sherpa-onnx

!isEmpty(SHERPA_ONNX_DIR) {
    INCLUDEPATH += $$SHERPA_ONNX_DIR/include
    win32: LIBS += -L$$SHERPA_ONNX_DIR/lib -lsherpa-onnx-c-api
    unix: LIBS += -L$$SHERPA_ONNX_DIR/lib -lsherpa-onnx-c-api
}

CONFIG += c++11

TARGET = CrystalAsr

win32: LIBS += -luser32
unix:!macx: LIBS += -lxcb -lxcb-xtest -lxcb-keysyms

DEFINES += QT_DEPRECATED_WARNINGS

# Windows: set the executable icon if an .ico is provided.
# (Runtime window/tray icon is handled via Qt resources.)
win32:exists(icons/app.ico): RC_ICONS += icons/app.ico

SOURCES += \
    asr/asrengine.cpp \
    asr/vadmodule.cpp \
    main.cpp \
    mainwindow.cpp \
    custombutton.cpp \
    settingsdialog.cpp

HEADERS += \
    asr/asrengine.h \
    asr/vadmodule.h \
    mainwindow.h \
    custombutton.h \
    settingsdialog.h

RESOURCES += icons.qrc

TRANSLATIONS += \
    i18n/CrystalAsr_ja.ts

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
