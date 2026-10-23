# SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

add_library(plasmaassistdaemon STATIC)
ecm_qt_declare_logging_category(plasmaassistdaemon HEADER plasmaassist_daemon_debug.h IDENTIFIER PLASMA_ASSIST_DAEMON_LOG
    CATEGORY_NAME org.kde.plasma.assist.daemon DESCRIPTION "plasma-assist (daemon)" EXPORT PLASMA_ASSIST
)

set(plasmaassistdaemon_dbus_SRCS)
qt_add_dbus_adaptor(plasmaassistdaemon_dbus_SRCS org.kde.plasma.assist.Assistant.xml assistant.h Assistant assistantadaptor AssistantAdaptor)

target_sources(
    plasmaassistdaemon
    PRIVATE
        assistant.h
        assistant.cpp
        ${plasmaassistdaemon_dbus_SRCS}
)

if(COMPILE_WITH_UNITY_CMAKE_SUPPORT)
    set_target_properties(
        plasmaassistdaemon
        PROPERTIES
            UNITY_BUILD
                ON
    )
endif()

target_link_libraries(plasmaassistdaemon PUBLIC Qt::DBus)

add_executable(plasma-assistd main.cpp)
target_link_libraries(
    plasma-assistd
    plasmaassistdaemon
    KF6::CoreAddons
    KF6::I18n
)

install(
    TARGETS
        plasma-assistd
        ${KDE_INSTALL_TARGETS_DEFAULT_ARGS}
)

configure_file(org.kde.plasma.assist.service.in ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.assist.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.assist.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
install(FILES org.kde.plasma.assist.Assistant.xml DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()
