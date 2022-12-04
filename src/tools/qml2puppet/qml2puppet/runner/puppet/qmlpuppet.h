// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "../qmlbase.h"

class IconRenderer;
class QmlPuppet : public QmlBase
{
    using QmlBase::QmlBase;

private:
    void initCoreApp() override;
    void populateParser() override;
    int startTestMode() override;
    void initQmlRunner() override;

private:
    QSharedPointer<IconRenderer> m_iconRenderer;
};
