#ifndef DISPLAY_H
#define DISPLAY_H

#include "types.h"
#include <SFML/Graphics.hpp>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <thread>
#include <vector>
#include "gui.h"
#include "hq2x.h"

class PPU;
class Bus;

class Display {
public:
    Display(const char* title, Bus& bus, int scale = 3);
    ~Display();

    // Update display with PPU framebuffer
    void update(const u32* framebuffer);

    // Check if window should close
    bool isOpen() const;

    // Check if ESC was pressed to enter debugger
    bool wasEscapePressed();

    // Process window events
    void pollEvents();

    // Get window dimensions
    int getWidth() const { return window_width; }
    int getHeight() const { return window_height; }

    // Expose the SFML window for GUI integration
    sf::RenderWindow& getWindow() { return *window; }

    // Present the GUI and window contents (call after drawing)
    void present();

    // Retrieve Game Genie code submitted via GUI
    bool getGameGenieCode(std::string& code) { return gui_.getGameGenieCode(code); }

    // Get GUI for main loop to poll actions
    Gui& getGui() { return gui_; }

    // Check if emulator screen is being rendered in a window
    bool isEmulatorInGuiWindow() const { return gui_.isEmulatorInWindow(); }

private:
    void scalerThreadLoop();
    void queueFrame(const u32* framebuffer);
    bool consumeScaledFrame();

    static const int NES_WIDTH = 256;
    static const int NES_HEIGHT = 240;
    static const int HQ2X_SCALE = 2;
    static const int SCALED_WIDTH = NES_WIDTH * HQ2X_SCALE;
    static const int SCALED_HEIGHT = NES_HEIGHT * HQ2X_SCALE;

    std::unique_ptr<sf::RenderWindow> window;
    std::unique_ptr<sf::Texture> texture;
    std::unique_ptr<sf::Sprite> sprite;
    std::vector<u8> pixels;
    std::vector<u32> scaled_framebuffer;
    std::vector<u32> pending_framebuffer_;
    std::vector<u8> completed_pixels_;
    std::vector<u32> completed_scaled_framebuffer_;
    sf::Clock clock;
    Gui gui_;
    HQ2x scaler_;
    std::thread scaler_thread_;
    std::mutex scaler_mutex_;
    std::condition_variable scaler_cv_;

    int window_width;
    int window_height;
    int scale_factor;
    bool escape_pressed;
    bool stop_scaler_;
    bool scale_requested_;
    bool scaled_frame_ready_;
    bool gui_texture_was_needed_;

    static constexpr float TARGET_FRAME_TIME = 1.0f / 60.0f; // 60 FPS
};

#endif // DISPLAY_H
