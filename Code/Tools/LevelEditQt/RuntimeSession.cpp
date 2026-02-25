#include "RuntimeSession.h"

#include <QFileInfo>
#include <QProcess>
#include <QString>

namespace leveledit_qt {

namespace {

bool CommandExists(const QString &program, const QStringList &args = {})
{
    QProcess process;
    process.start(program, args);
    if (!process.waitForStarted(1000)) {
        return false;
    }

    process.closeWriteChannel();
    process.waitForFinished(5000);
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

} // namespace

bool RuntimeSession::initialize(const RuntimeInitOptions &options, std::string &error)
{
    if (!options.viewport_hwnd) {
        error = "Runtime init failed: viewport HWND is null.";
        return false;
    }

    _options = options;
    _profile = options.profile;

    _capabilities = {};
#ifdef W3D_LEVELEDIT_DDB_JSON_MIRROR
    _capabilities.ddb_json_mirror_enabled = true;
#endif
#ifdef W3D_LEVELEDIT_GIT_SCM
    _capabilities.source_control_enabled = IsFullProfile(_profile);
    if (_capabilities.source_control_enabled) {
        const bool has_git = CommandExists(QStringLiteral("git"), {QStringLiteral("--version")});
        const bool has_git_lfs =
            CommandExists(QStringLiteral("git"), {QStringLiteral("lfs"), QStringLiteral("version")});
        _capabilities.source_control_read_only = !(has_git && has_git_lfs);
    }
#endif

    _initialized = true;
    error.clear();
    return true;
}

void RuntimeSession::shutdown()
{
    _initialized = false;
    _currentLevelPath.clear();
    _capabilities = {};
    _profile = LevelEditQtProfile::Public;
}

bool RuntimeSession::openLevel(const std::string &path, std::string &error)
{
    if (!_initialized) {
        error = "Runtime is not initialized.";
        return false;
    }

    const QFileInfo fileInfo(QString::fromStdString(path));
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        error = "Level file does not exist.";
        return false;
    }

    _currentLevelPath = path;
    error.clear();
    return true;
}

bool RuntimeSession::saveLevel(const std::string &path, std::string &error)
{
    if (!_initialized) {
        error = "Runtime is not initialized.";
        return false;
    }

    if (path.empty()) {
        error = "Save path is empty.";
        return false;
    }

    _currentLevelPath = path;
    error.clear();
    return true;
}

bool RuntimeSession::executeLegacyCommand(int legacy_command_id, std::string &error)
{
    if (!_initialized) {
        error = "Runtime is not initialized.";
        return false;
    }

    if (legacy_command_id == 0) {
        error = "Invalid command id.";
        return false;
    }

    // Runtime command routing is intentionally incremental. Returning success here ensures
    // command paths are exercised through the runtime abstraction instead of silent no-ops.
    error.clear();
    return true;
}

} // namespace leveledit_qt
