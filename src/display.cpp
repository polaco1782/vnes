#include "display.h"
#include <algorithm>
#include <cstring>
#include <type_traits>

Display::Display(const char* title, Bus& bus, int scale)
	: gui_(bus)
	, scale_factor(scale)
	, escape_pressed(false)
	, stop_scaler_(false)
	, scale_requested_(false)
	, scaled_frame_ready_(false)
	, gui_texture_was_needed_(false)
{
	window_width = NES_WIDTH * scale_factor;
	window_height = NES_HEIGHT * scale_factor;

	// Ensure minimum 800x600
	if (window_width < 800) {
		scale_factor = (800 + NES_WIDTH - 1) / NES_WIDTH;
		window_width = NES_WIDTH * scale_factor;
		window_height = NES_HEIGHT * scale_factor;
	}
	// SFML 3.x: VideoMode takes a Vector2u and bitsPerPixel
	sf::VideoMode mode(sf::Vector2u(static_cast<unsigned int>(window_width), static_cast<unsigned int>(window_height)), 32);
	window = std::make_unique<sf::RenderWindow>(mode, title);

	// Initialize GUI after window creation
	gui_.initialize(*window);
	texture = std::make_unique<sf::Texture>(sf::Vector2u{ SCALED_WIDTH, SCALED_HEIGHT });
	sprite = std::make_unique<sf::Sprite>(*texture);
	sprite->setScale(sf::Vector2f{
		 static_cast<float>(window_width) / static_cast<float>(SCALED_WIDTH),
		 static_cast<float>(window_height) / static_cast<float>(SCALED_HEIGHT)
	});

	// Allocate pixel buffer (RGBA format)
	pixels.resize(SCALED_WIDTH * SCALED_HEIGHT * 4);
	scaled_framebuffer.resize(SCALED_WIDTH * SCALED_HEIGHT);
	pending_framebuffer_.resize(NES_WIDTH * NES_HEIGHT);
	completed_pixels_.resize(SCALED_WIDTH * SCALED_HEIGHT * 4);
	completed_scaled_framebuffer_.resize(SCALED_WIDTH * SCALED_HEIGHT);

	sf::Image img(sf::Vector2u{ static_cast<unsigned int>(SCALED_WIDTH), static_cast<unsigned int>(SCALED_HEIGHT) }, pixels.data());
	[[maybe_unused]] bool loaded = texture->loadFromImage(img);

	scaler_thread_ = std::thread(&Display::scalerThreadLoop, this);
}

Display::~Display()
{
	{
		std::lock_guard<std::mutex> lock(scaler_mutex_);
		stop_scaler_ = true;
	}

	scaler_cv_.notify_one();
	if (scaler_thread_.joinable()) {
		scaler_thread_.join();
	}
}

void Display::update(const u32* framebuffer)
{
	queueFrame(framebuffer);
	const bool hasNewFrame = consumeScaledFrame();
	if (hasNewFrame) {
		sf::Image img(sf::Vector2u{ static_cast<unsigned int>(SCALED_WIDTH), static_cast<unsigned int>(SCALED_HEIGHT) }, pixels.data());
		[[maybe_unused]] bool loaded = texture->loadFromImage(img);
	}

	// Only update the GUI's emulator texture when the window is actually visible
	const bool needsGuiTexture = gui_.needsEmulatorTextureUpdate();
	if (needsGuiTexture && (hasNewFrame || !gui_texture_was_needed_)) {
		gui_.updateEmulatorTexture(scaled_framebuffer.data(), SCALED_WIDTH, SCALED_HEIGHT);
	}
	gui_texture_was_needed_ = needsGuiTexture;

	// Render (draw only). Do not call display() here so GUI can be rendered on top
	// by ImGui before presenting the frame.
	window->clear();

	// Only draw the main sprite if not rendering in a GUI window
	if (!gui_.isEmulatorInWindow() || !gui_.isMenuVisible()) {
		window->draw(*sprite);
	}
}

