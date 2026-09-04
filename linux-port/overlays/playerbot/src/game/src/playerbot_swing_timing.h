#ifndef __INC_METIN2_PLAYERBOT_SWING_TIMING_H__
#define __INC_METIN2_PLAYERBOT_SWING_TIMING_H__

// How long a swing actually takes, per class, per weapon, per combo step.
//
// Generated from the client motion data the server itself ships in
// share/data/pc/<class>/<weapon>/combo_NN.msa. The number is
// DirectInputTime: the earliest moment the client will accept the next
// combo input. Sending before it means the client cannot chain, so it
// cuts the swing that is playing - which is why bots never finished a
// strike. The old code used a flat 480 ms for everything, and that is too
// early for every weapon in the game: a two-handed sword wants 932 ms and
// a bow a full second, so those were being cut roughly in half.
//
// Bows have no combo chain - they replay attack.msa - so the value there is
// MotionDuration; the arrow itself leaves at 0.846 of it.
//
// A combo step whose DirectInputTime is zero does not chain at all: it
// ends the sequence and the client plays it whole, so the value there is
// MotionDuration. The bell's fourth combo is one of those.
//
// Milliseconds at attack speed 100. Scale by 100/speed for anything else.
// Regenerate with tools/generate_swing_timing.py when the client data
// changes; do not hand-edit.

namespace
{
	// [job 0..3][weapon subtype 0..5][combo step 0..3]
	const DWORD PLAYERBOT_SWING_MS[4][6][4] = {
		{ // warrior
			{  533,  543,  418, 1058 },    // onehand_sword
			{  932,  846,  879, 1555 },    // dualhand_sword
			{ 1000, 1000, 1000, 1000 },    // bow
			{  932,  846,  879, 1555 },    // twohand_sword
			{  608,  533,  415, 1333 },    // bell
			{  749,  487,  515, 1167 },    // fan
		},
		{ // assassin
			{  592,  509,  488, 1267 },    // onehand_sword
			{  732,  636,  636,  667 },    // dualhand_sword
			{ 1000, 1000, 1000, 1000 },    // bow
			{  732,  636,  636,  667 },    // twohand_sword
			{  608,  533,  415, 1333 },    // bell
			{  749,  487,  515, 1167 },    // fan
		},
		{ // sura
			{  667,  541,  479, 1167 },    // onehand_sword
			{  932,  846,  879, 1555 },    // dualhand_sword
			{ 1000, 1000, 1000, 1000 },    // bow
			{  932,  846,  879, 1555 },    // twohand_sword
			{  608,  533,  415, 1333 },    // bell
			{  749,  487,  515, 1167 },    // fan
		},
		{ // shaman
			{ 0, 0, 0, 0 },        // onehand_sword: brak danych
			{  932,  846,  879, 1555 },    // dualhand_sword
			{ 1000, 1000, 1000, 1000 },    // bow
			{  932,  846,  879, 1555 },    // twohand_sword
			{  608,  533,  415, 1333 },    // bell
			{  749,  487,  515, 1167 },    // fan
		},
	};

	// A class that has no motion for the weapon it is holding - a Shaman with
	// a one-handed sword, which the client has no animation set for - still
	// has to swing at some rhythm. One second is the plain MotionDuration
	// shared by most of the on-foot combos.
	const DWORD PLAYERBOT_SWING_MS_FALLBACK = 1000;
}

#endif
