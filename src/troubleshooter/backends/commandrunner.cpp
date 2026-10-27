/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "commandrunner.h"
#include "plasmaassist_troubleshooter_debug.h"
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace
{
// Commands which don't answer are stopped
constexpr auto commandTimeout = 30s;
// Output is limited to avoid using too much memory
constexpr qsizetype maximumOutputSize = 4 * 1024 * 1024;
}

CommandRunner::CommandRunner() = default;

CommandRunner::~CommandRunner() = default;

ProcessCommandRunner::ProcessCommandRunner() = default;

ProcessCommandRunner::~ProcessCommandRunner() = default;

QString ProcessCommandRunner::program(Command command)
{
    switch (command) {
    case Command::Coredumpctl:
        return u"coredumpctl"_s;
    case Command::Journalctl:
        return u"journalctl"_s;
    }
    return {};
}

void ProcessCommandRunner::run(Command command, const QStringList &arguments, QObject *context, const Callback &callback)
{
    const QString executable = QStandardPaths::findExecutable(program(command));
    if (executable.isEmpty()) {
        callback({}, u"%1 is not installed"_s.arg(program(command)));
        return;
    }
    // Process is deleted with context: command is stopped when request is cancelled
    auto process = new QProcess(context);
    auto timer = new QTimer(process);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, process, [process]() {
        qCWarning(PLASMA_ASSIST_TROUBLESHOOTER_LOG) << "Command does not answer, stop it:" << process->program();
        process->kill();
    });
    auto output = std::make_shared<QByteArray>();
    QObject::connect(process, &QProcess::readyReadStandardOutput, process, [process, output]() {
        const QByteArray data = process->readAllStandardOutput();
        if (output->size() < maximumOutputSize) {
            output->append(data.left(maximumOutputSize - output->size()));
        }
    });
    QObject::connect(process, &QProcess::finished, context, [process, output, callback](int exitCode, QProcess::ExitStatus exitStatus) {
        process->deleteLater();
        output->append(process->readAllStandardOutput());
        if (exitStatus != QProcess::NormalExit) {
            callback(*output, u"%1 crashed or was stopped"_s.arg(process->program()));
        } else if (exitCode != 0 && output->isEmpty()) {
            callback(*output, QString::fromLocal8Bit(process->readAllStandardError()).trimmed());
        } else {
            // coredumpctl exits with 1 when there is no crash
            callback(*output, {});
        }
    });
    QObject::connect(process, &QProcess::errorOccurred, context, [process, callback](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            callback({}, process->errorString());
        }
    });
    // Language of output must not depend on user
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(u"LC_ALL"_s, u"C"_s);
    environment.insert(u"SYSTEMD_COLORS"_s, u"0"_s);
    process->setProcessEnvironment(environment);
    // Standard error is not mixed with output: it's only used for errors
    process->setProgram(executable);
    process->setArguments(arguments);
    timer->start(commandTimeout);
    process->start(QIODevice::ReadOnly);
}
