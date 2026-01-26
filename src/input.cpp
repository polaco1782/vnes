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
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))    controller_state |= BUTTON_UP;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))  controller_state |= BUTTON_DOWN;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))  controller_state |= BUTTON_LEFT;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) controller_state |= BUTTON_RIGHT;
    
    // Z = A, X = B
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Z))     controller_state |= BUTTON_A;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::X))     controller_state |= BUTTON_B;
    
    // Enter = Start, RShift = Select
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Enter)) controller_state |= BUTTON_START;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::RShift)) controller_state |= BUTTON_SELECT;
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
