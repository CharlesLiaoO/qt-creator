/****************************************************************************
**
** Copyright (C) 2016 Brian McGillion and Hugues Delorme
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "vcsbase_global.h"

#include <coreplugin/shellcommand.h>

namespace VcsBase {

class VcsOutputWindow;

class VCSBASE_EXPORT VcsCommand : public Core::ShellCommand
{
    Q_OBJECT

public:
    enum VcsRunFlags {
        SshPasswordPrompt = 0x1000, // Disable terminal on UNIX to force graphical prompt.
        ExpectRepoChanges = 0x2000, // Expect changes in repository by the command
    };

    VcsCommand(const Utils::FilePath &defaultWorkingDirectory, const Utils::Environment &environment);

    const Utils::Environment processEnvironment() const override;

    void runCommand(Utils::QtcProcess &process,
                    const Utils::CommandLine &command,
                    const QString &workDirectory = {}) override;

protected:
    void addTask(QFuture<void> &future) override;

private:
    void emitRepositoryChanged(const QString &workingDirectory);

    void coreAboutToClose() override;

    QString m_sshPrompt;
    bool m_preventRepositoryChanged;
};

} // namespace VcsBase
