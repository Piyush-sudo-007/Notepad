QT += widgets printsupport sql

CONFIG += c++17
win32:CONFIG += console app_bundle

#Direct compiler to your header search directories
INCLUDEPATH += \
        $$PWD/third_party/includee \
        $$PWD/third_party/sw/tokenizer

#Link against the ONXX Runtime library for your MinGW 64-bit environment
LIBS += -L$$PWD/third_party/lib -lonnxruntime

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    predictionworker.cpp

HEADERS += \
    mainwindow.h \
    predictionworker.h

FORMS += \
    mainwindow.ui

#Copy your assets directory automatically to your build executable target environment
#Mapping it cleanly for absolute system portability
DEPLOY_ASSETS.files = $$PWD/assets
DEPLOY_ASSETS.path = $$OUT_PWD/assets
DEPLOY_LIBS.files = $$PWD/third_party/lib/onnxruntime.dll \
                    $$PWD/third_party/lib/onnxruntime_providers_shared.dll
DEPLOY_LIBS.path = $$OUT_PWD/debug
COPIES += DEPLOY_ASSETS DEPLOY_LIBS

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
