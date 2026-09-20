#define SDL_MAIN_HANDLED

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

#include "ui.h"
#include "common.h"
#include "bwgame.h"
#include "replay.h"
#include "../replay_saver.h"
#include "../serialization.h"

#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <cctype>
#include <cerrno>
#include <limits>
#include <algorithm>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "../perf_metrics.h"

using namespace bwgame;

using ui::log;

FILE* log_file = nullptr;

namespace {

std::string g_data_dir;

bool file_exists(const std::string& path) {
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return false;
	fclose(f);
	return true;
}

void create_directory_if_not_exists(const std::string& path) {
#ifdef _WIN32
	CreateDirectoryA(path.c_str(), nullptr);
#else
	mkdir(path.c_str(), 0755);
#endif
}

std::string normalize_dir_path(std::string path) {
	while (!path.empty() && (path.back() == '/' || path.back() == '\\')) path.pop_back();
	return path;
}

void push_unique_dir(std::vector<std::string>& dst, std::string path) {
	path = normalize_dir_path(std::move(path));
	if (path.empty()) path = ".";
	if (std::find(dst.begin(), dst.end(), path) == dst.end()) dst.push_back(std::move(path));
}

std::string join_path(std::string base, const char* leaf) {
	if (base.empty() || base == ".") return leaf;
	char last = base.back();
	if (last != '/' && last != '\\') base += '/';
	base += leaf;
	return base;
}

std::string executable_directory() {
#ifdef _WIN32
	char buffer[MAX_PATH];
	DWORD n = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
	if (n == 0 || n >= MAX_PATH) return {};
	std::string path(buffer, n);
	size_t pos = path.find_last_of("\\/");
	if (pos == std::string::npos) return {};
	return path.substr(0, pos);
#else
	return {};
#endif
}

bool has_required_mpqs(const std::string& dir) {
	return file_exists(join_path(dir, "StarDat.mpq")) &&
	       file_exists(join_path(dir, "BrooDat.mpq")) &&
	       (file_exists(join_path(dir, "Patch_rt.mpq")) || file_exists(join_path(dir, "patch_rt.mpq")));
}

std::string describe_required_mpqs(const std::string& dir) {
	return format("expected '%s', '%s', and '%s'",
		join_path(dir, "StarDat.mpq").c_str(),
		join_path(dir, "BrooDat.mpq").c_str(),
		join_path(dir, "Patch_rt.mpq").c_str()).c_str();
}

std::string resolve_data_dir_or_throw(const char* argv0, const char* explicit_data_dir) {
	std::vector<std::string> candidates;
	if (explicit_data_dir && explicit_data_dir[0] != '\0') push_unique_dir(candidates, explicit_data_dir);

	if (const char* env_dir = std::getenv("OPENSNOWSTORM_DATA_DIR")) {
		if (*env_dir) push_unique_dir(candidates, env_dir);
	}

	push_unique_dir(candidates, ".");
	push_unique_dir(candidates, "data");

	std::string exe_dir = executable_directory();
	if (!exe_dir.empty()) {
		push_unique_dir(candidates, exe_dir);
		push_unique_dir(candidates, join_path(exe_dir, "data"));
	}

#ifdef _WIN32
	push_unique_dir(candidates, "C:/StarCraft");
	push_unique_dir(candidates, "C:/Program Files (x86)/StarCraft");
	push_unique_dir(candidates, "C:/Program Files/StarCraft");
	push_unique_dir(candidates, "D:/Games/StarCraft");
	push_unique_dir(candidates, "D:/Games/Starcraft");
	push_unique_dir(candidates, "E:/Starcraft");
	push_unique_dir(candidates, "E:/StarCraft");
#endif

	for (const std::string& dir : candidates) {
		if (has_required_mpqs(dir)) return dir == "." ? std::string() : dir;
	}

	if (explicit_data_dir && explicit_data_dir[0] != '\0') {
		error("Brood War data directory '%s' is incomplete; %s",
			explicit_data_dir,
			describe_required_mpqs(explicit_data_dir).c_str());
	}

	std::ostringstream oss;
	oss << "unable to locate Brood War data files. Searched:";
	for (const std::string& dir : candidates) {
		oss << "\n  - " << dir;
	}
	oss << "\nPass --data-dir <path> or set OPENSNOWSTORM_DATA_DIR to a folder containing "
	       "StarDat.mpq, BrooDat.mpq, and Patch_rt.mpq.";
	error("%s", oss.str().c_str());
	return {};
}

auto make_load_data_file() {
	return data_loading::data_files_directory(g_data_dir);
}

bool default_replay_exists() {
	return file_exists("maps/p49.rep");
}

enum class startup_content_kind {
	map,
	replay,
	campaign_browser,
	save,
	settings
};

struct startup_entry {
	startup_content_kind kind = startup_content_kind::map;
	std::string path;
	std::string title;
	std::string subtitle;
};

struct campaign_mission {
	std::string name;
	std::string path;
};

struct campaign_episode {
	std::string title;
	std::string subtitle;
	std::vector<campaign_mission> missions;
};

enum class frontend_view {
	startup,
	episodes,
	missions,
	settings
};

std::vector<campaign_episode> get_all_campaigns() {
	std::vector<campaign_episode> eps;

	// Original StarCraft
	eps.push_back({"EPISODE I", "REBEL YELL (TERRAN)", {
		{"1: WASTELAND", "campaign/terran/terran01.scx"},
		{"2: BACKWATER STATION", "campaign/terran/terran02.scx"},
		{"3: DESPERATE ALLIANCE", "campaign/terran/terran03.scx"},
		{"4: THE JACOBS INSTALLATION", "campaign/terran/terran04.scx"},
		{"5: REVOLUTION", "campaign/terran/terran05.scx"},
		{"6: ON THE DEADMAN'S TRAIL", "campaign/terran/terran06.scx"},
		{"7: THE TRUMP CARD", "campaign/terran/terran07.scx"},
		{"8: THE BIG PUSH", "campaign/terran/terran08.scx"},
		{"9: NEW GETTYSBURG", "campaign/terran/terran09.scx"},
		{"10: THE HAMMER FALLS", "campaign/terran/terran10.scx"}
	}});

	eps.push_back({"EPISODE II", "OVERMIND (ZERG)", {
		{"1: AMONG THE RUINS", "campaign/zerg/zerg01.scx"},
		{"2: EGRESSION", "campaign/zerg/zerg02.scx"},
		{"3: THE NEW DOMINION", "campaign/zerg/zerg03.scx"},
		{"4: AGENT OF THE SWARM", "campaign/zerg/zerg04.scx"},
		{"5: THE AMERIGO", "campaign/zerg/zerg05.scx"},
		{"6: THE TEMPERED MIRROR", "campaign/zerg/zerg06.scx"},
		{"7: CULLING THE HERD", "campaign/zerg/zerg07.scx"},
		{"8: EYE FOR AN EYE", "campaign/zerg/zerg08.scx"},
		{"9: THE INVASION OF AIUR", "campaign/zerg/zerg09.scx"},
		{"10: FULL CIRCLE", "campaign/zerg/zerg10.scx"}
	}});

	eps.push_back({"EPISODE III", "THE FALL (PROTOSS)", {
		{"1: FIRST STRIKE", "campaign/protoss/protoss01.scx"},
		{"2: INTO THE FLAMES", "campaign/protoss/protoss02.scx"},
		{"3: HIGHER GROUND", "campaign/protoss/protoss03.scx"},
		{"4: THE HUNT FOR TASSADAR", "campaign/protoss/protoss04.scx"},
		{"5: CHOOSING SIDES", "campaign/protoss/protoss05.scx"},
		{"6: INTO THE DARKNESS", "campaign/protoss/protoss06.scx"},
		{"7: HOMELAND", "campaign/protoss/protoss07.scx"},
		{"8: THE TRIAL OF TASSADAR", "campaign/protoss/protoss08.scx"},
		{"9: SHADOW HUNTERS", "campaign/protoss/protoss09.scx"},
		{"10: EYE OF THE STORM", "campaign/protoss/protoss10.scx"}
	}});

	// Brood War Expansion
	eps.push_back({"EPISODE IV", "THE STAND (PROTOSS)", {
		{"1: ESCAPE FROM AIUR", "campaign/protxp/protxp01.scx"},
		{"2: DUNES OF SHAKURAS", "campaign/protxp/protxp02.scx"},
		{"3: LEGACY OF THE XEL'NAGA", "campaign/protxp/protxp03.scx"},
		{"4: THE QUEST FOR URAJ", "campaign/protxp/protxp04.scx"},
		{"5: THE BATTLE OF BRAXIS", "campaign/protxp/protxp05.scx"},
		{"6: RETURN TO CHAR", "campaign/protxp/protxp06.scx"},
		{"7: THE INSURGENT", "campaign/protxp/protxp07.scx"},
		{"8: COUNTDOWN", "campaign/protxp/protxp08.scx"}
	}});

	eps.push_back({"EPISODE V", "THE IRON FIST (TERRAN)", {
		{"1: FIRST STRIKE", "campaign/terrxp/terrxp01.scx"},
		{"2: THE DYLARIAN SHIPYARDS", "campaign/terrxp/terrxp02.scx"},
		{"3: RUINS OF TARSONIS", "campaign/terrxp/terrxp03.scx"},
		{"4: ASSAULT ON KORHAL", "campaign/terrxp/terrxp04.scx"},
		{"5: EMPEROR'S FALL", "campaign/terrxp/terrxp05.scx"},
		{"6: EMPEROR'S FLIGHT", "campaign/terrxp/terrxp06.scx"},
		{"7: PATRIOT'S BLOOD", "campaign/terrxp/terrxp07.scx"},
		{"8: GROUND ZERO", "campaign/terrxp/terrxp08.scx"}
	}});

	eps.push_back({"EPISODE VI", "QUEEN OF BLADES (ZERG)", {
		{"1: VILE DISRUPTION", "campaign/zergxp/zergxp01.scx"},
		{"2: REIGN OF FIRE", "campaign/zergxp/zergxp02.scx"},
		{"3: THE KEL-MORIAN COMBINE", "campaign/zergxp/zergxp03.scx"},
		{"4: THE LIBERATION OF KORHAL", "campaign/zergxp/zergxp04.scx"},
		{"5: TRUE COLORS", "campaign/zergxp/zergxp05.scx"},
		{"6: FURY OF THE SWARM", "campaign/zergxp/zergxp06.scx"},
		{"7: DRAWING OF THE WEB", "campaign/zergxp/zergxp07.scx"},
		{"8: TO KILL A FLEDGLING", "campaign/zergxp/zergxp08.scx"},
		{"9: THE RECKONING", "campaign/zergxp/zergxp09.scx"},
		{"10: OMEGA", "campaign/zergxp/zergxp10.scx"}
	}});

	return eps;
}

std::string lowercase_copy(std::string value) {
	for (char& c : value) c = (char)std::tolower((unsigned char)c);
	return value;
}

std::string uppercase_copy(std::string value) {
	for (char& c : value) c = (char)std::toupper((unsigned char)c);
	return value;
}

std::string canonicalize_path_for_compare(std::string path) {
	for (char& c : path) {
		if (c == '\\') c = '/';
	}
	return lowercase_copy(std::move(path));
}

std::string path_basename(const std::string& path) {
	size_t pos = path.find_last_of("/\\");
	if (pos == std::string::npos) return path;
	return path.substr(pos + 1);
}

std::string strip_extension(const std::string& path) {
	size_t pos = path.find_last_of('.');
	if (pos == std::string::npos) return path;
	return path.substr(0, pos);
}

std::string humanize_map_stem(const std::string& path) {
	std::string stem = strip_extension(path_basename(path));
	if (stem.empty()) stem = path;
	for (char& c : stem) {
		if (c == '_' || c == '-') c = ' ';
	}
	return stem;
}

std::string shorten_middle(const std::string& text, size_t max_chars) {
	if (text.size() <= max_chars) return text;
	if (max_chars <= 3) return text.substr(0, max_chars);
	size_t head = (max_chars - 3) / 2;
	size_t tail = max_chars - 3 - head;
	return text.substr(0, head) + "..." + text.substr(text.size() - tail);
}

bool has_extension_ci(const std::string& path, const char* extension) {
	std::string lower_path = lowercase_copy(path);
	std::string lower_ext = lowercase_copy(extension);
	if (lower_path.size() < lower_ext.size()) return false;
	return lower_path.compare(lower_path.size() - lower_ext.size(), lower_ext.size(), lower_ext) == 0;
}

std::vector<uint8_t> read_binary_file_or_throw(const char* path) {
	std::ifstream f(path, std::ios::binary);
	if (!f) error("unable to open file '%s': %s", path, std::strerror(errno));
	f.seekg(0, std::ios::end);
	std::streamoff size = f.tellg();
	if (size < 0) error("unable to determine size of '%s'", path);
	std::vector<uint8_t> data((size_t)size);
	f.seekg(0, std::ios::beg);
	if (!data.empty()) {
		f.read((char*)data.data(), size);
		if (!f) error("failed while reading '%s'", path);
	}
	return data;
}

void write_binary_file_or_throw(const char* path, const uint8_t* data, size_t size) {
	std::ofstream f(path, std::ios::binary | std::ios::out | std::ios::trunc);
	if (!f) error("unable to open '%s' for writing: %s", path, std::strerror(errno));
	if (size) f.write((const char*)data, (std::streamsize)size);
	if (!f) error("failed while writing '%s'", path);
}

std::vector<uint8_t> load_scenario_chk_bytes_or_throw(const char* map_file) {
	if (has_extension_ci(map_file, ".chk")) return read_binary_file_or_throw(map_file);
	data_loading::mpq_file<> mpq{a_string(map_file)};
	a_vector<uint8_t> chk;
	mpq(chk, "staredit/scenario.chk");
	return std::vector<uint8_t>(chk.begin(), chk.end());
}

std::string make_startup_title(startup_content_kind kind, const std::string& path) {
	std::string stem = humanize_map_stem(path);
	std::string lower_path = lowercase_copy(path);
	if (kind == startup_content_kind::save) return "Saved Game  " + stem;
	if (kind == startup_content_kind::replay) return "Replay  " + stem;
	if (lower_path.find("campaign") != std::string::npos || lower_path.find("broodwar") != std::string::npos) {
		return "Campaign  " + stem;
	}
	return "Map  " + stem;
}

bool startup_entry_exists(const std::vector<startup_entry>& entries, const std::string& path) {
	std::string normalized = canonicalize_path_for_compare(path);
	for (const startup_entry& entry : entries) {
		if (canonicalize_path_for_compare(entry.path) == normalized) return true;
	}
	return false;
}

void push_startup_entry(std::vector<startup_entry>& entries, startup_content_kind kind, const std::string& path) {
	if (path.empty() || startup_entry_exists(entries, path)) return;
	startup_entry entry;
	entry.kind = kind;
	entry.path = path;
	entry.title = make_startup_title(kind, path);
	entry.subtitle = shorten_middle(path, 72);
	entries.push_back(std::move(entry));
}

struct directory_entry_info {
	std::string path;
	bool is_directory = false;
};

std::vector<directory_entry_info> list_directory_entries(const std::string& dir) {
	std::vector<directory_entry_info> result;
#ifdef _WIN32
	std::string pattern = join_path(dir, "*");
	WIN32_FIND_DATAA find_data;
	HANDLE h = FindFirstFileA(pattern.c_str(), &find_data);
	if (h == INVALID_HANDLE_VALUE) return result;
	do {
		const char* name = find_data.cFileName;
		if (!name || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
		directory_entry_info entry;
		entry.path = join_path(dir, name);
		entry.is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		result.push_back(std::move(entry));
	} while (FindNextFileA(h, &find_data));
	FindClose(h);
#else
	DIR* dp = opendir(dir.c_str());
	if (!dp) return result;
	while (dirent* dent = readdir(dp)) {
		const char* name = dent->d_name;
		if (!name || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
		directory_entry_info entry;
		entry.path = join_path(dir, name);
		bool is_directory = false;
#ifdef DT_DIR
		if (dent->d_type == DT_DIR) {
			is_directory = true;
		} else if (dent->d_type == DT_UNKNOWN) {
			struct stat st;
			if (stat(entry.path.c_str(), &st) == 0) is_directory = S_ISDIR(st.st_mode);
		}
#else
		struct stat st;
		if (stat(entry.path.c_str(), &st) == 0) is_directory = S_ISDIR(st.st_mode);
#endif
		entry.is_directory = is_directory;
		result.push_back(std::move(entry));
	}
	closedir(dp);
#endif
	std::sort(result.begin(), result.end(), [](const directory_entry_info& a, const directory_entry_info& b) {
		return canonicalize_path_for_compare(a.path) < canonicalize_path_for_compare(b.path);
	});
	return result;
}

void scan_startup_directory(
	const std::string& dir,
	int depth,
	int max_depth,
	std::vector<startup_entry>& out,
	size_t max_entries
) {
	if (dir.empty() || depth > max_depth || out.size() >= max_entries) return;
	for (const directory_entry_info& entry : list_directory_entries(dir)) {
		if (out.size() >= max_entries) break;
		if (entry.is_directory) {
			scan_startup_directory(entry.path, depth + 1, max_depth, out, max_entries);
			continue;
		}
		if (has_extension_ci(entry.path, ".scx") || has_extension_ci(entry.path, ".scm")) {
			push_startup_entry(out, startup_content_kind::map, entry.path);
		} else if (has_extension_ci(entry.path, ".rep")) {
			push_startup_entry(out, startup_content_kind::replay, entry.path);
		} else if (has_extension_ci(entry.path, ".osv")) {
			push_startup_entry(out, startup_content_kind::save, entry.path);
		}
	}
}

int startup_entry_priority(const startup_entry& entry) {
	// "Continue" entries (resume last-played campaign mission) are pinned to
	// the top so a player pressing Enter at the frontend resumes immediately.
	if (entry.title.compare(0, 9, "Continue ") == 0) return -1;
	if (entry.kind == startup_content_kind::campaign_browser) return 0;
	if (entry.kind == startup_content_kind::settings) return 1;
	if (entry.kind == startup_content_kind::save) return 2;
	std::string lower = lowercase_copy(entry.path);
	bool campaign_like = lower.find("campaign") != std::string::npos || lower.find("broodwar") != std::string::npos;
	if (entry.kind == startup_content_kind::map && campaign_like) return 5;
	if (entry.kind == startup_content_kind::map) return 20;
	if (lower.find("p49.rep") != std::string::npos) return 60;
	return 40;
}

std::string read_last_campaign_map();
void write_campaign_progress(const std::string& map_path, bool completed);
std::string find_next_campaign_map_in_folder(const std::string& current_map);

std::vector<startup_entry> discover_startup_entries(const std::string& data_dir) {
	std::vector<startup_entry> entries;

	// Inject a "Continue" entry at the top if a campaign_progress.txt file
	// exists and points to a reachable map, so relaunching the client resumes
	// exactly where the player left off.
	std::string resume = read_last_campaign_map();
	if (!resume.empty() && file_exists(resume)) {
		startup_entry entry;
		entry.kind = startup_content_kind::map;
		entry.path = resume;
		entry.title = std::string("Continue  ") + strip_extension(path_basename(resume));
		entry.subtitle = shorten_middle(resume, 72);
		entries.push_back(std::move(entry));
	}

	push_startup_entry(entries, startup_content_kind::replay, "maps/p49.rep");
	{
		startup_entry entry;
		entry.kind = startup_content_kind::campaign_browser;
		entry.title = "SINGLE PLAYER CAMPAIGNS";
		entry.subtitle = "PLAY STARCRAFT AND BROOD WAR MISSIONS";
		entries.push_back(std::move(entry));
	}
	{
		startup_entry entry;
		entry.kind = startup_content_kind::settings;
		entry.title = "SETTINGS";
		entry.subtitle = "SPEED, VOLUME, SCROLL, DISPLAY";
		entries.push_back(std::move(entry));
	}

	std::vector<std::string> candidate_dirs;
	push_unique_dir(candidate_dirs, "maps");
	push_unique_dir(candidate_dirs, "Maps");
	push_unique_dir(candidate_dirs, "saves");
	if (!data_dir.empty()) {
		push_unique_dir(candidate_dirs, data_dir);
		push_unique_dir(candidate_dirs, join_path(data_dir, "maps"));
		push_unique_dir(candidate_dirs, join_path(data_dir, "Maps"));
	}
	std::string exe_dir = executable_directory();
	if (!exe_dir.empty()) {
		push_unique_dir(candidate_dirs, exe_dir);
		push_unique_dir(candidate_dirs, join_path(exe_dir, "maps"));
		push_unique_dir(candidate_dirs, join_path(exe_dir, "Maps"));
	}

	for (const std::string& dir : candidate_dirs) {
		scan_startup_directory(dir, 0, 3, entries, 8);
		if (entries.size() >= 8) break;
	}

	std::sort(entries.begin(), entries.end(), [](const startup_entry& a, const startup_entry& b) {
		int ap = startup_entry_priority(a);
		int bp = startup_entry_priority(b);
		if (ap != bp) return ap < bp;
		return canonicalize_path_for_compare(a.title) < canonicalize_path_for_compare(b.title);
	});
	if (entries.size() > 8) entries.resize(8);
	return entries;
}

// ---------------------------------------------------------------------------
// Campaign progress persistence.
//
// The campaign progress file tracks the last-started mission so a player can
// close the client and resume the campaign later.  It is intentionally
// minimal: a single line with the last-launched map path.  This does NOT
// checkpoint mid-mission state (that requires full state serialisation), but
// closes the gap where quitting between missions would otherwise lose the
// player's place in the campaign chain.
// ---------------------------------------------------------------------------
const char* k_campaign_progress_file = "campaign_progress.txt";

std::string read_last_campaign_map() {
	std::ifstream f(k_campaign_progress_file);
	if (!f) return {};
	std::string line;
	std::string result;
	while (std::getline(f, line)) {
		while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
		size_t pos = 0;
		while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
		if (pos >= line.size() || line[pos] == '#') continue;
		const std::string prefix = "map:";
		if (line.compare(pos, prefix.size(), prefix) == 0) {
			pos += prefix.size();
			while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
			result = line.substr(pos);
			break;
		}
	}
	return result;
}

void write_campaign_progress(const std::string& map_path, bool completed) {
	std::ofstream f(k_campaign_progress_file, std::ios::out | std::ios::trunc);
	if (!f) return;
	f << "# OpenSnowstorm campaign progress v1\n";
	f << "map: " << map_path << "\n";
	if (completed) f << "completed: 1\n";
}

// Find the chronologically next campaign-like map in the same folder as the
// current map, by case-insensitive filename sort.  Used as a fallback when a
// mission ends without a Set Next Scenario trigger action firing, which is
// common for custom campaign packs that rely on external orchestration.
std::string find_next_campaign_map_in_folder(const std::string& current_map) {
	if (current_map.empty()) return {};
	std::string dir;
	size_t sep = current_map.find_last_of("/\\");
	if (sep != std::string::npos) dir = current_map.substr(0, sep);
	if (dir.empty()) dir = ".";

	std::string cur_name = canonicalize_path_for_compare(path_basename(current_map));
	std::vector<std::string> candidates;
	for (const directory_entry_info& entry : list_directory_entries(dir)) {
		if (entry.is_directory) continue;
		if (has_extension_ci(entry.path, ".scx") || has_extension_ci(entry.path, ".scm")) {
			candidates.push_back(entry.path);
		}
	}
	std::sort(candidates.begin(), candidates.end(), [](const std::string& a, const std::string& b) {
		return canonicalize_path_for_compare(path_basename(a)) < canonicalize_path_for_compare(path_basename(b));
	});
	for (size_t i = 0; i < candidates.size(); ++i) {
		if (canonicalize_path_for_compare(path_basename(candidates[i])) == cur_name) {
			if (i + 1 < candidates.size()) return candidates[i + 1];
			return {};
		}
	}
	return {};
}

constexpr uint32_t rgba32(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
	return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

const std::array<uint8_t, 7>& glyph_rows(char c) {
	static const std::array<uint8_t, 7> space = {{0, 0, 0, 0, 0, 0, 0}};
	static const std::array<uint8_t, 7> dash = {{0, 0, 0, 31, 0, 0, 0}};
	static const std::array<uint8_t, 7> dot = {{0, 0, 0, 0, 0, 12, 12}};
	static const std::array<uint8_t, 7> colon = {{0, 12, 12, 0, 12, 12, 0}};
	static const std::array<uint8_t, 7> slash = {{1, 2, 4, 8, 16, 0, 0}};
	static const std::array<uint8_t, 7> underscore = {{0, 0, 0, 0, 0, 0, 31}};
	static const std::array<uint8_t, 7> left_paren = {{2, 4, 8, 8, 8, 4, 2}};
	static const std::array<uint8_t, 7> right_paren = {{8, 4, 2, 2, 2, 4, 8}};
	static const std::array<uint8_t, 7> question = {{14, 17, 1, 2, 4, 0, 4}};
	static const std::array<uint8_t, 7> zero = {{14, 17, 19, 21, 25, 17, 14}};
	static const std::array<uint8_t, 7> one = {{4, 12, 4, 4, 4, 4, 14}};
	static const std::array<uint8_t, 7> two = {{14, 17, 1, 2, 4, 8, 31}};
	static const std::array<uint8_t, 7> three = {{30, 1, 1, 14, 1, 1, 30}};
	static const std::array<uint8_t, 7> four = {{2, 6, 10, 18, 31, 2, 2}};
	static const std::array<uint8_t, 7> five = {{31, 16, 16, 30, 1, 1, 30}};
	static const std::array<uint8_t, 7> six = {{14, 16, 16, 30, 17, 17, 14}};
	static const std::array<uint8_t, 7> seven = {{31, 1, 2, 4, 8, 8, 8}};
	static const std::array<uint8_t, 7> eight = {{14, 17, 17, 14, 17, 17, 14}};
	static const std::array<uint8_t, 7> nine = {{14, 17, 17, 15, 1, 1, 14}};
	static const std::array<uint8_t, 7> A = {{14, 17, 17, 31, 17, 17, 17}};
	static const std::array<uint8_t, 7> B = {{30, 17, 17, 30, 17, 17, 30}};
	static const std::array<uint8_t, 7> C = {{14, 17, 16, 16, 16, 17, 14}};
	static const std::array<uint8_t, 7> D = {{30, 17, 17, 17, 17, 17, 30}};
	static const std::array<uint8_t, 7> E = {{31, 16, 16, 30, 16, 16, 31}};
	static const std::array<uint8_t, 7> F = {{31, 16, 16, 30, 16, 16, 16}};
	static const std::array<uint8_t, 7> G = {{14, 17, 16, 23, 17, 17, 14}};
	static const std::array<uint8_t, 7> H = {{17, 17, 17, 31, 17, 17, 17}};
	static const std::array<uint8_t, 7> I = {{14, 4, 4, 4, 4, 4, 14}};
	static const std::array<uint8_t, 7> J = {{1, 1, 1, 1, 17, 17, 14}};
	static const std::array<uint8_t, 7> K = {{17, 18, 20, 24, 20, 18, 17}};
	static const std::array<uint8_t, 7> L = {{16, 16, 16, 16, 16, 16, 31}};
	static const std::array<uint8_t, 7> M = {{17, 27, 21, 17, 17, 17, 17}};
	static const std::array<uint8_t, 7> N = {{17, 25, 21, 19, 17, 17, 17}};
	static const std::array<uint8_t, 7> O = {{14, 17, 17, 17, 17, 17, 14}};
	static const std::array<uint8_t, 7> P = {{30, 17, 17, 30, 16, 16, 16}};
	static const std::array<uint8_t, 7> Q = {{14, 17, 17, 17, 21, 18, 13}};
	static const std::array<uint8_t, 7> R = {{30, 17, 17, 30, 20, 18, 17}};
	static const std::array<uint8_t, 7> S = {{15, 16, 16, 14, 1, 1, 30}};
	static const std::array<uint8_t, 7> T = {{31, 4, 4, 4, 4, 4, 4}};
	static const std::array<uint8_t, 7> U = {{17, 17, 17, 17, 17, 17, 14}};
	static const std::array<uint8_t, 7> V = {{17, 17, 17, 17, 17, 10, 4}};
	static const std::array<uint8_t, 7> W = {{17, 17, 17, 17, 21, 27, 17}};
	static const std::array<uint8_t, 7> X = {{17, 17, 10, 4, 10, 17, 17}};
	static const std::array<uint8_t, 7> Y = {{17, 17, 10, 4, 4, 4, 4}};
	static const std::array<uint8_t, 7> Z = {{31, 1, 2, 4, 8, 16, 31}};
	switch ((char)std::toupper((unsigned char)c)) {
	case ' ': return space;
	case '-': return dash;
	case '.': return dot;
	case ':': return colon;
	case '/': return slash;
	case '\\': return slash;
	case '_': return underscore;
	case '(': return left_paren;
	case ')': return right_paren;
	case '0': return zero;
	case '1': return one;
	case '2': return two;
	case '3': return three;
	case '4': return four;
	case '5': return five;
	case '6': return six;
	case '7': return seven;
	case '8': return eight;
	case '9': return nine;
	case 'A': return A;
	case 'B': return B;
	case 'C': return C;
	case 'D': return D;
	case 'E': return E;
	case 'F': return F;
	case 'G': return G;
	case 'H': return H;
	case 'I': return I;
	case 'J': return J;
	case 'K': return K;
	case 'L': return L;
	case 'M': return M;
	case 'N': return N;
	case 'O': return O;
	case 'P': return P;
	case 'Q': return Q;
	case 'R': return R;
	case 'S': return S;
	case 'T': return T;
	case 'U': return U;
	case 'V': return V;
	case 'W': return W;
	case 'X': return X;
	case 'Y': return Y;
	case 'Z': return Z;
	default: return question;
	}
}

void fill_rgba_rect(uint32_t* pixels, int pitch, int width, int height, int x, int y, int w, int h, uint32_t color) {
	if (!pixels || w <= 0 || h <= 0) return;
	int x0 = std::max(0, x);
	int y0 = std::max(0, y);
	int x1 = std::min(width, x + w);
	int y1 = std::min(height, y + h);
	if (x0 >= x1 || y0 >= y1) return;
	for (int py = y0; py < y1; ++py) {
		uint32_t* row = pixels + py * pitch;
		for (int px = x0; px < x1; ++px) {
			row[px] = color;
		}
	}
}

void draw_rgba_frame(uint32_t* pixels, int pitch, int width, int height, int x, int y, int w, int h, int thickness, uint32_t color) {
	fill_rgba_rect(pixels, pitch, width, height, x, y, w, thickness, color);
	fill_rgba_rect(pixels, pitch, width, height, x, y + h - thickness, w, thickness, color);
	fill_rgba_rect(pixels, pitch, width, height, x, y, thickness, h, color);
	fill_rgba_rect(pixels, pitch, width, height, x + w - thickness, y, thickness, h, color);
}

int text_pixel_width(const std::string& text, int scale) {
	if (text.empty()) return 0;
	return (int)text.size() * 6 * scale - scale;
}

void draw_rgba_text(uint32_t* pixels, int pitch, int width, int height, int x, int y, const std::string& text, int scale, uint32_t color) {
	if (!pixels || scale <= 0) return;
	int cursor_x = x;
	for (char c : text) {
		const auto& rows = glyph_rows(c);
		for (int row = 0; row < 7; ++row) {
			uint8_t bits = rows[(size_t)row];
			for (int col = 0; col < 5; ++col) {
				if (bits & (1 << (4 - col))) {
					fill_rgba_rect(pixels, pitch, width, height, cursor_x + col * scale, y + row * scale, scale, scale, color);
				}
			}
		}
		cursor_x += 6 * scale;
	}
}

std::vector<uint8_t> decode_grp_frame_indices(const grp_t::frame_t& frame) {
	std::vector<uint8_t> buf(frame.size.x * frame.size.y, 0);
	for (size_t y = 0; y < frame.size.y; ++y) {
		const uint8_t* d = frame.data_container.data() + frame.line_data_offset[y];
		size_t x = 0;
		while (x < frame.size.x) {
			int v = *d++;
			if (v & 0x80) {
				x += v & 0x7f;
			} else if (v & 0x40) {
				int count = v & 0x3f;
				int c = *d++;
				for (int i = 0; i < count && x < frame.size.x; ++i, ++x)
					buf[y * frame.size.x + x] = (uint8_t)c;
			} else {
				for (int i = 0; i < v && x < frame.size.x; ++i, ++x)
					buf[y * frame.size.x + x] = *d++;
			}
		}
	}
	return buf;
}

void draw_grp_scaled_rgba(uint32_t* pixels, int pitch, int width, int height,
                           const grp_t::frame_t& frame, const a_vector<uint8_t>& wpe,
                           int px, int py, int target_w, int target_h) {
	auto indices = decode_grp_frame_indices(frame);
	float sx = (float)frame.size.x / target_w;
	float sy = (float)frame.size.y / target_h;
	for (int ty = 0; ty < target_h; ++ty) {
		int src_y = (int)(ty * sy);
		if (src_y < 0 || src_y >= (int)frame.size.y) continue;
		int dy = py + ty;
		if (dy < 0 || dy >= height) continue;
		for (int tx = 0; tx < target_w; ++tx) {
			int src_x = (int)(tx * sx);
			if (src_x < 0 || src_x >= (int)frame.size.x) continue;
			uint8_t idx = indices[src_y * frame.size.x + src_x];
			if (idx == 0) continue;
			int dx = px + tx;
			if (dx < 0 || dx >= width) continue;
			pixels[dy * pitch + dx] = rgba32(wpe[idx * 4 + 0], wpe[idx * 4 + 1], wpe[idx * 4 + 2]);
		}
	}
}

}

namespace bwgame {

namespace ui {

void log_str(a_string str) {
	fwrite(str.data(), str.size(), 1, stdout);
	fflush(stdout);
	if (!log_file) log_file = fopen("gfxtest_log.txt", "ab");
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

enum class map_game_type_t {
	auto_detect,
	melee,
	ums
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

	// Campaign state preserved across mission transitions.
	std::string current_map_file; // path of the currently loaded map (native only)
	int campaign_local_player_slot = -1;
	int campaign_enemy_player_slot = -1;
	map_game_type_t campaign_game_type = map_game_type_t::auto_detect;
	int campaign_local_race = 5;
	int campaign_enemy_race = 5;
	bool campaign_fog_of_war = true;

	a_map<int, std::unique_ptr<saved_state>> saved_states;
	std::unique_ptr<saved_state> quicksave_slot;
	bool frontend_active = false;
	frontend_view frontend_current_view = frontend_view::startup;
	int frontend_selected_episode = -1;
	std::vector<startup_entry> frontend_entries;
	bool sound_enabled = true;
	int frontend_selected_index = 0;
	std::string frontend_status_message;
	std::unique_ptr<native_window_drawing::surface> frontend_surface;

	// When a campaign-like map is loaded we auto-pause on the first frame and
	// push a "press space to begin" briefing banner.  The briefing_armed flag
	// tracks whether the player still needs to dismiss the briefing; any pause
	// toggle (Space/P) while armed clears it so the simulation begins stepping.
	bool briefing_armed = false;

	// Post-mission result snapshot.  Populated once when victory or defeat
	// is detected, then rendered by the overlay callback until the player
	// dismisses it.  Debrief body lines are pre-formatted here so the
	// overlay does not allocate per frame.
	struct mission_result_t {
		bool active = false;
		bool won = false;
		bool next_mission_ready = false;
		std::string header;
		uint32_t header_color = 0;
		uint32_t veil_color = 0;
		std::vector<std::string> lines;
	};
	mission_result_t mission_result;

	std::string current_map_display_name;
	std::string pending_next_map_file;

	// Cached soft-wrapped + uppercased objectives text.  Rebuilt lazily when
	// ui.current_objectives_text changes so draw_objectives_overlay does not
	// reallocate every frame.
	std::string cached_objectives_source;
	std::vector<std::string> cached_objectives_lines;

	void save_replay_state_if_missing(int frame) {
		auto i = saved_states.find(frame);
		if (i != saved_states.end()) return;

		auto v = std::make_unique<saved_state>();
		v->st = copy_state(ui.st);
		v->action_st = copy_state(ui.action_st, ui.st, v->st);
		v->apm = ui.apm;

		a_map<int, std::unique_ptr<saved_state>> new_saved_states;
		new_saved_states[frame] = std::move(v);
		while (!saved_states.empty()) {
			auto e = saved_states.begin();
			auto entry = std::move(*e);
			saved_states.erase(e);
			new_saved_states[entry.first] = std::move(entry.second);
		}
		std::swap(saved_states, new_saved_states);
	}

	void play_music_for_current_race() {
		if (ui.local_player_id == -1) return;
		race_t race = ui.st.players[ui.local_player_id].race;
		std::string track;
		if (race == race_t::terran) track = "terran1.wav";
		else if (race == race_t::zerg) track = "zerg1.wav";
		else if (race == race_t::protoss) track = "protoss1.wav";
		if (!track.empty()) ui.play_music(a_string(track.c_str()));
	}

	void reset() {
		saved_states.clear();
		quicksave_slot.reset();
		ui.reset();
		live_result_reported = false;
		current_map_file.clear();
		current_map_display_name.clear();
		mission_result = {};
		pending_next_map_file.clear();
		briefing_armed = false;
	}

	void capture_mission_result(bool won, bool next_ready) {
		if (mission_result.active) return;
		ui.is_paused = true;
		if (won) {
			ui.play_music("victory.wav");
		} else {
			ui.play_music("defeat.wav");
		}
		// uid 229 is the engine's documented "all unit types" sentinel for
		// trigger_player_kills — it sums the entire unit_deaths table.
		constexpr int k_all_units = 229;
		const int fps = 24;

		int elapsed_seconds = ui.st.current_frame / fps;
		int local = ui.local_player_id;
		int enemy = ui.enemy_player_id;
		int units_built = 0, buildings_built = 0;
		int minerals = 0, gas = 0;
		int unit_score = 0, building_score = 0;
		int losses = 0, kills = 0;
		if (local >= 0 && local < 12) {
			units_built = ui.st.total_non_buildings_ever_completed[(size_t)local];
			buildings_built = ui.st.total_buildings_ever_completed[(size_t)local];
			minerals = ui.st.total_minerals_gathered[(size_t)local];
			gas = ui.st.total_gas_gathered[(size_t)local];
			unit_score = ui.st.unit_score[(size_t)local];
			building_score = ui.st.building_score[(size_t)local];
			losses = ui.trigger_player_kills(local, k_all_units);
		}
		if (enemy >= 0 && enemy < 12) {
			kills = ui.trigger_player_kills(enemy, k_all_units);
		}

		std::string mission_name = current_map_display_name.empty()
			? std::string("MISSION") : current_map_display_name;

		auto pad_stat = [](const std::string& label, const std::string& value, size_t total_w) {
			if (label.size() + value.size() + 2 >= total_w) return label + "  " + value;
			return label + std::string(total_w - label.size() - value.size(), ' ') + value;
		};
		const size_t col_w = 28;

		mission_result_t r;
		r.active = true;
		r.won = won;
		r.next_mission_ready = next_ready;
		r.header = won ? "MISSION ACCOMPLISHED" : "MISSION FAILED";
		r.header_color = won ? rgba32(170, 255, 170, 255) : rgba32(255, 160, 160, 255);
		r.veil_color = won ? rgba32(20, 40, 12, 160) : rgba32(40, 12, 12, 160);

		char time_buf[32];
		std::snprintf(time_buf, sizeof(time_buf), "%d:%02d", elapsed_seconds / 60, elapsed_seconds % 60);

		r.lines.push_back(uppercase_copy(mission_name));
		r.lines.push_back("");
		r.lines.push_back(pad_stat("TIME", time_buf, col_w));
		r.lines.push_back(pad_stat("UNITS BUILT", std::to_string(units_built), col_w));
		r.lines.push_back(pad_stat("BUILDINGS BUILT", std::to_string(buildings_built), col_w));
		r.lines.push_back(pad_stat("UNITS LOST", std::to_string(losses), col_w));
		r.lines.push_back(pad_stat("KILLS", std::to_string(kills), col_w));
		r.lines.push_back(pad_stat("MINERALS GATHERED", std::to_string(minerals), col_w));
		r.lines.push_back(pad_stat("GAS GATHERED", std::to_string(gas), col_w));
		r.lines.push_back(pad_stat("UNIT SCORE", std::to_string(unit_score), col_w));
		r.lines.push_back(pad_stat("BUILDING SCORE", std::to_string(building_score), col_w));
		r.lines.push_back("");
		mission_result = std::move(r);
		
		// Stable campaign progression: persist progress to disk immediately when a mission ends.
		if (!current_map_file.empty()) {
			write_campaign_progress(current_map_file, won);
		}

		log("result: %s name='%s' time=%ds units=%d buildings=%d kills=%d losses=%d\n",
			won ? "victory" : "defeat",
			mission_name.c_str(),
			elapsed_seconds,
			units_built,
			buildings_built,
			kills,
			losses);
	}

#ifndef EMSCRIPTEN
	void return_to_startup_shell() {
		try {
			enable_frontend(discover_startup_entries(g_data_dir));
		} catch (const std::exception& e) {
			log("client: could not open startup shell: %s\n", e.what());
		}
	}
#endif

	void service_client_requests() {
		auto try_load = [this](const std::string& path, const char* verb, const char* fail_msg) {
			log("client: %s '%s'\n", verb, path.c_str());
			try {
				load_single_player_map(path);
				ui.set_image_data();
				center_view_on_loaded_content();
			} catch (const std::exception& e) {
				log("client: %s failed: %s\n", verb, e.what());
				ui.push_hud_message(fail_msg, 6 * 24);
			}
		};

		if (ui.request_continue_after_debrief) {
			ui.request_continue_after_debrief = false;
			if (mission_result.active) {
				if (mission_result.won && !pending_next_map_file.empty()) {
					try_load(pending_next_map_file, "advancing to", "Load failed.");
				} else {
#ifndef EMSCRIPTEN
					return_to_startup_shell();
#endif
				}
			}
		}
		if (ui.request_restart_mission) {
			ui.request_restart_mission = false;
			if (!current_map_file.empty()) {
				try_load(current_map_file, "restarting mission", "Restart failed.");
			}
		}
		if (ui.request_quit_to_menu) {
			ui.request_quit_to_menu = false;
#ifndef EMSCRIPTEN
			return_to_startup_shell();
#endif
		}
	}

	void center_view_on_loaded_content() {
		int map_w = (int)ui.game_st.map_width;
		int map_h = (int)ui.game_st.map_height;
		if (map_w <= 0 || map_h <= 0) {
			ui.screen_pos = {};
			return;
		}
		ui.screen_pos = {
			map_w / 2 - (int)ui.view_width / 2,
			map_h / 2 - (int)ui.view_height / 2
		};
		if (ui.screen_pos.x < 0) ui.screen_pos.x = 0;
		if (ui.screen_pos.y < 0) ui.screen_pos.y = 0;
	}

	void load_replay_session(const std::string& replay_file) {
		reset();
		ui.is_replay_mode = true;
		ui.is_live_game_mode = false;
		ui.default_enforce_local_visibility = false;
		ui.enforce_local_visibility = false;
		ui.local_player_id = -1;
		ui.enemy_player_id = -1;
		ui.load_replay_file(replay_file.c_str());
		log("replay: file='%s'\n", replay_file.c_str());
	}

	void load_save_file(const std::string& path) {
		reset();
		ui.is_replay_mode = false;
		ui.is_live_game_mode = true;
		ui.default_enforce_local_visibility = true;
		ui.enforce_local_visibility = true;
		serialization::state_serializer serializer{ui.st};
		serialization::state_serializer::save_metadata_t md;
		if (serializer.load_full(a_string(path.c_str()), ui.action_st, ui.apm, ui.st.global, ui.st.game, md)) {
			ui.replay_frame = ui.st.current_frame;
			live_result_reported = false;
			mission_result = {};
			current_map_file = md.current_map_file.c_str();
			ui.current_objectives_text = md.objectives_text;
			ui.pending_next_scenario = md.pending_next_scenario;
			ui.active_portrait.unit_type = md.portrait_unit_type;
			ui.active_portrait.end_frame = md.portrait_end_frame;
			pending_next_map_file.clear();
			ui.is_paused = false;
			ui.set_image_data();
			log("client: restored from %s to frame %d\n", path.c_str(), ui.st.current_frame);
		} else {
			error("failed to load save %s", path.c_str());
		}
	}

	void enable_frontend(std::vector<startup_entry> entries) {
		frontend_entries = std::move(entries);
		frontend_active = true;
		frontend_current_view = frontend_view::startup;
		frontend_selected_episode = -1;
		frontend_selected_index = frontend_entries.empty() ? -1 : 0;
		frontend_status_message = frontend_entries.empty()
			? "NO MAPS OR REPLAYS WERE DISCOVERED. PASS --MAP OR --REPLAY."
			: "PRESS ENTER OR CLICK TO LAUNCH.";
		frontend_surface.reset();
		log("frontend: startup shell active (%d entries)\n", (int)frontend_entries.size());
		for (size_t i = 0; i < frontend_entries.size(); ++i) {
			log("frontend: [%d] %s -> %s\n", (int)i + 1, frontend_entries[i].title.c_str(), frontend_entries[i].path.c_str());
		}
	}

	int frontend_get_entry_count() const {
		if (frontend_current_view == frontend_view::startup) return (int)frontend_entries.size();
		if (frontend_current_view == frontend_view::episodes) return (int)get_all_campaigns().size();
		if (frontend_current_view == frontend_view::settings) return (int)settings_item::count;
		if (frontend_current_view == frontend_view::missions) {
			auto eps = get_all_campaigns();
			if (frontend_selected_episode >= 0 && frontend_selected_episode < (int)eps.size()) {
				return (int)eps[frontend_selected_episode].missions.size();
			}
		}
		return 0;
	}

	rect frontend_entry_rect(size_t index) const {
		int box_w = std::min<int>((int)ui.screen_width - 120, 900);
		if (box_w < 320) box_w = std::max<int>((int)ui.screen_width - 40, 200);
		int count = frontend_get_entry_count();
		int box_h = count > 10 ? 34 : (count > 8 ? 48 : 64);
		int gap = count > 10 ? 4 : (count > 8 ? 8 : 12);
		int total_h = count * box_h + std::max<int>(0, count - 1) * gap;
		int start_y = std::max(150, ((int)ui.screen_height - total_h) / 2 + 30);
		if (start_y + total_h > (int)ui.screen_height - 70)
			start_y = std::max(140, (int)ui.screen_height - 70 - total_h);
		int x = ((int)ui.screen_width - box_w) / 2;
		int y = start_y + (int)index * (box_h + gap);
		return rect{xy(x, y), xy(x + box_w, y + box_h)};
	}

	void launch_frontend_entry(size_t index) {
		if (index >= frontend_entries.size()) return;
		const startup_entry& entry = frontend_entries[index];
		try {
			if (entry.kind == startup_content_kind::map) {
				load_single_player_map(entry.path);
			} else if (entry.kind == startup_content_kind::replay) {
				load_replay_session(entry.path);
			} else if (entry.kind == startup_content_kind::save) {
				load_save_file(entry.path);
			} else if (entry.kind == startup_content_kind::campaign_browser) {
				frontend_current_view = frontend_view::episodes;
				frontend_selected_index = 0;
				return;
			} else if (entry.kind == startup_content_kind::settings) {
				frontend_current_view = frontend_view::settings;
				frontend_selected_index = 0;
				return;
			}
			ui.set_image_data();
			center_view_on_loaded_content();
			frontend_active = false;
			frontend_surface.reset();
			log("frontend: launched %s\n", entry.path.c_str());
		} catch (const std::exception& e) {
			frontend_status_message = uppercase_copy(std::string("LAUNCH FAILED: ") + e.what());
			log("frontend: failed to launch '%s': %s\n", entry.path.c_str(), e.what());
		}
	}

	void launch_campaign_episode(size_t index) {
		auto eps = get_all_campaigns();
		if (index >= eps.size()) return;
		frontend_selected_episode = (int)index;
		frontend_current_view = frontend_view::missions;
		frontend_selected_index = 0;
	}

	void launch_campaign_mission(size_t ep_index, size_t mission_index) {
		auto eps = get_all_campaigns();
		if (ep_index >= eps.size()) return;
		auto& missions = eps[ep_index].missions;
		if (mission_index >= missions.size()) return;
		
		try {
			load_single_player_map(missions[mission_index].path);
			ui.set_image_data();
			center_view_on_loaded_content();
			frontend_active = false;
			frontend_surface.reset();
			log("frontend: launched campaign %s\n", missions[mission_index].path.c_str());
		} catch (const std::exception& e) {
			frontend_status_message = uppercase_copy(std::string("LAUNCH FAILED: ") + e.what());
			log("frontend: failed to launch campaign '%s': %s\n", missions[mission_index].path.c_str(), e.what());
		}
	}

	void render_frontend() {
		if (!ui.wnd) return;
		if (!frontend_surface || frontend_surface->w != (int)ui.screen_width || frontend_surface->h != (int)ui.screen_height) {
			frontend_surface = native_window_drawing::create_rgba_surface((int)ui.screen_width, (int)ui.screen_height);
		}

		uint32_t* pixels = (uint32_t*)frontend_surface->lock();
		int pitch = frontend_surface->pitch / 4;
		int width = frontend_surface->w;
		int height = frontend_surface->h;

		auto now = clock.now();
		double t = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() / 1000.0;

		// Deep space gradient
		for (int y = 0; y < height; ++y) {
			double f = (double)y / std::max(1, height);
			uint8_t r = (uint8_t)(4 + 8 * f);
			uint8_t g = (uint8_t)(6 + 12 * f);
			uint8_t b = (uint8_t)(12 + 24 * f);
			uint32_t color = rgba32(r, g, b, 255);
			uint32_t* row = pixels + y * pitch;
			for (int x = 0; x < width; ++x) row[x] = color;
		}

		// Nebula effect - layered noise
		auto draw_nebula = [&](double ox, double oy, double scale, uint32_t color_base, double alpha) {
			for (int i = 0; i < 6; ++i) {
				double nx = ox + sin(t * 0.1 + i * 1.5) * 100;
				double ny = oy + cos(t * 0.15 + i * 1.2) * 80;
				int rx = (int)(width * 0.5 + nx);
				int ry = (int)(height * 0.4 + ny);
				int rw = (int)(800 * scale * (1.0 + 0.1 * sin(t * 0.5 + i)));
				int rh = (int)(600 * scale * (1.0 + 0.1 * cos(t * 0.4 + i)));
				fill_rgba_rect(pixels, pitch, width, height, rx - rw / 2, ry - rh / 2, rw, rh, 
					rgba32((color_base >> 0) & 0xFF, (color_base >> 8) & 0xFF, (color_base >> 16) & 0xFF, (uint8_t)(alpha * 20)));
			}
		};
		draw_nebula(150, -100, 1.2, rgba32(40, 20, 80), 1.0);  // Purple
		draw_nebula(-200, 150, 1.0, rgba32(20, 60, 100), 0.8); // Blue
		draw_nebula(300, 200, 0.8, rgba32(80, 40, 20), 0.5);   // Red/Orange

		// Starfield
		auto drift = (long long)(t * 25);
		for (int i = 0; i < 128; ++i) {
			uint32_t seed = (uint32_t)(i * 1103515245u + 12345u);
			int star_x = (int)((seed + drift * (1 + seed % 3)) % (uint32_t)std::max(1, width));
			int star_y = (int)((seed / 97u) % (uint32_t)std::max(1, height));
			uint8_t shade = (uint8_t)(160 + seed % 95u);
			if (seed % 12 == 0) shade = (uint8_t)(shade * (0.5 + 0.5 * sin(t * 2.0 + i)));
			fill_rgba_rect(pixels, pitch, width, height, star_x, star_y, 2, 2, rgba32(shade, shade, 255, 255));
		}

		// CRT Scanlines & Visual Static
		for (int y = 0; y < height; y += 2) {
			uint8_t alpha = (y % 4 == 0) ? 40 : 20;
			fill_rgba_rect(pixels, pitch, width, height, 0, y, width, 1, rgba32(0, 0, 0, alpha));
		}
		// Moving Scanline Beam
		int beam_y = (int)(t * 150) % (height + 200) - 100;
		if (beam_y >= 0 && beam_y < height) {
			fill_rgba_rect(pixels, pitch, width, height, 0, beam_y, width, 4, rgba32(180, 220, 255, 30));
		}

		// Main container
		fill_rgba_rect(pixels, pitch, width, height, 40, 36, width - 80, height - 72, rgba32(2, 4, 12, 180));
		draw_rgba_frame(pixels, pitch, width, height, 40, 36, width - 80, height - 72, 2, rgba32(171, 124, 48, 255));

		std::string title = "OPENSNOWSTORM";
		std::string subtitle = "BROOD WAR ENGINE";
		uint32_t subtitle_color = rgba32(216, 180, 104, 255);

		if (frontend_current_view == frontend_view::episodes) {
			subtitle = "SELECT CAMPAIGN EPISODE";
		} else if (frontend_current_view == frontend_view::settings) {
			subtitle = "GAME OPTIONS";
		} else if (frontend_current_view == frontend_view::missions) {
			auto eps = get_all_campaigns();
			if (frontend_selected_episode >= 0 && frontend_selected_episode < (int)eps.size()) {
				subtitle = eps[frontend_selected_episode].title + ": " + eps[frontend_selected_episode].subtitle;
				// Race colors
				if (subtitle.find("TERRAN") != std::string::npos) subtitle_color = rgba32(100, 160, 255, 255);
				else if (subtitle.find("ZERG") != std::string::npos) subtitle_color = rgba32(255, 100, 255, 255);
				else if (subtitle.find("PROTOSS") != std::string::npos) subtitle_color = rgba32(100, 255, 100, 255);
			}
		}

		draw_rgba_text(pixels, pitch, width, height, (width - text_pixel_width(title, 4)) / 2 + 3, 62 + 3, title, 4, rgba32(0, 0, 0, 160));
		draw_rgba_text(pixels, pitch, width, height, (width - text_pixel_width(title, 4)) / 2, 62, title, 4, rgba32(255, 220, 132, 255));
		draw_rgba_text(pixels, pitch, width, height, (width - text_pixel_width(subtitle, 2)) / 2, 106, subtitle, 2, subtitle_color);

		std::string hint = "UP DOWN OR CLICK TO CHOOSE. ENTER TO LAUNCH.";
		if (frontend_current_view == frontend_view::settings) {
			hint = "UP DOWN TO CHOOSE. LEFT RIGHT OR ENTER TO CHANGE. BACKSPACE TO GO BACK.";
		} else if (frontend_current_view != frontend_view::startup) {
			hint = "UP DOWN OR CLICK TO CHOOSE. ENTER TO SELECT. BACKSPACE TO GO BACK.";
		} else if (frontend_entries.empty()) {
			hint = "ADD .SCX .SCM OR .REP FILES BESIDE THE DATA INSTALL OR PASS --MAP.";
		}
		draw_rgba_text(pixels, pitch, width, height, (width - text_pixel_width(hint, 1)) / 2, 138, hint, 1, rgba32(176, 188, 208, 255));
		int entry_count = frontend_get_entry_count();
		for (int i = 0; i < entry_count; ++i) {
			rect box = frontend_entry_rect(i);
			bool selected = i == frontend_selected_index;
			uint32_t fill = selected ? rgba32(60, 36, 10, 230) : rgba32(16, 20, 32, 220);
			uint32_t frame = selected ? rgba32(255, 210, 96, 255) : rgba32(90, 104, 128, 255);
			fill_rgba_rect(pixels, pitch, width, height, box.from.x, box.from.y, box.to.x - box.from.x, box.to.y - box.from.y, fill);
			draw_rgba_frame(pixels, pitch, width, height, box.from.x, box.from.y, box.to.x - box.from.x, box.to.y - box.from.y, 2, frame);

			std::string entry_title, entry_subtitle;
			if (frontend_current_view == frontend_view::startup) {
				entry_title = frontend_entries[i].title;
				entry_subtitle = frontend_entries[i].subtitle;
			} else if (frontend_current_view == frontend_view::episodes) {
				auto eps = get_all_campaigns();
				entry_title = eps[i].title;
				entry_subtitle = eps[i].subtitle;
			} else if (frontend_current_view == frontend_view::missions) {
				auto eps = get_all_campaigns();
				auto& mission = eps[frontend_selected_episode].missions[i];
				entry_title = mission.name;
				entry_subtitle = mission.path;
			} else if (frontend_current_view == frontend_view::settings) {
				entry_title = settings_item_label(i);
				entry_subtitle = settings_item_value(ui.settings, i);
			}

			int box_h = box.to.y - box.from.y;
			int y_off = box_h > 48 ? 0 : -4;
			std::string index_text = std::to_string(i + 1);
			draw_rgba_text(pixels, pitch, width, height, box.from.x + 14, box.from.y + 18 + y_off, index_text, 2, selected ? rgba32(255, 232, 160, 255) : rgba32(170, 184, 208, 255));
			draw_rgba_text(pixels, pitch, width, height, box.from.x + 48, box.from.y + 12 + y_off, uppercase_copy(entry_title), 2, selected ? rgba32(255, 232, 160, 255) : rgba32(214, 220, 232, 255));
			if (frontend_current_view == frontend_view::settings) {
				int vw = text_pixel_width(uppercase_copy(entry_subtitle), 2);
				draw_rgba_text(pixels, pitch, width, height, box.to.x - vw - 18, box.from.y + 12 + y_off, uppercase_copy(entry_subtitle), 2, selected ? rgba32(255, 210, 96, 255) : rgba32(180, 196, 220, 255));
			} else {
				draw_rgba_text(pixels, pitch, width, height, box.from.x + 48, box.from.y + 38 + y_off, uppercase_copy(shorten_middle(entry_subtitle, 52)), 1, rgba32(142, 154, 178, 255));
			}
		}


		if (!frontend_status_message.empty()) {
			std::string status = uppercase_copy(shorten_middle(frontend_status_message, 88));
			draw_rgba_text(pixels, pitch, width, height, (width - text_pixel_width(status, 1)) / 2, height - 76, status, 1, rgba32(255, 194, 112, 255));
		}
		std::string footer = std::string("F3 DEBUG OVERLAY   F4 SOUND: ") + (ui.settings.sound_enabled ? "ENABLED" : "DISABLED") + "   ALT+ENTER FULLSCREEN   ESC CLOSE";
		draw_rgba_text(pixels, pitch, width, height, (width - text_pixel_width(footer, 1)) / 2, height - 52, footer, 1, rgba32(112, 132, 164, 255));

		frontend_surface->unlock();

		auto window_surface = native_window_drawing::get_window_surface(&ui.wnd);
		frontend_surface->blit(&*window_surface, 0, 0);
		ui.wnd.update_surface();
	}
	void adjust_frontend_setting(int delta) {
		if (frontend_current_view != frontend_view::settings) return;
		if (frontend_selected_index < 0 || frontend_selected_index >= (int)settings_item::count) return;
		ui.settings_index = frontend_selected_index;
		ui.apply_settings_item(delta);
		sound_enabled = ui.settings.sound_enabled;
		campaign_fog_of_war = ui.settings.fog_of_war;
		frontend_status_message = std::string(settings_item_label(frontend_selected_index)) + "  " +
			settings_item_value(ui.settings, frontend_selected_index);
	}

	void activate_frontend_selection() {
		if (frontend_current_view == frontend_view::startup) launch_frontend_entry(frontend_selected_index);
		else if (frontend_current_view == frontend_view::episodes) launch_campaign_episode(frontend_selected_index);
		else if (frontend_current_view == frontend_view::missions) launch_campaign_mission(frontend_selected_episode, frontend_selected_index);
		else if (frontend_current_view == frontend_view::settings) adjust_frontend_setting(1);
	}

	void frontend_go_back() {
		if (frontend_current_view == frontend_view::episodes || frontend_current_view == frontend_view::settings)
			frontend_current_view = frontend_view::startup;
		else if (frontend_current_view == frontend_view::missions)
			frontend_current_view = frontend_view::episodes;
		frontend_selected_index = 0;
	}

	void update_frontend() {
		if (!ui.wnd) return;
		native_window::event_t e;
		while (ui.wnd.peek_message(e)) {
			switch (e.type) {
			case native_window::event_t::type_quit:
				std::exit(0);
				break;
			case native_window::event_t::type_resize:
				ui.resize(e.width, e.height);
				frontend_surface.reset();
				break;
			case native_window::event_t::type_mouse_motion:
				for (int i = 0; i < frontend_get_entry_count(); ++i) {
					rect box = frontend_entry_rect(i);
					if (e.mouse_x >= box.from.x && e.mouse_x < box.to.x && e.mouse_y >= box.from.y && e.mouse_y < box.to.y) {
						frontend_selected_index = i;
						break;
					}
				}
				break;
			case native_window::event_t::type_mouse_button_down:
				if (e.button == 1) {
					for (int i = 0; i < frontend_get_entry_count(); ++i) {
						rect box = frontend_entry_rect(i);
						if (e.mouse_x >= box.from.x && e.mouse_x < box.to.x && e.mouse_y >= box.from.y && e.mouse_y < box.to.y) {
							frontend_selected_index = i;
							activate_frontend_selection();
							break;
						}
					}
				}
				break;
			case native_window::event_t::type_key_down:
				if (e.sym == 27) {
					if (frontend_current_view != frontend_view::startup) {
						frontend_go_back();
					} else {
						std::exit(0);
					}
				} else if (e.sym == 8) { // Backspace
					frontend_go_back();
				} else if (e.scancode == 60) {
					ui.show_debug_overlay = !ui.show_debug_overlay;
				} else if (e.scancode == 61) {
					ui.settings.sound_enabled = !ui.settings.sound_enabled;
					ui.apply_client_settings(ui.settings);
					ui.persist_client_settings();
					sound_enabled = ui.settings.sound_enabled;
					log("client: sound %s\n", sound_enabled ? "enabled" : "disabled");
				} else if ((ui.wnd.get_key_state(226) || ui.wnd.get_key_state(230)) &&
					(e.sym == '\r' || e.sym == '\n' || e.scancode == 40)) {
					ui.toggle_fullscreen_setting();
				} else {
					int count = frontend_get_entry_count();
					if (count > 0) {
						if (e.scancode == 82 || e.sym == 'w') {
							frontend_selected_index = (frontend_selected_index + count - 1) % count;
						} else if (e.scancode == 81 || e.sym == 's') {
							frontend_selected_index = (frontend_selected_index + 1) % count;
						} else if (e.scancode == 80) {
							adjust_frontend_setting(-1);
						} else if (e.scancode == 79) {
							adjust_frontend_setting(1);
						} else if (e.sym == '\r' || e.sym == ' ' || e.scancode == 40 || e.scancode == 88) {
							activate_frontend_selection();
						} else if (e.sym >= '1' && e.sym <= '9') {
							int requested = e.sym - '1';
							if (requested >= 0 && requested < count) {
								frontend_selected_index = requested;
								activate_frontend_selection();
							}
						}
					}
				}
				break;
			default:
				break;
			}
			if (!frontend_active) return;
		}
		if (!frontend_active) return;
		render_frontend();
	}

	void load_single_player_map(const std::string& map_file) {
		reset();
		ui.is_replay_mode = false;
		ui.is_live_game_mode = true;
		ui.default_enforce_local_visibility = campaign_fog_of_war;
		ui.enforce_local_visibility = campaign_fog_of_war;

		log("client: load_single_player_map(fog=%s, local_slot=%d, local_race=%d, enemy_race=%d)\n",
			campaign_fog_of_war ? "on" : "off", campaign_local_player_slot, campaign_local_race, campaign_enemy_race);


		int selected_local = -1;
		int selected_enemy = -1;
		bool selected_game_type_melee = false;

		game_load_functions load_funcs(ui.st);
		load_funcs.load_map_file(map_file, [&]() {
			auto is_melee_pickable = [&](int controller) {
				return controller == player_t::controller_open || controller == player_t::controller_computer;
			};
			auto is_ums_local_pickable = [&](int controller) {
				return controller == player_t::controller_occupied ||
					controller == player_t::controller_computer_game ||
					controller == player_t::controller_open ||
					controller == player_t::controller_computer;
			};
			auto is_ums_enemy_pickable = [&](int controller) {
				return controller == player_t::controller_occupied ||
					controller == player_t::controller_computer_game ||
					controller == player_t::controller_computer ||
					controller == player_t::controller_open ||
					controller == player_t::controller_rescue_passive ||
					controller == player_t::controller_unused_rescue_active ||
					controller == player_t::controller_neutral;
			};

			static_vector<size_t, 12> melee_slots;
			static_vector<size_t, 12> ums_local_slots;
			static_vector<size_t, 12> ums_enemy_slots;
			for (size_t i = 0; i != 8; ++i) {
				int controller = ui.st.players[i].controller;
				if (is_melee_pickable(controller)) melee_slots.push_back(i);
				if (is_ums_local_pickable(controller)) ums_local_slots.push_back(i);
				if (is_ums_enemy_pickable(controller)) ums_enemy_slots.push_back(i);
			}

			auto contains_slot = [&](const auto& slots, int slot) {
				for (size_t v : slots) {
					if ((int)v == slot) return true;
				}
				return false;
			};

			bool game_type_melee = false;
			if (campaign_game_type == map_game_type_t::melee) {
				game_type_melee = true;
			} else if (campaign_game_type == map_game_type_t::ums) {
				game_type_melee = false;
			} else {
				bool has_authored_campaign_slots = false;
				for (size_t v : ums_local_slots) {
					int controller = ui.st.players[v].controller;
					if (controller == player_t::controller_occupied || controller == player_t::controller_computer_game) {
						has_authored_campaign_slots = true;
						break;
					}
				}
				game_type_melee = !has_authored_campaign_slots;
			}

			selected_game_type_melee = game_type_melee;

			if (game_type_melee) {
				if (melee_slots.size() < 2) {
					error("%s: melee mode requires at least two open/computer slots; try --game-type ums for authored UMS/campaign layouts", map_file.c_str());
				}

				selected_local = campaign_local_player_slot == -1 ? (int)melee_slots.front() : campaign_local_player_slot;
				if (!contains_slot(melee_slots, selected_local)) {
					error("%s: requested local slot %d is not an open/computer slot in melee mode", map_file.c_str(), selected_local);
				}

				selected_enemy = campaign_enemy_player_slot;
				if (selected_enemy == -1) {
					for (size_t v : melee_slots) {
						if ((int)v != selected_local) {
							selected_enemy = (int)v;
							break;
						}
					}
				}
				if (selected_enemy == -1 || selected_enemy == selected_local || !contains_slot(melee_slots, selected_enemy)) {
					error("%s: unable to pick a valid enemy slot in melee mode (local=%d enemy=%d)", map_file.c_str(), selected_local, selected_enemy);
				}

				for (size_t v : melee_slots) ui.st.players[v].controller = player_t::controller_closed;
				ui.st.players[(size_t)selected_local].controller = player_t::controller_occupied;
				ui.st.players[(size_t)selected_enemy].controller = player_t::controller_computer_game;
			} else {
				if (ums_local_slots.empty()) {
					error("%s: no suitable UMS local slot found (need occupied/computer_game/open/computer in slots 0-7)", map_file.c_str());
				}

				if (campaign_local_player_slot != -1) {
					if (!contains_slot(ums_local_slots, campaign_local_player_slot)) {
						error("%s: requested local slot %d is not UMS-pickable (expected occupied/computer_game/open/computer)", map_file.c_str(), campaign_local_player_slot);
					}
					selected_local = campaign_local_player_slot;
				} else {
					selected_local = -1;
					for (size_t v : ums_local_slots) {
						if (ui.st.players[v].controller == player_t::controller_occupied) {
							selected_local = (int)v;
							break;
						}
					}
					if (selected_local == -1) {
						for (size_t v : ums_local_slots) {
							if (ui.st.players[v].controller == player_t::controller_computer_game) {
								selected_local = (int)v;
								break;
							}
						}
					}
					if (selected_local == -1) selected_local = (int)ums_local_slots.front();
				}

				auto& local_player = ui.st.players.at((size_t)selected_local);
				if (local_player.controller == player_t::controller_open ||
				    local_player.controller == player_t::controller_computer ||
				    local_player.controller == player_t::controller_computer_game) {
					local_player.controller = player_t::controller_occupied;
				}

				if (campaign_enemy_player_slot != -1) {
					if (campaign_enemy_player_slot == selected_local) {
						error("%s: --enemy-player slot %d must be different from local slot %d", map_file.c_str(), campaign_enemy_player_slot, selected_local);
					}
					if (!contains_slot(ums_enemy_slots, campaign_enemy_player_slot)) {
						error("%s: requested enemy slot %d is not UMS-pickable", map_file.c_str(), campaign_enemy_player_slot);
					}
					selected_enemy = campaign_enemy_player_slot;
				} else {
					selected_enemy = -1;
					for (size_t v : ums_enemy_slots) {
						if ((int)v != selected_local && ui.st.players[v].controller == player_t::controller_computer_game) {
							selected_enemy = (int)v;
							break;
						}
					}
					if (selected_enemy == -1) {
						for (size_t v : ums_enemy_slots) {
							if ((int)v != selected_local && ui.st.players[v].controller == player_t::controller_occupied) {
								selected_enemy = (int)v;
								break;
							}
						}
					}
					if (selected_enemy == -1) {
						for (size_t v : ums_enemy_slots) {
							if ((int)v != selected_local) {
								selected_enemy = (int)v;
								break;
							}
						}
					}
				}

				if (selected_enemy != -1) {
					auto& enemy_player = ui.st.players.at((size_t)selected_enemy);
					if (enemy_player.controller == player_t::controller_open ||
					    enemy_player.controller == player_t::controller_computer) {
						enemy_player.controller = player_t::controller_computer_game;
					}
				}
			}

			auto& local_player = ui.st.players.at((size_t)selected_local);
			if ((int)local_player.race == 5) local_player.race = (race_t)campaign_local_race;
			if (selected_enemy != -1) {
				auto& enemy_player = ui.st.players.at((size_t)selected_enemy);
				if ((int)enemy_player.race == 5) enemy_player.race = (race_t)campaign_enemy_race;
			}

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

		play_music_for_current_race();
		ui.set_image_data();
		current_map_file = map_file;
		current_map_display_name = humanize_map_stem(map_file);

		const char* game_mode_name = selected_game_type_melee ? "melee" : "ums";
		const char* mode_origin = campaign_game_type == map_game_type_t::auto_detect ? " (auto)" : "";
		if (selected_enemy == -1) {
			log("single-player: map='%s' local_slot=%d enemy_slot=none mode=%s%s fog=%s\n",
				map_file.c_str(),
				selected_local,
				game_mode_name,
				mode_origin,
				campaign_fog_of_war ? "on" : "off");
		} else {
			log("single-player: map='%s' local_slot=%d enemy_slot=%d mode=%s%s fog=%s\n",
				map_file.c_str(),
				selected_local,
				selected_enemy,
				game_mode_name,
				mode_origin,
				campaign_fog_of_war ? "on" : "off");
		}

#ifndef EMSCRIPTEN
		// Record the mission path so the player can resume the campaign after
		// closing the client.  Campaign-like is a loose heuristic; non-campaign
		// one-off skirmish maps overwrite earlier progress the same way.
		write_campaign_progress(map_file, false);

		if (!ui.game_st.briefing_triggers.empty()) {
			ui.st.is_mission_briefing = true;
			ui.is_paused = false;
			briefing_armed = false;
			log("single-player: map has %d briefing triggers; entering briefing mode\n", ui.game_st.briefing_triggers.size());
		} else {
			// Auto-pause and push a briefing banner so the player has a beat to
			// orient before the simulation starts.  The first unpause (Space/P)
			// clears the briefing; any right-click on the map is ignored while
			// paused so the banner serves as a soft briefing gate.
			ui.is_paused = true;
			briefing_armed = true;
			std::string mission_name = strip_extension(path_basename(map_file));
			for (char& c : mission_name) {
				if (c == '_' || c == '-') c = ' ';
			}
			if (mission_name.empty()) mission_name = map_file;
			ui.push_hud_message(a_string("Mission: ") + a_string(mission_name.c_str()), 20 * 24);
			ui.push_hud_message("Press Space to begin.", 20 * 24);
		}
#endif
	}

#ifndef EMSCRIPTEN
	// Locate the next campaign map file given the scenario name set by the
	// Set Next Scenario trigger action.  Searches in:
	//   1. The same directory as the current map (with .scx / .scm extension)
	//   2. A "campaign/" subdirectory relative to the current map directory
	//   3. The scenario name as-is (may already be a full path or have extension)
	std::string find_next_campaign_map(const std::string& scenario_name) const {
		if (scenario_name.empty()) return {};

		// Extract the directory portion of the current map file.
		std::string dir;
		size_t sep = current_map_file.find_last_of("/\\");
		if (sep != std::string::npos) dir = current_map_file.substr(0, sep + 1);

		static const char* const extensions[] = {".scx", ".SCX", ".scm", ".SCM", nullptr};
		for (int i = 0; extensions[i]; ++i) {
			// Same directory as current map.
			std::string p = dir + scenario_name + extensions[i];
			{
				std::ifstream f(p, std::ios::binary);
				if (f.good()) return p;
			}
			// campaign/ subdirectory.
			std::string p2 = dir + "campaign/" + scenario_name + extensions[i];
			{
				std::ifstream f(p2, std::ios::binary);
				if (f.good()) return p2;
			}
		}
		// Try the scenario name verbatim (already has extension, or is a path).
		{
			std::ifstream f(scenario_name, std::ios::binary);
			if (f.good()) return scenario_name;
		}
		return {};
	}
#endif

	void update() {
		auto now = clock.now();

		if (frontend_active) {
			update_frontend();
			return;
		}

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
					save_replay_state_if_missing(ui.st.current_frame);
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
						save_replay_state_if_missing(ui.st.current_frame);
						auto i = saved_states.lower_bound(ui.replay_frame);
						if (i != saved_states.begin()) --i;
						if (i == saved_states.end()) i = saved_states.begin();
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
					ui.is_paused = true;
					log("single-player: victory at frame %d\n", ui.st.current_frame);
#ifndef EMSCRIPTEN
					// Mark this mission completed so the frontend can offer the
					// next unplayed entry on next launch.
					write_campaign_progress(current_map_file, true);
					std::string next_map;
					if (!ui.pending_next_scenario.empty()) {
						log("single-player: next scenario -> '%s'\n", ui.pending_next_scenario.c_str());
						next_map = find_next_campaign_map(ui.pending_next_scenario);
					}
					// Fallback: if no trigger-driven next scenario, walk the
					// same folder to find the chronologically next campaign map.
					// This lets curated map packs chain without per-map triggers.
					if (next_map.empty()) {
						next_map = find_next_campaign_map_in_folder(current_map_file);
						if (!next_map.empty()) {
							log("campaign: chaining to next folder map '%s'\n", next_map.c_str());
						}
					}
					capture_mission_result(true, !next_map.empty());
					if (!next_map.empty()) {
						log("campaign: next map ready -> '%s' (press Enter to continue)\n", next_map.c_str());
						pending_next_map_file = next_map;
					} else if (!ui.pending_next_scenario.empty()) {
						log("campaign: next map '%s' not found beside current map\n",
						    ui.pending_next_scenario.c_str());
						ui.push_hud_message(a_string("Next: ") + ui.pending_next_scenario, 12 * 24);
					}
#else
					if (!ui.pending_next_scenario.empty()) {
						log("single-player: next scenario -> '%s'\n", ui.pending_next_scenario.c_str());
					}
					capture_mission_result(true, false);
#endif
				} else if (ui.player_defeated(ui.local_player_id)) {
					live_result_reported = true;
					ui.is_paused = true;
					log("single-player: defeat at frame %d\n", ui.st.current_frame);
					capture_mission_result(false, false);
				}
			}
		}

		ui.update();

		// Briefing release: the very first unpause after a campaign map load
		// clears the briefing-armed flag so the pre-mission pause happens only
		// once.  Subsequent Space/P toggles behave as normal pause/resume.
		if (briefing_armed && !ui.is_paused) {
			briefing_armed = false;
			log("briefing: dismissed at frame %d\n", ui.st.current_frame);
		}

		handle_save_load();

		service_client_requests();
	}

	std::string save_slot_path(int slot) const {
		char buf[64];
		if (slot <= 0) sprintf(buf, "saves/quicksave.osv");
		else sprintf(buf, "saves/slot%d.osv", slot);
		return buf;
	}

	void do_save(const std::string& path) {
		serialization::state_serializer serializer{ui.st};
		serialization::state_serializer::save_metadata_t md;
		md.current_map_file = a_string(current_map_file.c_str());
		md.objectives_text = ui.current_objectives_text;
		md.pending_next_scenario = ui.pending_next_scenario;
		md.portrait_unit_type = ui.active_portrait.unit_type;
		md.portrait_end_frame = ui.active_portrait.end_frame;
		create_directory_if_not_exists("saves");
		serializer.save_full(a_string(path.c_str()), ui.action_st, ui.apm, md);
		log("save: saved to %s at frame %d\n", path.c_str(), ui.st.current_frame);
		ui.push_hud_message("Saved.", 3 * 24);
	}

	void do_load(const std::string& path) {
		serialization::state_serializer serializer{ui.st};
		serialization::state_serializer::save_metadata_t md;
		if (serializer.load_full(a_string(path.c_str()), ui.action_st, ui.apm, ui.st.global, ui.st.game, md)) {
			ui.replay_frame = ui.st.current_frame;
			live_result_reported = false;
			mission_result = {};
			current_map_file = md.current_map_file.c_str();
			ui.current_objectives_text = md.objectives_text;
			ui.pending_next_scenario = md.pending_next_scenario;
			ui.active_portrait.unit_type = md.portrait_unit_type;
			ui.active_portrait.end_frame = md.portrait_end_frame;
			pending_next_map_file.clear();
			ui.is_paused = false;
			log("load: restored from %s to frame %d\n", path.c_str(), ui.st.current_frame);
			ui.push_hud_message("Loaded.", 3 * 24);
		} else {
			log("load: no save at %s or incompatible version\n", path.c_str());
			ui.push_hud_message("No save.", 3 * 24);
		}
	}

	void handle_save_load() {
		if (ui.quicksave_pending) {
			ui.quicksave_pending = false;
			do_save(save_slot_path(0));
		}
		if (ui.quickload_pending) {
			ui.quickload_pending = false;
			do_load(save_slot_path(0));
		}
		if (ui.save_slot_save_pending > 0) {
			int slot = ui.save_slot_save_pending;
			ui.save_slot_save_pending = -1;
			do_save(save_slot_path(slot));
		}
		if (ui.save_slot_load_pending > 0) {
			int slot = ui.save_slot_load_pending;
			ui.save_slot_load_pending = -1;
			do_load(save_slot_path(slot));
		}
	}

	// Overlays render on top of the engine's RGBA framebuffer via
	// rgba_overlay_cb: persistent objectives panel, speed indicator, pause
	// menu, and post-mission debrief.

	void refresh_objectives_cache() {
		if (cached_objectives_source == ui.current_objectives_text.c_str()) return;
		cached_objectives_source.assign(ui.current_objectives_text.c_str());
		cached_objectives_lines.clear();
		std::string text = uppercase_copy(cached_objectives_source);
		const size_t max_w = 40;
		size_t i = 0;
		while (i < text.size()) {
			size_t end = std::min(text.size(), i + max_w);
			if (end < text.size()) {
				size_t space = text.rfind(' ', end);
				if (space != std::string::npos && space > i + max_w / 2) end = space;
			}
			cached_objectives_lines.push_back(text.substr(i, end - i));
			i = end;
			while (i < text.size() && text[i] == ' ') ++i;
			if (cached_objectives_lines.size() >= 6) break;
		}
	}

	void draw_objectives_overlay(uint32_t* pixels, int pitch, int width, int height) {
		if (mission_result.active || frontend_active || !ui.is_live_game_mode) return;
		if (!ui.settings.show_objectives) return;
		if (ui.current_objectives_text.empty()) return;
		refresh_objectives_cache();
		if (cached_objectives_lines.empty()) return;

		int scale = 1;
		int line_h = 10 * scale;
		int pad = 12;
		int longest = text_pixel_width("MISSION OBJECTIVES", scale);
		for (const std::string& line : cached_objectives_lines) longest = std::max(longest, text_pixel_width(line, scale));
		int box_w = longest + 2 * pad;
		int box_h = (int)cached_objectives_lines.size() * line_h + pad * 2 + 16;
		int x = 24;
		int y = 24;

		// HUD Terminal background
		fill_rgba_rect(pixels, pitch, width, height, x, y, box_w, box_h, rgba32(4, 6, 12, 190));
		uint32_t border = rgba32(171, 124, 48, 200);
		
		// Race header color
		uint32_t header_color = rgba32(255, 220, 132, 255);
		race_t local_race = ui.st.players[ui.local_player_id].race;
		if (local_race == race_t::zerg) { border = rgba32(180, 80, 180, 200); header_color = rgba32(255, 100, 255, 255); }
		else if (local_race == race_t::terran) { border = rgba32(80, 140, 220, 200); header_color = rgba32(100, 200, 255, 255); }
		else if (local_race == race_t::protoss) { border = rgba32(80, 200, 100, 200); header_color = rgba32(150, 255, 150, 255); }

		draw_rgba_frame(pixels, pitch, width, height, x, y, box_w, box_h, 1, border);
		fill_rgba_rect(pixels, pitch, width, height, x, y, box_w, 18, rgba32((border >> 0) & 0xFF, (border >> 8) & 0xFF, (border >> 16) & 0xFF, 128));

		draw_rgba_text(pixels, pitch, width, height, x + pad, y + 4, "MISSION OBJECTIVES", scale, header_color);
		
		int ty = y + 24;
		for (const std::string& line : cached_objectives_lines) {
			draw_rgba_text(pixels, pitch, width, height, x + pad, ty, line, scale, rgba32(220, 228, 240, 255));
			ty += line_h;
		}

		// Subtle scanlines in objectives panel
		for (int sy = y; sy < y + box_h; sy += 2) {
			fill_rgba_rect(pixels, pitch, width, height, x, sy, box_w, 1, rgba32(0, 0, 0, 32));
		}
	}

	void draw_speed_indicator(uint32_t* pixels, int pitch, int width, int height) {
		if (frontend_active) return;
		if (!ui.is_live_game_mode) return;
		int s = ui.game_speed.raw_value;
		std::string label;
		if (s <= 128) label = "0.5X";
		else if (s <= 384) label = "1X";
		else if (s <= 768) label = "2X";
		else if (s <= 1536) label = "4X";
		else label = "8X+";
		if (ui.is_paused) label = "PAUSED";
		int scale = 1;
		int w = text_pixel_width(label, scale) + 12;
		int h = 16;
		int x = width - w - 16;
		int y = 16;
		fill_rgba_rect(pixels, pitch, width, height, x, y, w, h, rgba32(8, 12, 22, 200));
		draw_rgba_frame(pixels, pitch, width, height, x, y, w, h, 1, rgba32(171, 124, 48, 200));
		draw_rgba_text(pixels, pitch, width, height, x + 6, y + 4, label, scale,
			ui.is_paused ? rgba32(255, 220, 132, 255) : rgba32(200, 220, 240, 255));
	}

	void draw_centered_panel(uint32_t* pixels, int pitch, int width, int height,
	                         const std::vector<std::string>& lines,
	                         int scale, uint32_t border, uint32_t veil) {
		if (lines.empty()) return;
		int line_h = 9 * scale + 4;
		int pad = 24;
		int longest = 0;
		for (const std::string& l : lines) longest = std::max(longest, text_pixel_width(l, scale));
		int box_w = longest + 2 * pad;
		int box_h = (int)lines.size() * line_h + 2 * pad;
		int x = (width - box_w) / 2;
		int y = (height - box_h) / 2;
		if (veil) fill_rgba_rect(pixels, pitch, width, height, 0, 0, width, height, veil);
		
		// Styled box with double border
		fill_rgba_rect(pixels, pitch, width, height, x, y, box_w, box_h, rgba32(8, 12, 24, 230));
		draw_rgba_frame(pixels, pitch, width, height, x, y, box_w, box_h, 2, border);
		draw_rgba_frame(pixels, pitch, width, height, x + 4, y + 4, box_w - 8, box_h - 8, 1, rgba32((border >> 0) & 0xFF, (border >> 8) & 0xFF, (border >> 16) & 0xFF, 100));
		
		// Corner accents
		int cs = 12;
		fill_rgba_rect(pixels, pitch, width, height, x - 2, y - 2, cs, 4, border);
		fill_rgba_rect(pixels, pitch, width, height, x - 2, y - 2, 4, cs, border);
		fill_rgba_rect(pixels, pitch, width, height, x + box_w - cs + 2, y - 2, cs, 4, border);
		fill_rgba_rect(pixels, pitch, width, height, x + box_w - 2, y - 2, 4, cs, border);
		
		int ty = y + pad;
		bool first = true;
		for (const std::string& l : lines) {
			int lw = text_pixel_width(l, scale);
			int lx = x + (box_w - lw) / 2;
			uint32_t color = first ? rgba32(255, 232, 160, 255) : rgba32(214, 220, 232, 255);
			draw_rgba_text(pixels, pitch, width, height, lx, ty, l, scale, color);
			ty += line_h;
			first = false;
		}
	}

	void draw_game_menu_overlay(uint32_t* pixels, int pitch, int width, int height) {
		if (!ui.game_menu_open || frontend_active) return;
		auto layout = make_game_menu_layout(width, height);
		fill_rgba_rect(pixels, pitch, width, height, 0, 0, width, height, rgba32(0, 0, 0, 120));
		fill_rgba_rect(pixels, pitch, width, height, layout.x, layout.y, layout.w, layout.h, rgba32(8, 12, 24, 236));
		draw_rgba_frame(pixels, pitch, width, height, layout.x, layout.y, layout.w, layout.h, 2, rgba32(255, 210, 96, 255));
		draw_rgba_text(pixels, pitch, width, height,
			layout.x + (layout.w - text_pixel_width("GAME MENU", 2)) / 2,
			layout.y + 16, "GAME MENU", 2, rgba32(255, 232, 160, 255));
		for (int i = 0; i < layout.count; ++i) {
			overlay_rect r = layout.item_rect(i);
			bool selected = i == ui.game_menu_index;
			fill_rgba_rect(pixels, pitch, width, height, r.x, r.y, r.w, r.h,
				selected ? rgba32(60, 36, 10, 230) : rgba32(16, 20, 32, 210));
			draw_rgba_frame(pixels, pitch, width, height, r.x, r.y, r.w, r.h, 1,
				selected ? rgba32(255, 210, 96, 255) : rgba32(90, 104, 128, 200));
			std::string label = game_menu_item_label(i);
			int lx = r.x + (r.w - text_pixel_width(label, 2)) / 2;
			draw_rgba_text(pixels, pitch, width, height, lx, r.y + (r.h - 16) / 2, label, 2,
				selected ? rgba32(255, 232, 160, 255) : rgba32(214, 220, 232, 255));
		}
	}

	void draw_settings_overlay(uint32_t* pixels, int pitch, int width, int height) {
		if (!ui.settings_open || frontend_active) return;
		auto layout = make_settings_layout(width, height);
		fill_rgba_rect(pixels, pitch, width, height, 0, 0, width, height, rgba32(0, 0, 0, 140));
		fill_rgba_rect(pixels, pitch, width, height, layout.x, layout.y, layout.w, layout.h, rgba32(8, 12, 24, 240));
		draw_rgba_frame(pixels, pitch, width, height, layout.x, layout.y, layout.w, layout.h, 2, rgba32(255, 210, 96, 255));
		draw_rgba_text(pixels, pitch, width, height,
			layout.x + (layout.w - text_pixel_width("OPTIONS", 2)) / 2,
			layout.y + 16, "OPTIONS", 2, rgba32(255, 232, 160, 255));
		for (int i = 0; i < layout.count; ++i) {
			overlay_rect r = layout.item_rect(i);
			bool selected = i == ui.settings_index;
			fill_rgba_rect(pixels, pitch, width, height, r.x, r.y, r.w, r.h,
				selected ? rgba32(60, 36, 10, 230) : rgba32(16, 20, 32, 210));
			draw_rgba_frame(pixels, pitch, width, height, r.x, r.y, r.w, r.h, 1,
				selected ? rgba32(255, 210, 96, 255) : rgba32(90, 104, 128, 200));
			std::string label = settings_item_label(i);
			std::string value = uppercase_copy(settings_item_value(ui.settings, i));
			draw_rgba_text(pixels, pitch, width, height, r.x + 10, r.y + (r.h - 16) / 2, label, 1,
				selected ? rgba32(255, 232, 160, 255) : rgba32(200, 210, 224, 255));
			int vw = text_pixel_width(value, 2);
			draw_rgba_text(pixels, pitch, width, height, r.x + r.w - vw - 12, r.y + (r.h - 16) / 2, value, 2,
				selected ? rgba32(255, 210, 96, 255) : rgba32(180, 196, 220, 255));
		}
	}

	void draw_pause_overlay(uint32_t* pixels, int pitch, int width, int height) {
		if (frontend_active || !ui.is_live_game_mode) return;
		if (!ui.is_paused || mission_result.active) return;
		if (ui.game_menu_open || ui.settings_open) return;

		static const std::vector<std::string> paused_lines = {
			"PAUSED",
			"",
			"SPACE OR P   RESUME",
			"F10 GAME MENU   ESC GAME MENU",
			"F5 SAVE  F6 SLOT-SAVE  F8 LOAD  F9 SLOT-LOAD",
			"F7 RESTART",
		};
		std::vector<std::string> briefing_lines;
		const std::vector<std::string>* lines = &paused_lines;
		if (briefing_armed) {
			std::string name = current_map_display_name.empty()
				? std::string("MISSION")
				: uppercase_copy(current_map_display_name);
			briefing_lines = {
				"MISSION: " + name,
				"",
				"PRESS SPACE TO BEGIN",
				"F10 OR ESC FOR GAME MENU",
			};
			lines = &briefing_lines;
		}
		draw_centered_panel(pixels, pitch, width, height, *lines, 2,
			rgba32(255, 210, 96, 240), rgba32(0, 0, 0, 96));
	}

	void draw_debrief_overlay(uint32_t* pixels, int pitch, int width, int height) {
		if (frontend_active || !mission_result.active || !ui.is_live_game_mode) return;

		const auto& r = mission_result;
		int scale = 2;
		int line_h = 9 * scale + 4;
		int pad = 24;
		int header_scale = 3;
		int header_h = 9 * header_scale + 12;
		int longest = text_pixel_width(r.header, header_scale);
		for (const std::string& l : r.lines) longest = std::max(longest, text_pixel_width(l, scale));
		int box_w = longest + 2 * pad;
		int box_h = header_h + (int)r.lines.size() * line_h + 2 * pad;
		int x = (width - box_w) / 2;
		int y = (height - box_h) / 2;

		fill_rgba_rect(pixels, pitch, width, height, 0, 0, width, height, r.veil_color);
		fill_rgba_rect(pixels, pitch, width, height, x, y, box_w, box_h, rgba32(10, 16, 30, 240));
		draw_rgba_frame(pixels, pitch, width, height, x, y, box_w, box_h, 2, rgba32(255, 210, 96, 255));

		int hx = x + (box_w - text_pixel_width(r.header, header_scale)) / 2;
		draw_rgba_text(pixels, pitch, width, height, hx, y + pad, r.header, header_scale, r.header_color);

		int ty = y + pad + header_h;
		bool first = true;
		for (const std::string& l : r.lines) {
			int lw = text_pixel_width(l, scale);
			int lx = x + (box_w - lw) / 2;
			uint32_t color = first ? rgba32(255, 232, 160, 255) : rgba32(214, 220, 232, 255);
			draw_rgba_text(pixels, pitch, width, height, lx, ty, l, scale, color);
			ty += line_h;
			first = false;
		}
	}

	void draw_portrait_overlay(uint32_t* pixels, int pitch, int width, int height) {
		if (frontend_active || !ui.is_live_game_mode || mission_result.active) return;
		if (ui.st.is_mission_briefing) {
			draw_briefing_portrait_overlay(pixels, pitch, width, height);
			return;
		}
		if (ui.active_portrait.unit_type == -1 || ui.st.current_frame >= ui.active_portrait.end_frame) return;

		int pw = 120;
		int ph = 120;
		int px = width - pw - 32;
		int py = height - ph - 160; 

		draw_high_fidelity_portrait(pixels, pitch, width, height, px, py, pw, ph, "TRANSMISSION", ui.active_portrait.unit_type);
	}

	void draw_briefing_portrait_overlay(uint32_t* pixels, int pitch, int width, int height) {
		int portrait_w = 140;
		int portrait_h = 140;
		for (int i = 0; i < 4; ++i) {
			auto& slot = ui.briefing_slots[i];
			if (slot.unit_type == -1 || ui.st.current_frame >= slot.end_frame) continue;

			int sx = 24 + (portrait_w + 8) * i;
			if (sx + portrait_w > 16 + 160) break;
			int sy = 50;
			draw_high_fidelity_portrait(pixels, pitch, width, height, sx, sy, portrait_w, portrait_h, "BRIEFING", slot.unit_type);
		}
	}

	void draw_high_fidelity_portrait(uint32_t* pixels, int pitch, int width, int height, int px, int py, int pw, int ph, const char* label, int unit_type) {
		fill_rgba_rect(pixels, pitch, width, height, px - 4, py - 4, pw + 8, ph + 8, rgba32(8, 12, 24, 230));
		draw_rgba_frame(pixels, pitch, width, height, px - 4, py - 4, pw + 8, ph + 8, 2, rgba32(100, 140, 220, 180));

		auto& portraits_grp = ui.img.portraits;
		auto& wpe = ui.tileset_img.wpe;
		bool rendered = false;

		if (unit_type >= 0 && (size_t)unit_type < portraits_grp.frames.size() && wpe.size() == 256 * 4) {
			auto& frame = portraits_grp.frames[unit_type];
			if (!frame.data_container.empty() && frame.size.x > 0 && frame.size.y > 0) {
				draw_grp_scaled_rgba(pixels, pitch, width, height, frame, wpe, px, py, pw, ph);
				rendered = true;
			}
		}

		if (!rendered) {
			auto now = clock.now();
			double t = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() / 1000.0;
			for (int sy = py; sy < py + ph; ++sy) {
				if (sy < 0 || sy >= height) continue;
				for (int sx = px; sx < px + pw; ++sx) {
					if (sx < 0 || sx >= width) continue;
					uint32_t seed = (uint32_t)(sx * 374761393u + sy * 668265263u + (uint32_t)(t * 120));
					uint8_t noise = (uint8_t)(seed % 28);
					pixels[sy * pitch + sx] = rgba32(15 + noise, 20 + noise, 40 + noise);
				}
			}
			int scan_y = (int)(t * 120) % (ph + 100) - 50;
			if (scan_y >= 0 && scan_y < ph) {
				fill_rgba_rect(pixels, pitch, width, height, px, py + scan_y, pw, 3, rgba32(180, 240, 255, 40));
			}
			draw_rgba_text(pixels, pitch, width, height, px + (pw - text_pixel_width("NO SIGNAL", 1)) / 2, py + ph / 2 - 4, "NO SIGNAL", 1, rgba32(180, 60, 60, 255));
		} else {
			auto now = clock.now();
			double t = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() / 1000.0;
			int scan_y = (int)(t * 80) % (ph + 60) - 30;
			if (scan_y >= 0 && scan_y < ph) {
				for (int sx = px; sx < px + pw; ++sx) {
					if (sx < 0 || sx >= width) continue;
					int dy = py + scan_y;
					if (dy < 0 || dy >= height) continue;
					uint32_t& p = pixels[dy * pitch + sx];
					p = ((p & 0xFEFEFEFE) >> 1) + rgba32(0, 30, 60, 0);
				}
			}
		}

		draw_rgba_text(pixels, pitch, width, height, px + (pw - text_pixel_width(label, 1)) / 2, py - 18, label, 1, rgba32(140, 200, 255, 255));
	}

	void draw_briefing_bg_overlay(uint32_t* pixels, int pitch, int width, int height) {
		if (!ui.st.is_mission_briefing) return;

		fill_rgba_rect(pixels, pitch, width, height, 0, 0, width, height, rgba32(2, 4, 12, 255));

		draw_rgba_frame(pixels, pitch, width, height, 10, 10, width - 20, height - 20, 2, rgba32(100, 140, 220, 180));

		fill_rgba_rect(pixels, pitch, width, height, 12, 12, width - 24, 24, rgba32(8, 16, 32, 230));
		draw_rgba_frame(pixels, pitch, width, height, 12, 12, width - 24, 24, 1, rgba32(80, 120, 180, 150));

		int portrait_area_w = 160;
		draw_rgba_frame(pixels, pitch, width, height, 16, 40, portrait_area_w, height - 100, 1, rgba32(60, 100, 160, 120));

		draw_rgba_frame(pixels, pitch, width, height, 20 + portrait_area_w, 40, width - 36 - portrait_area_w, height - 100, 1, rgba32(50, 90, 140, 120));

		fill_rgba_rect(pixels, pitch, width, height, 12, height - 48, width - 24, 36, rgba32(8, 16, 32, 230));
		draw_rgba_frame(pixels, pitch, width, height, 12, height - 48, width - 24, 36, 1, rgba32(80, 120, 180, 150));
	}

	void draw_mission_timer(uint32_t* pixels, int pitch, int width, int height) {
		if (frontend_active || !ui.is_live_game_mode) return;
		int frames = (int)ui.st.current_frame;
		int seconds = frames / 24;
		int minutes = seconds / 60;
		int hours = minutes / 60;
		seconds %= 60;
		minutes %= 60;
		char buf[16];
		if (hours > 0) sprintf(buf, "%d:%02d:%02d", hours, minutes, seconds);
		else sprintf(buf, "%02d:%02d", minutes, seconds);
		
		int scale = 1;
		int w = text_pixel_width(buf, scale) + 12;
		int h = 16;
		int x = width - w - 16;
		int y = 16 + 16 + 8; // Below speed indicator
		fill_rgba_rect(pixels, pitch, width, height, x, y, w, h, rgba32(8, 12, 22, 200));
		draw_rgba_frame(pixels, pitch, width, height, x, y, w, h, 1, rgba32(171, 124, 48, 200));
		draw_rgba_text(pixels, pitch, width, height, x + 6, y + 4, buf, scale, rgba32(200, 220, 240, 255));
	}

	void draw_briefing_text_overlay(uint32_t* pixels, int pitch, int width, int height) {
		if (!ui.st.is_mission_briefing) return;

		int text_x = 20 + 160 + 10;
		int text_y_start = 48;
		int text_y = text_y_start;
		int text_w = width - text_x - 20;
		int text_y_max = height - 70;

		std::string mission_name(ui.game_st.scenario_name.c_str());
		if (!mission_name.empty()) {
			std::string display = uppercase_copy(mission_name);
			draw_rgba_text(pixels, pitch, width, height, text_x + 4, text_y, display, 2, rgba32(255, 220, 132, 255));
			text_y += 24;
		}

		std::string desc(ui.game_st.scenario_description.c_str());
		if (!desc.empty()) {
			int max_chars = text_w / 6;
			size_t pos = 0;
			while (pos < desc.size() && text_y < text_y_max - 12) {
				std::string line = desc.substr(pos, max_chars);
				draw_rgba_text(pixels, pitch, width, height, text_x + 4, text_y, line, 1, rgba32(200, 210, 230, 255));
				text_y += 12;
				pos += line.size();
			}
			text_y += 8;
		}

		text_y += 4;
		if (text_y < text_y_max - 14)
			draw_rgba_text(pixels, pitch, width, height, text_x + 4, text_y, "TRANSMISSIONS:", 1, rgba32(255, 200, 100, 255));
		text_y += 14;

		int hud_count = 0;
		for (int i = 0; i < ui.k_hud_max_lines; ++i) {
			if (ui.hud_messages[i].expiry_frame > 0 && ui.st.current_frame < ui.hud_messages[i].expiry_frame) {
				++hud_count;
			}
		}
		if (hud_count > 0) {
			for (int slot = ui.hud_next_slot - 1; slot >= ui.hud_next_slot - ui.k_hud_max_lines; --slot) {
				int idx = ((slot % ui.k_hud_max_lines) + ui.k_hud_max_lines) % ui.k_hud_max_lines;
				auto& hm = ui.hud_messages[idx];
				if (hm.expiry_frame <= 0 || ui.st.current_frame >= hm.expiry_frame) continue;
				std::string msg(hm.text.c_str());
				int max_chars = text_w / 6;
				size_t pos = 0;
				while (pos < msg.size() && text_y < text_y_max) {
					std::string line = msg.substr(pos, max_chars);
					draw_rgba_text(pixels, pitch, width, height, text_x + 8, text_y, line, 1, rgba32(180, 220, 255, 255));
					text_y += 11;
					pos += line.size();
				}
				text_y += 4;
			}
		}

		int bottom_y = height - 44;
		draw_rgba_text(pixels, pitch, width, height, (width - text_pixel_width("PRESS SPACE TO BEGIN", 2)) / 2, bottom_y, "PRESS SPACE TO BEGIN", 2, rgba32(255, 210, 96, 255));
	}

	void draw_save_slot_indicator(uint32_t* pixels, int pitch, int width, int height) {
		if (frontend_active || !ui.is_live_game_mode) return;
		char buf[32];
		sprintf(buf, "SLOT %d", ui.current_save_slot);
		std::string label(buf);
		int scale = 1;
		int w = text_pixel_width(label, scale) + 12;
		int h = 16;
		int x = width - w - 16;
		int y = 16 + 16 + 8 + 16 + 4;
		fill_rgba_rect(pixels, pitch, width, height, x, y, w, h, rgba32(8, 12, 22, 200));
		draw_rgba_frame(pixels, pitch, width, height, x, y, w, h, 1, rgba32(171, 124, 48, 200));
		draw_rgba_text(pixels, pitch, width, height, x + 6, y + 4, label, scale, rgba32(200, 220, 240, 255));
	}

	void draw_client_overlays(uint32_t* pixels, int pitch, int width, int height) {
		draw_briefing_bg_overlay(pixels, pitch, width, height);
		draw_portrait_overlay(pixels, pitch, width, height);
		draw_objectives_overlay(pixels, pitch, width, height);
		draw_speed_indicator(pixels, pitch, width, height);
		draw_save_slot_indicator(pixels, pitch, width, height);
		draw_mission_timer(pixels, pitch, width, height);
		draw_pause_overlay(pixels, pitch, width, height);
		draw_game_menu_overlay(pixels, pitch, width, height);
		draw_settings_overlay(pixels, pitch, width, height);
		draw_debrief_overlay(pixels, pitch, width, height);
		draw_briefing_text_overlay(pixels, pitch, width, height);
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
	// 7: pointer to the pending_next_scenario C-string (0 if none pending).
	case 7:
		return (double)(uintptr_t)(m->ui.pending_next_scenario.empty() ? nullptr : m->ui.pending_next_scenario.c_str());
	// 8: local player victory state (0=none, ≥3=won, 1-2=defeated).
	case 8: {
		int local = m->ui.local_player_id;
		if (local < 0 || local >= 12) return 0;
		return (double)m->ui.st.players[local].victory_state;
	}
	// 9: 1 if victory was recorded AND a next-scenario name is pending (JS campaign
	//    transition ready); 0 otherwise.  JS should read 7 for the scenario name,
	//    call load_map_data with the new map bytes, then clear via set_value(7,0).
	case 9:
		return (m->live_result_reported && !m->ui.pending_next_scenario.empty()) ? 1 : 0;
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
	// 7: clear the pending_next_scenario field (call after JS has handled it).
	case 7:
		m->ui.pending_next_scenario.clear();
		break;
	}
}

// ---------------------------------------------------------------------------
// Emscripten: load a map from raw byte data for interactive single-player
// campaign play.  The JS layer calls this with the raw .scx/.scm bytes and
// passes player/race parameters for slot configuration.
//
// Parameters:
//   data        – pointer to raw map file bytes
//   len         – byte length of the map data
//   local_player – preferred local slot (0-7; -1 for auto)
//   local_race   – race index (0=zerg, 1=terran, 2=protoss, 5=random)
// ---------------------------------------------------------------------------
extern "C" void load_map_data(const uint8_t* data, size_t len, int local_player, int local_race) {
	m->reset();
	auto& ui = m->ui;

	ui.is_replay_mode = false;
	ui.is_live_game_mode = true;
	ui.default_enforce_local_visibility = true;
	ui.enforce_local_visibility = true;

	// Write the map bytes to the Emscripten virtual filesystem so that the
	// existing file-based load_map_file path can read them.
	const char* tmp_map_path = "/tmp/campaign_map.scx";
	{
		FILE* f = fopen(tmp_map_path, "wb");
		if (f) {
			fwrite(data, 1, len, f);
			fclose(f);
		}
	}

	int selected_local = -1;
	game_load_functions load_funcs(ui.st);
	load_funcs.load_map_file(tmp_map_path, [&]() {
		// Determine local slot.
		if (local_player >= 0 && local_player < 8) {
			selected_local = local_player;
		} else {
			// Auto-select: prefer authored occupied slot, then first available.
			for (int i = 0; i < 8; ++i) {
				int c = ui.st.players[i].controller;
				if (c == player_t::controller_occupied) { selected_local = i; break; }
			}
			if (selected_local == -1) {
				for (int i = 0; i < 8; ++i) {
					int c = ui.st.players[i].controller;
					if (c == player_t::controller_open || c == player_t::controller_computer) {
						selected_local = i;
						break;
					}
				}
			}
		}
		if (selected_local == -1) selected_local = 0;

		// Ensure the local slot is occupiable.
		auto& lp = ui.st.players[selected_local];
		if (lp.controller == player_t::controller_open ||
		    lp.controller == player_t::controller_computer ||
		    lp.controller == player_t::controller_computer_game) {
			lp.controller = player_t::controller_occupied;
		}
		if (local_race >= 0 && local_race <= 5 && (int)lp.race == 5) {
			lp.race = (race_t)local_race;
		}
		if ((int)lp.race > 2) {
			// Simple LCG (same multiplier/increment used elsewhere in the codebase)
			// to pick a random concrete race (0=zerg, 1=terran, 2=protoss) when the
			// slot still has race=random (>2) after the caller's assignment.
			uint32_t seed = (uint32_t)len ^ (uint32_t)local_player;
			seed = seed * 22695477u + 1u;
			lp.race = (race_t)((seed >> 16) % 3u);
		}

		// Set AI for remaining open slots.
		for (int i = 0; i < 8; ++i) {
			if (i == selected_local) continue;
			int c = ui.st.players[i].controller;
			if (c == player_t::controller_open || c == player_t::controller_computer) {
				ui.st.players[i].controller = player_t::controller_computer_game;
				if ((int)ui.st.players[i].race > 2) {
					ui.st.players[i].race = (race_t)((i % 3));
				}
			}
		}

		load_funcs.setup_info.victory_condition = 1;
		load_funcs.setup_info.starting_units = 1;
	});

	ui.local_player_id = selected_local;
	ui.enemy_player_id = -1;
	ui.replay_frame = ui.st.current_frame;
	m->live_result_reported = false;
	ui.set_image_data();
	any_replay_loaded = true;
	ui::log("load_map_data: local_slot=%d map=%s\n", selected_local, tmp_map_path);
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

	auto load_data_file = make_load_data_file();

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

	auto load_data_file = make_load_data_file();

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
			"  checkpoints : %d\n",
			map_name_str,
			end_frame,
			(int)checkpoints.size());
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

		auto load_data_file = make_load_data_file();

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
				log("verify-hashes: ... %d additional mismatches omitted\n",
					(int)(mismatch_count - printed_mismatches));
			}
			log("verify-hashes: FAIL (%d/%d checkpoints mismatched)\n",
				(int)mismatch_count,
				(int)fixture.checkpoints.size());
			return 1;
		}

		const char* map_name = player.replay_st.map_name.empty() ? "<unknown>" : player.replay_st.map_name.c_str();
		log("verify-hashes: PASS\n"
			"  map         : %s\n"
			"  end frame   : %d\n"
			"  checkpoints : %d\n",
			map_name,
			player.replay_st.end_frame,
			(int)fixture.checkpoints.size());
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
		"  %s [--replay <file.rep>] [--data-dir <path>] [--config <options.ini>] [--headless]\n"
		"  %s                              (interactive startup frontend)\n"
		"  %s --map <file.scx|file.scm> [--local-player <0-7>] [--enemy-player <0-7>]\n"
		"     [--data-dir <path>]\n"
		"     [--game-type <auto|melee|ums>] [--local-race <zerg|terran|protoss|random>]\n"
		"     [--enemy-race <zerg|terran|protoss|random>] [--fog|--no-fog] [--headless]\n"
		"     [--headless-map [<frame-limit>]]  (headless smoke test; default limit 72000)\n"
		"     [--debug-overlay]  (show frame/fps/speed overlay on startup; also toggled by F3)\n"
		"  %s --bench <frames> [--replay <file.rep>] [--data-dir <path>]\n"
		"  %s --validate-replay [--replay <file.rep>] [--data-dir <path>]\n"
		"  %s --record-hashes <fixture.txt> [--hash-interval <n>] [--replay <file.rep>] [--data-dir <path>]\n"
		"  %s --verify-hashes <fixture.txt> [--replay <file.rep>] [--data-dir <path>]\n"
		"  %s --extract-scenario-chk <output.chk> --map <file.scx|file.scm> [--data-dir <path>]\n"
		"  %s --gen-test-replay <output.rep> --map <file.scx|file.scm|scenario.chk>\n"
		"     [--frames <n>] [--record-hashes <fixture.txt>] [--hash-interval <n>] [--use-map-settings]\n"
		"     [--fixture-script <transport_unload|worker_resource>]\n"
		"     [--data-dir <path>]\n"
		"\n"
		"note: --game-type auto (default) selects ums for authored/campaign-like slots, else melee.\n"
		"      --game-type ums preserves authored slot topology by default.\n"
		"      data files are auto-discovered from common locations, or pass --data-dir explicitly.\n"
		"      interactive launches without --map/--replay now open a startup shell instead of forcing maps/p49.rep.\n"
		"      campaign progress is persisted to campaign_progress.txt beside the executable; the startup shell\n"
		"      offers a 'Continue' entry that resumes the last-played mission.  On victory the client tries the\n"
		"      Set Next Scenario trigger target first, then falls back to the next alphabetically-sorted campaign\n"
		"      map in the same folder so curated campaign packs chain without needing per-map triggers.\n"
		"      Campaign maps auto-pause on load with a 'Press Space to begin' briefing banner; the first unpause\n"
		"      dismisses the briefing.  F5 saves to disk; F8 restores the last quicksave.\n"
		"      Mission objectives set by map triggers render as a persistent top-left panel.\n"
		"      Victory and defeat show a debrief overlay with time, unit/building stats, kills, and resources.\n"
		"      Press Enter on the debrief to advance (next mission if available, else startup shell).\n"
		"      F10 or Esc opens the Game Menu (Options, Save/Load, Restart, Exit to Menu).\n"
		"      Settings are stored in options.ini (or --config) and include speed, volume, scroll, and display.\n"
		"\n"
		"single-player controls (map mode):\n"
		"  left drag/select    left click command panel (multi tactical + single-unit production/abilities)   middle drag camera\n"
		"  left click map place armed building / landing\n"
		"  right click issue order (or cancel armed building / landing)\n"
		"  s stop              h hold position            a attack-move (next right click)\n"
		"  t patrol (next right click)   x cancel current production/research/morph/nuke\n"
		"  b burrow/unburrow   g siege/unsiege   c cloak/decloak\n"
		"  r return cargo      l unload all (transport) / lift off-land (Terran flying buildings)\n"
		"  i stim pack (Marine/Firebat)   m merge archon/dark archon (High/Dark Templar)\n"
		"  tab center camera on selection\n"
		"  ctrl+<1-0> set group   shift+<1-0> add group   <1-0> recall group\n"
		"  esc cancel targeting; otherwise open Game Menu\n"
		"  f toggle fog of war\n"
		"  F3 toggle debug overlay (frame counter, draw fps, game speed)\n"
		"  F5 quicksave   F6 save to slot   F8 quickload   F9 load from slot\n"
		"  Ctrl+1-9 select save slot   F7 restart   F10 Game Menu\n"
		"  alt+enter toggle fullscreen\n"
		"  enter dismiss post-mission debrief (continue / retry / return to shell)\n"
		"  space/p pause       u speed up                 z/d speed down\n"
		"\n"
		"spell targeting (command panel or ability hotkey arms a targeting mode):\n"
		"  right click on map/unit fires the spell; esc cancels\n"
		"  Science Vessel: defensive matrix, irradiate, emp shockwave, scanner sweep\n"
		"  Battlecruiser: yamato gun   Ghost: lockdown   Vulture: spider mines\n"
		"  Medic: healing, restoration, optical flare\n"
		"  Queen: spawn broodlings, parasite, ensnare, infestation\n"
		"  Defiler: dark swarm, plague, consume\n"
		"  High Templar: psionic storm, hallucination\n"
		"  Arbiter: recall, stasis field\n"
		"  Dark Archon: mind control, feedback, maelstrom\n"
		"  Corsair: disruption web\n",
		argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0);
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

// ---------------------------------------------------------------------------
// gen-test-replay: generate a minimal deterministic replay from a map file.
//
// Steps N frames of a melee session (default: 240) and writes the result as a
// BW-format replay to <output.rep>.  A companion hash fixture is written to
// <output.hashes> when --record-hashes is also given.  Together these two
// files constitute a self-contained regression fixture that CI can verify
// without needing extra configuration.
//
// Usage:
//   gfxtest --extract-scenario-chk <output.chk> --map <file.scx|scm>
//   gfxtest --gen-test-replay <output.rep> --map <file.scx|scm|scenario.chk>
//              [--frames <n>] [--record-hashes <fixture.txt>]
//              [--hash-interval <n>] [--use-map-settings]
// ---------------------------------------------------------------------------
static int run_extract_scenario_chk(const char* map_file, const char* output_chk) {
	if (!map_file || map_file[0] == '\0') {
		log("extract-scenario-chk: FAIL (--map is required)\n");
		return 2;
	}
	if (!output_chk || output_chk[0] == '\0') {
		log("extract-scenario-chk: FAIL (output chk path is required)\n");
		return 2;
	}

	try {
		auto chk = load_scenario_chk_bytes_or_throw(map_file);
		write_binary_file_or_throw(output_chk, chk.data(), chk.size());
		log("extract-scenario-chk: PASS\n"
			"  map         : %s\n"
			"  scenario    : %s\n"
			"  size        : %d bytes\n",
			map_file,
			output_chk,
			(int)chk.size());
		return 0;
	} catch (const exception& e) {
		log("extract-scenario-chk: FAIL (%s)\n", e.what());
		return 1;
	} catch (const std::exception& e) {
		log("extract-scenario-chk: FAIL (%s)\n", e.what());
		return 1;
	} catch (...) {
		log("extract-scenario-chk: FAIL (unknown exception)\n");
		return 1;
	}
}

static int run_gen_test_replay(
		const char* map_file,
		const char* output_rep,
		const char* output_hashes,
		int frames,
		int hash_interval,
		bool use_map_settings,
		const char* fixture_script_name) {
	using namespace bwgame;

	if (!map_file || map_file[0] == '\0') {
		log("gen-test-replay: FAIL (--map is required)\n");
		return 2;
	}
	if (!output_rep || output_rep[0] == '\0') {
		log("gen-test-replay: FAIL (output replay path is required)\n");
		return 2;
	}
	if (frames <= 0) frames = 240;

	log("gen-test-replay: map='%s' output='%s' frames=%d mode=%s\n",
		map_file, output_rep, frames, use_map_settings ? "map-settings" : "melee");

	enum class fixture_script_kind {
		none,
		transport_unload,
		worker_resource
	};
	fixture_script_kind fixture_script = fixture_script_kind::none;
	if (fixture_script_name && fixture_script_name[0] != '\0') {
		if (std::strcmp(fixture_script_name, "transport_unload") == 0) {
			fixture_script = fixture_script_kind::transport_unload;
		} else if (std::strcmp(fixture_script_name, "worker_resource") == 0) {
			fixture_script = fixture_script_kind::worker_resource;
		} else {
			log("gen-test-replay: FAIL (unknown fixture script '%s')\n", fixture_script_name);
			return 2;
		}
	}

	try {
		auto load_data_file = make_load_data_file();

		// Set up the game state and load the map.
		game_player player(load_data_file);
		state& st = player.st();

		replay_saver_state saver_st;

		game_load_functions load_funcs(st);

		// Capture the raw map bytes during load so the saver can embed them.
		std::vector<uint8_t> map_data_buf;
		bool raw_chk_input = has_extension_ci(map_file, ".chk");
		if (raw_chk_input) map_data_buf = read_binary_file_or_throw(map_file);

		auto setup_fixture = [&]() {
			// Deterministic two-player fixture setup: slot 0 = player, slot 1 = computer.
			for (size_t i = 0; i < 8; ++i) {
				if (st.players[i].controller == player_t::controller_open ||
				    st.players[i].controller == player_t::controller_computer) {
					st.players[i].controller = player_t::controller_closed;
				}
			}
			st.players[0].controller = player_t::controller_occupied;
			st.players[0].race = race_t::terran;
			st.players[1].controller = player_t::controller_computer_game;
			st.players[1].race = race_t::zerg;

			load_funcs.setup_info.victory_condition = use_map_settings ? 0 : 1;
			load_funcs.setup_info.tournament_mode = 0;
			load_funcs.setup_info.starting_units = use_map_settings ? 0 : 1;

			saver_st.random_seed = 42;
			st.lcg_rand_state = 42;
		};

		if (raw_chk_input) {
			load_funcs.load_map_data(map_data_buf.data(), map_data_buf.size(), setup_fixture);
		} else {
			load_funcs.load_map_file(map_file, setup_fixture);
			map_data_buf = read_binary_file_or_throw(map_file);
		}
		saver_st.map_data = map_data_buf.data();
		saver_st.map_data_size = map_data_buf.size();

		saver_st.map_tile_width  = st.game->map_tile_width;
		saver_st.map_tile_height = st.game->map_tile_height;
		saver_st.tileset         = (int)st.game->tileset_index;
		saver_st.game_type       = use_map_settings ? 10 : 2; // UMS/custom vs melee
		saver_st.game_speed      = 3;
		saver_st.player_name     = "OpenSnowstorm";
		saver_st.game_name       = "Test";
		saver_st.map_name        = map_file;
		saver_st.setup_info      = load_funcs.setup_info;
		for (size_t i = 0; i < 12; ++i) {
			saver_st.players[i]      = st.players[i];
			saver_st.player_names[i] = "";
		}

		// Step the simulation.
		state_functions sf(st);
		bool transport_move_unload_issued = false;
		for (int f = 0; f < frames; ++f) {
			if (fixture_script == fixture_script_kind::transport_unload) {
				auto distance_sq = [](xy a, xy b) {
					int dx = a.x - b.x;
					int dy = a.y - b.y;
					return dx * dx + dy * dy;
				};
				unit_t* dropship = nullptr;
				unit_t* closest_marine = nullptr;
				unit_t* closest_enemy = nullptr;
				int closest_marine_dist = std::numeric_limits<int>::max();
				int closest_enemy_dist = std::numeric_limits<int>::max();
				for (unit_t* u : ptr(st.visible_units)) {
					if (sf.unit_dying(u)) continue;
					if (u->owner == 0 && u->unit_type->id == UnitTypes::Terran_Dropship) {
						dropship = u;
					}
				}
				if (dropship) {
					for (unit_t* u : ptr(st.visible_units)) {
						if (sf.unit_dying(u)) continue;
						if (u->owner == 0 && u->unit_type->id == UnitTypes::Terran_Marine && !sf.u_loaded(u)) {
							int dist = distance_sq(u->sprite->position, dropship->sprite->position);
							if (dist < closest_marine_dist) {
								closest_marine_dist = dist;
								closest_marine = u;
							}
						}
						if (u->owner == 1) {
							int dist = distance_sq(u->sprite->position, dropship->sprite->position);
							if (dist < closest_enemy_dist) {
								closest_enemy_dist = dist;
								closest_enemy = u;
							}
						}
					}

					bool has_loaded_units = !sf.loaded_units(dropship).empty();
					if (!has_loaded_units && closest_marine) {
						sf.set_unit_order(dropship, sf.get_order_type(Orders::PickupTransport), closest_marine);
					} else if (has_loaded_units && !transport_move_unload_issued && closest_enemy) {
						xy unload_pos = closest_enemy->sprite->position - xy(32, 0);
						sf.set_unit_order(dropship, sf.get_order_type(Orders::MoveUnload), unload_pos);
						transport_move_unload_issued = true;
					}
				}
			}
			if (fixture_script == fixture_script_kind::worker_resource) {
				unit_t* worker = nullptr;
				unit_t* mineral = nullptr;
				int closest_mineral_dist = std::numeric_limits<int>::max();
				auto distance_sq = [](xy a, xy b) {
					int dx = a.x - b.x;
					int dy = a.y - b.y;
					return dx * dx + dy * dy;
				};
				for (unit_t* u : ptr(st.visible_units)) {
					if (sf.unit_dying(u)) continue;
					if (!worker && u->owner == 0 &&
						(u->unit_type->id == UnitTypes::Terran_SCV ||
						 u->unit_type->id == UnitTypes::Protoss_Probe ||
						 u->unit_type->id == UnitTypes::Zerg_Drone)) {
						worker = u;
					}
				}
				if (worker &&
					worker->order_type->id == Orders::Nothing &&
					worker->worker.resources_carried == 0 &&
					worker->worker.target_resource_unit == nullptr) {
					for (unit_t* u : ptr(st.visible_units)) {
						if (sf.unit_dying(u)) continue;
						if (!sf.unit_is_mineral_field(u)) continue;
						int dist = distance_sq(u->sprite->position, worker->sprite->position);
						if (dist < closest_mineral_dist) {
							closest_mineral_dist = dist;
							mineral = u;
						}
					}
					if (mineral) {
						sf.set_unit_order(worker, sf.get_order_type(Orders::MoveToMinerals), mineral);
					}
				}
			}
			sf.next_frame();
		}

		// Write the replay file.
		{
			data_loading::file_writer<> fw(output_rep);
			replay_saver_functions saver(saver_st);
			saver.save_replay((int)st.current_frame, fw);
		}

		log("gen-test-replay: wrote replay '%s' (%d frames)\n", output_rep, (int)st.current_frame);

		// Write hash fixture if requested. Generate checkpoints from the saved
		// replay so fixture generation and verification share the exact same
		// hashing path.
		if (output_hashes && output_hashes[0] != '\0') {
			if (hash_interval <= 0) hash_interval = 240;
			a_string map_name;
			int end_frame = 0;
			auto checkpoints = sample_replay_hashes_or_throw(output_rep, hash_interval, &map_name, &end_frame);
			std::ofstream hf(output_hashes, std::ios::out | std::ios::trunc);
			if (!hf) {
				error("gen-test-replay: unable to open fixture '%s' for writing: %s",
					output_hashes, std::strerror(errno));
			}
			hf << "# OpenSnowstorm replay hash fixture v1\n";
			hf << "# Generated by gfxtest --gen-test-replay\n";
			hf << "replay: " << output_rep << "\n";
			hf << "# frame hash\n";
			for (const auto& cp : checkpoints) {
				hf << format("%d 0x%08x\n", cp.frame, cp.hash);
			}
			if (!hf) {
				error("gen-test-replay: failed writing fixture '%s'", output_hashes);
			}
			log("gen-test-replay: wrote hash fixture '%s' (%d checkpoints)\n",
				output_hashes, (int)checkpoints.size());
		}

		log("gen-test-replay: PASS\n");
		return 0;
	} catch (const exception& e) {
		log("gen-test-replay: FAIL (%s)\n", e.what());
		return 1;
	} catch (const std::exception& e) {
		log("gen-test-replay: FAIL (%s)\n", e.what());
		return 1;
	} catch (...) {
		log("gen-test-replay: FAIL (unknown exception)\n");
		return 1;
	}
}

#endif

int main(int argc, char** argv) {
	std::cerr << "OpenSnowstorm gfxtest starting..." << std::endl;
	using namespace bwgame;

#ifndef EMSCRIPTEN
	try {
		// Argument parsing
		const char* replay_file = nullptr;
		const char* map_file = nullptr;
		const char* data_dir_arg = nullptr;
		int bench_frames = 0;
		const char* verify_hashes_file = nullptr;
		const char* record_hashes_file = nullptr;
		int hash_interval = 240;
		bool validate_replay = false;
		bool headless = false;
		bool show_help = false;
		map_game_type_t map_game_type = map_game_type_t::auto_detect;
		bool debug_overlay = false;
		bool map_fog_of_war = true;
		int local_player_slot = -1;
		int enemy_player_slot = -1;
		int local_race = 5;
		int enemy_race = 5;
		int headless_map_frame_limit = 0;
		const char* gen_test_replay_file = nullptr;
		int gen_test_replay_frames = 240;
		const char* extract_scenario_chk_file = nullptr;
		bool gen_test_replay_use_map_settings = false;
		const char* fixture_script_name = nullptr;
		const char* config_file = nullptr;

		for (int i = 1; i < argc; ++i) {
			if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
				show_help = true;
			} else if (strcmp(argv[i], "--replay") == 0) {
				if (i + 1 >= argc) {
					log("error: --replay requires a file path\n");
					return 2;
				}
				replay_file = argv[++i];
			} else if (strcmp(argv[i], "--map") == 0) {
				if (i + 1 >= argc) {
					log("error: --map requires a file path\n");
					return 2;
				}
				map_file = argv[++i];
			} else if (strcmp(argv[i], "--data-dir") == 0) {
				if (i + 1 >= argc) {
					log("error: --data-dir requires a folder path\n");
					return 2;
				}
				data_dir_arg = argv[++i];
			} else if (strcmp(argv[i], "--config") == 0) {
				if (i + 1 >= argc) {
					log("error: --config requires a file path\n");
					return 2;
				}
				config_file = argv[++i];
			} else if (strcmp(argv[i], "--bench") == 0) {
				if (i + 1 >= argc) {
					log("error: --bench requires frame count\n");
					return 2;
				}
				bench_frames = atoi(argv[++i]);
			} else if (strcmp(argv[i], "--validate-replay") == 0) {
				validate_replay = true;
			} else if (strcmp(argv[i], "--record-hashes") == 0) {
				if (i + 1 >= argc) {
					log("error: --record-hashes requires a file path\n");
					return 2;
				}
				record_hashes_file = argv[++i];
			} else if (strcmp(argv[i], "--verify-hashes") == 0) {
				if (i + 1 >= argc) {
					log("error: --verify-hashes requires a file path\n");
					return 2;
				}
				verify_hashes_file = argv[++i];
			} else if (strcmp(argv[i], "--hash-interval") == 0) {
				if (i + 1 >= argc) {
					log("error: --hash-interval requires an integer\n");
					return 2;
				}
				hash_interval = atoi(argv[++i]);
			} else if (strcmp(argv[i], "--gen-test-replay") == 0) {
				if (i + 1 >= argc) {
					log("error: --gen-test-replay requires an output path\n");
					return 2;
				}
				gen_test_replay_file = argv[++i];
			} else if (strcmp(argv[i], "--extract-scenario-chk") == 0) {
				if (i + 1 >= argc) {
					log("error: --extract-scenario-chk requires an output path\n");
					return 2;
				}
				extract_scenario_chk_file = argv[++i];
			} else if (strcmp(argv[i], "--use-map-settings") == 0) {
				gen_test_replay_use_map_settings = true;
			} else if (strcmp(argv[i], "--fixture-script") == 0) {
				if (i + 1 >= argc) {
					log("error: --fixture-script requires a script name\n");
					return 2;
				}
				fixture_script_name = argv[++i];
			} else if (strcmp(argv[i], "--headless") == 0) {
				headless = true;
			} else if (strcmp(argv[i], "--debug-overlay") == 0) {
				debug_overlay = true;
			} else if (strcmp(argv[i], "--no-fog") == 0) {
				map_fog_of_war = false;
			} else if (strcmp(argv[i], "--local-player") == 0 || strcmp(argv[i], "--slot") == 0) {
				if (i + 1 >= argc) {
					log("error: --local-player requires index\n");
					return 2;
				}
				local_player_slot = parse_slot_or_error(argv[++i], "--local-player");
			} else if (strcmp(argv[i], "--enemy-player") == 0 || strcmp(argv[i], "--enemy-slot") == 0) {
				if (i + 1 >= argc) {
					log("error: --enemy-player requires index\n");
					return 2;
				}
				enemy_player_slot = parse_slot_or_error(argv[++i], "--enemy-player");
			} else if (strcmp(argv[i], "--local-race") == 0 || strcmp(argv[i], "--race") == 0) {
				if (i + 1 >= argc) {
					log("error: --local-race requires name (zerg|terran|protoss|random)\n");
					return 2;
				}
				local_race = parse_race_or_error(argv[++i], "--local-race");
			} else if (strcmp(argv[i], "--enemy-race") == 0) {
				if (i + 1 >= argc) {
					log("error: --enemy-race requires name\n");
					return 2;
				}
				enemy_race = parse_race_or_error(argv[++i], "--enemy-race");
			} else if (strcmp(argv[i], "--game-type") == 0) {
				if (i + 1 >= argc) {
					log("error: --game-type requires name (auto|melee|ums)\n");
					return 2;
				}
				std::string v = argv[++i];
				if (v == "auto") map_game_type = map_game_type_t::auto_detect;
				else if (v == "melee") map_game_type = map_game_type_t::melee;
				else if (v == "ums") map_game_type = map_game_type_t::ums;
				else error("invalid --game-type '%s' (expected auto|melee|ums)", v.c_str());
			} else if (strcmp(argv[i], "--frames") == 0 || strcmp(argv[i], "--headless-map") == 0) {
				if (i + 1 < argc && argv[i + 1][0] != '-') {
					headless_map_frame_limit = atoi(argv[++i]);
				} else {
					headless_map_frame_limit = 72000;
				}
				headless = true;
			}
		}

		if (show_help) {
			print_usage(argv[0]);
			return 0;
		}

		std::cerr << "Resolving data directory..." << std::endl;
		g_data_dir = resolve_data_dir_or_throw(argv[0], data_dir_arg);
		std::cerr << "Data directory: " << g_data_dir << std::endl;

		// -------------------------------------------------------------------
		// Headless Diagnostics & Validation Handlers
		// -------------------------------------------------------------------
		if (validate_replay) {
			return run_validate_replay(replay_file);
		}
		if (bench_frames > 0) {
			return run_bench(bench_frames, replay_file);
		}
		if (verify_hashes_file) {
			return run_verify_hashes(replay_file, verify_hashes_file);
		}
		if (record_hashes_file && !gen_test_replay_file) {
			return run_record_hashes(record_hashes_file, replay_file, hash_interval);
		}
		if (extract_scenario_chk_file) {
			return run_extract_scenario_chk(map_file, extract_scenario_chk_file);
		}
		if (gen_test_replay_file) {
			return run_gen_test_replay(map_file, gen_test_replay_file, record_hashes_file, gen_test_replay_frames, hash_interval, gen_test_replay_use_map_settings, fixture_script_name);
		}

		auto load_data_file = make_load_data_file();
		game_player player(load_data_file);

		std::cerr << "Initializing engine..." << std::endl;
		auto m_ptr = std::make_unique<main_t>(std::move(player));
		main_t& m = *m_ptr;
		auto& ui = m.ui;
		std::cerr << "Engine initialized." << std::endl;

		m.ui.load_all_image_data(load_data_file);

		ui.load_data_file = [&](a_vector<uint8_t>& data, a_string filename) {
			load_data_file(data, std::move(filename));
		};
		ui.load_data_file_exists = [&](const a_string& filename) {
			return load_data_file.file_exists(filename);
		};

		log("initializing ui...\n");
		if (config_file) ui.options_path = config_file;
		ui.init();
		log("ui initialized\n");
		m.campaign_fog_of_war = ui.settings.fog_of_war;
		m.sound_enabled = ui.settings.sound_enabled;

		ui.rgba_overlay_cb = [&m](uint32_t* pixels, int pitch, int width, int height) {
			m.draw_client_overlays(pixels, pitch, width, height);
		};

		ui.local_player_id = local_player_slot;
		ui.enemy_player_id = enemy_player_slot;
		ui.show_debug_overlay = debug_overlay;

		if (map_file) {
			m.campaign_fog_of_war = map_fog_of_war;
			ui.settings.fog_of_war = map_fog_of_war;
			ui.default_enforce_local_visibility = map_fog_of_war;
			m.campaign_local_race = local_race;
			m.campaign_enemy_race = enemy_race;
			m.campaign_game_type = map_game_type;
			m.load_single_player_map(map_file);
		} else if (replay_file) {
			m.load_replay_session(replay_file);
		} else if (!headless) {
			m.enable_frontend(discover_startup_entries(g_data_dir));
		} else if (file_exists("maps/p49.rep")) {
			m.load_replay_session("maps/p49.rep");
		} else {
			log("Warning: No map/replay provided in headless mode, and maps/p49.rep not found. Idling.\n");
		}

		size_t screen_width = 1280;
		size_t screen_height = 800;

		auto& wnd = ui.wnd;
		if (!headless) {
			std::cerr << "Creating window..." << std::endl;
			wnd.create("OpenSnowstorm - Brood War", 0, 0, (int)screen_width, (int)screen_height);
			std::cerr << "Window created." << std::endl;
		}

		ui.resize((int)screen_width, (int)screen_height);
		if (!m.frontend_active) {
			m.center_view_on_loaded_content();
			ui.set_image_data();
		}

		::g_m = &m;

		if (headless) {
			log("Headless mode active.\n");
			if (ui.is_replay_mode) {
				while (!ui.is_done()) {
					ui.replay_functions::next_frame();
				}
			} else {
				const int frame_limit = headless_map_frame_limit > 0 ? headless_map_frame_limit : 72000;
				log("single-player headless: stepping until local victory/defeat (limit=%d)\n", frame_limit);
				int stepped_frames = 0;
				while (stepped_frames < frame_limit) {
					ui.state_functions::next_frame();
					++stepped_frames;
					if (ui.has_local_player()) {
						if (ui.player_won(ui.local_player_id)) {
							log("single-player headless: PASS (victory at frame %d)\n", ui.st.current_frame);
							break;
						}
						if (ui.player_defeated(ui.local_player_id)) {
							log("single-player headless: PASS (defeat at frame %d)\n", ui.st.current_frame);
							break;
						}
					}
				}
			}
			return 0;
		}

		std::chrono::high_resolution_clock clock;
		while (!ui.window_closed) {
			auto frame_start = clock.now();
			m.update();
			auto frame_elapsed = clock.now() - frame_start;
			auto target = std::chrono::milliseconds(16);
			if (frame_elapsed < target) {
				std::this_thread::sleep_for(target - frame_elapsed);
			}
		}

		return 0;
	} catch (const std::exception& e) {
		std::cerr << "FATAL ERROR: " << e.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "FATAL ERROR: unknown exception" << std::endl;
		return 1;
	}
#endif
	return 0;
}

