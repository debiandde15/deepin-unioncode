/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhouyi<zhouyi1@uniontech.com>
 *
 * Maintainer: zhouyi<zhouyi1@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAVENMANAGER_H
#define MAVENMANAGER_H

#include "services/builder/builderglobals.h"
#include "services/builder/task.h"

#include <QObject>

namespace dpfservice {
class WindowService;
}

class MavenManagerPrivate;
class MavenManager : public QObject
{
    Q_OBJECT
public:
    static MavenManager *instance();
    void executeBuildCommand(const QString &program, const QStringList &arguments, const QString &workingDir);
    void buildProject();
    void rebuildProject();
    void cleanProject();

signals:
    void addCompileOutput(const QString &content, OutputFormat format);
    void addProblemOutput(const Task &task, int linkedOutputLines, int skipLines);
    void buildStateChanged(BuildState state, QString originCmds);
    void buildStart();

public slots:

private:
    explicit MavenManager(QObject *parent = nullptr);
    virtual ~MavenManager();

    void processReadyReadStdOutput();
    void processReadyReadStdError();
    bool execCommand(const QString &program, const QStringList &arguments, const QString &workingDir);

    MavenManagerPrivate *const d;
};

#endif // MAVENMANAGER_H
