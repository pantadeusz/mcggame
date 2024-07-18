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


#define SDL_MAIN_HANDLED

#include "engine.h"
#include <stdexcept>
#include <memory>
#include <vector>
#include <iostream>
#include <array>

namespace mcggame {

class race_track_t {
    SDL_Rect _track_dims;
    SDL_Texture *_track_tex;
    std::vector<unsigned char> _track;
    SDL_Renderer *_renderer;
public:

    static std::vector<unsigned char> get_pixel(SDL_Surface *surface, int x, int y) {
        std::vector<unsigned char> ret;   
        SDL_PixelFormat *format = surface->format;
        if (x >= surface->w) throw std::invalid_argument("x should be lower than surface width");
        if (y >= surface->h) throw std::invalid_argument("y should be lower than surface width");
        if (x < 0) throw std::invalid_argument("x should be non negative");
        if (y < 0) throw std::invalid_argument("y should be non negative");
        int bpp = format->BytesPerPixel;
        int pitch = surface->pitch;
        unsigned char* pixel_data = (unsigned char*)surface->pixels;
        unsigned char* row = pixel_data + (pitch*y);
        for (int i = 0; i < format->BytesPerPixel; i++) ret.push_back(*(row+(x*bpp+i)));
        return ret;
    }

    static std::vector<unsigned char> extract_track_details(SDL_Surface *surface) {
        std::vector<unsigned char> pixels;
        for (int y = 0; y < surface->h; ++y) {
            for (int x = 0; x < surface->w; ++x) {
                unsigned char attr = 255;
                auto p = get_pixel(surface, x, y);
                for (int i = 0; i < 4-p.size(); i++) p.push_back(0);
                u_int64_t *p_p = (u_int64_t *)p.data();
                *p_p = *p_p & 0x0ffffff;
                if (*p_p == 0x000ffff) {
                    std::cout << "r__> " << x << " " << y << std::endl;
                    attr = 0;
                }
                pixels.push_back(attr);
            }
        }

        return pixels;
    }

    void draw(int cam_x, int cam_y) const {
            SDL_Rect source_rect = {cam_x-game_view_width/2,cam_y-game_view_height/2,
            game_view_width,
                game_view_height};
            SDL_Rect destination_rect = {0,0,game_view_width,game_view_height};
            if ((source_rect.x > -game_view_width) && (source_rect.y > -game_view_height)) {
                if (source_rect.x < 0) {
                    destination_rect.x = -source_rect.x;
                    source_rect.w += source_rect.x;
                    destination_rect.w += source_rect.x;
                    source_rect.x = 0;
                }
                if (source_rect.y < 0) {
                    destination_rect.y = -source_rect.y;
                    source_rect.h += source_rect.y;
                    destination_rect.h += source_rect.y;
                    source_rect.y = 0;
                }

                if ((source_rect.x+game_view_width) > _track_dims.w) {
                    source_rect.w -= ((source_rect.x+game_view_width) - _track_dims.w);
                    destination_rect.w -= ((source_rect.x+game_view_width) - _track_dims.w);
                }
                if ((source_rect.y+game_view_height) > _track_dims.h) {
                    source_rect.h -= ((source_rect.y+game_view_height) - _track_dims.h);
                    destination_rect.h -= ((source_rect.y+game_view_height) - _track_dims.h);
                }


                SDL_RenderCopyEx(_renderer, _track_tex, &source_rect, &destination_rect,
                                            0, nullptr, SDL_FLIP_NONE);
            }
    }

    int width() const {return _track_dims.w; }
    int height() const {return _track_dims.h; }    

    race_track_t(const std::string fname, SDL_Renderer *renderer) {
        _renderer = renderer;
        SDL_Surface *surface;
        surface = SDL_LoadBMP(fname.c_str());
        if (!surface) {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_SetColorKey(surface, SDL_TRUE, 0x0ffff);
        _track_tex = SDL_CreateTextureFromSurface(_renderer, surface);
        _track = extract_track_details(surface);
        _track_dims = {0,0,surface->w, surface->h};

        if (!_track_tex) {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_FreeSurface(surface);
    }

    virtual ~race_track_t() {
        SDL_DestroyTexture(_track_tex);
    }
};
using p_race_track = std::shared_ptr<race_track_t>;


class race_t {
    p_race_track _race_track;
public:
    race_t(p_race_track rt) {
        _race_track = rt;
    }

    void draw() {
        _race_track->draw(100,100);
    }
};


class car_t {
    public:
        position_t p;
        position_t v;
        position_t a;
        double alpha;
    car_t(  position_t p_ = {0.0,0.0}, position_t v_ = {0.0,0.0}, position_t a_ = {0.0,0.0}) {
        p = p_;
        v = v_;
        a = a_;
    }

    void update(double dt) {
        std::array<position_t,3> r = update_phys_point(p,v,a, dt);
        p = r[0];
        v = r[1];
        a = r[2];
    }
};


int mcg_main(int argc, char *argv[])
{

    game_context([](SDL_Renderer *renderer){
        SDL_Event event;
        auto race_track = std::make_shared<race_track_t>("assets/map_01.bmp", renderer);

        car_t car({100.0,100.0}, {100.0, 160.0});
        bool game_continues = true;
        while (game_continues) {
            while(SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    game_continues = false;
                }
            }

            car.update(0.01);

            SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
            SDL_RenderClear(renderer);

            race_track->draw(car.p[0], car.p[1]);
            
            SDL_RenderPresent(renderer);
            SDL_Delay(10);
            if (car.p[0] >= race_track->width()) {
                car.v[0] = -100.0;
            }
            if (car.p[1] >= race_track->height()) {
                car.v[1] = -160.0;
            }
            if (car.p[0] <= 0) {
                car.v[0] = 100.0;
            }
            if (car.p[1] <= 0) {
                car.v[1] = 160.0;
            }
        }


    });



    return 0;
}


    
}



int main(int argc, char *argv[])
{
    mcggame::mcg_main(argc, argv);
}
