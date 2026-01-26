#ifndef DISPLAY_H
#define DISPLAY_H

#include "types.h"
#include <SFML/Graphics.hpp>

class PPU;

class Display {
public:
    Display(const char* title, int scale = 3);
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
    
private:
    static const int NES_WIDTH = 256;
    static const int NES_HEIGHT = 240;
    
    sf::RenderWindow window;
    sf::Texture texture;
    sf::Sprite sprite;
    sf::Uint8* pixels;
    sf::Clock clock;
    
    int window_width;
    int window_height;
    int scale_factor;
    bool escape_pressed;
    
    static constexpr float TARGET_FRAME_TIME = 1.0f / 60.0f; // 60 FPS
};

#endif // DISPLAY_H
