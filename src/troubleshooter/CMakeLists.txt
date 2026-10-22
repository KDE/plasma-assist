# SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

add_library(plasmaassisttroubleshooter STATIC)
ecm_qt_declare_logging_category(plasmaassisttroubleshooter HEADER plasmaassist_troubleshooter_debug.h IDENTIFIER PLASMA_ASSIST_TROUBLESHOOTER_LOG
    CATEGORY_NAME org.kde.plasma.assist.troubleshooter DESCRIPTION "plasma-assist (troubleshooter MCP server)" EXPORT PLASMA_ASSIST
)

target_sources(
    plasmaassisttroubleshooter
    PRIVATE
        troubleshooterserver.h
        troubleshooterserver.cpp
)

if(COMPILE_WITH_UNITY_CMAKE_SUPPORT)
    set_target_properties(
        plasmaassisttroubleshooter
        PROPERTIES
            UNITY_BUILD
                ON
    )
endif()

target_link_libraries(plasmaassisttroubleshooter PUBLIC KF6::TextAutoGenerateTextMcpProtocolCore PRIVATE KF6::I18n)

add_executable(
    plasma-assist-troubleshooter
    main.cpp
    modules/desktopmodule.h
    modules/desktopmodule.cpp
)
target_link_libraries(
    plasma-assist-troubleshooter
    plasmaassisttroubleshooter
    KF6::CoreAddons
    KF6::I18n
)

install(
    TARGETS
        plasma-assist-troubleshooter
        ${KDE_INSTALL_TARGETS_DEFAULT_ARGS}
)

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()
