# SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

# Plasma loads applet translations from this domain
remove_definitions(-DTRANSLATION_DOMAIN="plasma_assist")
add_definitions(-DTRANSLATION_DOMAIN="plasma_applet_org.kde.plasma.assist")

plasma_add_applet(org.kde.plasma.assist
    QML_SOURCES
        qml/main.qml
    CPP_SOURCES
        assistapplet.h
        assistapplet.cpp
)

ecm_qt_declare_logging_category(org.kde.plasma.assist HEADER plasmaassist_applet_debug.h IDENTIFIER PLASMA_ASSIST_APPLET_LOG
    CATEGORY_NAME org.kde.plasma.assist.applet DESCRIPTION "plasma-assist (applet)" EXPORT PLASMA_ASSIST
)

set(plasmaassistapplet_dbus_SRCS)
qt_add_dbus_interface(plasmaassistapplet_dbus_SRCS ../daemon/org.kde.plasma.assist.Assistant.xml assistantinterface)
target_sources(org.kde.plasma.assist PRIVATE ${plasmaassistapplet_dbus_SRCS})

target_link_libraries(
    org.kde.plasma.assist
    PRIVATE
        Qt::DBus
        Qt::Quick
        Plasma::Plasma
        KF6::CoreAddons
        KF6::I18n
)
