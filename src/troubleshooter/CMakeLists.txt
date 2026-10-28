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
        core/toolhelpers.h
        core/toolhelpers.cpp
        core/privacyfilter.h
        core/privacyfilter.cpp
        core/changejournal.h
        core/changejournal.cpp
        core/actions.h
        core/actions.cpp
        backends/audiobackend.h
        backends/audiobackend.cpp
        backends/pulseaudiobackend.h
        backends/pulseaudiobackend.cpp
        backends/displaybackend.h
        backends/displaybackend.cpp
        backends/kscreenbackend.h
        backends/kscreenbackend.cpp
        backends/networkbackend.h
        backends/networkbackend.cpp
        backends/networkmanagerbackend.h
        backends/networkmanagerbackend.cpp
        backends/powerbackend.h
        backends/powerbackend.cpp
        backends/systempowerbackend.h
        backends/systempowerbackend.cpp
        backends/commandrunner.h
        backends/commandrunner.cpp
        modules/systemmodule.h
        modules/systemmodule.cpp
        modules/audiomodule.h
        modules/audiomodule.cpp
        modules/displaymodule.h
        modules/displaymodule.cpp
        modules/networkmodule.h
        modules/networkmodule.cpp
        modules/powermodule.h
        modules/powermodule.cpp
        modules/crashmodule.h
        modules/crashmodule.cpp
        modules/logmodule.h
        modules/logmodule.cpp
        modules/reportmodule.h
        modules/reportmodule.cpp
        modules/audioactions.h
        modules/audioactions.cpp
        modules/displayactions.h
        modules/displayactions.cpp
        modules/systemactions.h
        modules/systemactions.cpp
        modules/bugreportmodule.h
        modules/bugreportmodule.cpp
        prompts/scenarioprompts.h
        prompts/scenarioprompts.cpp
)

if(COMPILE_WITH_UNITY_CMAKE_SUPPORT)
    set_target_properties(
        plasmaassisttroubleshooter
        PROPERTIES
            UNITY_BUILD
                ON
    )
endif()

target_include_directories(plasmaassisttroubleshooter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    plasmaassisttroubleshooter
    PUBLIC
        KF6::TextAutoGenerateTextMcpProtocolCore
    PRIVATE
        Qt::Concurrent
        Qt::DBus
        Qt::Network
        KF6::ConfigCore
        KF6::CoreAddons
        KF6::NetworkManagerQt
        KF6::Solid
        KF6::I18n
        KF6::PulseAudioQt
        KF6::Screen
)

add_executable(
    plasma-assist-troubleshooter
    main.cpp
    modules/desktopmodule.h
    modules/desktopmodule.cpp
)
target_link_libraries(
    plasma-assist-troubleshooter
    plasmaassisttroubleshooter
    Qt::Gui
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
