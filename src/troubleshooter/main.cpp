/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "config-plasma-assist.h"
#include "troubleshooterserver.h"
#include <KAboutData>
#include <KLocalizedString>
#include <QCommandLineParser>
#include <QGuiApplication>

using namespace Qt::Literals::StringLiterals;
int main(int argc, char *argv[])
{
    // stdout is used by MCP protocol: never write anything else on it
    // QGuiApplication (not widgets): libkscreen Wayland backend needs it
    QGuiApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("plasma_assist"_ba);

    KAboutData aboutData(u"plasma-assist-troubleshooter"_s,
                         i18n("Plasma Assist Troubleshooter"),
                         QStringLiteral(PLASMA_ASSIST_VERSION),
                         i18n("MCP server to diagnose and fix KDE Plasma desktop"),
                         KAboutLicense::GPL_V2,
                         i18n("Copyright © 2026 Laurent Montel"));
    aboutData.addAuthor(i18nc("@info:credit", "Laurent Montel"), i18n("Maintainer"), u"montel@kde.org"_s);
    aboutData.setOrganizationDomain("kde.org"_ba);
    KAboutData::setApplicationData(aboutData);

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    TroubleshooterServer server;
    QObject::connect(&server, &TroubleshooterServer::runningChanged, &app, [&server](bool running) {
        // Client closed stdin
        if (!running) {
            server.revertPendingChanges([]() {
                QCoreApplication::quit();
            });
        }
    });
    server.start();
    if (!server.isRunning()) {
        return 1;
    }
    return app.exec();
}
