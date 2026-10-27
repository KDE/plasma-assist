# SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

macro(add_plasma_assist_daemon_unittest _source)
    get_filename_component(_name ${_source} NAME_WE)
    ecm_add_test(${_source} ${_name}.h TEST_NAME ${_name}
        NAME_PREFIX "plasma-assist-daemon-"
        LINK_LIBRARIES Qt::Test plasmaassistdaemon
    )
    # Tests call troubleshooter built next to them
    add_dependencies(${_name} plasma-assist-troubleshooter)
endmacro()

add_plasma_assist_daemon_unittest(assistanttest.cpp)
add_plasma_assist_daemon_unittest(confirmationmanagertest.cpp)
