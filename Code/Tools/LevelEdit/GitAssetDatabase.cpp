/*
** Command & Conquer Renegade(tm)
** Copyright 2025 Electronic Arts Inc.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
*/

#include "StdAfx.h"
#include "GitAssetDatabase.h"

#include "rawfile.h"
#include "wwstring.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>

namespace {

std::string ToUtf8(LPCTSTR text)
{
    return text ? std::string(text) : std::string();
}

std::string EscapeShellArg(std::string value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);

    for (const char ch : value) {
        if (ch == '\\' || ch == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }

    return escaped;
}

bool ExecuteCapture(const std::string &command, std::string *output)
{
#if defined(_WIN32)
    FILE *pipe = _popen(command.c_str(), "r");
#else
    FILE *pipe = popen(command.c_str(), "r");
#endif

    if (!pipe) {
        if (output) {
            output->clear();
        }
        return false;
    }

    std::string data;
    char buffer[1024] = {0};
    while (fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr) {
        data += buffer;
    }

#if defined(_WIN32)
    const int exit_code = _pclose(pipe);
#else
    const int exit_code = pclose(pipe);
#endif

    if (output) {
        *output = data;
    }

    return exit_code == 0;
}

void TrimTrailingWhitespace(std::string &text)
{
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ' || text.back() == '\t')) {
        text.pop_back();
    }
}

} // namespace

GitAssetDatabaseClass::GitAssetDatabaseClass()
    : m_Initialized(false)
    , m_ReadOnly(true)
    , m_HasGit(false)
    , m_HasGitLfs(false)
    , m_RequireLocks(true)
{
}

GitAssetDatabaseClass::~GitAssetDatabaseClass() = default;

bool GitAssetDatabaseClass::Open_Database(LPCTSTR ini_filename, LPCTSTR /*username*/, LPCTSTR /*password*/)
{
    m_RepoRoot = ToUtf8(ini_filename);
    if (m_RepoRoot.empty()) {
        m_RepoRoot = ".";
    }

    m_HasGit = RunGit("--version");
    if (!m_HasGit) {
        m_ReadOnly = true;
        m_Initialized = false;
        return false;
    }

    if (!RunGit("rev-parse --is-inside-work-tree")) {
        m_ReadOnly = true;
        m_Initialized = false;
        return false;
    }

    std::string user_name;
    if (RunGit("config user.name", &user_name)) {
        TrimTrailingWhitespace(user_name);
        m_UserName = user_name;
    }

    m_HasGitLfs = RunGit("lfs version");

    const char *require_locks_env = std::getenv("W3D_LEVELEDIT_GIT_LOCKS");
    if (require_locks_env != nullptr) {
        const std::string value(require_locks_env);
        m_RequireLocks = !(value == "0" || value == "false" || value == "FALSE");
    }

    m_ReadOnly = (m_RequireLocks && !m_HasGitLfs);
    m_Initialized = true;
    return true;
}

bool GitAssetDatabaseClass::RunGit(const char *args, std::string *output) const
{
    if (!args || !*args) {
        return false;
    }

    const std::string command =
        "git -C \"" + EscapeShellArg(m_RepoRoot) + "\" " + args + " 2>&1";
    return ExecuteCapture(command, output);
}

bool GitAssetDatabaseClass::RunGitForFile(const char *args_prefix, LPCTSTR local_filename, std::string *output) const
{
    if (!args_prefix || !local_filename) {
        return false;
    }

    std::string args(args_prefix);
    args += " \"";
    args += EscapeShellArg(ToUtf8(local_filename));
    args += "\"";
    return RunGit(args.c_str(), output);
}

bool GitAssetDatabaseClass::Add_File(LPCTSTR local_filename, LPCTSTR comment)
{
    if (!local_filename || m_ReadOnly) {
        return false;
    }

    if (!RunGitForFile("add --", local_filename)) {
        return false;
    }

    if (comment && comment[0] != 0) {
        return CommitSingleFile(local_filename, comment);
    }

    return true;
}

bool GitAssetDatabaseClass::CommitSingleFile(LPCTSTR local_filename, LPCTSTR comment)
{
    if (!local_filename || m_ReadOnly) {
        return false;
    }

    std::string message = ToUtf8(comment);
    if (message.empty()) {
        message = "LevelEdit check-in";
    }

    std::string args = "commit -m \"" + EscapeShellArg(message) + "\" -- \"" +
                       EscapeShellArg(ToUtf8(local_filename)) + "\"";
    return RunGit(args.c_str());
}

bool GitAssetDatabaseClass::LockFile(LPCTSTR local_filename, bool force_unlock)
{
    if (!local_filename || !m_RequireLocks || !m_HasGitLfs) {
        return true;
    }

    if (force_unlock) {
        std::string args = "lfs unlock --force \"" + EscapeShellArg(ToUtf8(local_filename)) + "\"";
        return RunGit(args.c_str());
    }

    std::string args = "lfs lock \"" + EscapeShellArg(ToUtf8(local_filename)) + "\"";
    return RunGit(args.c_str());
}

