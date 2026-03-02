
#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

#include "ui.h"
#include "common.h"
#include "bwgame.h"
#include "replay.h"

#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <cerrno>
#include <limits>

#include "../perf_metrics.h"

using namespace bwgame;

using ui::log;

FILE* log_file = nullptr;

namespace bwgame {

namespace ui {

void log_str(a_string str) {
	fwrite(str.data(), str.size(), 1, stdout);
	fflush(stdout);
	if (!log_file) log_file = fopen("log.txt", "wb");
	if (log_file) {
		fwrite(str.data(), str.size(), 1, log_file);
		fflush(log_file);
	}
}

void fatal_error_str(a_string str) {
#ifdef EMSCRIPTEN
	const char* p = str.c_str();
	EM_ASM_({js_fatal_error($0);}, p);
#endif
	log("fatal error: %s\n", str);
	std::terminate();
}

}

}

struct saved_state {
	state st;
	action_state action_st;
	std::array<apm_t, 12> apm;
};

struct main_t {
	ui_functions ui;

	main_t(game_player player) : ui(std::move(player)) {}

	std::chrono::high_resolution_clock clock;
	std::chrono::high_resolution_clock::time_point last_tick;

	std::chrono::high_resolution_clock::time_point last_fps;
	int fps_counter = 0;

	perf::frame_timer sim_timer;
	bool live_result_reported = false;

	a_map<int, std::unique_ptr<saved_state>> saved_states;

	void reset() {
		saved_states.clear();
		ui.reset();
		live_result_reported = false;
	}

	void update() {
		auto now = clock.now();

		auto tick_speed = std::chrono::milliseconds((fp8::integer(42) / ui.game_speed).integer_part());

		if (now - last_fps >= std::chrono::seconds(1)) {
			if (fps_counter > 0) {
				log("[perf] sim: %.1f fps  mean=%.0f us  p95=%.0f us\n",
				    sim_timer.fps(),
				    sim_timer.mean_us(),
				    (double)sim_timer.percentile_us(95));
			}
			last_fps = now;
			fps_counter = 0;
		}

		auto next = [&]() {
			if (ui.is_replay_mode) {
				int save_interval = 10 * 1000 / 42;
				if (ui.st.current_frame == 0 || ui.st.current_frame % save_interval == 0) {
					auto i = saved_states.find(ui.st.current_frame);
					if (i == saved_states.end()) {
						auto v = std::make_unique<saved_state>();
						v->st = copy_state(ui.st);
						v->action_st = copy_state(ui.action_st, ui.st, v->st);
						v->apm = ui.apm;

						a_map<int, std::unique_ptr<saved_state>> new_saved_states;
						new_saved_states[ui.st.current_frame] = std::move(v);
						while (!saved_states.empty()) {
							auto i = saved_states.begin();
							auto v = std::move(*i);
							saved_states.erase(i);
							new_saved_states[v.first] = std::move(v.second);
						}
						std::swap(saved_states, new_saved_states);
					}
				}
				ui.replay_functions::next_frame();
			} else {
				ui.state_functions::next_frame();
			}
			sim_timer.tick();
			for (auto& v : ui.apm) v.update(ui.st.current_frame);
		};

		if (ui.is_replay_mode) {
			if (!ui.is_done() || ui.st.current_frame != ui.replay_frame) {
				if (ui.st.current_frame != ui.replay_frame) {
					if (ui.st.current_frame != ui.replay_frame) {
						auto i = saved_states.lower_bound(ui.replay_frame);
						if (i != saved_states.begin()) --i;
						auto& v = i->second;
						if (ui.st.current_frame > ui.replay_frame || v->st.current_frame > ui.st.current_frame) {
							ui.st = copy_state(v->st);
							ui.action_st = copy_state(v->action_st, v->st, ui.st);
							ui.apm = v->apm;
						}
					}
					if (ui.st.current_frame < ui.replay_frame) {
						for (size_t i = 0; i != 32 && ui.st.current_frame != ui.replay_frame; ++i) {
							for (size_t i2 = 0; i2 != 4 && ui.st.current_frame != ui.replay_frame; ++i2) {
								next();
							}
							if (clock.now() - now >= std::chrono::milliseconds(50)) break;
						}
					}
					last_tick = now;
				} else {
					if (ui.is_paused) {
						last_tick = now;
					} else {
						auto tick_t = now - last_tick;
						if (tick_t >= tick_speed * 16) {
							last_tick = now - tick_speed * 16;
							tick_t = tick_speed * 16;
						}
						auto tick_n = tick_speed.count() == 0 ? 128 : tick_t / tick_speed;
						for (auto i = tick_n; i;) {
							--i;
							++fps_counter;
							last_tick += tick_speed;

							if (!ui.is_done()) next();
							else break;
							if (i % 4 == 3 && clock.now() - now >= std::chrono::milliseconds(50)) break;
						}
						ui.replay_frame = ui.st.current_frame;
					}
				}
			}
		} else {
			if (ui.is_paused) {
				last_tick = now;
			} else {
				auto tick_t = now - last_tick;
				if (tick_t >= tick_speed * 16) {
					last_tick = now - tick_speed * 16;
					tick_t = tick_speed * 16;
				}
				auto tick_n = tick_speed.count() == 0 ? 128 : tick_t / tick_speed;
				for (auto i = tick_n; i;) {
					--i;
					++fps_counter;
					last_tick += tick_speed;
					next();
					if (i % 4 == 3 && clock.now() - now >= std::chrono::milliseconds(50)) break;
				}
				ui.replay_frame = ui.st.current_frame;
			}
			if (!live_result_reported && ui.has_local_player()) {
				if (ui.player_won(ui.local_player_id)) {
					live_result_reported = true;
					log("single-player: victory at frame %d\n", ui.st.current_frame);
				} else if (ui.player_defeated(ui.local_player_id)) {
					live_result_reported = true;
					log("single-player: defeat at frame %d\n", ui.st.current_frame);
				}
			}
		}

		ui.update();
	}
};

main_t* g_m = nullptr;

#ifdef EMSCRIPTEN

// ---------------------------------------------------------------------------
// Emscripten-only: custom allocator with memory-pressure eviction.
//
// Emscripten exposes dlmalloc/dlfree as its internal allocator API, which
// lets us track live allocation sizes and evict saved states when the WASM
// heap is under pressure.  On native platforms the system allocator is used
// directly and this whole section is excluded.
// ---------------------------------------------------------------------------

