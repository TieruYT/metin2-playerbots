#ifndef __INC_METIN2_PLAYERBOT_LOG_H__
#define __INC_METIN2_PLAYERBOT_LOG_H__

// Saying something once for three hundred bots.
//
// A message written from the tick is written by every bot on the map, four
// times a second. "This bot is too low for Orc Valley" is a decision working
// exactly as intended, and it had put ninety-one thousand SYSERR lines into one
// core's log - which is not a report, it is a way of making the real errors
// impossible to find.
//
// So: a tag, a minute, and a count. The first occurrence in each window is
// written in full and the rest are counted, and the next line to come through
// says how many were swallowed. Nothing is lost - the fact that it happened
// eleven thousand times is more useful than eleven thousand copies of it, and
// it is one line instead of eleven thousand.
//
// The manager already had this shape once, hand-rolled, for the spawn refusals.
// This is that idea with a name, so the next place that needs it does not grow
// its own.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, early - anything may want to log.

namespace
{
	const DWORD PLAYERBOT_LOG_THROTTLE_WINDOW = 60000;

	struct TPlayerBotLogThrottle
	{
		DWORD dwSuppressed;
		DWORD dwNextTime;
		TPlayerBotLogThrottle() : dwSuppressed(0), dwNextTime(0) {}
	};

	// Keyed by whatever the caller decides one "kind" of message is. Make the
	// tag as specific as the thing worth hearing about separately: the travel
	// refusals are tagged by their reason, so a new reason still speaks up
	// within the minute instead of hiding behind an old one.
	std::map<std::string, TPlayerBotLogThrottle> s_mapPlayerBotLogThrottle;

	void PlayerBotLogThrottledV(bool bError, const char* szTag, DWORD dwNow,
			const char* szFormat, va_list args)
	{
		TPlayerBotLogThrottle& throttle = s_mapPlayerBotLogThrottle[szTag ? szTag : "?"];
		if (throttle.dwNextTime != 0 && dwNow < throttle.dwNextTime)
		{
			++throttle.dwSuppressed;
			return;
		}

		char body[512];
		vsnprintf(body, sizeof(body), szFormat, args);

		const DWORD dwSuppressed = throttle.dwSuppressed;
		throttle.dwSuppressed = 0;
		throttle.dwNextTime = dwNow + PLAYERBOT_LOG_THROTTLE_WINDOW;

		if (dwSuppressed == 0)
		{
			if (bError)
				sys_err("%s", body);
			else
				sys_log(0, "%s", body);
			return;
		}
		if (bError)
			sys_err("%s [+%u more in the last minute]", body, dwSuppressed);
		else
			sys_log(0, "%s [+%u more in the last minute]", body, dwSuppressed);
	}

	// Not an error: a decision, a state, something an operator may want to see
	// but that nothing is wrong about.
	void PlayerBotLogThrottled(const char* szTag, DWORD dwNow, const char* szFormat, ...)
	{
		va_list args;
		va_start(args, szFormat);
		PlayerBotLogThrottledV(false, szTag, dwNow, szFormat, args);
		va_end(args);
	}

	// Genuinely wrong, and still not worth eleven thousand copies of.
	void PlayerBotErrThrottled(const char* szTag, DWORD dwNow, const char* szFormat, ...)
	{
		va_list args;
		va_start(args, szFormat);
		PlayerBotLogThrottledV(true, szTag, dwNow, szFormat, args);
		va_end(args);
	}
}

#endif
