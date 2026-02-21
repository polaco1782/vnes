#include "display.h"
#include <cstring>
#include <type_traits>

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
    // SFML 3.x: VideoMode takes a Vector2u and bitsPerPixel
    sf::VideoMode mode(sf::Vector2u(static_cast<unsigned int>(window_width), static_cast<unsigned int>(window_height)), 32);
    window = std::make_unique<sf::RenderWindow>(mode, title);
    // Texture: use Vector2u constructor
    texture = std::make_unique<sf::Texture>(sf::Vector2u{NES_WIDTH, NES_HEIGHT});
    sprite = std::make_unique<sf::Sprite>(*texture);
    sprite->setScale(sf::Vector2f{static_cast<float>(scale_factor), static_cast<float>(scale_factor)});
    
    // Allocate pixel buffer (RGBA format)
    pixels = new u8[NES_WIDTH * NES_HEIGHT * 4];
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
        pixels[i * 4 + 0] = static_cast<u8>((color >> 16) & 0xFF);  // R
        pixels[i * 4 + 1] = static_cast<u8>((color >> 8) & 0xFF);   // G
        pixels[i * 4 + 2] = static_cast<u8>(color & 0xFF);          // B
        pixels[i * 4 + 3] = static_cast<u8>((color >> 24) & 0xFF);  // A
    }
    
    // Create image from pixels and update texture
    sf::Image img(sf::Vector2u{static_cast<unsigned int>(NES_WIDTH), static_cast<unsigned int>(NES_HEIGHT)}, pixels);
    [[maybe_unused]] bool loaded = texture->loadFromImage(img);
    
    // Render
    window->clear();
    window->draw(*sprite);
    window->display();
    
    // Frame timing - wait until target frame time has elapsed
    float elapsed = clock.getElapsedTime().asSeconds();
    if (elapsed < TARGET_FRAME_TIME) {
        sf::sleep(sf::seconds(TARGET_FRAME_TIME - elapsed));
    }
    clock.restart();
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
        if (event->is<sf::Event::Closed>()) {
            window->close();
        } else if (event->is<sf::Event::KeyPressed>()) {
            if (event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape) {
                escape_pressed = true;
            }
        }
    }
}