uint32_t freemem_rand_state = (uint32_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
auto freemem_rand() {
	freemem_rand_state = freemem_rand_state * 22695477 + 1;
	return (freemem_rand_state >> 16) & 0x7fff;
}

void out_of_memory() {
	printf("out of memory :(\n");
	const char* p = "out of memory :(";
	EM_ASM_({js_fatal_error($0);}, p);
	throw std::bad_alloc();
}

size_t bytes_allocated = 0;

void free_memory() {
	if (!g_m) out_of_memory();
	size_t n_states = g_m->saved_states.size();
	printf("n_states is %zu\n", n_states);
	if (n_states <= 2) out_of_memory();
	size_t n;
	// For large collections use random eviction (O(1)); for small collections use
	// the O(n) greedy strategy that removes whichever state is closest to one of
	// its two adjacent neighbours, minimising the worst-case seek penalty.
	if (n_states >= 64) {
		n = 1 + freemem_rand() % (n_states - 2);
	} else {
		auto begin = std::next(g_m->saved_states.begin());
		auto end = std::prev(g_m->saved_states.end());
		n = 1;
		int best_gap = std::numeric_limits<int>::max();
		size_t i_n = 1;
		auto prev_i = begin;
		auto cur_i = std::next(begin);
		for (; cur_i != end; ++prev_i, ++cur_i, ++i_n) {
			// Gap to left neighbour
			int gap = cur_i->first - prev_i->first;
			if (gap < best_gap) {
				best_gap = gap;
				n = i_n;
			}
		}
	}
	g_m->saved_states.erase(std::next(g_m->saved_states.begin(), n));
}

struct dlmalloc_chunk {
	size_t prev_foot;
	size_t head;
	dlmalloc_chunk* fd;
	dlmalloc_chunk* bk;
};

size_t alloc_size(void* ptr) {
	dlmalloc_chunk* c = (dlmalloc_chunk*)((char*)ptr - sizeof(size_t) * 2);
	return c->head & ~7;
}

extern "C" void* dlmalloc(size_t);
extern "C" void dlfree(void*);

size_t max_bytes_allocated = 160 * 1024 * 1024;

extern "C" void* malloc(size_t n) {
	void* r = dlmalloc(n);
	while (!r) {
		printf("failed to allocate %zu bytes\n", n);
		free_memory();
		r = dlmalloc(n);
	}
	bytes_allocated += alloc_size(r);
	while (bytes_allocated > max_bytes_allocated) free_memory();
	return r;
}

extern "C" void free(void* ptr) {
	if (!ptr) return;
	bytes_allocated -= alloc_size(ptr);
	dlfree(ptr);
}

// Emscripten JS file-reader and entry point continue below.

namespace bwgame {
namespace data_loading {

template<bool default_little_endian = true>
struct js_file_reader {
	a_string filename;
	size_t index = ~(size_t)0;
	size_t file_pointer = 0;
	js_file_reader() = default;
	explicit js_file_reader(a_string filename) {
		open(std::move(filename));
	}
	void open(a_string filename) {
		if (filename == "StarDat.mpq") index = 0;
		else if (filename == "BrooDat.mpq") index = 1;
		else if (filename == "Patch_rt.mpq") index = 2;
		else ui::xcept("js_file_reader: unknown filename '%s'", filename);
		this->filename = std::move(filename);
	}

	void get_bytes(uint8_t* dst, size_t n) {
		EM_ASM_({js_read_data($0, $1, $2, $3);}, index, dst, file_pointer, n);
		file_pointer += n;
	}

	void seek(size_t offset) {
		file_pointer = offset;
	}
	size_t tell() const {
		return file_pointer;
	}

	size_t size() {
		return EM_ASM_INT({return js_file_size($0);}, index);
	}

};

}
}

main_t* m;

int current_width = -1;
int current_height = -1;

extern "C" void ui_resize(int width, int height) {
	if (width == current_width && height == current_height) return;
	if (width <= 0 || height <= 0) return;
	current_width = width;
	current_height = height;
	if (!m) return;
	m->ui.window_surface.reset();
	m->ui.indexed_surface.reset();
	m->ui.rgba_surface.reset();
	m->ui.wnd.destroy();
	m->ui.wnd.create("test", 0, 0, width, height);
	m->ui.resize(width, height);
}

extern "C" double replay_get_value(int index) {
	switch (index) {
	case 0:
		return m->ui.game_speed.raw_value / 256.0;
	case 1:
		return m->ui.is_paused ? 1 : 0;
	case 2:
		return (double)m->ui.st.current_frame;
	case 3:
		return (double)m->ui.replay_frame;
	case 4:
		return (double)m->ui.replay_st.end_frame;
	case 5:
		return (double)(uintptr_t)m->ui.replay_st.map_name.data();
	case 6:
		return (double)m->ui.replay_frame / m->ui.replay_st.end_frame;
	default:
		return 0;
	}
}

extern "C" void replay_set_value(int index, double value) {
	switch (index) {
	case 0:
		m->ui.game_speed.raw_value = (int)(value * 256.0);
		if (m->ui.game_speed < 1_fp8) m->ui.game_speed = 1_fp8;
		break;
	case 1:
		m->ui.is_paused = value != 0.0;
		break;
	case 3:
		m->ui.replay_frame = (int)value;
		if (m->ui.replay_frame < 0) m->ui.replay_frame = 0;
		if (m->ui.replay_frame > m->ui.replay_st.end_frame) m->ui.replay_frame = m->ui.replay_st.end_frame;
		break;
	case 6:
		m->ui.replay_frame = (int)(m->ui.replay_st.end_frame * value);
		if (m->ui.replay_frame < 0) m->ui.replay_frame = 0;
		if (m->ui.replay_frame > m->ui.replay_st.end_frame) m->ui.replay_frame = m->ui.replay_st.end_frame;
		break;
	}
}

#include <emscripten/bind.h>
#include <emscripten/val.h>
using namespace emscripten;

struct js_unit_type {
	const unit_type_t* ut = nullptr;
	js_unit_type() {}
	js_unit_type(const unit_type_t* ut) : ut(ut) {}
	auto id() const {return ut ? (int)ut->id : 228;}
	auto build_time() const {return ut->build_time;}
};

struct js_unit {
	unit_t* u = nullptr;
	js_unit() {}
	js_unit(unit_t* u) : u(u) {}
	auto owner() const {return u->owner;}
	auto remaining_build_time() const {return u->remaining_build_time;}
	auto unit_type() const {return u->unit_type;}
	auto build_type() const {return u->build_queue.empty() ? nullptr : u->build_queue.front();}
};


struct util_functions: state_functions {
	util_functions(state& st) : state_functions(st) {}

	double worker_supply(int owner) {
		double r = 0.0;
		for (const unit_t* u : ptr(st.player_units.at(owner))) {
			if (!ut_worker(u)) continue;
			if (!u_completed(u)) continue;
			r += u->unit_type->supply_required.raw_value / 2.0;
		}
		return r;
	}

	double army_supply(int owner) {
		double r = 0.0;
		for (const unit_t* u : ptr(st.player_units.at(owner))) {
			if (ut_worker(u)) continue;
			if (!u_completed(u)) continue;
			r += u->unit_type->supply_required.raw_value / 2.0;
		}
		return r;
	}

	auto get_all_incomplete_units() {
		val r = val::array();
		size_t i = 0;
		for (unit_t* u : ptr(st.visible_units)) {
			if (u_completed(u)) continue;
			r.set(i++, u);
		}
		for (unit_t* u : ptr(st.hidden_units)) {
			if (u_completed(u)) continue;
			r.set(i++, u);
		}
		return r;
	}

	auto get_all_completed_units() {
		val r = val::array();
		size_t i = 0;
		for (unit_t* u : ptr(st.visible_units)) {
			if (!u_completed(u)) continue;
			r.set(i++, u);
		}
		for (unit_t* u : ptr(st.hidden_units)) {
			if (!u_completed(u)) continue;
			r.set(i++, u);
		}
		return r;
	}

	auto get_all_units() {
		val r = val::array();
		size_t i = 0;
		for (unit_t* u : ptr(st.visible_units)) {
			r.set(i++, u);
		}
		for (unit_t* u : ptr(st.hidden_units)) {
			r.set(i++, u);
		}
		for (unit_t* u : ptr(st.map_revealer_units)) {
			r.set(i++, u);
		}
		return r;
	}

	auto get_completed_upgrades(int owner) {
		val r = val::array();
		size_t n = 0;
		for (size_t i = 0; i != 61; ++i) {
			int level = player_upgrade_level(owner, (UpgradeTypes)i);
			if (level == 0) continue;
			val o = val::object();
			o.set("id", val((int)i));
			o.set("icon", val(get_upgrade_type((UpgradeTypes)i)->icon));
			o.set("level", val(level));
			r.set(n++, o);
		}
		return r;
	}

	auto get_completed_research(int owner) {
		val r = val::array();
		size_t n = 0;
		for (size_t i = 0; i != 44; ++i) {
			if (!player_has_researched(owner, (TechTypes)i)) continue;
			val o = val::object();
			o.set("id", val((int)i));
			o.set("icon", val(get_tech_type((TechTypes)i)->icon));
			r.set(n++, o);
		}
		return r;
	}

	auto get_incomplete_upgrades(int owner) {
		val r = val::array();
		size_t i = 0;
		for (unit_t* u : ptr(st.player_units[owner])) {
			if (u->order_type->id == Orders::Upgrade && u->building.upgrading_type) {
				val o = val::object();
				o.set("id", val((int)u->building.upgrading_type->id));
				o.set("icon", val((int)u->building.upgrading_type->icon));
				o.set("level", val(u->building.upgrading_level));
				o.set("remaining_time", val(u->building.upgrade_research_time));
				o.set("total_time", val(upgrade_time_cost(owner, u->building.upgrading_type)));
				r.set(i++, o);
			}
		}
		return r;
	}

	auto get_incomplete_research(int owner) {
		val r = val::array();
		size_t i = 0;
		for (unit_t* u : ptr(st.player_units[owner])) {
			if (u->order_type->id == Orders::ResearchTech && u->building.researching_type) {
				val o = val::object();
				o.set("id", val((int)u->building.researching_type->id));
				o.set("icon", val((int)u->building.researching_type->icon));
				o.set("remaining_time", val(u->building.upgrade_research_time));
				o.set("total_time", val(u->building.researching_type->research_time));
				r.set(i++, o);
			}
		}
		return r;
	}

};

optional<util_functions> m_util_funcs;

util_functions& get_util_funcs() {
	m_util_funcs.emplace(m->ui.st);
	return *m_util_funcs;
}

const unit_type_t* unit_t_unit_type(const unit_t* u) {
	return u->unit_type;
}
const unit_type_t* unit_t_build_type(const unit_t* u) {
	if (u->build_queue.empty()) return nullptr;
	return u->build_queue.front();
}

int unit_type_t_id(const unit_type_t& ut) {
	return (int)ut.id;
}

void set_volume(double percent) {
	m->ui.set_volume((int)(percent * 100));
}

double get_volume() {
	return m->ui.global_volume / 100.0;
}

EMSCRIPTEN_BINDINGS(openbw) {
	register_vector<js_unit>("vector_js_unit");
	class_<util_functions>("util_functions")
		.function("worker_supply", &util_functions::worker_supply)
		.function("army_supply", &util_functions::army_supply)
		.function("get_all_incomplete_units", &util_functions::get_all_incomplete_units, allow_raw_pointers())
		.function("get_all_completed_units", &util_functions::get_all_completed_units, allow_raw_pointers())
		.function("get_all_units", &util_functions::get_all_units, allow_raw_pointers())
		.function("get_completed_upgrades", &util_functions::get_completed_upgrades)
		.function("get_completed_research", &util_functions::get_completed_research)
		.function("get_incomplete_upgrades", &util_functions::get_incomplete_upgrades)
		.function("get_incomplete_research", &util_functions::get_incomplete_research)
		;
	function("get_util_funcs", &get_util_funcs);

	function("set_volume", &set_volume);
	function("get_volume", &get_volume);

	class_<unit_type_t>("unit_type_t")
		.property("id", &unit_type_t_id)
		.property("build_time", &unit_type_t::build_time)
		;

	class_<unit_t>("unit_t")
		.property("owner", &unit_t::owner)
		.property("remaining_build_time", &unit_t::remaining_build_time)
		.function("unit_type", &unit_t_unit_type, allow_raw_pointers())
		.function("build_type", &unit_t_build_type, allow_raw_pointers())
		;
}

extern "C" double player_get_value(int player, int index) {
	if (player < 0 || player >= 12) return 0;
	switch (index) {
	case 0:
		return m->ui.st.players.at(player).controller == player_t::controller_occupied ? 1 : 0;
	case 1:
		return (double)m->ui.st.players.at(player).color;
	case 2:
		return (double)(uintptr_t)m->ui.replay_st.player_name.at(player).data();
	case 3:
		return m->ui.st.supply_used.at(player)[0].raw_value / 2.0;
	case 4:
		return m->ui.st.supply_used.at(player)[1].raw_value / 2.0;
	case 5:
		return m->ui.st.supply_used.at(player)[2].raw_value / 2.0;
	case 6:
		return std::min(m->ui.st.supply_available.at(player)[0].raw_value / 2.0, 200.0);
	case 7:
		return std::min(m->ui.st.supply_available.at(player)[1].raw_value / 2.0, 200.0);
	case 8:
		return std::min(m->ui.st.supply_available.at(player)[2].raw_value / 2.0, 200.0);
	case 9:
		return (double)m->ui.st.current_minerals.at(player);
	case 10:
		return (double)m->ui.st.current_gas.at(player);
	case 11:
		return util_functions(m->ui.st).worker_supply(player);
	case 12:
		return util_functions(m->ui.st).army_supply(player);
	case 13:
		return (double)(int)m->ui.st.players.at(player).race;
	case 14:
		return (double)m->ui.apm.at(player).current_apm;
	default:
		return 0;
	}
}

bool any_replay_loaded = false;

extern "C" void load_replay(const uint8_t* data, size_t len) {
	m->reset();
	m->ui.load_replay_data(data, len);
	m->ui.set_image_data();
	any_replay_loaded = true;
}

#endif

// ---------------------------------------------------------------------------
// Benchmark mode: step N frames without any rendering and report throughput.
// Usage: gfxtest --bench <frames> [--replay <file>]
//
// This intentionally avoids all SDL/UI initialisation so the reported number
// reflects pure simulation cost.
// ---------------------------------------------------------------------------
#ifndef EMSCRIPTEN
struct replay_hash_checkpoint {
	int frame = 0;
	uint32_t hash = 0;
};

struct replay_hash_fixture {
	std::string replay_file;
	std::vector<replay_hash_checkpoint> checkpoints;
};

struct replay_validation_report {
	a_string map_name;
	int end_frame = 0;
	bool is_broodwar = false;
	size_t action_bytes = 0;
	size_t action_frame_chunks = 0;
	int first_action_frame = -1;
	int last_action_frame = -1;
	int stepped_frames = 0;
};

static std::string trim_ascii_copy(const std::string& s) {
	size_t begin = 0;
	while (begin < s.size() && std::isspace((unsigned char)s[begin])) ++begin;
	size_t end = s.size();
	while (end > begin && std::isspace((unsigned char)s[end - 1])) --end;
	return s.substr(begin, end - begin);
}

static int parse_fixture_int_or_throw(
	const char* fixture_file,
	int line_no,
	const char* field_name,
	const std::string& token) {
	errno = 0;
	char* end = nullptr;
	long long v = std::strtoll(token.c_str(), &end, 0);
	if (errno != 0 || end == token.c_str() || *end != '\0' ||
		v < std::numeric_limits<int>::min() ||
		v > std::numeric_limits<int>::max()) {
		error("verify_hashes: %s:%d invalid %s '%s'",
			fixture_file, line_no, field_name, token.c_str());
	}
	return (int)v;
}

static uint32_t parse_fixture_u32_or_throw(
	const char* fixture_file,
	int line_no,
	const char* field_name,
	const std::string& token) {
	errno = 0;
	char* end = nullptr;
	unsigned long long v = std::strtoull(token.c_str(), &end, 0);
	if (errno != 0 || end == token.c_str() || *end != '\0' ||
		v > std::numeric_limits<uint32_t>::max()) {
		error("verify_hashes: %s:%d invalid %s '%s'",
			fixture_file, line_no, field_name, token.c_str());
	}
	return (uint32_t)v;
}

static replay_hash_fixture load_hash_fixture_or_throw(const char* fixture_file) {
	std::ifstream in(fixture_file, std::ios::in);
	if (!in) {
		error("verify_hashes: unable to open fixture '%s': %s",
			fixture_file, std::strerror(errno));
	}

	replay_hash_fixture fixture;
	std::string line;
	int line_no = 0;
	int prev_frame = -1;
	while (std::getline(in, line)) {
		++line_no;
		std::string t = trim_ascii_copy(line);
		if (t.empty() || t[0] == '#') continue;

		if (t.compare(0, 7, "replay:") == 0) {
			std::string replay_file = trim_ascii_copy(t.substr(7));
			if (replay_file.empty()) {
				error("verify_hashes: %s:%d missing replay path after 'replay:'",
					fixture_file, line_no);
			}
			fixture.replay_file = replay_file;
			continue;
		}

		std::istringstream iss(t);
		std::string frame_token;
		std::string hash_token;
		std::string extra_token;
		if (!(iss >> frame_token >> hash_token) || (iss >> extra_token)) {
			error("verify_hashes: %s:%d expected '<frame> <hash>' or 'replay: <path>'",
				fixture_file, line_no);
		}

		int frame = parse_fixture_int_or_throw(fixture_file, line_no, "frame", frame_token);
		if (frame < 0) {
			error("verify_hashes: %s:%d frame must be >= 0, got %d",
				fixture_file, line_no, frame);
		}
		if (!fixture.checkpoints.empty() && frame <= prev_frame) {
			error("verify_hashes: %s:%d frame %d is not strictly increasing (previous %d)",
				fixture_file, line_no, frame, prev_frame);
		}

		replay_hash_checkpoint cp;
		cp.frame = frame;
		cp.hash = parse_fixture_u32_or_throw(fixture_file, line_no, "hash", hash_token);
		fixture.checkpoints.push_back(cp);
		prev_frame = frame;
	}

	if (!in.eof()) {
		error("verify_hashes: failed while reading fixture '%s'", fixture_file);
	}
	if (fixture.checkpoints.empty()) {
		error("verify_hashes: fixture '%s' contains no checkpoints", fixture_file);
	}

	return fixture;
}

static uint32_t compute_replay_checkpoint_hash(const replay_player& player) {
	const state& st = player.st();
	const action_state& action_st = player.action_st;

	uint32_t hash = 2166136261u;
	auto add = [&](auto v) {
		hash ^= (uint32_t)v;
		hash *= 16777619u;
	};

	// Keep this aligned with sync-side insync hashing, plus replay-specific
	// action stream cursor fields for fixture verification.
	add(st.current_frame);
	add(action_st.actions_data_position);
	add(action_st.next_action_frame);
	add(st.lcg_rand_state);
	for (auto v : st.current_minerals) add(v);
	for (auto v : st.current_gas) add(v);
	for (auto v : st.total_minerals_gathered) add(v);
	for (auto v : st.total_gas_gathered) add(v);
	add(st.active_orders_size);
	add(st.active_bullets_size);
	add(st.active_thingies_size);
	for (const unit_t* u : ptr(st.visible_units)) {
		add((u->shield_points + u->hp).raw_value);
		add(u->exact_position.x.raw_value);
		add(u->exact_position.y.raw_value);
		add(u->owner);
		add((int)u->order_type->id);
	}

	return hash;
}

static std::vector<replay_hash_checkpoint> sample_replay_hashes_or_throw(
	const char* replay_file,
	int hash_interval,
	a_string* map_name,
	int* end_frame) {
	if (hash_interval <= 0) {
		error("record_hashes: hash interval must be > 0, got %d", hash_interval);
	}

	auto load_data_file = data_loading::data_files_directory("");

	replay_player player;
	player.init(load_data_file);
	player.load_replay_file(replay_file);

	if (map_name) *map_name = player.replay_st.map_name;
	if (end_frame) *end_frame = player.replay_st.end_frame;

	std::vector<replay_hash_checkpoint> checkpoints;
	int last_recorded_frame = -1;
	while (true) {
		int frame = player.st().current_frame;
		bool should_record = frame == 0 || frame % hash_interval == 0 || player.is_done();
		if (should_record && frame != last_recorded_frame) {
			replay_hash_checkpoint cp;
			cp.frame = frame;
			cp.hash = compute_replay_checkpoint_hash(player);
			checkpoints.push_back(cp);
			last_recorded_frame = frame;
		}
		if (player.is_done()) break;
		player.next_frame();
	}

	if (player.action_st.actions_data_position != player.replay_st.actions_data_buffer.size()) {
		error("record_hashes: consumed %lld/%lld action bytes during playback",
			(long long)player.action_st.actions_data_position,
			(long long)player.replay_st.actions_data_buffer.size());
	}

	return checkpoints;
}

static replay_validation_report validate_replay_or_throw(const char* replay_file) {
	using namespace bwgame;

	auto load_data_file = data_loading::data_files_directory("");

	// replay_player owns its own global/game/sim state.
	replay_player player;
	player.init(load_data_file);
	player.load_replay_file(replay_file);

	replay_validation_report report;
	report.map_name = player.replay_st.map_name;
	report.end_frame = player.replay_st.end_frame;
	report.is_broodwar = player.replay_st.is_broodwar;
	report.action_bytes = player.replay_st.actions_data_buffer.size();

	if (report.end_frame < 0) {
		error("validate_replay: invalid end frame %d", report.end_frame);
	}

	// Pass 1: verify action frame block structure and bounds without mutating
	// replay playback state.
	const auto& action_buffer = player.replay_st.actions_data_buffer;
	static const uint8_t empty_action_buffer_byte = 0;
	const uint8_t* action_begin = action_buffer.empty() ? &empty_action_buffer_byte : action_buffer.data();
	const uint8_t* action_end = action_begin + action_buffer.size();
	data_loading::data_reader_le frame_reader(action_begin, action_end);
	int prev_frame = -1;
	while (frame_reader.left() != 0) {
		int frame = frame_reader.get<int32_t>();
		size_t actions_size = frame_reader.get<uint8_t>();
		frame_reader.get_n(actions_size);

		if (frame < 0) {
			error("validate_replay: found negative action frame %d", frame);
		}
		if (frame < prev_frame) {
			error("validate_replay: non-monotonic action frame chunk %d after %d", frame, prev_frame);
		}
		if (frame >= report.end_frame) {
			error("validate_replay: action frame %d is outside replay end frame %d", frame, report.end_frame);
		}

		if (report.first_action_frame == -1) report.first_action_frame = frame;
		report.last_action_frame = frame;
		++report.action_frame_chunks;
		prev_frame = frame;
	}

	// Pass 2: run normal replay stepping to force action decoding/dispatch.
	while (!player.is_done()) {
		player.next_frame();
		++report.stepped_frames;
	}

	if (player.action_st.actions_data_position != action_buffer.size()) {
		error("validate_replay: consumed %lld/%lld action bytes during playback",
			(long long)player.action_st.actions_data_position,
			(long long)action_buffer.size());
	}
	if (player.st().current_frame != report.end_frame) {
		error("validate_replay: playback stopped at frame %d, expected %d",
			player.st().current_frame,
			report.end_frame);
	}

	return report;
}

static int run_validate_replay(const char* replay_file) {
	using namespace bwgame;

	const char* rep = replay_file ? replay_file : "maps/p49.rep";
	log("validate: checking replay '%s'\n", rep);

	try {
		auto report = validate_replay_or_throw(rep);
		const char* map_name = report.map_name.empty() ? "<unknown>" : report.map_name.c_str();
		log("validate: PASS\n"
			"  map              : %s\n"
			"  broodwar         : %s\n"
			"  end frame        : %d\n"
			"  action bytes     : %lld\n"
			"  action chunks    : %lld\n"
			"  first/last chunk : %d / %d\n"
			"  stepped frames   : %d\n",
			map_name,
			report.is_broodwar ? "yes" : "no",
			report.end_frame,
			(long long)report.action_bytes,
			(long long)report.action_frame_chunks,
			report.first_action_frame,
			report.last_action_frame,
			report.stepped_frames);
		return 0;
	} catch (const exception& e) {
		log("validate: FAIL (%s)\n", e.what());
		return 1;
	} catch (const std::exception& e) {
		log("validate: FAIL (%s)\n", e.what());
		return 1;
	} catch (...) {
		log("validate: FAIL (unknown exception)\n");
		return 1;
	}
}

static int run_record_hashes(const char* replay_file, const char* fixture_file, int hash_interval) {
	using namespace bwgame;

	if (!fixture_file || fixture_file[0] == '\0') {
		log("record-hashes: FAIL (missing fixture output path)\n");
		return 2;
	}

	const char* rep = replay_file ? replay_file : "maps/p49.rep";
	log("record-hashes: replay '%s' -> fixture '%s' (interval=%d)\n",
		rep, fixture_file, hash_interval);

	try {
		a_string map_name;
		int end_frame = 0;
		auto checkpoints = sample_replay_hashes_or_throw(rep, hash_interval, &map_name, &end_frame);

		std::ofstream out(fixture_file, std::ios::out | std::ios::trunc);
		if (!out) {
			error("record_hashes: unable to open fixture '%s' for writing: %s",
				fixture_file, std::strerror(errno));
		}

		out << "# OpenSnowstorm replay hash fixture v1\n";
		out << "# Generated by gfxtest --record-hashes\n";
		out << "replay: " << rep << "\n";
		out << "# frame hash\n";
		for (const auto& cp : checkpoints) {
			out << format("%d 0x%08x\n", cp.frame, cp.hash);
		}
		if (!out) {
			error("record_hashes: failed while writing fixture '%s'", fixture_file);
		}

		const char* map_name_str = map_name.empty() ? "<unknown>" : map_name.c_str();
		log("record-hashes: PASS\n"
			"  map         : %s\n"
			"  end frame   : %d\n"
			"  checkpoints : %lld\n",
			map_name_str,
			end_frame,
			(long long)checkpoints.size());
		return 0;
	} catch (const exception& e) {
		log("record-hashes: FAIL (%s)\n", e.what());
		return 1;
	} catch (const std::exception& e) {
		log("record-hashes: FAIL (%s)\n", e.what());
		return 1;
	} catch (...) {
		log("record-hashes: FAIL (unknown exception)\n");
		return 1;
	}
}

static int run_verify_hashes(const char* replay_file, const char* fixture_file) {
	using namespace bwgame;

	if (!fixture_file || fixture_file[0] == '\0') {
		log("verify-hashes: FAIL (missing fixture path)\n");
		return 2;
	}

	try {
		replay_hash_fixture fixture = load_hash_fixture_or_throw(fixture_file);
		const char* rep = replay_file ? replay_file :
			(!fixture.replay_file.empty() ? fixture.replay_file.c_str() : "maps/p49.rep");

		log("verify-hashes: replay '%s' against fixture '%s'\n", rep, fixture_file);

		auto load_data_file = data_loading::data_files_directory("");

		replay_player player;
		player.init(load_data_file);
		player.load_replay_file(rep);

		if (fixture.checkpoints.back().frame > player.replay_st.end_frame) {
			error("verify_hashes: fixture frame %d exceeds replay end frame %d",
				fixture.checkpoints.back().frame,
				player.replay_st.end_frame);
		}

		size_t next_checkpoint = 0;
		size_t mismatch_count = 0;
		size_t printed_mismatches = 0;
		const size_t max_mismatch_logs = 20;
		while (next_checkpoint < fixture.checkpoints.size()) {
			const auto& cp = fixture.checkpoints[next_checkpoint];
			while (player.st().current_frame < cp.frame && !player.is_done()) {
				player.next_frame();
			}
			if (player.st().current_frame != cp.frame) {
				error("verify_hashes: checkpoint frame %d is unreachable (replay stopped at frame %d)",
					cp.frame,
					player.st().current_frame);
			}

			uint32_t got = compute_replay_checkpoint_hash(player);
			if (got != cp.hash) {
				++mismatch_count;
				if (printed_mismatches < max_mismatch_logs) {
					log("verify-hashes: mismatch frame %d expected=0x%08x got=0x%08x\n",
						cp.frame, cp.hash, got);
					++printed_mismatches;
				}
			}
			++next_checkpoint;
		}

		while (!player.is_done()) {
			player.next_frame();
		}

		if (player.action_st.actions_data_position != player.replay_st.actions_data_buffer.size()) {
			error("verify_hashes: consumed %lld/%lld action bytes during playback",
				(long long)player.action_st.actions_data_position,
				(long long)player.replay_st.actions_data_buffer.size());
		}

		if (mismatch_count != 0) {
			if (mismatch_count > printed_mismatches) {
				log("verify-hashes: ... %lld additional mismatches omitted\n",
					(long long)(mismatch_count - printed_mismatches));
			}
			log("verify-hashes: FAIL (%lld/%lld checkpoints mismatched)\n",
				(long long)mismatch_count,
				(long long)fixture.checkpoints.size());
			return 1;
		}

		const char* map_name = player.replay_st.map_name.empty() ? "<unknown>" : player.replay_st.map_name.c_str();
		log("verify-hashes: PASS\n"
			"  map         : %s\n"
			"  end frame   : %d\n"
			"  checkpoints : %lld\n",
			map_name,
			player.replay_st.end_frame,
			(long long)fixture.checkpoints.size());
		return 0;
	} catch (const exception& e) {
		log("verify-hashes: FAIL (%s)\n", e.what());
		return 1;
	} catch (const std::exception& e) {
		log("verify-hashes: FAIL (%s)\n", e.what());
		return 1;
	} catch (...) {
		log("verify-hashes: FAIL (unknown exception)\n");
		return 1;
	}
}

static int run_bench(int bench_frames, const char* replay_file) {
	using namespace bwgame;

	const char* rep = replay_file ? replay_file : "maps/p49.rep";
	log("bench: loading replay '%s', stepping %d frames (no UI)\n", rep, bench_frames);

	auto load_data_file = data_loading::data_files_directory("");

	// replay_player owns its own global/game/sim state.
	replay_player player;
	player.init(load_data_file);
	player.load_replay_file(rep);

	int64_t total_us = 0;
	int64_t min_us   = std::numeric_limits<int64_t>::max();
	int64_t max_us   = 0;

	int frames = 0;
	while (!player.is_done() && frames < bench_frames) {
		int64_t frame_us = 0;
		{
			perf::scope_timer t(frame_us);
			player.next_frame();
		}
		total_us += frame_us;
		if (frame_us < min_us) min_us = frame_us;
		if (frame_us > max_us) max_us = frame_us;
		++frames;
	}

	double elapsed_s = total_us * 1e-6;
	double fps        = elapsed_s > 0 ? frames / elapsed_s : 0;
	double mean_us    = frames  > 0 ? (double)total_us / frames : 0;

	log("bench: %d frames\n"
	    "  fps    : %.1f\n"
	    "  mean   : %.1f us\n"
	    "  min    : %lld us\n"
	    "  max    : %lld us\n",
	    frames, fps, mean_us,
	    (long long)min_us,
	    (long long)max_us);
	return 0;
}

static void print_usage(const char* argv0) {
	log(
		"usage:\n"
		"  %s [--replay <file.rep>] [--headless]\n"
		"  %s --map <file.scx|file.scm> [--local-player <0-7>] [--enemy-player <0-7>]\n"
		"     [--game-type <melee|ums>] [--local-race <zerg|terran|protoss|random>]\n"
		"     [--enemy-race <zerg|terran|protoss|random>] [--headless]\n"
		"  %s --bench <frames> [--replay <file.rep>]\n"
		"  %s --validate-replay [--replay <file.rep>]\n"
		"  %s --record-hashes <fixture.txt> [--hash-interval <n>] [--replay <file.rep>]\n"
		"  %s --verify-hashes <fixture.txt> [--replay <file.rep>]\n"
		"\n"
		"single-player controls (map mode):\n"
		"  left drag/select    left click command panel   middle drag camera\n"
		"  left click map place armed building\n"
		"  right click issue order (or cancel armed building)\n"
		"  s stop              h hold position            a attack-move (next right click)\n"
		"  t patrol (next right click)\n"
		"  ctrl+<1-0> set group   shift+<1-0> add group   <1-0> recall group\n"
		"  esc cancel armed building placement\n"
		"  space/p pause       u speed up                 z/d speed down\n",
		argv0, argv0, argv0, argv0, argv0, argv0);
}

static int parse_slot_or_error(const char* value, const char* flag_name) {
	errno = 0;
	char* end = nullptr;
	long v = std::strtol(value, &end, 10);
	if (errno != 0 || end == value || *end != '\0' || v < 0 || v > 7) {
		error("invalid %s value '%s' (expected integer 0-7)", flag_name, value);
	}
	return (int)v;
}

static int parse_race_or_error(const char* value, const char* flag_name) {
	std::string v = value ? value : "";
	for (char& c : v) c = (char)std::tolower((unsigned char)c);
	if (v == "zerg") return 0;
	if (v == "terran") return 1;
	if (v == "protoss") return 2;
	if (v == "random") return 5;
	error("invalid %s value '%s' (expected zerg|terran|protoss|random)", flag_name, value ? value : "");
	return 5;
}

#endif

int main(int argc, char** argv) {

	using namespace bwgame;

	log("v25\n");

#ifndef EMSCRIPTEN
	// Argument parsing
	const char* replay_file = nullptr;
	const char* map_file = nullptr;
	int bench_frames = 0;
	const char* verify_hashes_file = nullptr;
	const char* record_hashes_file = nullptr;
	int hash_interval = 240;
	bool validate_replay = false;
	bool headless = false;
	bool show_help = false;
	bool game_type_melee = true;
	int local_player_slot = -1;
	int enemy_player_slot = -1;
	int local_race = 5;
	int enemy_race = 5;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			show_help = true;
		} else if (strcmp(argv[i], "--bench") == 0 && i + 1 < argc) {
			bench_frames = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--map") == 0) {
			if (i + 1 >= argc) {
				log("error: --map requires a map file path\n");
				return 2;
			}
			map_file = argv[++i];
		} else if (strcmp(argv[i], "--local-player") == 0) {
			if (i + 1 >= argc) {
				log("error: --local-player requires a slot index (0-7)\n");
				return 2;
			}
			try {
				local_player_slot = parse_slot_or_error(argv[++i], "--local-player");
			} catch (const exception& e) {
				log("error: %s\n", e.what());
				return 2;
			}
		} else if (strcmp(argv[i], "--enemy-player") == 0) {
			if (i + 1 >= argc) {
				log("error: --enemy-player requires a slot index (0-7)\n");
				return 2;
			}
			try {
				enemy_player_slot = parse_slot_or_error(argv[++i], "--enemy-player");
			} catch (const exception& e) {
				log("error: %s\n", e.what());
				return 2;
			}
		} else if (strcmp(argv[i], "--game-type") == 0) {
			if (i + 1 >= argc) {
				log("error: --game-type requires value melee|ums\n");
				return 2;
			}
			std::string v = argv[++i];
			for (char& c : v) c = (char)std::tolower((unsigned char)c);
			if (v == "melee") game_type_melee = true;
			else if (v == "ums" || v == "use_map_settings") game_type_melee = false;
			else {
				log("error: invalid --game-type value '%s' (expected melee|ums)\n", argv[i]);
				return 2;
			}
		} else if (strcmp(argv[i], "--local-race") == 0) {
			if (i + 1 >= argc) {
				log("error: --local-race requires value zerg|terran|protoss|random\n");
				return 2;
			}
			try {
				local_race = parse_race_or_error(argv[++i], "--local-race");
			} catch (const exception& e) {
				log("error: %s\n", e.what());
				return 2;
			}
		} else if (strcmp(argv[i], "--enemy-race") == 0) {
			if (i + 1 >= argc) {
				log("error: --enemy-race requires value zerg|terran|protoss|random\n");
				return 2;
			}
			try {
				enemy_race = parse_race_or_error(argv[++i], "--enemy-race");
			} catch (const exception& e) {
				log("error: %s\n", e.what());
				return 2;
			}
		} else if (strcmp(argv[i], "--validate-replay") == 0) {
			validate_replay = true;
		} else if (strcmp(argv[i], "--verify-hashes") == 0) {
			if (i + 1 >= argc) {
				log("error: --verify-hashes requires a fixture file path\n");
				return 2;
			}
			verify_hashes_file = argv[++i];
		} else if (strcmp(argv[i], "--record-hashes") == 0) {
			if (i + 1 >= argc) {
				log("error: --record-hashes requires an output fixture file path\n");
				return 2;
			}
			record_hashes_file = argv[++i];
		} else if (strcmp(argv[i], "--hash-interval") == 0) {
			if (i + 1 >= argc) {
				log("error: --hash-interval requires a positive integer\n");
				return 2;
			}
			errno = 0;
			char* end = nullptr;
			long v = std::strtol(argv[++i], &end, 10);
			if (errno != 0 || end == argv[i] || *end != '\0' || v <= 0 || v > std::numeric_limits<int>::max()) {
				log("error: invalid --hash-interval value '%s' (expected positive integer)\n", argv[i]);
				return 2;
			}
			hash_interval = (int)v;
		} else if (strcmp(argv[i], "--headless") == 0) {
			headless = true;
		} else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) {
			replay_file = argv[++i];
		} else if (argv[i][0] == '-') {
			log("error: unknown option '%s'\n", argv[i]);
			return 2;
		} else if (argv[i][0] != '-') {
			replay_file = argv[i];
		}
	}

	if (show_help) {
		print_usage(argv[0]);
		return 0;
	}

	if (bench_frames > 0 && validate_replay) {
		log("error: --bench and --validate-replay cannot be used together\n");
		return 2;
	}
	if (bench_frames > 0 && verify_hashes_file) {
		log("error: --bench and --verify-hashes cannot be used together\n");
		return 2;
	}
	if (bench_frames > 0 && record_hashes_file) {
		log("error: --bench and --record-hashes cannot be used together\n");
		return 2;
	}
	if (validate_replay && verify_hashes_file) {
		log("error: --validate-replay and --verify-hashes cannot be used together\n");
		return 2;
	}
	if (validate_replay && record_hashes_file) {
		log("error: --validate-replay and --record-hashes cannot be used together\n");
		return 2;
	}
	if (verify_hashes_file && record_hashes_file) {
		log("error: --verify-hashes and --record-hashes cannot be used together\n");
		return 2;
	}
	if (map_file && replay_file) {
		log("error: --map cannot be used with --replay/positional replay file\n");
		return 2;
	}
	if (map_file && bench_frames > 0) {
		log("error: --map and --bench cannot be used together\n");
		return 2;
	}
	if (map_file && validate_replay) {
		log("error: --map and --validate-replay cannot be used together\n");
		return 2;
	}
	if (map_file && verify_hashes_file) {
		log("error: --map and --verify-hashes cannot be used together\n");
		return 2;
	}
	if (map_file && record_hashes_file) {
		log("error: --map and --record-hashes cannot be used together\n");
		return 2;
	}
	if ((local_player_slot != -1 || enemy_player_slot != -1 || local_race != 5 || enemy_race != 5 || !game_type_melee) && !map_file) {
		log("error: --map is required when using single-player map options\n");
		return 2;
	}
	if (local_player_slot != -1 && enemy_player_slot != -1 && local_player_slot == enemy_player_slot) {
		log("error: --local-player and --enemy-player must be different slots\n");
		return 2;
	}
	if (record_hashes_file) {
		return run_record_hashes(replay_file, record_hashes_file, hash_interval);
	}
	if (verify_hashes_file) {
		return run_verify_hashes(replay_file, verify_hashes_file);
	}
	if (validate_replay) {
		return run_validate_replay(replay_file);
	}
	if (bench_frames > 0) {
		return run_bench(bench_frames, replay_file);
	}
