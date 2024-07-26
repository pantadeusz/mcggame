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
#include <map>

namespace mcggame {

std::vector<unsigned char> get_pixel(SDL_Surface *surface, int x, int y) {
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

std::shared_ptr<SDL_Texture> load_texture(SDL_Renderer *_renderer, const std::string fname, std::function<void(SDL_Surface *)> callback = [](SDL_Surface *){}) {
        SDL_Surface *surface;
        surface = SDL_LoadBMP(fname.c_str());
        if (!surface) {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_SetColorKey(surface, SDL_TRUE, 0x0ffff);
        auto _track_tex = SDL_CreateTextureFromSurface(_renderer, surface);
        callback(surface);


        if (!_track_tex) {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_FreeSurface(surface);
        return std::shared_ptr<SDL_Texture>(_track_tex, [](auto p){SDL_DestroyTexture(p);});
}

class race_track_t {
    SDL_Rect _track_dims;
    SDL_Texture *_track_tex;
    std::shared_ptr<SDL_Texture> _track_tex_p;
    std::vector<unsigned char> _track;
    SDL_Renderer *_renderer;
public:



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
                    // std::cout << "r__> " << x << " " << y << std::endl;
                    attr = 0;
                }
                pixels.push_back(attr);
            }
        }

        return pixels;
    }

    static position_t to_screen_coordinates(const position_t p, const position_t cam, double scale = 1.0) {
        auto p2 = (p - cam)*scale;
        return p2 + position_t{game_view_width*0.5, game_view_height*0.5};
    }

    void draw(double cam_x, double cam_y, double scale = 1.0) const {
            SDL_Rect source_rect = {0,0,
            width(),
                height()};
            
            auto draw_dst_pos = to_screen_coordinates({0.0, 0.0}, {cam_x, cam_y}, scale);
            SDL_Rect destination_rect = {draw_dst_pos[0],
                                        draw_dst_pos[1],
                                        (int)(width()*scale),(int)(height()*scale)};

            SDL_RenderCopyEx(_renderer, _track_tex, &source_rect, &destination_rect,
                                        0, nullptr, SDL_FLIP_NONE);

            auto pp = to_screen_coordinates({cam_x, cam_y}, {cam_x, cam_y}, scale);
            SDL_RenderDrawLine(_renderer, pp[0], pp[1],pp[0]+10,pp[1]+10);

            pp = to_screen_coordinates({0.0, 0.0}, {cam_x, cam_y}, scale);
            SDL_RenderDrawLine(_renderer, pp[0], pp[1],pp[0]+10,pp[1]+10);

            pp = to_screen_coordinates({100.0, 0.0}, {cam_x, cam_y}, scale);
            SDL_RenderDrawLine(_renderer, pp[0], pp[1],pp[0]+10,pp[1]+10);

            pp = to_screen_coordinates({0.0, 100.0}, {cam_x, cam_y}, scale);
            SDL_RenderDrawLine(_renderer, pp[0], pp[1],pp[0]+10,pp[1]+10);

            pp = to_screen_coordinates({100.0, 100.0}, {cam_x, cam_y}, scale);
            SDL_RenderDrawLine(_renderer, pp[0], pp[1],pp[0]+10,pp[1]+10);
    }

    int width() const {return _track_dims.w; }
    int height() const {return _track_dims.h; }    

    race_track_t(const std::string fname, SDL_Renderer *renderer) {
        _renderer = renderer;


        _track_tex_p = load_texture(_renderer,fname, [&](SDL_Surface *surface){
            _track = extract_track_details(surface);
            _track_dims = {0,0,surface->w, surface->h};
        });

        _track_tex = _track_tex_p.get();
    }

