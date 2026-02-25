/*
** Command & Conquer Renegade(tm)
** Copyright 2025 Electronic Arts Inc.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
*/

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __GIT_ASSET_DATABASE_H
#define __GIT_ASSET_DATABASE_H

#include "assetdatabase.h"

#include <string>

class GitAssetDatabaseClass final : public AssetDatabaseClass
{
public:
    GitAssetDatabaseClass();
    ~GitAssetDatabaseClass() override;

    bool Open_Database(LPCTSTR ini_filename, LPCTSTR username = NULL, LPCTSTR password = NULL) override;

    bool Add_File(LPCTSTR local_filename, LPCTSTR comment = NULL) override;
    bool Check_In(LPCTSTR local_filename, LPCTSTR comment = NULL) override;
    bool Check_Out(LPCTSTR local_filename, bool get_locally = true) override;
    bool Undo_Check_Out(LPCTSTR local_filename) override;
    bool Get(LPCTSTR local_filename) override;
    bool Get_Subproject(LPCTSTR local_filename) override;
    bool Get_All(LPCTSTR dest_path, LPCTSTR search_mask) override;

    FileClass *Get_File(LPCTSTR local_filename) override;

    bool Check_Out_Ex(LPCTSTR local_filename, HWND parent_wnd) override;
    bool Check_In_Ex(LPCTSTR local_filename, HWND parent_wnd) override;

    bool Retry_Check_Out(LPCTSTR local_filename, int attempts = 1, int delay = 250) override;
    bool Retry_Check_In(LPCTSTR local_filename, int attempts = 1, int delay = 250) override;

    FILE_STATUS Get_File_Status(LPCTSTR local_filename, StringClass *checked_out_user_name = NULL) override;
    bool Is_File_Different(LPCTSTR local_filename) override;
    bool Does_File_Exist(LPCTSTR local_filename) override;

    bool Is_Read_Only(void) const override { return m_ReadOnly; }

private:
    bool RunGit(const char *args, std::string *output = nullptr) const;
    bool RunGitForFile(const char *args_prefix, LPCTSTR local_filename, std::string *output = nullptr) const;
    bool CommitSingleFile(LPCTSTR local_filename, LPCTSTR comment);
    bool LockFile(LPCTSTR local_filename, bool force_unlock);

    std::string m_RepoRoot;
    std::string m_UserName;
    bool m_Initialized;
    bool m_ReadOnly;
    bool m_HasGit;
    bool m_HasGitLfs;
    bool m_RequireLocks;
};

#endif // __GIT_ASSET_DATABASE_H