#endif

	size_t screen_width = 1280;
	size_t screen_height = 800;

	std::chrono::high_resolution_clock clock;
	auto start = clock.now();

#ifdef EMSCRIPTEN
	if (current_width != -1) {
		screen_width = current_width;
		screen_height = current_height;
	}
	auto load_data_file = data_loading::data_files_directory<data_loading::data_files_loader<data_loading::mpq_file<data_loading::js_file_reader<>>>>("");
#else
	auto load_data_file = data_loading::data_files_directory("");
#endif

	game_player player(load_data_file);

	main_t m(std::move(player));
	auto& ui = m.ui;

	m.ui.load_all_image_data(load_data_file);

	ui.load_data_file = [&](a_vector<uint8_t>& data, a_string filename) {
		load_data_file(data, std::move(filename));
	};

	ui.init();

#ifndef EMSCRIPTEN
	if (map_file) {
		ui.is_replay_mode = false;
		ui.is_live_game_mode = true;
		ui.enforce_local_visibility = true;

		int selected_local = -1;
		int selected_enemy = -1;

		game_load_functions load_funcs(ui.st);
		load_funcs.load_map_file(map_file, [&]() {
			static_vector<size_t, 12> available_slots;
			for (auto& v : ui.st.players) {
				if (v.controller == player_t::controller_open || v.controller == player_t::controller_computer) {
					size_t idx = (size_t)(&v - ui.st.players.data());
					if (idx < 8) available_slots.push_back(idx);
					v.controller = player_t::controller_closed;
				}
			}
			if (available_slots.size() < 2) {
				error("%s: not enough open/computer slots for single-player (need 2)", map_file);
			}

			auto contains_slot = [&](int slot) {
				for (size_t v : available_slots) {
					if ((int)v == slot) return true;
				}
				return false;
			};

			selected_local = local_player_slot;
			if (selected_local == -1) selected_local = (int)available_slots.front();
			if (!contains_slot(selected_local)) {
				error("%s: requested local slot %d is not an open/computer slot", map_file, selected_local);
			}

			selected_enemy = enemy_player_slot;
			if (selected_enemy == -1) {
				for (size_t v : available_slots) {
					if ((int)v != selected_local) {
						selected_enemy = (int)v;
						break;
					}
				}
			}
			if (selected_enemy == -1 || selected_enemy == selected_local || !contains_slot(selected_enemy)) {
				error("%s: unable to pick a valid enemy slot (local=%d enemy=%d)", map_file, selected_local, selected_enemy);
			}

			auto& local_player = ui.st.players.at((size_t)selected_local);
			auto& enemy_player = ui.st.players.at((size_t)selected_enemy);
			local_player.controller = player_t::controller_occupied;
			enemy_player.controller = player_t::controller_computer_game;

			if ((int)local_player.race == 5) local_player.race = (race_t)local_race;
			if ((int)enemy_player.race == 5) enemy_player.race = (race_t)enemy_race;

			uint32_t race_seed = (uint32_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
			auto roll_race = [&]() {
				race_seed = race_seed * 22695477u + 1u;
				return (int)((race_seed >> 16) % 3u);
			};
			for (auto& v : ui.st.players) {
				if (v.controller == player_t::controller_occupied || v.controller == player_t::controller_computer_game) {
					if ((int)v.race > 2) v.race = (race_t)roll_race();
				}
			}

			if (game_type_melee) {
				load_funcs.setup_info.victory_condition = 1;
				load_funcs.setup_info.starting_units = 1;
			}

			ui.st.lcg_rand_state = race_seed;
		});

		ui.local_player_id = selected_local;
		ui.enemy_player_id = selected_enemy;
		ui.replay_frame = ui.st.current_frame;
		log("single-player: map='%s' local_slot=%d enemy_slot=%d mode=%s\n",
			map_file,
			selected_local,
			selected_enemy,
			game_type_melee ? "melee" : "ums");
	} else {
		ui.is_replay_mode = true;
		ui.is_live_game_mode = false;
		ui.enforce_local_visibility = false;
		ui.local_player_id = -1;
		ui.enemy_player_id = -1;
		ui.load_replay_file(replay_file ? replay_file : "maps/p49.rep");
		log("replay: file='%s'\n", replay_file ? replay_file : "maps/p49.rep");
	}
#endif

	auto& wnd = ui.wnd;
	wnd.create("test", 0, 0, screen_width, screen_height);

	ui.resize(screen_width, screen_height);
	ui.screen_pos = {(int)ui.game_st.map_width / 2 - (int)screen_width / 2, (int)ui.game_st.map_height / 2 - (int)screen_height / 2};

	ui.set_image_data();

	log("loaded in %dms\n", std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - start).count());

	//set_malloc_fail_handler(malloc_fail_handler);

