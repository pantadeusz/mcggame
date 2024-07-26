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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) < 0) {
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

std::ostream &operator<<(std::ostream &o, const position_t &a) {
    o << "[ " << a[0] << " " << a[1] << " ]";
    return o;
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

position_t rotate_around(const position_t &p, const double &angle, const position_t &d) {
    position_t ret;
    // ret[0] = ((p[0] - d[0]) * std::cos(angle)) - ((d[1] - p[1]) * std::sin(angle)) + d[0];
    // ret[1] = d[1] - ((d[1] - p[1]) * std::cos(angle)) + ((p[0] - d[0]) * std::sin(angle));


    ret[0] = ((p[0] - d[0]) * cos(angle)) - ((p[1] - d[1]) * sin(angle)) + d[0];
    ret[1] = ((p[0] - d[0]) * sin(angle)) + ((p[1] - d[1]) * cos(angle)) + d[1];

    return ret;
}

double angle_crop_to_range(double a) {
    if (std::abs(a) > std::abs(a + 2.0*M_PI)) a = a + 2.0*M_PI;
    if (std::abs(a) > std::abs(a - 2.0*M_PI)) a = a - 2.0*M_PI;
    return a;
}

// Function to calculate the rotation angle between two ordered arrays of points in 2D Euclidean space
double angle_between_vectors(const position_t& v1,
                              const position_t& v2) {
    auto n_v1 = v1*(1.0/~v1); ///< normalized v1
    auto n_v2 = v2*(1.0/~v2); ///< normalized v2
    auto angle1 = std::atan2(n_v1[1], n_v1[0]);
    auto angle2 = std::atan2(n_v2[1], n_v2[0]);
    auto a = angle2 - angle1;
    a = angle_crop_to_range(a);
    return a;
}

double angle_between_shapes(const std::vector<position_t>& shape1,
                              const std::vector<position_t>& shape2) {
    double angle_sum = 0.0;
    double angle_count = 0.0;
    if (shape1.size() != shape2.size()) throw std::invalid_argument("Shapes must be of the same size");
    for (int i = 0; i < shape1.size(); i++) {
        auto v1 = shape1[(i+1) % shape1.size()] - shape1[i];
        auto v2 = shape2[(i+1) % shape2.size()] - shape2[i];
        auto a = angle_between_vectors(v1,v2);
        if (std::abs(a) > std::abs(a + 2.0*M_PI)) a = a + 2.0*M_PI;
        if (std::abs(a) > std::abs(a - 2.0*M_PI)) a = a - 2.0*M_PI;
        angle_sum += a;
    }
    return angle_sum/(double)shape1.size();
}



std::array<position_t,3> update_phys_point(position_t p, position_t v, position_t a, const double dt) {
    std::array<position_t,3> updated = {p,v,a};
    updated[0] = p + v * dt + a*dt*dt/2.0;
    updated[1] = v + a*dt;
    updated[2] = a;
    return updated;
}
position_t calculate_friction_acceleration(position_t v, const double coefficient) {
    //position_t a = {0.0,0.0};
    double l = ~v;
    if (l < 0.0001) return {0.0,0.0};
    //a = v*1/l;
    return v*(-coefficient);
}


}