    virtual ~race_track_t() {
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

class input_i {
public:
    virtual position_t get_state() const = 0;
};


class car_t {
    SDL_Renderer * _renderer;
    public:
        position_t p;
        position_t v;
        position_t a;
        double alpha;
        double angle;

        std::shared_ptr<SDL_Texture> texture;

        std::shared_ptr<input_i> input;

    car_t(  SDL_Renderer * renderer, std::shared_ptr<input_i> input_, const position_t p_ = {0.0,0.0}, const position_t v_ = {0.0,0.0}, const position_t a_ = {0.0,0.0},
        const std::string car_texture_name = "assets/car_01.bmp") {
            input = input_;
            _renderer = renderer;
        texture = load_texture(_renderer,car_texture_name);
        p = p_;
        v = v_;
        a = a_;
    }

    void update(double dt) {
        auto accel = calculate_friction_acceleration(v, 0.5);
        std::cout << accel << std::endl;
        std::array<position_t,3> r = update_phys_point(p,v, input->get_state() + accel, dt);
        p = r[0];
        v = r[1];
        a = r[2];
        if (~v > 0.0) {
            auto normv = v*(1.0/~v);
            angle = std::atan2(normv[1], normv[0]);
        }
    }
    void draw(position_t cam, double scale = 1.0) const {


        auto p1 = p - position_t{32.0, 32.0};
        auto p2 = p + position_t{32.0, 32.0};
        p1 = race_track_t::to_screen_coordinates(p1,cam,scale);
        p2 = race_track_t::to_screen_coordinates(p2,cam,scale);
        auto dp = p2-p1;
            SDL_Rect destination_rect = {p1[0],
                                        p1[1],
                                        dp[0], dp[1]};

            SDL_RenderCopyEx(_renderer, texture.get(), nullptr, &destination_rect,
                                        (angle/M_PI)*180.0, nullptr, SDL_FLIP_NONE);
    }
};




class input_keyboard_c : public input_i {
public:
    position_t get_state() const {
        auto keyboard_state = SDL_GetKeyboardState(nullptr);
        position_t a = {0.0,0.0};
        if (keyboard_state[SDL_SCANCODE_RIGHT]) a[0] += 100;
        if (keyboard_state[SDL_SCANCODE_LEFT]) a[0] -= 100;
        if (keyboard_state[SDL_SCANCODE_UP]) a[1] -= 100;
        if (keyboard_state[SDL_SCANCODE_DOWN]) a[1] += 100;
        return a;
    };
};

int mcg_main(int argc, char *argv[])
{

    game_context([](SDL_Renderer *renderer){
        SDL_Event event;
        auto race_track = std::make_shared<race_track_t>("assets/map_01.bmp", renderer);

        car_t car(renderer, std::make_shared<input_keyboard_c>(), {100.0,100.0}, {0.0, 0.0}, {0.0,0.0}, "assets/car_01.bmp");
        std::map<Sint32,car_t> cars;
        
        position_t camera_position = {};
        double scale = 1.0;
        bool game_continues = true;
        while (game_continues) {
            while(SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    game_continues = false;
                } else if (event.type == SDL_JOYDEVICEADDED) {
                    std::cout << "Joistick added" << event.jdevice.type << " : " << event.jdevice.which <<  std::endl;
                } else if (event.type == SDL_JOYDEVICEREMOVED) {
                    std::cout << "Joistick removed" << event.jdevice.type << " : " << event.jdevice.which <<  std::endl;
                } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
                    std::cout << "Controller added" << event.cdevice.type << " : " << event.cdevice.which <<  std::endl;
                } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                    std::cout << "Controller removed" << event.cdevice.type << " : " << event.cdevice.which <<  std::endl;
                }
            }
            auto keyboard_state = SDL_GetKeyboardState(nullptr);
            if (keyboard_state[SDL_SCANCODE_INSERT]) scale *= 1.1;
            if (keyboard_state[SDL_SCANCODE_DELETE]) scale *= 0.9;
            car.update(0.01);

            SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
            SDL_RenderClear(renderer);

            race_track->draw(car.p[0], car.p[1],scale);//, 1.5);//+0.5*std::sin(car.p[0]/100.0));
            car.draw(car.p,scale);
            
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
