# SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

# Plasma loads applet translations from this domain
remove_definitions(-DTRANSLATION_DOMAIN="plasma_assist")
add_definitions(-DTRANSLATION_DOMAIN="plasma_applet_org.kde.plasma.assist")

# Models are in a static library to be tested
add_library(plasmaassistappletmodels STATIC)
target_sources(
    plasmaassistappletmodels
    PRIVATE
        conversationmodel.h
        conversationmodel.cpp
        scenariomodel.h
        scenariomodel.cpp
)
set_target_properties(
    plasmaassistappletmodels
    PROPERTIES
        POSITION_INDEPENDENT_CODE
            ON
)
target_include_directories(plasmaassistappletmodels PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    plasmaassistappletmodels
    PUBLIC
        Qt::Core
        KF6::I18n
)

plasma_add_applet(org.kde.plasma.assist
    QML_SOURCES
        qml/main.qml
        qml/HomePage.qml
        qml/ConversationPage.qml
        qml/ToolCallCard.qml
        qml/ConfirmationCard.qml
        qml/ChangesPage.qml
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
        plasmaassistappletmodels
        Qt::DBus
        Qt::Quick
        Plasma::Plasma
        KF6::CoreAddons
        KF6::I18n
        KF6::Notifications
)

install(FILES plasma_assist.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()
