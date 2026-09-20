#include "native_window.h"
#include "native_window_drawing.h"
#include "native_sound.h"
#include <vector>

namespace native_window {
	struct window_impl {};
	window::window() : impl(std::make_unique<window_impl>()) {}
	window::~window() {}
	window::window(window&& n) : impl(std::move(n.impl)) {}
	void window::destroy() {}
	bool window::create(const char*, int, int, int, int) { return false; }
	void window::get_cursor_pos(int* x, int* y) { *x = 0; *y = 0; }
	bool window::peek_message(event_t&) { return false; }
	bool window::show_cursor(bool) { return false; }
	bool window::get_key_state(int) { return false; }
	bool window::get_mouse_button_state(int) { return false; }
	void window::update_surface() {}
	void window::set_fullscreen(bool) {}
	bool window::is_fullscreen() const { return false; }
	window::operator bool() const { return false; }
}

namespace native_window_drawing {
	struct palette_impl : palette {
		virtual void set_colors(color colors[256]) override {}
	};
	struct dummy_surface : surface {
		dummy_surface(int w_, int h_) { w = w_; h = h_; pitch = w * 4; }
		virtual void set_palette(palette*) override {}
		virtual void* lock() override { return nullptr; }
		virtual void unlock() override {}
		virtual void blit(surface*, int, int) override {}
		virtual void blit_scaled(surface*, int, int, int, int) override {}
		virtual void fill(int, int, int, int) override {}
		virtual void set_alpha(int) override {}
		virtual void set_blend_mode(blend_mode) override {}
	};
	std::unique_ptr<surface> create_rgba_surface(int w, int h) { return std::make_unique<dummy_surface>(w, h); }
	std::unique_ptr<surface> get_window_surface(native_window::window*) { return nullptr; }
	std::unique_ptr<surface> convert_to_8_bit_indexed(surface* s) { return std::make_unique<dummy_surface>(s->w, s->h); }
	palette* new_palette() { return new palette_impl(); }
	void delete_palette(palette* p) { delete p; }
	std::unique_ptr<surface> load_image(const char*) { return nullptr; }
	std::unique_ptr<surface> load_image(const void*, size_t) { return nullptr; }
}

namespace native_sound {
	int frequency = 44100;
	int channels = 2;
	void init() {}
	void play(int, sound*, int, int) {}
	bool is_playing(int) { return false; }
	void stop(int) {}
	void set_volume(int, int) {}
	std::unique_ptr<sound> load_wav(const void*, size_t) { return nullptr; }
	void play_music(const void*, size_t) {}
	void stop_music() {}
	void set_music_volume(int) {}
}
