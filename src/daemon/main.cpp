/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "assistant.h"
#include "config-plasma-assist.h"
#include <KAboutData>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineParser>

using namespace Qt::Literals::StringLiterals;
int main(int argc, char *argv[])
{
    // Widgets: configuration dialog of language model
    QApplication app(argc, argv);
    // Daemon has no main window: closing configuration dialog must not quit it
    app.setQuitOnLastWindowClosed(false);

    KLocalizedString::setApplicationDomain("plasma_assist"_ba);

    KAboutData aboutData(u"plasma-assistd"_s,
                         i18n("Plasma Assist Daemon"),
                         QStringLiteral(PLASMA_ASSIST_VERSION),
                         i18n("Daemon which hosts Plasma Assist conversations"),
                         KAboutLicense::GPL_V2,
                         i18n("Copyright © 2026 Laurent Montel"));
    aboutData.addAuthor(i18nc("@info:credit", "Laurent Montel"), i18n("Maintainer"), u"montel@kde.org"_s);
    aboutData.setOrganizationDomain("kde.org"_ba);
    KAboutData::setApplicationData(aboutData);

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    Assistant assistant;
    if (!assistant.registerOnSessionBus()) {
        return 1;
    }
    // Started again by D-Bus activation when needed
    QObject::connect(&assistant, &Assistant::idleTimeout, &app, &QApplication::quit);
    return app.exec();
}
