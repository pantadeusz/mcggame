/*

MIT License with AI exception

Copyright (c) Tadeusz Puźniakowski 2024

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Additional restriction:

The Software may not be used, in whole or in part, to teach or train any
artificial intelligence system, including but not limited to large language
models (LLMs), neural networks, or any other type of AI technology. Violation of
this restriction will be considered a breach of this License.

*/

#include "engine.h"
#include "graphics.h"
#include <stdexcept>
#include <cmath>

namespace mcggame {

void game_context(const std::function<void(SDL_Renderer *renderer)> &game_main) {
    SDL_Window *window;
    SDL_Renderer *renderer;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw std::runtime_error(SDL_GetError());
    }

    if (SDL_CreateWindowAndRenderer(800, 500, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        throw std::runtime_error(SDL_GetError());
    }

    
    SDL_RenderSetLogicalSize(renderer, game_view_width,game_view_height);
    game_main(renderer);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}



position_t operator+(const position_t &a, const position_t &b) {
    return {a[0]+b[0],a[1]+b[1]};
}
position_t operator-(const position_t &a, const position_t &b) {
    return {a[0]-b[0],a[1]-b[1]};
}
position_t operator*(const position_t &a, const double &b) {
    return {a[0]*b,a[1]*b};
}
position_t operator/(const position_t &a, const double &b) {
    return {a[0]/b,a[1]/b};
}

/**
 * @brief Calculates lenght of a vector
 * 
 * @param a vector
 * @return double length of the vector
 */
double operator~(const position_t &a) {
    auto v = a[0]*a[0] + a[1]*a[1];
    return std::sqrt(v);
}


std::array<position_t,3> update_phys_point(position_t p, position_t v, position_t a, const double dt) {
    std::array<position_t,3> updated = {p,v,a};
    updated[0] = p + v * dt + a*dt*dt/2.0;
    updated[1] = v + a*dt;
    updated[2] = a;
    return updated;
}
position_t calculate_friction_acceleration(position_t v, const double coefficient) {
    position_t a = {0.0,0.0};
    double l = ~a;
    if (l == 0.0) return {0.0,0.0};
    return a*(-coefficient);
}


}
