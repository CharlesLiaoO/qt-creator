// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "propertycomponentgenerator.h"

#include <nodemetainfo.h>

namespace QmlDesigner {

#ifdef UNIT_TESTS
using PropertyComponentGeneratorType = PropertyComponentGeneratorInterface;
#else
using PropertyComponentGeneratorType = PropertyComponentGenerator;
#endif
class PropertyEditorTemplateGenerator
{

public:
    PropertyEditorTemplateGenerator(const PropertyComponentGeneratorType &propertyGenerator);

    QString create(const NodeMetaInfo &nodeInfo, bool isComponent);

private:
    const PropertyComponentGeneratorType &m_propertyGenerator;
};

} // namespace QmlDesigner
