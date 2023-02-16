// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "studioquickwidget.h"

#include <QVBoxLayout>
#include <QtQml/QQmlEngine>

QQmlEngine *s_engine = nullptr;

StudioQuickWidget::StudioQuickWidget(QWidget *parent)
    : QWidget{parent}
{
    if (!s_engine)
        s_engine = new QQmlEngine;

    m_quickWidget = new QQuickWidget(s_engine, this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_quickWidget);
}

QQmlEngine *StudioQuickWidget::engine() const
{
    return m_quickWidget->engine();
}

QQmlContext *StudioQuickWidget::rootContext() const
{
    return m_quickWidget->rootContext();
}

QQuickItem *StudioQuickWidget::rootObject() const
{
    return m_quickWidget->rootObject();
}

void StudioQuickWidget::setResizeMode(QQuickWidget::ResizeMode mode)
{
    m_quickWidget->setResizeMode(mode);
}

void StudioQuickWidget::setSource(const QUrl &url)
{
    m_quickWidget->setSource(url);
}

void StudioQuickWidget::refresh() {}

void StudioQuickWidget::setClearColor(const QColor &color)
{
    m_quickWidget->setClearColor(color);
}

QList<QQmlError> StudioQuickWidget::errors() const
{
    return m_quickWidget->errors();
}