bool GitAssetDatabaseClass::Check_In(LPCTSTR local_filename, LPCTSTR comment)
{
    if (!local_filename || m_ReadOnly) {
        return false;
    }

    if (!RunGitForFile("add --", local_filename)) {
        return false;
    }

    if (!CommitSingleFile(local_filename, comment)) {
        return false;
    }

    if (!LockFile(local_filename, true)) {
        return false;
    }

    return true;
}

bool GitAssetDatabaseClass::Check_Out(LPCTSTR local_filename, bool get_locally)
{
    if (!local_filename || m_ReadOnly) {
        return false;
    }

    if (get_locally) {
        Get(local_filename);
    }

    return LockFile(local_filename, false);
}

bool GitAssetDatabaseClass::Undo_Check_Out(LPCTSTR local_filename)
{
    if (!local_filename || m_ReadOnly) {
        return false;
    }

    return LockFile(local_filename, true);
}

bool GitAssetDatabaseClass::Get(LPCTSTR local_filename)
{
    if (!local_filename) {
        return false;
    }

    return RunGitForFile("checkout --", local_filename);
}

bool GitAssetDatabaseClass::Get_Subproject(LPCTSTR local_filename)
{
    if (!local_filename) {
        return false;
    }

    return RunGitForFile("checkout --", local_filename);
}

bool GitAssetDatabaseClass::Get_All(LPCTSTR /*dest_path*/, LPCTSTR /*search_mask*/)
{
    return RunGit("pull --ff-only");
}

FileClass *GitAssetDatabaseClass::Get_File(LPCTSTR local_filename)
{
    RawFileClass *file_obj = new RawFileClass;
    file_obj->Set_Name(local_filename);
    return file_obj;
}

bool GitAssetDatabaseClass::Check_Out_Ex(LPCTSTR local_filename, HWND /*parent_wnd*/)
{
    return Check_Out(local_filename, true);
}

bool GitAssetDatabaseClass::Check_In_Ex(LPCTSTR local_filename, HWND /*parent_wnd*/)
{
    return Check_In(local_filename, nullptr);
}

bool GitAssetDatabaseClass::Retry_Check_Out(LPCTSTR local_filename, int attempts, int delay)
{
    for (int i = 0; i < std::max(1, attempts); ++i) {
        if (Check_Out(local_filename, true)) {
            return true;
        }

        if (i + 1 < attempts) {
            std::this_thread::sleep_for(std::chrono::milliseconds(std::max(0, delay)));
        }
    }

    return false;
}

bool GitAssetDatabaseClass::Retry_Check_In(LPCTSTR local_filename, int attempts, int delay)
{
    for (int i = 0; i < std::max(1, attempts); ++i) {
        if (Check_In(local_filename, nullptr)) {
            return true;
        }

        if (i + 1 < attempts) {
            std::this_thread::sleep_for(std::chrono::milliseconds(std::max(0, delay)));
        }
    }

    return false;
}

AssetDatabaseClass::FILE_STATUS GitAssetDatabaseClass::Get_File_Status(
    LPCTSTR local_filename,
    StringClass *checked_out_user_name)
{
    if (checked_out_user_name != nullptr) {
        (*checked_out_user_name) = "";
    }

    if (!local_filename || m_ReadOnly) {
        return NOT_CHECKED_OUT;
    }

    if (!m_RequireLocks || !m_HasGitLfs) {
        return CHECKED_OUT_TO_ME;
    }

    std::string output;
    std::string args = "lfs locks --path \"" + EscapeShellArg(ToUtf8(local_filename)) + "\"";
    if (!RunGit(args.c_str(), &output)) {
        return NOT_CHECKED_OUT;
    }

    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find(ToUtf8(local_filename)) == std::string::npos) {
            continue;
        }

        if (!m_UserName.empty() && line.find(m_UserName) != std::string::npos) {
            if (checked_out_user_name != nullptr) {
                (*checked_out_user_name) = m_UserName.c_str();
            }
            return CHECKED_OUT_TO_ME;
        }

        if (checked_out_user_name != nullptr) {
            (*checked_out_user_name) = line.c_str();
        }
        return CHECKED_OUT;
    }

    return NOT_CHECKED_OUT;
}

bool GitAssetDatabaseClass::Is_File_Different(LPCTSTR local_filename)
{
    if (!local_filename) {
        return false;
    }

    std::string output;
    if (!RunGitForFile("status --porcelain --", local_filename, &output)) {
        return false;
    }

    return !output.empty();
}

bool GitAssetDatabaseClass::Does_File_Exist(LPCTSTR local_filename)
{
    if (!local_filename) {
        return false;
    }

#if defined(_WIN32)
    return ::GetFileAttributes(local_filename) != 0xFFFFFFFF;
#else
    FILE *file = std::fopen(local_filename, "rb");
    if (!file) {
        return false;
    }
    std::fclose(file);
    return true;
#endif
}

