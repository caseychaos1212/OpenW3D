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
#include "DdbJsonMirror.h"

#include "Preset.h"
#include "PresetMgr.h"
#include "definition.h"
#include "definitionmgr.h"
#include "wwstring.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct DefinitionEntry
{
    uint32 id = 0;
    uint32 class_id = 0;
    std::string name;
};

struct PresetEntry
{
    uint32 id = 0;
    uint32 class_id = 0;
    uint32 parent_id = 0;
    bool temporary = false;
    std::string name;
};

std::string ToUtf8(LPCTSTR text)
{
    return text ? std::string(text) : std::string();
}

std::string EscapeJson(std::string text)
{
    std::string escaped;
    escaped.reserve(text.size() + 8);

    for (unsigned char ch : text) {
        switch (ch) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (ch < 0x20) {
                char buffer[8] = {0};
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned int>(ch));
                escaped += buffer;
            } else {
                escaped.push_back(static_cast<char>(ch));
            }
            break;
        }
    }

    return escaped;
}

void AppendQuoted(std::ostream &out, const std::string &text)
{
    out << '"' << EscapeJson(text) << '"';
}

} // namespace

bool WriteDdbJsonMirror(LPCTSTR ddb_path, LPCTSTR json_path, StringClass *error)
{
    if (error != nullptr) {
        (*error) = "";
    }

    if (ddb_path == NULL || json_path == NULL || ddb_path[0] == 0 || json_path[0] == 0) {
        if (error != nullptr) {
            (*error) = "WriteDdbJsonMirror: invalid source or destination path.";
        }
        return false;
    }

    std::vector<DefinitionEntry> definitions;
    for (DefinitionClass *definition = DefinitionMgrClass::Get_First(); definition != NULL;
         definition = DefinitionMgrClass::Get_Next(definition)) {
        DefinitionEntry entry;
        entry.id = definition->Get_ID();
        entry.class_id = definition->Get_Class_ID();

        const char *name = definition->Get_Name();
        if (name != NULL) {
            entry.name = name;
        }

        definitions.push_back(entry);
    }

    std::sort(definitions.begin(), definitions.end(), [](const DefinitionEntry &lhs, const DefinitionEntry &rhs) {
        if (lhs.id != rhs.id) {
            return lhs.id < rhs.id;
        }

        if (lhs.class_id != rhs.class_id) {
            return lhs.class_id < rhs.class_id;
        }

        return lhs.name < rhs.name;
    });

    std::vector<PresetEntry> presets;
    for (PresetClass *preset = PresetMgrClass::Get_First(); preset != NULL;
         preset = PresetMgrClass::Get_Next(preset)) {
        PresetEntry entry;
        entry.id = preset->Get_ID();
        entry.class_id = preset->Get_Class_ID();
        entry.temporary = preset->Get_IsTemporary();

        if (PresetClass *parent = preset->Get_Parent()) {
            entry.parent_id = parent->Get_ID();
        }

        LPCTSTR name = preset->Get_Name();
        if (name != NULL) {
            entry.name = name;
        }

        presets.push_back(entry);
    }

    std::sort(presets.begin(), presets.end(), [](const PresetEntry &lhs, const PresetEntry &rhs) {
        if (lhs.id != rhs.id) {
            return lhs.id < rhs.id;
        }

        if (lhs.class_id != rhs.class_id) {
            return lhs.class_id < rhs.class_id;
        }

        return lhs.name < rhs.name;
    });

    std::ofstream out(ToUtf8(json_path).c_str(), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        if (error != nullptr) {
            StringClass message;
            message.Format("WriteDdbJsonMirror: unable to open JSON output path '%s'.", json_path);
            (*error) = message;
        }
        return false;
    }

    out << "{\n";
    out << "  \"schemaVersion\": 1,\n";
    out << "  \"sourceFile\": ";
    AppendQuoted(out, ToUtf8(ddb_path));
    out << ",\n";

    out << "  \"definitions\": [\n";
    for (size_t index = 0; index < definitions.size(); ++index) {
        const DefinitionEntry &entry = definitions[index];
        out << "    {\"id\": " << entry.id << ", \"classId\": " << entry.class_id << ", \"name\": ";
        AppendQuoted(out, entry.name);
        out << "}";
        if (index + 1 < definitions.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ],\n";

    out << "  \"presets\": [\n";
    for (size_t index = 0; index < presets.size(); ++index) {
        const PresetEntry &entry = presets[index];
        out << "    {\"id\": " << entry.id << ", \"classId\": " << entry.class_id
            << ", \"parentId\": " << entry.parent_id << ", \"isTemporary\": "
            << (entry.temporary ? "true" : "false") << ", \"name\": ";
        AppendQuoted(out, entry.name);
        out << "}";
        if (index + 1 < presets.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";

    if (!out.good()) {
        if (error != nullptr) {
            StringClass message;
            message.Format("WriteDdbJsonMirror: failed while writing '%s'.", json_path);
            (*error) = message;
        }
        return false;
    }

    return true;
}
