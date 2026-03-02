#ifndef BWGAME_TABLES_H
#define BWGAME_TABLES_H

// Static lookup tables used by the bwgame simulation engine.
//
// These tables are computed offline and encode angle/direction, combat,
// Protoss psi-field geometry, and player-slot constants used throughout
// bwgame.h and its consumers.
//
// Provenance:
//   tan_table               - fixed-point tangent values for 64 angular steps
//   repulse_adjust_table    - collision repulsion deltas (cyclic, 16 entries x 2)
//   cardinal_direction_xy   - unit vectors for the 4 cardinal directions
//   direction_table         - full 256-direction unit vectors in fp8 space
//   hit_near_target_positions - melee/near-target scatter offsets (14 entries)
//   psi_field_mask          - Protoss Pylon psionic field coverage bitmap (5x8)
//   all_player_slots        - ordered list of all 12 player slot indices

#include "util.h"

#include <array>
#include <limits>

namespace bwgame {

static const std::array<unsigned int, 64> tan_table = {
	7, 13, 19, 26, 32, 38, 45, 51, 58, 65, 71, 78, 85, 92,
	99, 107, 114, 122, 129, 137, 146, 154, 163, 172, 181,
	190, 200, 211, 221, 233, 244, 256, 269, 283, 297, 312,
	329, 346, 364, 384, 405, 428, 452, 479, 509, 542, 578,
	619, 664, 716, 775, 844, 926, 1023, 1141, 1287, 1476,
	1726, 2076, 2600, 3471, 5211, 10429, std::numeric_limits<unsigned int>::max()
};

static const std::array<int, 16 * 2> repulse_adjust_table = {
	-5, -5, -5, 5, 5, -5, 5, 5, 5, -5, 5, 5, -5, -5, -5, 5, -5, 5, 5, -5, -5, -5, 5, 5, -5, 5, 5, -5, 5, 5, -5, -5
};

static const std::array<xy, 4> cardinal_direction_xy = {xy{1, 0}, xy{0, 1}, xy{-1, 0}, xy{0, -1}};

static const xy_fp8 direction_table[256] = {
	{0_fp8,-256_fp8},{6_fp8,-256_fp8},{13_fp8,-256_fp8},{19_fp8,-255_fp8},{25_fp8,-255_fp8},{31_fp8,-254_fp8},{38_fp8,-253_fp8},{44_fp8,-252_fp8},
	{50_fp8,-251_fp8},{56_fp8,-250_fp8},{62_fp8,-248_fp8},{68_fp8,-247_fp8},{74_fp8,-245_fp8},{80_fp8,-243_fp8},{86_fp8,-241_fp8},{92_fp8,-239_fp8},
	{98_fp8,-237_fp8},{104_fp8,-234_fp8},{109_fp8,-231_fp8},{115_fp8,-229_fp8},{121_fp8,-226_fp8},{126_fp8,-223_fp8},{132_fp8,-220_fp8},{137_fp8,-216_fp8},
	{142_fp8,-213_fp8},{147_fp8,-209_fp8},{152_fp8,-206_fp8},{157_fp8,-202_fp8},{162_fp8,-198_fp8},{167_fp8,-194_fp8},{172_fp8,-190_fp8},{177_fp8,-185_fp8},
	{181_fp8,-181_fp8},{185_fp8,-177_fp8},{190_fp8,-172_fp8},{194_fp8,-167_fp8},{198_fp8,-162_fp8},{202_fp8,-157_fp8},{206_fp8,-152_fp8},{209_fp8,-147_fp8},
	{213_fp8,-142_fp8},{216_fp8,-137_fp8},{220_fp8,-132_fp8},{223_fp8,-126_fp8},{226_fp8,-121_fp8},{229_fp8,-115_fp8},{231_fp8,-109_fp8},{234_fp8,-104_fp8},
	{237_fp8,-98_fp8},{239_fp8,-92_fp8},{241_fp8,-86_fp8},{243_fp8,-80_fp8},{245_fp8,-74_fp8},{247_fp8,-68_fp8},{248_fp8,-62_fp8},{250_fp8,-56_fp8},
	{251_fp8,-50_fp8},{252_fp8,-44_fp8},{253_fp8,-38_fp8},{254_fp8,-31_fp8},{255_fp8,-25_fp8},{255_fp8,-19_fp8},{256_fp8,-13_fp8},{256_fp8,-6_fp8},
	{256_fp8,0_fp8},{256_fp8,6_fp8},{256_fp8,13_fp8},{255_fp8,19_fp8},{255_fp8,25_fp8},{254_fp8,31_fp8},{253_fp8,38_fp8},{252_fp8,44_fp8},
	{251_fp8,50_fp8},{250_fp8,56_fp8},{248_fp8,62_fp8},{247_fp8,68_fp8},{245_fp8,74_fp8},{243_fp8,80_fp8},{241_fp8,86_fp8},{239_fp8,92_fp8},
	{237_fp8,98_fp8},{234_fp8,104_fp8},{231_fp8,109_fp8},{229_fp8,115_fp8},{226_fp8,121_fp8},{223_fp8,126_fp8},{220_fp8,132_fp8},{216_fp8,137_fp8},
	{213_fp8,142_fp8},{209_fp8,147_fp8},{206_fp8,152_fp8},{202_fp8,157_fp8},{198_fp8,162_fp8},{194_fp8,167_fp8},{190_fp8,172_fp8},{185_fp8,177_fp8},
	{181_fp8,181_fp8},{177_fp8,185_fp8},{172_fp8,190_fp8},{167_fp8,194_fp8},{162_fp8,198_fp8},{157_fp8,202_fp8},{152_fp8,206_fp8},{147_fp8,209_fp8},
	{142_fp8,213_fp8},{137_fp8,216_fp8},{132_fp8,220_fp8},{126_fp8,223_fp8},{121_fp8,226_fp8},{115_fp8,229_fp8},{109_fp8,231_fp8},{104_fp8,234_fp8},
	{98_fp8,237_fp8},{92_fp8,239_fp8},{86_fp8,241_fp8},{80_fp8,243_fp8},{74_fp8,245_fp8},{68_fp8,247_fp8},{62_fp8,248_fp8},{56_fp8,250_fp8},
	{50_fp8,251_fp8},{44_fp8,252_fp8},{38_fp8,253_fp8},{31_fp8,254_fp8},{25_fp8,255_fp8},{19_fp8,255_fp8},{13_fp8,256_fp8},{6_fp8,256_fp8},
	{0_fp8,256_fp8},{-6_fp8,256_fp8},{-13_fp8,256_fp8},{-19_fp8,255_fp8},{-25_fp8,255_fp8},{-31_fp8,254_fp8},{-38_fp8,253_fp8},{-44_fp8,252_fp8},
	{-50_fp8,251_fp8},{-56_fp8,250_fp8},{-62_fp8,248_fp8},{-68_fp8,247_fp8},{-74_fp8,245_fp8},{-80_fp8,243_fp8},{-86_fp8,241_fp8},{-92_fp8,239_fp8},
	{-98_fp8,237_fp8},{-104_fp8,234_fp8},{-109_fp8,231_fp8},{-115_fp8,229_fp8},{-121_fp8,226_fp8},{-126_fp8,223_fp8},{-132_fp8,220_fp8},{-137_fp8,216_fp8},
	{-142_fp8,213_fp8},{-147_fp8,209_fp8},{-152_fp8,206_fp8},{-157_fp8,202_fp8},{-162_fp8,198_fp8},{-167_fp8,194_fp8},{-172_fp8,190_fp8},{-177_fp8,185_fp8},
	{-181_fp8,181_fp8},{-185_fp8,177_fp8},{-190_fp8,172_fp8},{-194_fp8,167_fp8},{-198_fp8,162_fp8},{-202_fp8,157_fp8},{-206_fp8,152_fp8},{-209_fp8,147_fp8},
	{-213_fp8,142_fp8},{-216_fp8,137_fp8},{-220_fp8,132_fp8},{-223_fp8,126_fp8},{-226_fp8,121_fp8},{-229_fp8,115_fp8},{-231_fp8,109_fp8},{-234_fp8,104_fp8},
	{-237_fp8,98_fp8},{-239_fp8,92_fp8},{-241_fp8,86_fp8},{-243_fp8,80_fp8},{-245_fp8,74_fp8},{-247_fp8,68_fp8},{-248_fp8,62_fp8},{-250_fp8,56_fp8},
	{-251_fp8,50_fp8},{-252_fp8,44_fp8},{-253_fp8,38_fp8},{-254_fp8,31_fp8},{-255_fp8,25_fp8},{-255_fp8,19_fp8},{-256_fp8,13_fp8},{-256_fp8,6_fp8},
	{-256_fp8,0_fp8},{-256_fp8,-6_fp8},{-256_fp8,-13_fp8},{-255_fp8,-19_fp8},{-255_fp8,-25_fp8},{-254_fp8,-31_fp8},{-253_fp8,-38_fp8},{-252_fp8,-44_fp8},
	{-251_fp8,-50_fp8},{-250_fp8,-56_fp8},{-248_fp8,-62_fp8},{-247_fp8,-68_fp8},{-245_fp8,-74_fp8},{-243_fp8,-80_fp8},{-241_fp8,-86_fp8},{-239_fp8,-92_fp8},
	{-237_fp8,-98_fp8},{-234_fp8,-104_fp8},{-231_fp8,-109_fp8},{-229_fp8,-115_fp8},{-226_fp8,-121_fp8},{-223_fp8,-126_fp8},{-220_fp8,-132_fp8},{-216_fp8,-137_fp8},
	{-213_fp8,-142_fp8},{-209_fp8,-147_fp8},{-206_fp8,-152_fp8},{-202_fp8,-157_fp8},{-198_fp8,-162_fp8},{-194_fp8,-167_fp8},{-190_fp8,-172_fp8},{-185_fp8,-177_fp8},
	{-181_fp8,-181_fp8},{-177_fp8,-185_fp8},{-172_fp8,-190_fp8},{-167_fp8,-194_fp8},{-162_fp8,-198_fp8},{-157_fp8,-202_fp8},{-152_fp8,-206_fp8},{-147_fp8,-209_fp8},
	{-142_fp8,-213_fp8},{-137_fp8,-216_fp8},{-132_fp8,-220_fp8},{-126_fp8,-223_fp8},{-121_fp8,-226_fp8},{-115_fp8,-229_fp8},{-109_fp8,-231_fp8},{-104_fp8,-234_fp8},
	{-98_fp8,-237_fp8},{-92_fp8,-239_fp8},{-86_fp8,-241_fp8},{-80_fp8,-243_fp8},{-74_fp8,-245_fp8},{-68_fp8,-247_fp8},{-62_fp8,-248_fp8},{-56_fp8,-250_fp8},
	{-50_fp8,-251_fp8},{-44_fp8,-252_fp8},{-38_fp8,-253_fp8},{-31_fp8,-254_fp8},{-25_fp8,-255_fp8},{-19_fp8,-255_fp8},{-13_fp8,-256_fp8},{-6_fp8,-256_fp8}
};

static const xy hit_near_target_positions[14] = {
	{-1, -1}, {-51, -31}, {-7, -12}, {36, 52}, {49, -25}, {-40, 46}, {4, 19}, {-28, -50},
	{-50, 15}, {54, 13}, {-11, 53}, {-15, 9}, {19, -50}, {17, -8}
};

static const bool psi_field_mask[5][8] = {
	{ 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 0 },
	{ 1, 1, 1, 1, 1, 1, 0, 0 },
	{ 1, 1, 1, 0, 0, 0, 0, 0 }
};

static const std::array<int, 12> all_player_slots = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

} // namespace bwgame

#endif // BWGAME_TABLES_H
