#include "input.h"

Input::Input()
    : controller_state(0)
    , controller_latch(0)
    , shift_count(0)
{
}

void Input::updateFromKeyboard()
{
    controller_state = 0;
    
    // Map keyboard to NES controller
    // Arrow keys for D-pad
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))    controller_state |= BUTTON_UP;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))  controller_state |= BUTTON_DOWN;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))  controller_state |= BUTTON_LEFT;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) controller_state |= BUTTON_RIGHT;
    
    // Z/J = A, X/K = B (multiple bindings to avoid ghosting)
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Z) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)) controller_state |= BUTTON_A;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::X) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)) controller_state |= BUTTON_B;
    
    // Enter = Start, RShift = Select
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter)) controller_state |= BUTTON_START;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift)) controller_state |= BUTTON_SELECT;
}

void Input::strobe()
{
    // Latch current controller state
    controller_latch = controller_state;
    shift_count = 0;
}

u8 Input::read()
{
    // Return current bit (LSB first)
    u8 value = (controller_latch & 0x01) ? 0x01 : 0x00;
    
    // Shift to next bit
    controller_latch >>= 1;
    shift_count++;
    
    // After 8 reads, return 1 (open bus behavior)
    if (shift_count > 8) {
        value = 0x01;
    }
    
    return value | 0x40;  // Open bus bits
}
