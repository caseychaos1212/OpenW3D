/*
** Copyright 2026 OpenW3D Contributors.
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#ifndef COOPNETUPDATETIMER_H
#define COOPNETUPDATETIMER_H

#include <cstdint>

// Preserve fractional intervals across integer-millisecond frame timestamps.
// Resetting the deadline to now on each send loses updates at 60/120 Hz.
class CoopNetUpdateTimer {
public:
	bool Is_Due(uint32_t now, int rate, bool urgent = false)
	{
		if (rate <= 0) return false;
		if (urgent || rate != Rate) {
			Rate = rate;
			LastTime = now;
			Credit = 0;
			return true;
		}
		const uint32_t elapsed = now - LastTime; // Also handles clock wrap.
		LastTime = now;
		if (elapsed == 0) return false;
		Credit += uint64_t(elapsed) * rate;
		if (Credit < 1000) return false;
		Credit -= 1000;
		// Retain at most one interval after a stall; never burst to catch up.
		if (Credit > 1000) Credit = 1000;
		return true;
	}

private:
	uint32_t LastTime = 0;
	uint64_t Credit = 0;
	int Rate = 0;
};

#endif
