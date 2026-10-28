/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QPoint>
#include <QSize>
#include <QString>
#include <functional>

class QObject;

// Snapshot of display configuration: modules don't depend on KScreen (tests use fake outputs)
struct DisplayMode {
    QString id;
    QSize size;
    double refreshRate = 0;
};

struct DisplayOutput {
    int id = 0;
    QString name; // connector: HDMI-A-1, eDP-1…
    QString type; // Panel, HDMI, DisplayPort…
    QString vendor;
    QString model;
    QList<DisplayMode> modes;
    QString currentModeId;
    QString preferredModeId;
    QPoint position;
    QString rotation; // none, left, inverted, right
    double scale = 1.0;
    uint32_t priority = 0; // 1 is primary
    bool connected = false;
    bool enabled = false;
};

// Settings changed by set_output_config
struct DisplayOutputSettings {
    bool enabled = false;
    // Ignored when output is disabled
    QString modeId;
    double scale = 1.0;
    QPoint position;
};

class DisplayBackend
{
public:
    // Error is empty on success
    using OutputsCallback = std::function<void(const QList<DisplayOutput> &outputs, const QString &error)>;
    using ErrorCallback = std::function<void(const QString &error)>;

    DisplayBackend();
    virtual ~DisplayBackend();

    // Asynchronous. Callback is not called when \a context is deleted
    virtual void fetchOutputs(QObject *context, OutputsCallback callback) = 0;
    // Applies \a settings to output \a name (connector). Callback is not called when \a context is deleted
    virtual void applyOutputSettings(const QString &name, const DisplayOutputSettings &settings, QObject *context, ErrorCallback callback) = 0;

private:
    Q_DISABLE_COPY_MOVE(DisplayBackend)
};