#ifdef EMSCRIPTEN
	::m = &m;
	::g_m = &m;
	//EM_ASM({js_load_done();});
	emscripten_set_main_loop_arg([](void* ptr) {
		if (!any_replay_loaded) return;
		EM_ASM({js_pre_main_loop();});
		((main_t*)ptr)->update();
		EM_ASM({js_post_main_loop();});
	}, &m, 0, 1);
#else
	::g_m = &m;

	if (headless) {
		// Headless mode: run the simulation forward without rendering.
		// Useful for profiling simulation throughput or bot testing.
		if (ui.is_replay_mode) {
			while (!ui.is_done()) {
				ui.replay_functions::next_frame();
			}
		} else {
			log("single-player headless: stepping until local victory/defeat\n");
			while (true) {
				ui.state_functions::next_frame();
				if (ui.has_local_player()) {
					if (ui.player_won(ui.local_player_id)) {
						log("single-player headless: victory at frame %d\n", ui.st.current_frame);
						break;
					}
					if (ui.player_defeated(ui.local_player_id)) {
						log("single-player headless: defeat at frame %d\n", ui.st.current_frame);
						break;
					}
				}
			}
		}
	} else {
		// Normal display loop with adaptive sleep: sleep only until the next
		// tick is due rather than a fixed 20 ms, reducing latency jitter.
		while (true) {
			auto frame_start = clock.now();
			m.update();
			// Sleep adaptively: target ~16ms frame time (roughly 60 Hz UI).
			// std::chrono arithmetic keeps us from accumulating drift.
			auto frame_elapsed = clock.now() - frame_start;
			auto target = std::chrono::milliseconds(16);
			if (frame_elapsed < target) {
				std::this_thread::sleep_for(target - frame_elapsed);
			}
		}
	}
#endif
	::g_m = nullptr;

	return 0;
}

