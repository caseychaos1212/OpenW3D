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

#ifndef __DDB_JSON_MIRROR_H
#define __DDB_JSON_MIRROR_H

#include "always.h"

class StringClass;

bool WriteDdbJsonMirror(LPCTSTR ddb_path, LPCTSTR json_path, StringClass *error);

#endif // __DDB_JSON_MIRROR_H
