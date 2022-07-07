/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     luzhen<luzhen@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *             luzhen<huangyub@uniontech.com>
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
#include "cmakeparser.h"

#include "services/builder/fileutils.h"
#include "common/util/qtcassert.h"


const char COMMON_ERROR_PATTERN[] = "^CMake Error at (.*):([0-9]*)( \\((.*)\\))?:";
const char NEXT_SUBERROR_PATTERN[] = "^CMake Error in (.*):";
const char LOCATION_LINE_PATTERN[] = ":(\\d+):(?:(\\d+))?$";

const char TASK_CATEGORY_BUILDSYSTEM[] = "Task.Category.Buildsystem";

CMakeParser::CMakeParser()
{
    commonError.setPattern(QLatin1String(COMMON_ERROR_PATTERN));
    commonError.setMinimal(true);
    QTC_CHECK(commonError.isValid());

    nextSubError.setPattern(QLatin1String(NEXT_SUBERROR_PATTERN));
    nextSubError.setMinimal(true);
    QTC_CHECK(nextSubError.isValid());

    locationLine.setPattern(QLatin1String(LOCATION_LINE_PATTERN));
    QTC_CHECK(locationLine.isValid());
}

void CMakeParser::stdError(const QString &line)
{
    QString trimmedLine = rightTrimmed(line);

    switch (expectTripleLineErrorData) {
    case NONE:
        if (trimmedLine.isEmpty() && !lastTask.isNull()) {
            if (skippedFirstEmptyLine)
                doFlush();
            else
                skippedFirstEmptyLine = true;
            return;
        }
        if (skippedFirstEmptyLine)
            skippedFirstEmptyLine = false;

        if (commonError.indexIn(trimmedLine) != -1) {
            lastTask = Task(Task::Error, QString(), Utils::FileName::fromUserInput(commonError.cap(1)),
                              commonError.cap(2).toInt(), TASK_CATEGORY_BUILDSYSTEM);
            lines = 1;
            return;
        } else if (nextSubError.indexIn(trimmedLine) != -1) {
            lastTask = Task(Task::Error, QString(), Utils::FileName::fromUserInput(nextSubError.cap(1)), -1,
                              TASK_CATEGORY_BUILDSYSTEM);
            lines = 1;
            return;
        } else if (trimmedLine.startsWith(QLatin1String("  ")) && !lastTask.isNull()) {
            if (!lastTask.description.isEmpty())
                lastTask.description.append(QLatin1Char(' '));
            lastTask.description.append(trimmedLine.trimmed());
            ++lines;
            return;
        } else if (trimmedLine.endsWith(QLatin1String("in cmake code at"))) {
            expectTripleLineErrorData = LINE_LOCATION;
            doFlush();
            lastTask = Task(trimmedLine.contains(QLatin1String("Error")) ? Task::Error : Task::Warning,
                              QString(), Utils::FileName(), -1, TASK_CATEGORY_BUILDSYSTEM);
            return;
        } else if (trimmedLine.startsWith("CMake Error: ")) {
            lastTask = Task(Task::Error, trimmedLine.mid(13),
                              Utils::FileName(), -1, TASK_CATEGORY_BUILDSYSTEM);
            lines = 1;
            return;
        }
        IOutputParser::stdError(line);
        return;
    case LINE_LOCATION:
        {
            QRegularExpressionMatch m = locationLine.match(trimmedLine);
            QTC_CHECK(m.hasMatch());
            lastTask.file = Utils::FileName::fromUserInput(trimmedLine.mid(0, m.capturedStart()));
            lastTask.line = m.captured(1).toInt();
            expectTripleLineErrorData = LINE_DESCRIPTION;
        }
        return;
    case LINE_DESCRIPTION:
        lastTask.description = trimmedLine;
        if (trimmedLine.endsWith(QLatin1Char('\"')))
            expectTripleLineErrorData = LINE_DESCRIPTION2;
        else {
            expectTripleLineErrorData = NONE;
            doFlush();
        }
        return;
    case LINE_DESCRIPTION2:
        lastTask.description.append(QLatin1Char('\n'));
        lastTask.description.append(trimmedLine);
        expectTripleLineErrorData = NONE;
        doFlush();
        return;
    }
}

void CMakeParser::doFlush()
{
    if (lastTask.isNull())
        return;
    Task t = lastTask;
    lastTask.clear();
    emit addTask(t, lines, 1);
    lines = 0;
}