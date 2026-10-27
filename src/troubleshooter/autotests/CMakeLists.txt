# SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

macro(add_plasma_assist_troubleshooter_unittest _source)
    get_filename_component(_name ${_source} NAME_WE)
    ecm_add_test(${_source} ${_name}.h TEST_NAME ${_name}
        NAME_PREFIX "plasma-assist-troubleshooter-"
        LINK_LIBRARIES Qt::Test plasmaassisttroubleshooter
    )
    target_compile_definitions(${_name} PRIVATE TROUBLESHOOTER_BINARY="$<TARGET_FILE:plasma-assist-troubleshooter>")
    add_dependencies(${_name} plasma-assist-troubleshooter)
endmacro()

add_plasma_assist_troubleshooter_unittest(troubleshooterservertest.cpp)
add_plasma_assist_troubleshooter_unittest(systemmoduletest.cpp)
add_plasma_assist_troubleshooter_unittest(audiomoduletest.cpp)
add_plasma_assist_troubleshooter_unittest(displaymoduletest.cpp)
add_plasma_assist_troubleshooter_unittest(privacyfiltertest.cpp)
add_plasma_assist_troubleshooter_unittest(networkmoduletest.cpp)
add_plasma_assist_troubleshooter_unittest(powermoduletest.cpp)
add_plasma_assist_troubleshooter_unittest(crashmoduletest.cpp)
add_plasma_assist_troubleshooter_unittest(logmoduletest.cpp)
add_plasma_assist_troubleshooter_unittest(reportmoduletest.cpp)
add_plasma_assist_troubleshooter_unittest(changejournaltest.cpp)
add_plasma_assist_troubleshooter_unittest(actionrunnertest.cpp)
add_plasma_assist_troubleshooter_unittest(audioactionstest.cpp)
add_plasma_assist_troubleshooter_unittest(scenariopromptstest.cpp)
