#include "ui/client_settings.h"

#include <cstdio>
#include <string>

static int fail(const char* msg) {
	std::fprintf(stderr, "test_client_settings: FAIL (%s)\n", msg);
	return 1;
}

int main() {
	bwgame::client_settings_t s;
	s.game_speed = 8;
	s.sfx_volume = 77;
	s.music_volume = 12;
	s.sound_enabled = false;
	s.unit_speech = false;
	s.fog_of_war = false;
	s.mouse_scroll = 0;
	s.keyboard_scroll = 6;
	s.fullscreen = true;
	s.show_objectives = false;
	s.hotkey_stop = 'q';

	std::string text = bwgame::serialize_client_settings(s);
	bwgame::client_settings_t t;
	if (!bwgame::parse_client_settings_text(text, t)) return fail("parse serialized settings");
	if (t.game_speed != 8) return fail("game_speed");
	if (t.sfx_volume != 77) return fail("sfx_volume");
	if (t.music_volume != 12) return fail("music_volume");
	if (t.sound_enabled) return fail("sound_enabled");
	if (t.unit_speech) return fail("unit_speech");
	if (t.fog_of_war) return fail("fog_of_war");
	if (t.mouse_scroll != 0) return fail("mouse_scroll");
	if (t.keyboard_scroll != 6) return fail("keyboard_scroll");
	if (!t.fullscreen) return fail("fullscreen");
	if (t.show_objectives) return fail("show_objectives");
	if (t.hotkey_stop != 'q') return fail("hotkey_stop");

	bwgame::client_settings_t legacy;
	if (!bwgame::parse_client_settings_text("volume=40\ngame_speed=2\nsound=off\n", legacy))
		return fail("parse legacy");
	if (legacy.sfx_volume != 40 || legacy.music_volume != 40) return fail("legacy volume");
	if (legacy.game_speed != 2) return fail("legacy game_speed");
	if (legacy.sound_enabled) return fail("legacy sound");

	bwgame::cycle_settings_item(s, (int)bwgame::settings_item::sfx_volume, 1);
	if (s.sfx_volume != 82) return fail("cycle sfx_volume");
	bwgame::cycle_settings_item(s, (int)bwgame::settings_item::game_speed, -1);
	if (s.game_speed != 4) return fail("cycle game_speed");

	const char* tmp = "test_client_settings.tmp.ini";
	if (!bwgame::save_client_settings_file(tmp, t)) return fail("save file");
	bwgame::client_settings_t u;
	if (!bwgame::load_client_settings_file(tmp, u)) return fail("load file");
	if (u.sfx_volume != 77 || u.hotkey_stop != 'q') return fail("roundtrip file");
	std::remove(tmp);

	std::printf("test_client_settings: PASS\n");
	return 0;
}
