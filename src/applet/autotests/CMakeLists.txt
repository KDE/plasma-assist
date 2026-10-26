# SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

macro(add_plasma_assist_applet_unittest _source)
    get_filename_component(_name ${_source} NAME_WE)
    ecm_add_test(${_source} ${_name}.h TEST_NAME ${_name}
        NAME_PREFIX "plasma-assist-applet-"
        LINK_LIBRARIES Qt::Test plasmaassistappletmodels
    )
endmacro()

add_plasma_assist_applet_unittest(conversationmodeltest.cpp)
add_plasma_assist_applet_unittest(scenariomodeltest.cpp)
