#define _XOPEN_SOURCE_EXTENDED 1
#include "game_state.h"
#include <cstdlib>
#include <iostream>

int main() {
    try {
        Game game;
        game.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
