/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     luzhen<luzhen@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *             luzhen<luzhen@uniontech.com>
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
#include "breakpointmodel.h"

#include <QStyledItemDelegate>

BreakpointModel::BreakpointModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    setObjectName("BreakpointModel");
}

BreakpointModel::~BreakpointModel() = default;

void BreakpointModel::setBreakpoints(const Internal::Breakpoints &breakpoints, bool canExpand)
{
    beginResetModel();
    contentsValid = true;
    this->canExpand = canExpand;
    bps.clear();
    for (auto it : breakpoints) {
        bps.push_back(BreakpointItem(it));
    }
    if (bps.size() >= 0)
        setCurrentIndex(0);
    else
        currentIndex = -1;
    endResetModel();
    emit breakpointChanged();
}

void BreakpointModel::insertBreakpoint(const Internal::Breakpoint &breakpoint)
{
    beginResetModel();
    contentsValid = true;
    bps.push_back(BreakpointItem(breakpoint));
    endResetModel();
    emit breakpointChanged();
}

void BreakpointModel::removeBreakpoint(const Internal::Breakpoint &breakpoint)
{
    beginResetModel();
    auto it = bps.begin();
    for (; it != bps.end();) {
        if (it->breakpoint() == breakpoint) {
            it = bps.erase(it);
        } else {
            ++it;
        }
    }
    endResetModel();
    emit breakpointChanged();
}

void BreakpointModel::setCurrentIndex(int level)
{
    if (level == -1 || level == currentIndex)
        return;

    // Emit changed for previous frame
    QModelIndex i = index(currentIndex, 0);
    emit dataChanged(i, i);

    currentIndex = level;
    emit currentIndexChanged();

    // Emit changed for new frame
    i = index(currentIndex, 0);
    emit dataChanged(i, i);
}

void BreakpointModel::removeAll()
{
    beginResetModel();
    bps.clear();
    setCurrentIndex(-1);
    endResetModel();
}

Internal::Breakpoint BreakpointModel::currentBreakpoint() const
{
    if (currentIndex == -1)
        return Internal::Breakpoint();

    return bps.at(currentIndex).breakpoint();
}

int BreakpointModel::rowCount(const QModelIndex &parent) const
{
    // Since the stack is not a tree, row count is 0 for any valid parent
    return parent.isValid() ? 0 : (bps.size() + canExpand);
}

int BreakpointModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : kStackColumnCount;
}

QVariant BreakpointModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= bps.size() + canExpand)
        return QVariant();

    if (index.row() == bps.size()) {
        if (role == Qt::DisplayRole && index.column() == kIndexColumn)
            return tr("...");
        if (role == Qt::DisplayRole && index.column() == kFunctionNameColumn)
            return tr("<More>");
        if (role == Qt::DecorationRole && index.column() == kIndexColumn)
            return "";
        return QVariant();
    }

    const BreakpointItem &bp = bps.at(index.row());
    return bp.data(index.row(), index.column(), role);
}

QVariant BreakpointModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if (orient == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case kIndexColumn:
            return tr("Index");
        case kFunctionNameColumn:
            return tr("Function");
        case kFileNameColumn:
            return tr("File");
        case kLineNumberColumn:
            return tr("Line");
        case kAddressColumn:
            return tr("Address");
        };
    }
    return QVariant();
}

bool BreakpointModel::setData(const QModelIndex &idx, const QVariant &data, int role)
{
    Q_UNUSED(data)
    if (role == ItemActivatedRole || role == ItemClickedRole) {
        setCurrentIndex(idx.row());
        return true;
    }

    return false;
}