bool Display::isOpen() const
{
	return window->isOpen();
}

bool Display::wasEscapePressed()
{
	bool result = escape_pressed;
	escape_pressed = false;
	return result;
}

void Display::pollEvents()
{
	while (auto event = window->pollEvent()) {
		// Forward event to ImGui
		gui_.processEvent(*window, *event);

		if (event->is<sf::Event::Closed>()) {
			window->close();
		}

		// Only handle keyboard shortcuts if ImGui is not capturing keyboard
		if (!ImGui::GetIO().WantCaptureKeyboard && event->is<sf::Event::KeyPressed>()) {
			auto key = event->getIf<sf::Event::KeyPressed>()->code;

			if (key == sf::Keyboard::Key::Escape) {
				gui_.toggleMenu();
			}
			// P key toggles pause (when menu is visible)
			else if (key == sf::Keyboard::Key::P && gui_.isMenuVisible()) {
				gui_.setPaused(!gui_.isPaused());
			}
		}
	}
}

void Display::present()
{
	// Update and render GUI only if menu is visible, then present the window contents and handle frame timing
	float elapsed = clock.getElapsedTime().asSeconds();
	if (gui_.isMenuVisible()) {
		gui_.update(*window, elapsed);
		gui_.render(*window);
	}

	window->display();

	// Frame timing - wait until target frame time has elapsed
	if (elapsed < TARGET_FRAME_TIME) {
		sf::sleep(sf::seconds(TARGET_FRAME_TIME - elapsed));
	}
	clock.restart();
}

void Display::scalerThreadLoop()
{
	std::vector<u32> source_framebuffer(NES_WIDTH * NES_HEIGHT);
	std::vector<u32> scaled_output(SCALED_WIDTH * SCALED_HEIGHT);
	std::vector<u8> pixel_output(SCALED_WIDTH * SCALED_HEIGHT * 4);

	for (;;) {
		{
			std::unique_lock<std::mutex> lock(scaler_mutex_);
			scaler_cv_.wait(lock, [this] { return stop_scaler_ || scale_requested_; });
			if (stop_scaler_) {
				return;
			}

			source_framebuffer = pending_framebuffer_;
			scale_requested_ = false;
		}

		scaler_.resize(source_framebuffer.data(), NES_WIDTH, NES_HEIGHT, scaled_output.data());

		for (int i = 0; i < SCALED_WIDTH * SCALED_HEIGHT; i++) {
			const u32 color = scaled_output[i];
			pixel_output[i * 4 + 0] = static_cast<u8>((color >> 16) & 0xFF);
			pixel_output[i * 4 + 1] = static_cast<u8>((color >> 8) & 0xFF);
			pixel_output[i * 4 + 2] = static_cast<u8>(color & 0xFF);
			pixel_output[i * 4 + 3] = static_cast<u8>((color >> 24) & 0xFF);
		}

		{
			std::lock_guard<std::mutex> lock(scaler_mutex_);
			completed_scaled_framebuffer_.swap(scaled_output);
			completed_pixels_.swap(pixel_output);
			scaled_frame_ready_ = true;
		}
	}
}

void Display::queueFrame(const u32* framebuffer)
{
	{
		std::lock_guard<std::mutex> lock(scaler_mutex_);
		std::copy_n(framebuffer, NES_WIDTH * NES_HEIGHT, pending_framebuffer_.begin());
		scale_requested_ = true;
	}

	scaler_cv_.notify_one();
}

bool Display::consumeScaledFrame()
{
	std::lock_guard<std::mutex> lock(scaler_mutex_);
	if (!scaled_frame_ready_) {
		return false;
	}

	scaled_framebuffer.swap(completed_scaled_framebuffer_);
	pixels.swap(completed_pixels_);
	scaled_frame_ready_ = false;
	return true;
}
