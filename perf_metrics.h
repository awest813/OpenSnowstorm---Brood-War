#ifndef BWGAME_PERF_METRICS_H
#define BWGAME_PERF_METRICS_H

// Lightweight, zero-dependency performance instrumentation for the bwgame
// simulation engine.  Everything here is header-only and opt-in; including
// this file has no effect on simulation determinism.
//
// Key types
// ---------
//   frame_timer        - rolling-window statistics for per-frame wall-clock
//                        time.  Call tick() once per simulated frame and read
//                        stats via fps(), mean_us(), min_us(), max_us().
//
//   scope_timer        - RAII microsecond timer.  Write the elapsed time into
//                        a caller-supplied int64_t on destruction.
//
//   perf_categories    - compile-time category tags for future structured
//                        logging (sync, replay, ui, data, perf).

#include <chrono>
#include <cstdint>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace bwgame {
namespace perf {

// ---------------------------------------------------------------------------
// scope_timer — measures the wall-clock elapsed time of a lexical scope.
// ---------------------------------------------------------------------------
struct scope_timer {
	using clock     = std::chrono::high_resolution_clock;
	using time_point = clock::time_point;

	int64_t& out_us;
	time_point start;

	explicit scope_timer(int64_t& out_us)
	    : out_us(out_us), start(clock::now()) {}

	~scope_timer() {
		out_us = std::chrono::duration_cast<std::chrono::microseconds>(
		             clock::now() - start)
		             .count();
	}

	scope_timer(const scope_timer&) = delete;
	scope_timer& operator=(const scope_timer&) = delete;
};

// ---------------------------------------------------------------------------
// frame_timer — rolling-window statistics for repeated events (frames).
//
// Usage:
//   frame_timer ft;
//   while (running) {
//       simulate_one_frame();
//       ft.tick();
//   }
//   printf("fps=%.1f  mean=%.1fus\n", ft.fps(), ft.mean_us());
// ---------------------------------------------------------------------------
template<size_t WindowSize = 256>
struct basic_frame_timer {
	static_assert(WindowSize >= 2, "WindowSize must be at least 2");

	using clock      = std::chrono::high_resolution_clock;
	using time_point = clock::time_point;
	using duration   = std::chrono::duration<double>;

	std::array<int64_t, WindowSize> samples{};
	size_t head    = 0;
	size_t count   = 0;
	time_point last = clock::now();

	// Call once per frame (or per event you want to measure).
	void tick() {
		auto now   = clock::now();
		int64_t us = std::chrono::duration_cast<std::chrono::microseconds>(
		                 now - last)
		                 .count();
		last = now;

		samples[head] = us;
		head = (head + 1) % WindowSize;
		if (count < WindowSize) ++count;
	}

	// Reset all accumulated statistics.
	void reset() {
		head  = 0;
		count = 0;
		last  = clock::now();
		samples.fill(0);
	}

	// Frames per second based on the rolling window.
	double fps() const {
		if (count == 0) return 0.0;
		double total_us = 0;
		for (size_t i = 0; i < count; ++i) total_us += static_cast<double>(samples[i]);
		return count / (total_us * 1e-6);
	}

	// Mean frame time in microseconds.
	double mean_us() const {
		if (count == 0) return 0.0;
		int64_t total = 0;
		for (size_t i = 0; i < count; ++i) total += samples[i];
		return static_cast<double>(total) / count;
	}

	// Minimum frame time in microseconds (over the window).
	int64_t min_us() const {
		if (count == 0) return 0;
		int64_t m = std::numeric_limits<int64_t>::max();
		for (size_t i = 0; i < count; ++i) m = std::min(m, samples[i]);
		return m;
	}

	// Maximum frame time in microseconds (over the window).
	int64_t max_us() const {
		if (count == 0) return 0;
		int64_t m = 0;
		for (size_t i = 0; i < count; ++i) m = std::max(m, samples[i]);
		return m;
	}

	// Approximate percentile (p in [0,100]).  Uses a sorted copy of the window
	// so this is O(n log n) — intended for post-hoc reporting, not hot paths.
	int64_t percentile_us(int p) const {
		if (count == 0) return 0;
		std::array<int64_t, WindowSize> sorted;
		for (size_t i = 0; i < count; ++i) sorted[i] = samples[i];
		std::sort(sorted.begin(), sorted.begin() + count);
		size_t idx = static_cast<size_t>(p) * (count - 1) / 100;
		return sorted[idx];
	}

	size_t window_size() const { return count; }
};

// Default frame_timer with a 256-sample rolling window.
using frame_timer = basic_frame_timer<256>;

// ---------------------------------------------------------------------------
// perf_categories — string constants for structured log tags.
//
// Intended use: log("[perf] frame %d: %.1fus\n", frame, us);
// Matching the category string lets grep/dashboards filter easily.
// ---------------------------------------------------------------------------
struct perf_categories {
	static constexpr const char* sim    = "sim";
	static constexpr const char* sync   = "sync";
	static constexpr const char* replay = "replay";
	static constexpr const char* ui     = "ui";
	static constexpr const char* data   = "data";
	static constexpr const char* perf   = "perf";
};

} // namespace perf
} // namespace bwgame

#endif // BWGAME_PERF_METRICS_H
