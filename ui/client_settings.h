#ifndef OPENSNOWSTORM_CLIENT_SETTINGS_H
#define OPENSNOWSTORM_CLIENT_SETTINGS_H

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace bwgame {

// Runtime client options persisted to options.ini.  Simulation state is never
// derived from these values except for fog-of-war rendering and game speed,
// both of which are local-only presentation / pacing knobs.
struct client_settings_t {
	int game_speed = 4;       // 1X / 2X / 4X / 8X multiplier
	int sfx_volume = 50;      // 0-100
	int music_volume = 50;    // 0-100
	bool sound_enabled = true;
	bool unit_speech = true;
	bool unit_sounds = true;
	bool fog_of_war = true;
	bool show_objectives = true;
	bool show_health_bars = true;
	int mouse_scroll = 3;     // 0 = off, 1-6
	int keyboard_scroll = 3;  // 1-6
	bool fullscreen = false;

	char hotkey_stop = 's';
	char hotkey_hold = 'h';
	char hotkey_attack = 'a';
	char hotkey_patrol = 't';
	char hotkey_build = 'b';
	char hotkey_cloak = 'c';
	char hotkey_burrow = 'b';
	char hotkey_siege = 'g';
	char hotkey_stim = 'i';
	char hotkey_unload = 'l';
	char hotkey_lift = 'l';
	char hotkey_return_cargo = 'r';
	char hotkey_merge = 'm';
	char hotkey_cancel = 'x';
};

enum class game_menu_item {
	return_to_game = 0,
	options,
	save_game,
	load_game,
	restart_mission,
	exit_to_menu,
	count
};

enum class settings_item {
	game_speed = 0,
	music_volume,
	sfx_volume,
	sound,
	unit_speech,
	unit_sounds,
	fog_of_war,
	mouse_scroll,
	keyboard_scroll,
	fullscreen,
	objectives,
	health_bars,
	count
};

struct overlay_rect {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	bool contains(int px, int py) const {
		return px >= x && py >= y && px < x + w && py < y + h;
	}
};

struct overlay_panel_layout {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	int item_y0 = 0;
	int item_h = 0;
	int count = 0;
	overlay_rect item_rect(int i) const {
		if (i < 0 || i >= count) return {};
		int iy = item_y0 + i * item_h;
		return overlay_rect{x + 16, iy, w - 32, item_h - 4};
	}
};

inline overlay_panel_layout make_menu_panel_layout(int screen_w, int screen_h, int count, int item_h, int header_h) {
	const int pad = 24;
	int w = std::min(560, std::max(320, screen_w - 80));
	int h = pad + header_h + count * item_h + pad;
	if (h > screen_h - 40) {
		item_h = std::max(18, (screen_h - 40 - pad * 2 - header_h) / std::max(1, count));
		h = pad + header_h + count * item_h + pad;
	}
	int x = (screen_w - w) / 2;
	int y = (screen_h - h) / 2;
	if (y < 16) y = 16;
	return overlay_panel_layout{x, y, w, h, y + pad + header_h, item_h, count};
}

inline overlay_panel_layout make_game_menu_layout(int screen_w, int screen_h) {
	return make_menu_panel_layout(screen_w, screen_h, (int)game_menu_item::count, 40, 36);
}

inline overlay_panel_layout make_settings_layout(int screen_w, int screen_h) {
	return make_menu_panel_layout(screen_w, screen_h, (int)settings_item::count, 32, 36);
}

inline const char* game_menu_item_label(int index) {
	switch ((game_menu_item)index) {
	case game_menu_item::return_to_game: return "RETURN TO GAME";
	case game_menu_item::options: return "OPTIONS";
	case game_menu_item::save_game: return "SAVE GAME";
	case game_menu_item::load_game: return "LOAD GAME";
	case game_menu_item::restart_mission: return "RESTART MISSION";
	case game_menu_item::exit_to_menu: return "EXIT TO MENU";
	default: return "";
	}
}

inline int clamp_int(int v, int lo, int hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

const int k_game_speed_presets[] = {1, 2, 4, 8};
const char* const k_game_speed_labels[] = {"1X", "2X", "4X", "8X"};
const int k_game_speed_preset_count = 4;

inline int game_speed_preset_index(int speed) {
	if (speed <= 1) return 0;
	if (speed <= 2) return 1;
	if (speed <= 4) return 2;
	return 3;
}

inline int cycle_game_speed(int speed, int delta) {
	int idx = game_speed_preset_index(speed) + delta;
	idx = clamp_int(idx, 0, k_game_speed_preset_count - 1);
	return k_game_speed_presets[idx];
}

inline const char* game_speed_label(int speed) {
	return k_game_speed_labels[game_speed_preset_index(speed)];
}

inline const char* settings_item_label(int index) {
	switch ((settings_item)index) {
	case settings_item::game_speed: return "GAME SPEED";
	case settings_item::music_volume: return "MUSIC VOLUME";
	case settings_item::sfx_volume: return "SFX VOLUME";
	case settings_item::sound: return "SOUND";
	case settings_item::unit_speech: return "UNIT SPEECH";
	case settings_item::unit_sounds: return "UNIT SOUNDS";
	case settings_item::fog_of_war: return "FOG OF WAR";
	case settings_item::mouse_scroll: return "MOUSE SCROLL";
	case settings_item::keyboard_scroll: return "KEYBOARD SCROLL";
	case settings_item::fullscreen: return "FULLSCREEN";
	case settings_item::objectives: return "OBJECTIVES PANEL";
	case settings_item::health_bars: return "HEALTH BARS";
	default: return "";
	}
}

inline std::string on_off(bool v) { return v ? "ON" : "OFF"; }

inline std::string settings_item_value(const client_settings_t& s, int index) {
	switch ((settings_item)index) {
	case settings_item::game_speed: return game_speed_label(s.game_speed);
	case settings_item::music_volume: return std::to_string(s.music_volume);
	case settings_item::sfx_volume: return std::to_string(s.sfx_volume);
	case settings_item::sound: return on_off(s.sound_enabled);
	case settings_item::unit_speech: return on_off(s.unit_speech);
	case settings_item::unit_sounds: return on_off(s.unit_sounds);
	case settings_item::fog_of_war: return on_off(s.fog_of_war);
	case settings_item::mouse_scroll: return s.mouse_scroll <= 0 ? std::string("OFF") : std::to_string(s.mouse_scroll);
	case settings_item::keyboard_scroll: return std::to_string(s.keyboard_scroll);
	case settings_item::fullscreen: return on_off(s.fullscreen);
	case settings_item::objectives: return on_off(s.show_objectives);
	case settings_item::health_bars: return on_off(s.show_health_bars);
	default: return {};
	}
}

inline void cycle_settings_item(client_settings_t& s, int index, int delta) {
	if (delta == 0) delta = 1;
	switch ((settings_item)index) {
	case settings_item::game_speed:
		s.game_speed = cycle_game_speed(s.game_speed, delta);
		break;
	case settings_item::music_volume:
		s.music_volume = clamp_int(s.music_volume + delta * 5, 0, 100);
		break;
	case settings_item::sfx_volume:
		s.sfx_volume = clamp_int(s.sfx_volume + delta * 5, 0, 100);
		break;
	case settings_item::sound:
		s.sound_enabled = !s.sound_enabled;
		break;
	case settings_item::unit_speech:
		s.unit_speech = !s.unit_speech;
		break;
	case settings_item::unit_sounds:
		s.unit_sounds = !s.unit_sounds;
		break;
	case settings_item::fog_of_war:
		s.fog_of_war = !s.fog_of_war;
		break;
	case settings_item::mouse_scroll:
		s.mouse_scroll = clamp_int(s.mouse_scroll + delta, 0, 6);
		break;
	case settings_item::keyboard_scroll:
		s.keyboard_scroll = clamp_int(s.keyboard_scroll + delta, 1, 6);
		break;
	case settings_item::fullscreen:
		s.fullscreen = !s.fullscreen;
		break;
	case settings_item::objectives:
		s.show_objectives = !s.show_objectives;
		break;
	case settings_item::health_bars:
		s.show_health_bars = !s.show_health_bars;
		break;
	default:
		break;
	}
}

inline void trim_inplace(std::string& s) {
	size_t first = s.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		s.clear();
		return;
	}
	size_t last = s.find_last_not_of(" \t\r\n");
	s = s.substr(first, last - first + 1);
}

inline bool parse_bool_value(const std::string& val, bool fallback) {
	std::string v = val;
	for (char& c : v) c = (char)std::tolower((unsigned char)c);
	if (v == "1" || v == "true" || v == "on" || v == "yes") return true;
	if (v == "0" || v == "false" || v == "off" || v == "no") return false;
	return fallback;
}

inline bool parse_client_settings_text(const std::string& text, client_settings_t& out) {
	bool saw_sfx = false;
	bool saw_music = false;
	std::string line;
	for (size_t i = 0; i <= text.size(); ++i) {
		if (i < text.size() && text[i] != '\n') {
			line.push_back(text[i]);
			continue;
		}
		if (!line.empty() && line.back() == '\r') line.pop_back();
		std::string l = line;
		line.clear();
		trim_inplace(l);
		if (l.empty() || l[0] == ';' || l[0] == '#') continue;
		size_t eq = l.find('=');
		if (eq == std::string::npos) continue;
		std::string key = l.substr(0, eq);
		std::string val = l.substr(eq + 1);
		trim_inplace(key);
		trim_inplace(val);
		if (key == "game_speed") {
			int v = atoi(val.c_str());
			if (v > 0) out.game_speed = v;
		} else if (key == "volume") {
			int v = clamp_int(atoi(val.c_str()), 0, 100);
			if (!saw_sfx) out.sfx_volume = v;
			if (!saw_music) out.music_volume = v;
			out.sound_enabled = v > 0 || out.sound_enabled;
		} else if (key == "sfx_volume") {
			out.sfx_volume = clamp_int(atoi(val.c_str()), 0, 100);
			saw_sfx = true;
		} else if (key == "music_volume") {
			out.music_volume = clamp_int(atoi(val.c_str()), 0, 100);
			saw_music = true;
		} else if (key == "sound" || key == "sound_enabled") {
			out.sound_enabled = parse_bool_value(val, out.sound_enabled);
		} else if (key == "unit_speech") {
			out.unit_speech = parse_bool_value(val, out.unit_speech);
		} else if (key == "unit_sounds") {
			out.unit_sounds = parse_bool_value(val, out.unit_sounds);
		} else if (key == "fog_of_war") {
			out.fog_of_war = parse_bool_value(val, out.fog_of_war);
		} else if (key == "show_objectives") {
			out.show_objectives = parse_bool_value(val, out.show_objectives);
		} else if (key == "show_health_bars") {
			out.show_health_bars = parse_bool_value(val, out.show_health_bars);
		} else if (key == "mouse_scroll") {
			out.mouse_scroll = clamp_int(atoi(val.c_str()), 0, 6);
		} else if (key == "keyboard_scroll") {
			out.keyboard_scroll = clamp_int(atoi(val.c_str()), 1, 6);
		} else if (key == "fullscreen") {
			out.fullscreen = parse_bool_value(val, out.fullscreen);
		} else if (key == "hotkey_stop" && !val.empty()) out.hotkey_stop = val[0];
		else if (key == "hotkey_hold" && !val.empty()) out.hotkey_hold = val[0];
		else if (key == "hotkey_attack" && !val.empty()) out.hotkey_attack = val[0];
		else if (key == "hotkey_patrol" && !val.empty()) out.hotkey_patrol = val[0];
		else if (key == "hotkey_build" && !val.empty()) out.hotkey_build = val[0];
		else if (key == "hotkey_cloak" && !val.empty()) out.hotkey_cloak = val[0];
		else if (key == "hotkey_burrow" && !val.empty()) out.hotkey_burrow = val[0];
		else if (key == "hotkey_siege" && !val.empty()) out.hotkey_siege = val[0];
		else if (key == "hotkey_stim" && !val.empty()) out.hotkey_stim = val[0];
		else if (key == "hotkey_unload" && !val.empty()) out.hotkey_unload = val[0];
		else if (key == "hotkey_lift" && !val.empty()) out.hotkey_lift = val[0];
		else if (key == "hotkey_return_cargo" && !val.empty()) out.hotkey_return_cargo = val[0];
		else if (key == "hotkey_merge" && !val.empty()) out.hotkey_merge = val[0];
		else if (key == "hotkey_cancel" && !val.empty()) out.hotkey_cancel = val[0];
	}
	return true;
}

inline bool load_client_settings_file(const char* path, client_settings_t& out) {
	if (!path || !path[0]) return false;
	FILE* f = fopen(path, "r");
	if (!f) return false;
	std::string text;
	char buf[512];
	while (fgets(buf, sizeof(buf), f)) text += buf;
	fclose(f);
	return parse_client_settings_text(text, out);
}

inline std::string serialize_client_settings(const client_settings_t& s) {
	char buf[2048];
	std::snprintf(buf, sizeof(buf),
		"; OpenSnowstorm Options\n"
		"; Game Speed: 1=1X, 2=2X, 4=4X, 8=8X\n"
		"game_speed=%d\n"
		"sfx_volume=%d\n"
		"music_volume=%d\n"
		"volume=%d\n"
		"sound=%d\n"
		"unit_speech=%d\n"
		"unit_sounds=%d\n"
		"fog_of_war=%d\n"
		"mouse_scroll=%d\n"
		"keyboard_scroll=%d\n"
		"fullscreen=%d\n"
		"show_objectives=%d\n"
		"show_health_bars=%d\n"
		"\n"
		"; Hotkeys (ASCII characters)\n"
		"hotkey_stop=%c\n"
		"hotkey_hold=%c\n"
		"hotkey_attack=%c\n"
		"hotkey_patrol=%c\n"
		"hotkey_build=%c\n"
		"hotkey_cloak=%c\n"
		"hotkey_burrow=%c\n"
		"hotkey_siege=%c\n"
		"hotkey_stim=%c\n"
		"hotkey_unload=%c\n"
		"hotkey_lift=%c\n"
		"hotkey_return_cargo=%c\n"
		"hotkey_merge=%c\n"
		"hotkey_cancel=%c\n",
		s.game_speed,
		s.sfx_volume,
		s.music_volume,
		s.sfx_volume,
		s.sound_enabled ? 1 : 0,
		s.unit_speech ? 1 : 0,
		s.unit_sounds ? 1 : 0,
		s.fog_of_war ? 1 : 0,
		s.mouse_scroll,
		s.keyboard_scroll,
		s.fullscreen ? 1 : 0,
		s.show_objectives ? 1 : 0,
		s.show_health_bars ? 1 : 0,
		s.hotkey_stop, s.hotkey_hold, s.hotkey_attack, s.hotkey_patrol,
		s.hotkey_build, s.hotkey_cloak, s.hotkey_burrow, s.hotkey_siege,
		s.hotkey_stim, s.hotkey_unload, s.hotkey_lift, s.hotkey_return_cargo,
		s.hotkey_merge, s.hotkey_cancel);
	return std::string(buf);
}

inline bool save_client_settings_file(const char* path, const client_settings_t& s) {
	if (!path || !path[0]) return false;
	FILE* f = fopen(path, "w");
	if (!f) return false;
	std::string text = serialize_client_settings(s);
	size_t n = fwrite(text.data(), 1, text.size(), f);
	fclose(f);
	return n == text.size();
}

} // namespace bwgame

#endif
