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



#ifndef MCGGAME_ENGINE_H
#define MCGGAME_ENGINE_H

#include <SDL.h>

#include <functional>
#include <array>
#include <ostream>



namespace mcggame {

const int game_view_width = 640;
const int game_view_height = 480;

void game_context(const std::function<void(SDL_Renderer *renderer)> &game_main);





using position_t = std::array<double, 2>;

position_t operator+(const position_t &a, const position_t &b);
position_t operator-(const position_t &a, const position_t &b);
position_t operator*(const position_t &a, const double &b);
position_t operator/(const position_t &a, const double &b);

std::ostream &operator<<(std::ostream &o, const position_t &a);
double operator~(const position_t &a);

position_t rotate_around(const position_t &p, const double &angle, const position_t &d = {0,0});
double angle_between_vectors(const position_t& v1,
                              const position_t& v2);
double angle_between_shapes(const std::vector<position_t>& shape1,
                              const std::vector<position_t>& shape2);
double angle_crop_to_range(double a);

std::array<position_t,3> update_phys_point(position_t p, position_t v, position_t a, const double dt);
position_t calculate_friction_acceleration(position_t v, const double coefficient) ;


class game_context_c {
    SDL_Window *window;
public:
    SDL_Renderer *renderer;

    game_context_c();
    virtual ~game_context_c();
};

}

#endif