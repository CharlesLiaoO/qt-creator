/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

import Qt 4.6

// the style used the items view

Item {
    property string backgroundColor: "#4f4f4f"
    property string raisedBackgroundColor: "#e0e0e0"

    property string scrollbarBorderColor: "#8F8F8F"
    property string scrollbarGradientStartColor: "#7E7E7E"
    property string scrollbarGradientEndColor: "#C6C6C6"
    property int scrollbarClickScrollAmount: 40

    property string itemNameTextColor: "#FFFFFF"

    property string gridLineLighter: "#5f5f5f"
    property string gridLineDarker: "#3f3f3f"

    property string sectionArrowColor: "#aeaeae"
    property string sectionTitleTextColor: "#f0f0f0"
    property string sectionTitleBackgroundColor: "#909090"

    property int sectionTitleHeight: 20
    property int sectionTitleSpacing: 2

    property int iconWidth: 32
    property int iconHeight: 32

    property int textWidth: 95
    property int textHeight: 15

    property int cellHorizontalMargin: 5
    property int cellVerticalSpacing: 7
    property int cellVerticalMargin: 10

    // the following depend on the actual shape of the item delegate
    property int cellWidth: textWidth + 2 * cellHorizontalMargin
    property int cellHeight: itemLibraryIconHeight + textHeight +
    2 * cellVerticalMargin + cellVerticalSpacing
}

