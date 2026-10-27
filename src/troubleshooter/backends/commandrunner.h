/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <functional>

class QObject;

// Runs a command of a fixed list. Arguments are given as list (no shell)
class CommandRunner
{
public:
    enum class Command : uint8_t {
        Coredumpctl,
        Journalctl,
        // Only to restart services of user session
        Systemctl,
    };
    // \a output is standard output, \a error is empty when command succeeded
    using Callback = std::function<void(const QByteArray &output, const QString &error)>;

    CommandRunner();
    virtual ~CommandRunner();

    // \a callback is not called when \a context is deleted (command is stopped)
    virtual void run(Command command, const QStringList &arguments, QObject *context, const Callback &callback) = 0;

private:
    Q_DISABLE_COPY_MOVE(CommandRunner)
};

// Runs commands with QProcess
class ProcessCommandRunner : public CommandRunner
{
public:
    ProcessCommandRunner();
    ~ProcessCommandRunner() override;

    void run(Command command, const QStringList &arguments, QObject *context, const Callback &callback) override;
    [[nodiscard]] static QString program(Command command);
};
