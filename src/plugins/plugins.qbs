import qbs

Project {
    name: "Plugins"

    references: [
        "android/android.qbs",
        "autotest/autotest.qbs",
        "autotoolsprojectmanager/autotoolsprojectmanager.qbs",
        "axivion/axivion.qbs",
        "baremetal/baremetal.qbs",
        "bazaar/bazaar.qbs",
        "beautifier/beautifier.qbs",
        "bineditor/bineditor.qbs",
        "boot2qt/boot2qt.qbs",
        "clangcodemodel/clangcodemodel.qbs",
        "clangformat/clangformat.qbs",
        "clangtools/clangtools.qbs",
        "classview/classview.qbs",
        "clearcase/clearcase.qbs",
        "cmakeprojectmanager/cmakeprojectmanager.qbs",
        "compilerexplorer/compilerexplorer.qbs",
        "mesonprojectmanager/mesonprojectmanager.qbs",
        "coco/coco.qbs",
        "compilationdatabaseprojectmanager/compilationdatabaseprojectmanager.qbs",
        "conan/conan.qbs",
        "copilot/copilot.qbs",
        "coreplugin/coreplugin.qbs",
        "cpaster/cpaster.qbs",
        "cpaster/frontend/frontend.qbs",
        "cppcheck/cppcheck.qbs",
        "cppeditor/cppeditor.qbs",
        "ctfvisualizer/ctfvisualizer.qbs",
        "cvs/cvs.qbs",
        "debugger/debugger.qbs",
        "debugger/ptracepreload.qbs",
        "designer/designer.qbs",
        "diffeditor/diffeditor.qbs",
        "docker/docker.qbs",
        "extensionmanager/extensionmanager.qbs",
        "fakevim/fakevim.qbs",
        "fossil/fossil.qbs",
        "emacskeys/emacskeys.qbs",
        "genericprojectmanager/genericprojectmanager.qbs",
        "git/git.qbs",
        "gitlab/gitlab.qbs",
        "glsleditor/glsleditor.qbs",
        "haskell/haskell.qbs",
        "helloworld/helloworld.qbs",
        "help/help.qbs",
        "imageviewer/imageviewer.qbs",
        "incredibuild/incredibuild.qbs",
        "ios/ios.qbs",
        "languageclient/languageclient.qbs",
        "lua/lua.qbs",
        "macros/macros.qbs",
        "marketplace/marketplace.qbs",
        "mcusupport/mcusupport.qbs",
        "mercurial/mercurial.qbs",
        "modeleditor/modeleditor.qbs",
        "nim/nim.qbs",
        "perforce/perforce.qbs",
        "perfprofiler/perfprofiler.qbs",
        "projectexplorer/projectexplorer.qbs",
        "qbsprojectmanager/qbsprojectmanager.qbs",
        "python/python.qbs",
        "qmldesigner/qmldesigner.qbs",
        "qmljseditor/qmljseditor.qbs",
        "qmljstools/qmljstools.qbs",
        "qmlpreview/qmlpreview.qbs",
        "qmlprofiler/qmlprofiler.qbs",
        "qmlprojectmanager/qmlprojectmanager.qbs",
        "qnx/qnx.qbs",
        "qmakeprojectmanager/qmakeprojectmanager.qbs",
        "qmldesignerbase/qmldesignerbase.qbs",
        "qtsupport/qtsupport.qbs",
        "remotelinux/remotelinux.qbs",
        "resourceeditor/resourceeditor.qbs",
        "saferenderer/saferenderer.qbs",
        "screenrecorder/screenrecorder.qbs",
        "scxmleditor/scxmleditor.qbs",
        "serialterminal/serialterminal.qbs",
        "silversearcher/silversearcher.qbs",
        "squish/squish.qbs",
        "studiowelcome/studiowelcome.qbs",
        "subversion/subversion.qbs",
        "terminal/terminal.qbs",
        "texteditor/texteditor.qbs",
        "todo/todo.qbs",
        "updateinfo/updateinfo.qbs",
        "valgrind/valgrind.qbs",
        "vcpkg/vcpkg.qbs",
        "vcsbase/vcsbase.qbs",
        "webassembly/webassembly.qbs",
        "welcome/welcome.qbs",
        "qtapplicationmanager/qtapplicationmanager.qbs",
    ].concat(project.additionalPlugins)
}
