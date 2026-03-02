#ifndef BWGAME_STATE_H
#define BWGAME_STATE_H

// State type definitions for the bwgame simulation engine.
//
// Ownership model
// ---------------
//   global_state
//     Immutable game-asset data loaded once at startup (DAT files, iscript,
//     tileset tables, GRP graphics).  Owned by game_player via unique_ptr;
//     exposed to state_functions as a const pointer through
//     state_base_copyable::global.  Must not be mutated during simulation.
//
//   game_state
//     Per-match configuration that is fixed at map-load time: map dimensions,
//     type tables (unit/weapon/upgrade/tech), region graph, triggers.
//     Owned by game_player via unique_ptr; accessible via the mutable pointer
//     state_base_copyable::game.  Should be treated as read-only once a match
//     has started.
//
//   state_base_copyable
//     Snapshottable per-frame simulation state.  Contains all fields whose
//     values must be replicated to reproduce a deterministic frame.  The
//     state_copier utility operates on this portion of state.
//
//   state_base_non_copyable
//     Intrusive-list and object-container state that cannot be bitwise-copied.
//     Holds live unit, bullet, sprite, image, order, path and thingy pools.
//
//   state
//     Full simulation state: inherits both state_base_copyable and
//     state_base_non_copyable.  The single authoritative mutable context
//     advanced each frame by state_functions::next_frame().

#include "util.h"
#include "data_types.h"
#include "game_types.h"
#include "bwenums.h"

#include <array>

namespace bwgame {

// ---------------------------------------------------------------------------
// autocast – implicit conversion helper used for unit-type lookups.
// ---------------------------------------------------------------------------
template<typename T>
struct autocast {
	T val;
	operator T() {
		return val;
	}
	T operator->() {
		return val;
	}
	autocast(T val) : val(val) {}
	template<typename T2, typename std::enable_if<std::is_same<typename std::decay<T2>::type, unit_t>::value>::type* = nullptr>
	autocast(T2* ptr) : val(ptr->unit_type) {}
	template<typename T2>
	autocast(type_id<T2> ptr) : val(ptr) {}
};

using unit_type_autocast = autocast<const unit_type_t*>;

// ---------------------------------------------------------------------------
// global_state – immutable game asset data (loaded once, never mutated).
// ---------------------------------------------------------------------------
struct global_state {

	global_state() = default;
	global_state(global_state&) = delete;
	global_state(global_state&&) = default;
	global_state& operator=(global_state&) = delete;
	global_state& operator=(global_state&&) = default;

	flingy_types_t flingy_types;
	sprite_types_t sprite_types;
	image_types_t image_types;
	order_types_t order_types;
	iscript_t iscript;

	a_vector<grp_t> grps;
	a_vector<grp_t*> image_grp;
	a_vector<a_vector<a_vector<xy>>> lo_offsets;
	a_vector<std::array<a_vector<a_vector<xy>>*, 6>> image_lo_offsets;

	a_vector<uint8_t> units_dat;
	a_vector<uint8_t> weapons_dat;
	a_vector<uint8_t> upgrades_dat;
	a_vector<uint8_t> techdata_dat;

	a_vector<uint8_t> melee_trg;

	std::array<a_vector<uint8_t>, 8> tileset_vf4;
	std::array<a_vector<uint8_t>, 8> tileset_cv5;
};

// ---------------------------------------------------------------------------
// game_state – per-match configuration (map load time, effectively immutable
//              during simulation).
// ---------------------------------------------------------------------------
struct game_state {

	game_state() = default;
	game_state(const game_state&) = delete;
	game_state(game_state&&) = default;
	game_state& operator=(const game_state&) = delete;
	game_state& operator=(game_state&&) = default;

	size_t map_tile_width;
	size_t map_tile_height;
	size_t map_walk_width;
	size_t map_walk_height;
	size_t map_width;
	size_t map_height;

	a_vector<a_string> map_strings;

	a_string scenario_name;
	a_string scenario_description;

	type_indexed_array<int, UnitTypes> unit_air_strength;
	type_indexed_array<int, UnitTypes> unit_ground_strength;

	struct force_t {
		a_string name;
		uint8_t flags;
	};
	std::array<force_t, 4> forces;

	std::array<sight_values_t, 12> sight_values;

	size_t tileset_index;

	a_vector<tile_id> gfx_tiles;
	a_vector<cv5_entry> cv5;
	a_vector<vf4_entry> vf4;
	a_vector<uint16_t> mega_tile_flags;

	unit_types_t unit_types;
	weapon_types_t weapon_types;
	upgrade_types_t upgrade_types;
	tech_types_t tech_types;

	std::array<type_indexed_array<bool, UnitTypes>, 12> unit_type_allowed;
	std::array<type_indexed_array<int, UpgradeTypes>, 12> max_upgrade_levels;
	std::array<type_indexed_array<bool, TechTypes>, 12> tech_available;

	std::array<xy, 12> start_locations;

	int max_unit_width;
	int max_unit_height;

	size_t repulse_field_width;
	size_t repulse_field_height;

	regions_t regions;

	a_vector<trigger> triggers;
};

// ---------------------------------------------------------------------------
// state_base_copyable – snapshottable per-frame simulation state.
//
// All fields here must be deterministically replicable.  This is the slice
// of state copied by state_copier for replay / save-state support.
// ---------------------------------------------------------------------------
struct state_base_copyable {

	// Ownership pointers – global_state is const; game_state is mutable only
	// at map-load time and should be treated as read-only during simulation.
	const global_state* global;
	game_state* game;

