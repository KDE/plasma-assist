#! /usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

$XGETTEXT $(find . -name "*.cpp" -o -name "*.h" -o -name "*.qml") -o "$podir"/plasma_applet_org.kde.plasma.assist.pot
