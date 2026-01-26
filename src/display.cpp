#include "display.h"
#include <cstring>

Display::Display(const char* title, int scale)
    : scale_factor(scale), escape_pressed(false)
{
    window_width = NES_WIDTH * scale_factor;
    window_height = NES_HEIGHT * scale_factor;
    
    // Ensure minimum 800x600
    if (window_width < 800) {
        scale_factor = (800 + NES_WIDTH - 1) / NES_WIDTH;
        window_width = NES_WIDTH * scale_factor;
        window_height = NES_HEIGHT * scale_factor;
    }
    
    // Create window
    window.create(sf::VideoMode(window_width, window_height), title);
    
    // Create texture for NES framebuffer
    texture.create(NES_WIDTH, NES_HEIGHT);
    sprite.setTexture(texture);
    sprite.setScale(scale_factor, scale_factor);
    
    // Allocate pixel buffer (RGBA format)
    pixels = new sf::Uint8[NES_WIDTH * NES_HEIGHT * 4];
}

Display::~Display()
{
    delete[] pixels;
}

void Display::update(const u32* framebuffer)
{
    // Convert u32 ARGB framebuffer to RGBA pixel array
    for (int i = 0; i < NES_WIDTH * NES_HEIGHT; i++) {
        u32 color = framebuffer[i];
        pixels[i * 4 + 0] = (color >> 16) & 0xFF;  // R
        pixels[i * 4 + 1] = (color >> 8) & 0xFF;   // G
        pixels[i * 4 + 2] = color & 0xFF;          // B
        pixels[i * 4 + 3] = (color >> 24) & 0xFF;  // A
    }
    
    // Update texture
    texture.update(pixels);
    
    // Render
    window.clear();
    window.draw(sprite);
    window.display();
    
    // Frame timing - wait until target frame time has elapsed
    float elapsed = clock.getElapsedTime().asSeconds();
    if (elapsed < TARGET_FRAME_TIME) {
        sf::sleep(sf::seconds(TARGET_FRAME_TIME - elapsed));
    }
    clock.restart();
}

bool Display::isOpen() const
{
    return window.isOpen();
}

bool Display::wasEscapePressed()
{
    bool result = escape_pressed;
    escape_pressed = false;
    return result;
}

void Display::pollEvents()
{
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window.close();
        }
        else if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Escape) {
                escape_pressed = true;
            }
        }
    }
}
