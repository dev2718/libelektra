QT += quick  gui core  qml  widgets

SOURCES +=      src/main.cpp \
		src/treeviewmodel.cpp \
		src/confignode.cpp \
                src/keynode.cpp

HEADERS += \
                src/treeviewmodel.hpp \
                src/confignode.hpp \
                src/treeviewmodel.hpp \
                src/keynode.hpp \
                src/visitor.hpp \
                src/cpp_example_hierarchy.hpp

TRANSLATIONS += qml/i18n/qml_de.ts

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

INCLUDEPATH += /usr/include/elektra
INCLUDEPATH += /usr/local/include/elektra
INCLUDEPATH += ../../Desktop/p4n81s-libelektra/src/tools/kdb/


# Default rules for deployment.
include(deployment.pri)

RESOURCES += resources.qrc \
    i18n.qrc

OTHER_FILES += \
		qml/main.qml\
		qml/BasicWindow.qml\
		qml/ButtonRow.qml\
		qml/Page1.qml\
		qml/Page2.qml\
		qml/Page3.qml\
		qml/WizardLoader.qml\
		qml/WizardTemplate.qml\
		qml/Page4.qml\
		qml/NewMetaKey.qml\
		qml/BasicRectangle.qml \
		qml/UnmountBackendWindow.qml \
		qml/NewKeyWindow.qml \
		qml/NewArrayEntry.qml \
		qml/TreeView.qml

unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += elektra
