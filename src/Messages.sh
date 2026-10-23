#! /usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

# applet has its own translation domain (see applet/Messages.sh)
$XGETTEXT $(find . -path ./applet -prune -o \( -name "*.cpp" -o -name "*.h" \) -print) -o "$podir"/plasma_assist.pot