	int update_tiles_countdown;

	int order_timer_counter;
	int secondary_order_timer_counter;
	int current_frame;

	std::array<player_t, 12> players;

	std::array<std::array<int, 12>, 12> alliances;

	std::array<type_indexed_array<int, UpgradeTypes>, 12> upgrade_levels;
	std::array<type_indexed_array<bool, UpgradeTypes>, 12> upgrade_upgrading;
	std::array<type_indexed_array<bool, TechTypes>, 12> tech_researched;
	std::array<type_indexed_array<bool, TechTypes>, 12> tech_researching;

	std::array<type_indexed_array<int, UnitTypes>, 12> unit_counts;
	std::array<type_indexed_array<int, UnitTypes>, 12> completed_unit_counts;

	std::array<int, 12> factory_counts;
	std::array<int, 12> building_counts;
	std::array<int, 12> non_building_counts;

	std::array<int, 12> completed_factory_counts;
	std::array<int, 12> completed_building_counts;
	std::array<int, 12> completed_non_building_counts;

	std::array<int, 12> total_buildings_ever_completed;
	std::array<int, 12> total_non_buildings_ever_completed;

	std::array<int, 12> unit_score;
	std::array<int, 12> building_score;

	std::array<std::array<fp1, 3>, 12> supply_used;
	std::array<std::array<fp1, 3>, 12> supply_available;

	std::array<uint32_t, 12> shared_vision;

	a_vector<tile_t> tiles;
	a_vector<uint16_t> tiles_mega_tile_index;

	std::array<int, 0x100> random_counts;
	int total_random_counts;
	uint32_t lcg_rand_state;

	int last_error;

	int trigger_timer;
	std::array<a_vector<running_trigger>, 8> running_triggers;
	std::array<int, 12> trigger_wait_timers;
	std::array<bool, 12> trigger_waiting;

	size_t active_orders_size;
	size_t active_bullets_size;
	size_t active_thingies_size;

	a_vector<uint8_t> repulse_field;

	bool prev_bullet_heading_offset_clockwise;

	std::array<int, 12> current_minerals;
	std::array<int, 12> current_gas;
	std::array<int, 12> total_minerals_gathered;
	std::array<int, 12> total_gas_gathered;

	std::array<static_vector<std::pair<size_t, size_t>, 16>, 32> recent_lurker_hits;
	size_t recent_lurker_hit_current_index;

	creep_life_t creep_life;
	bool update_psionic_matrix;
	int disruption_webbed_units;
	bool cheats_enabled;
	bool cheat_operation_cwal;

	a_vector<location> locations;
};

// Intrusive-list link functor for Pylon psionic-matrix membership.
struct psionic_matrix_link_f {
	auto* operator()(unit_t* ptr) {
		return &ptr->building.pylon.psionic_matrix_link;
	}
	auto* operator()(const unit_t* ptr) {
		return &ptr->building.pylon.psionic_matrix_link;
	}
};

// ---------------------------------------------------------------------------
// state_base_non_copyable – intrusive-list and object-pool state.
//
// Holds live entity pools (units, bullets, sprites, images, orders, paths,
// thingies) and the unit-finder spatial index.  Cannot be bitwise-copied.
// ---------------------------------------------------------------------------
struct state_base_non_copyable {

	state_base_non_copyable() = default;
	state_base_non_copyable(const state_base_non_copyable&) = delete;
	state_base_non_copyable(state_base_non_copyable&&) = default;
	state_base_non_copyable& operator=(const state_base_non_copyable&) = delete;
	state_base_non_copyable& operator=(state_base_non_copyable&&) = default;

	intrusive_list<unit_t, default_link_f> visible_units;
	intrusive_list<unit_t, default_link_f> hidden_units;
	intrusive_list<unit_t, default_link_f> map_revealer_units;
	intrusive_list<unit_t, default_link_f> dead_units;

	std::array<intrusive_list<unit_t, void, &unit_t::player_units_link>, 12> player_units;
	intrusive_list<unit_t, void, &unit_t::cloaked_unit_link> cloaked_units;
	intrusive_list<unit_t, psionic_matrix_link_f> psionic_matrix_units;

	object_container<unit_t, 1700, 17> units_container;

	intrusive_list<bullet_t, default_link_f> active_bullets;
	object_container<bullet_t, 100, 10> bullets_container;

	a_vector<intrusive_list<sprite_t, default_link_f>> sprites_on_tile_line;
	object_container<sprite_t, 2500, 25> sprites_container;

	object_container<image_t, 5000, 50> images_container;

	object_container<order_t, 2000, 20> orders_container;

	intrusive_list<path_t, default_link_f> free_paths;
	a_list<path_t> paths;

	intrusive_list<thingy_t, default_link_f> active_thingies;
	intrusive_list<thingy_t, default_link_f> free_thingies;
	a_list<thingy_t> thingies;

	struct unit_finder_entry {
		unit_t* u;
		int value;
	};
	a_vector<unit_finder_entry> unit_finder_x;
	a_vector<unit_finder_entry> unit_finder_y;

	const unit_t* consider_collision_with_unit_bug;
	const unit_t* prev_bullet_source_unit;
};

// ---------------------------------------------------------------------------
// state – the full simulation context for one running match.
// ---------------------------------------------------------------------------
struct state : state_base_copyable, state_base_non_copyable {
};

} // namespace bwgame

#endif // BWGAME_STATE_H
