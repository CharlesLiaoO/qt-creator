// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
#pragma once

#include <QString>

namespace QmlDesigner {

class ImageUtils
{
public:
    ImageUtils();

    static QString imageInfo(const QString &path, bool sizeInfo = true);
};

} // namespace QmlDesigner
