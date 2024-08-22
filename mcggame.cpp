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
#include <iomanip>
#include <chrono>
#include <thread>
#include <tuple>


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

struct logic_bitmap_t {
    int w;
    int h;
    std::vector<unsigned char> bitmap;
    unsigned char &operator()(const int x, const int y){
        static unsigned char placeholder = 0;
        if ( (x >= 0) && (x < (w)) &&
                 (y >= 0) && (y < (h)) ) return bitmap[y*w+x];
        else
            return placeholder;
    }
    unsigned char operator()(const int x, const int y) const {
        if ( (x >= 0) && (x < (w)) &&
                 (y >= 0) && (y < (h)) ) return bitmap[y*w+x];
        else
            return 0;
    }

    static logic_bitmap_t from_surface(SDL_Surface *surface, std::function<unsigned char(int x, int y, u_int64_t v)> callback = [](int x, int y, u_int64_t v){return (unsigned char)(v&0x0ff);}) {
        logic_bitmap_t ret;
        ret.w = surface->w;
        ret.h = surface->h;
        ret.bitmap.reserve(ret.w*ret.h);
        for (int y = 0; y < surface->h; ++y) {
            for (int x = 0; x < surface->w; ++x) {
                unsigned char attr = 255;
                auto p = get_pixel(surface, x, y);
                for (int i = 0; i < 4-p.size(); i++) p.push_back(0);
                u_int64_t *p_p = (u_int64_t *)p.data();
                attr = callback(x,y,*p_p);
                ret.bitmap.push_back(attr);
            }
        }

        return ret;
    }
};

class race_track_t {
    SDL_Texture *_track_tex;
    std::shared_ptr<SDL_Texture> _track_tex_p;
    SDL_Renderer *_renderer;
    
public:

    logic_bitmap_t _collision_map;

    static position_t to_screen_coordinates(const position_t p, const position_t cam, double scale = 1.0) {
        auto p2 = (p - cam)*scale;
        return p2 + position_t{game_view_width*0.5, game_view_height*0.5};
    }

    void draw(double cam_x, double cam_y, double scale = 1.0) const {
            SDL_Rect source_rect = {0,0,
            width(),
            height()};
            
            auto draw_dst_pos = to_screen_coordinates({0.0, 0.0}, {cam_x, cam_y}, scale);
            SDL_Rect destination_rect = {(int)draw_dst_pos[0],
                                        (int)draw_dst_pos[1],
                                        (int)(width()*scale),(int)(height()*scale)};

            SDL_RenderCopyEx(_renderer, _track_tex, &source_rect, &destination_rect,
                                        0, nullptr, SDL_FLIP_NONE);
    }

    int width() const {return _collision_map.w; }
    int height() const {return _collision_map.h; }    

    race_track_t(const std::string fname, SDL_Renderer *renderer) {
        _renderer = renderer;


        _track_tex_p = load_texture(_renderer,fname, [&](SDL_Surface *surface){
            _collision_map = logic_bitmap_t::from_surface(surface, [](int x, int y, u_int64_t v){
                v = v & 0x0ffffff;
                if (v == 0x000ffff) {
                    return 0; // no collision
                }
                return 255; // collision
            });
        });

        _track_tex = _track_tex_p.get();
    }

    virtual ~race_track_t() {
    }
};
using p_race_track = std::shared_ptr<race_track_t>;


struct input_state_t {
    position_t p;
};

class input_i {
public:
    virtual input_state_t get_state() const = 0;
};

double radius_to_correct_point(const position_t &p, const std::shared_ptr<race_track_t> race_track) {
    if (race_track->_collision_map(p[0],p[1]) != 255) return 0;
    for (double r = 1.0; r < 16; r+= 1.0) {
        for (double a = 0.0; a < 2.0; a += 0.25) {
            auto np = p;
            np[0] += r*std::sin(a*M_PI);
            np[1] += r*std::cos(a*M_PI);
            if (race_track->_collision_map(np[0],np[1]) != 255) return r;
        }
    }
    return 1000.0;
}

std::vector<position_t> check_collision(const std::vector<position_t> &collision_pts, position_t p, double angle,const logic_bitmap_t &collision_map) {
    std::vector<position_t> in_collision;
    for (auto hp: collision_pts) {
        auto orig_hp = hp;
        hp = rotate_around(hp,angle) + p;
        if (collision_map(hp[0],hp[1]) == 255)
            //in_collision.push_back(orig_hp);
            in_collision.push_back(hp);
    }
    return in_collision;
}

class car_t {
    SDL_Renderer * _renderer;
    public:
        std::shared_ptr<std::vector<position_t>> wheels;
        position_t p;
        position_t v;
        position_t a;
        double angle;

        std::shared_ptr<SDL_Texture> texture;

        std::shared_ptr<input_i> input;

        std::shared_ptr<std::vector<position_t>> collision_pts;

    static car_t create( SDL_Renderer * renderer, 
            std::shared_ptr<input_i> input_,
            const position_t p_ = {0.0,0.0},
            const position_t v_ = {0.0,0.0},
            const position_t a_ = {0.0,0.0},
            const std::string car_texture_name = "assets/car_01.bmp") 
    {
        car_t ret;
        
        ret.input = input_;
        ret._renderer = renderer;
        ret.texture = load_texture(renderer, car_texture_name);
        ret.p = p_;
        ret.v = v_;
        ret.a = a_;
        ret.wheels = std::make_shared<std::vector<position_t>>();
        ret.wheels->push_back({30.0,0.0});
        ret.wheels->push_back({-30.0,0.0});

        std::vector<position_t> cp;
        for (double x = -32; x <= 32; x+= 8.0)
        for (double y = -16; y <= 16; y+= 8.0) {
            cp.push_back({x,y});
        }
        ret.collision_pts = std::make_shared<std::vector<position_t>>(cp);

        return ret;
    }

    
    
    car_t update(double dt) const {
        car_t ret = *this;

        auto friction = calculate_friction_acceleration(v, 0.5);
        auto input_v = input->get_state();
        
        
        auto forward_vector = rotate_around({1.0,0.0}, ret.angle);
        auto backward_vector = rotate_around({-1.0,0.0}, ret.angle);
        auto forward_acceleration = forward_vector * input_v.p[1]*160.0;

        
        

        if (~v > 0.0001) {
            auto angle_to_correct_a = angle_between_vectors(forward_vector, v);
            auto angle_to_correct_b = angle_between_vectors(backward_vector, v);
            bool is_moving_forward = (std::abs(angle_to_correct_a) < std::abs(angle_to_correct_b));
            auto angle_to_correct = is_moving_forward?angle_to_correct_a:angle_to_correct_b;
            auto movement_correction_angle = angle_to_correct * ((~v > 1.0)?0.02:0.9);
            if ((~v > 100.0) && (std::abs(angle_to_correct ) > 0.001)) {
                // std::cout << "drifting " << ~v << std::endl;
                friction = calculate_friction_acceleration(v, 0.9);
            }
            ret.v = rotate_around(ret.v,-movement_correction_angle);

            if (is_moving_forward) ret.angle = angle_crop_to_range(angle + input_v.p[0]*0.0001*~v);
            else ret.angle = angle_crop_to_range(angle + input_v.p[0]*(-0.0001)*~v);
        }
        
      
        std::array<position_t,3> r = update_phys_point(p, ret.v, forward_acceleration + friction, dt);
        ret.p = r[0];
        ret.v = r[1];
        ret.a = r[2];
        if (~ret.v < 0.005) {
            ret.v = {0.0,0.0};
        }
        return ret;
    }

    void draw(position_t cam, double scale = 1.0) const {


        auto p1 = p - position_t{32.0, 32.0};
        auto p2 = p + position_t{32.0, 32.0};
        p1 = race_track_t::to_screen_coordinates(p1,cam,scale);
        p2 = race_track_t::to_screen_coordinates(p2,cam,scale);
        auto dp = p2-p1;
            SDL_Rect destination_rect = {(int)p1[0],
                                         (int)p1[1],
                                         (int)dp[0],
                                         (int)dp[1]};

            SDL_RenderCopyEx(_renderer, texture.get(), nullptr, &destination_rect,
                                        (angle/M_PI)*180.0, nullptr, SDL_FLIP_NONE);
    }
};

car_t place_car_on_race_track(const race_track_t &race_track, const car_t &car) {
    car_t ct = car;

    for (double x = 0; x < race_track.width(); x+= 2.0) {
    for (double y = 0; y < race_track.height(); y+= 2.0) {
        ct.p = {x,y};
        std::vector<position_t> collisions = check_collision(*ct.collision_pts.get(), ct.p, ct.angle,race_track._collision_map);
        if (collisions.size() == 0) return ct;
    }
    }
    throw std::invalid_argument("could not place car on map due to not enough free space on the map");
}


class input_keyboard_c : public input_i {
public:
    input_state_t get_state() const {
        auto keyboard_state = SDL_GetKeyboardState(nullptr);
        position_t a = {0.0,0.0};
        if (keyboard_state[SDL_SCANCODE_RIGHT]) a[0] += 1;
        if (keyboard_state[SDL_SCANCODE_LEFT]) a[0] -= 1;
        if (keyboard_state[SDL_SCANCODE_UP]) a[1] += 1;
        if (keyboard_state[SDL_SCANCODE_DOWN]) a[1] -= 1;
        return {a};
    };
};

class input_joystick_c : public input_i {
public:
    input_state_t get_state() const {
        auto keyboard_state = SDL_GetKeyboardState(nullptr);
        position_t a = {0.0,0.0};
        if (keyboard_state[SDL_SCANCODE_D]) a[0] += 1;
        if (keyboard_state[SDL_SCANCODE_A]) a[0] -= 1;
        if (keyboard_state[SDL_SCANCODE_W]) a[1] += 1;
        if (keyboard_state[SDL_SCANCODE_S]) a[1] -= 1;
        return {a};
    };
};

namespace heuristic {

std::pair<double,std::vector<position_t>> goal_collision(const car_t &new_car, const car_t &current_car, const std::shared_ptr<race_track_t> race_track) {
    auto collision_points = check_collision(*(new_car.collision_pts).get(), new_car.p, new_car.angle,race_track->_collision_map);
    double diff_angle = std::abs(angle_between_vectors(rotate_around({1.0,0.0}, new_car.angle), rotate_around({1.0,0.0}, current_car.angle)));
    double diff_position = ~(new_car.p - current_car.p);
    double sum_col = 0.0;
    for (auto &p: collision_points) {
        sum_col += (radius_to_correct_point(p,race_track))*2.0;
    }
    if (sum_col > 0.0) sum_col += 100.0;
    return {diff_angle*4.0 + std::sqrt(diff_position+3.0) + sum_col,collision_points};

}

std::vector<car_t> generate_neighbors(car_t c) {
    std::vector<car_t> ret;
    car_t tmp = c;
    tmp.angle += 0.04;
    ret.push_back(tmp);
    tmp = c;
    tmp.angle -= 0.04;
    ret.push_back(tmp);
    tmp = c;
    tmp.p[0] -= 1.0;
    ret.push_back(tmp);
    tmp = c;
    tmp.p[0] += 1.0;
    ret.push_back(tmp);
    tmp = c;
    tmp.p[1] -= 1.0;
    ret.push_back(tmp);
    tmp = c;
    tmp.p[1] += 1.0;
    ret.push_back(tmp);

    tmp = c;
    tmp.p[0] -= 0.6;
    tmp.p[1] -= 0.6;
    ret.push_back(tmp);

    tmp = c;
    tmp.p[0] += 0.6;
    tmp.p[1] -= 0.6;
    ret.push_back(tmp);

    tmp = c;
    tmp.p[0] += 0.6;
    tmp.p[1] += 0.6;
    ret.push_back(tmp);

    tmp = c;
    tmp.p[0] += 0.6;
    tmp.p[1] -= 0.6;
    ret.push_back(tmp);

    return ret;
}

std::pair<car_t,std::vector<position_t>> find_best_corrected_position(car_t car_to_fix, const std::shared_ptr<race_track_t> race_track) {
    auto best_car = car_to_fix;
    auto [best_goal, collision_points] = goal_collision(best_car, car_to_fix, race_track);

    for (int i = 0; i < 200; i++) {
            auto neighbors = generate_neighbors(best_car);
            bool no_better = true;
            for (auto &c_car : neighbors)             {
                auto [c_goal, c_collision_points] = goal_collision(c_car, car_to_fix, race_track);
                if (c_goal < best_goal) {
                    best_goal = c_goal;
                    best_car = c_car;
                    collision_points = c_collision_points;
                    no_better = false;
                }
            }
            if (no_better) {
                std::cout << "no better " << i << std::endl;
                break;
            }
    }
    return {best_car, collision_points};
}
}



int mcg_main(int argc, char *argv[])
{
    using namespace std;
    using namespace std::chrono;

    const double dt = 0.01;
    game_context_c game;
    SDL_Renderer *renderer = game.renderer;

    SDL_Event event;
    auto race_track = std::make_shared<race_track_t>("assets/map_01.bmp", renderer);

    std::vector<car_t> cars;
    
    position_t camera_position = {};
    //double scale = 1.0;
    bool game_continues = true;

    cars.push_back(place_car_on_race_track(*race_track.get(), car_t::create(renderer, std::make_shared<input_keyboard_c>(), {100.0,100.0}, {0.0, 0.0}, {0.0,0.0}, "assets/car_01.bmp")));
    cars.push_back(place_car_on_race_track(*race_track.get(), car_t::create(renderer, std::make_shared<input_joystick_c>(), {100.0,100.0}, {0.0, 0.0}, {0.0,0.0}, "assets/car_01.bmp")));

    high_resolution_clock::time_point current_time = high_resolution_clock::now();
    std::vector<position_t> collisions_draw;
    std::cout << "Game loop start" <<std::endl;
    while (game_continues) {
        while(SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                game_continues = false;
            } else if (event.type == SDL_JOYDEVICEADDED) {
                std::cout << "Joistick added " << event.jdevice.type << " : " << event.jdevice.which <<  std::endl;
            } else if (event.type == SDL_JOYDEVICEREMOVED) {
                std::cout << "Joistick removed " << event.jdevice.type << " : " << event.jdevice.which <<  std::endl;
            } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
                std::cout << "Controller added " << event.cdevice.type << " : " << event.cdevice.which <<  std::endl;
            } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                std::cout << "Controller removed " << event.cdevice.type << " : " << event.cdevice.which <<  std::endl;
            }
        }
        auto keyboard_state = SDL_GetKeyboardState(nullptr);
        // if (keyboard_state[SDL_SCANCODE_INSERT]) scale *= 1.1;
        // if (keyboard_state[SDL_SCANCODE_DELETE]) scale *= 0.9;
        
        std::vector<car_t> new_cars;
        for (auto &car:cars)
            new_cars.push_back(car.update(dt));

        for (int i = 0; i < cars.size(); i++) {
            auto car = cars[i];
            auto new_car = new_cars[i];
            std::vector<position_t> collisions = check_collision(*new_car.collision_pts.get(), new_car.p, new_car.angle,race_track->_collision_map);
            if (collisions.size() > 0) {
                collisions_draw = collisions;


                auto [nncar, collisions] = heuristic::find_best_corrected_position(new_car, race_track);
                if (collisions.size() == 0) { 
                    std::cout << "fixed: " << car.p << " " << car.angle << " to " << nncar.p << " " << nncar.angle << std::endl;
                    car = nncar; // this is the correct car position
                    car.v = car.v * 0.98;
                    auto velocity = ~car.v;
                    if (velocity > 0.0001) {
                        auto intended_move_vector = new_car.p - car.p;
                        auto actual_move_vector = nncar.p - car.p;
                        auto fix_vector = actual_move_vector - intended_move_vector;
                        if ((~fix_vector > 0.001) && (~actual_move_vector > 0.001)) {
                            intended_move_vector = intended_move_vector *(1.0/~intended_move_vector);
                            actual_move_vector = actual_move_vector *(1.0/~actual_move_vector);
                            auto move_vector_mirrored = actual_move_vector + fix_vector;
                            car.v = (move_vector_mirrored * 1.0/~move_vector_mirrored) * velocity;
                        } else {
                            auto nv1 = rotate_around({1.0,0.0},car.angle)*~car.v;
                            auto nv2 = nv1*-1.0;
                            car.v = (~(nv1-car.v) < ~(nv2-car.v))?nv1:nv2;
                        }
                    }
                } else {
                    std::cout << "not fixed: " << car.p << " " << car.angle << " to " << nncar.p << " " << nncar.angle << "   c: " << collisions.size() <<  std::endl;
                    car.v = {0.0, 0.0};
                }
            } else {
                car = new_car;
            }

            new_cars[i] = car;
        }
        cars = new_cars;
        position_t avg_pos = {0.0,0.0};
        for (const auto &car:cars) {
            std::cout << car.p  << " ";
            avg_pos = avg_pos + car.p;
        }
        camera_position = avg_pos*(1.0/cars.size());
        std::cout << avg_pos << "-> " << camera_position << std::endl;

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 0xff, 0x00, 0x00, 0xff);

        double scale = 1.0;
        {
        std::vector<SDL_Point> points;
        SDL_Rect result;
        for (const auto &c:cars) points.push_back({(int)c.p[0],(int)c.p[1]});
        SDL_EnclosePoints(points.data(),
                                points.size(),
                                nullptr,
                                &result);
        scale = 400.0/std::max(result.w, result.h);
        if (scale > 2.0) scale = 2.0;
        std::cout << scale << std::endl;
        }

        race_track->draw(camera_position[0], camera_position[1],scale);
        for (auto &car: cars)
            car.draw(camera_position, scale);

        // for (auto p: collisions_draw) {
        //     p = race_track_t::to_screen_coordinates(p, camera_position, scale);
        //     SDL_RenderDrawPoint(renderer, p[0], p[1]);
        //     std:: cout << p << "  " << camera_position << std::endl;
        // }


        SDL_RenderPresent(renderer);

        auto next_time = current_time + microseconds ((long long int)(dt*1000000.0));
        std::this_thread::sleep_until(next_time);
        current_time = next_time;
    }





    return 0;
}


    
}



int main(int argc, char *argv[])
{
    mcggame::mcg_main(argc, argv);
}



/*
using namespace mcggame;



int main() {


    for (double i = -M_PI; i < M_PI; i+= 0.02) {
        double rotation_angle = angle_between_vectors({0.0,10.0},rotate_around({0.0,10.0},i));
        std::cout << "Rotation angle: " << std::fixed << std::setprecision(4) << rotation_angle << "  VS " << i <<  std::endl;
    }
    for (double i = -M_PI; i < M_PI; i+= 0.02) {
        std::vector<position_t> shape1 = {{-32,-28}, {32,-28}, {32,28}, {-32,28}};
        std::vector<position_t> shape2 = {{-33,-27}, {32,-28}, {32,28}, {-32,28}};

        for (auto & p : shape2) {
            p = rotate_around(p, i);
        }

        double rotation_angle = angle_between_shapes(shape1, shape2);

        std::cout << "Rotation angle (shapes): " << std::fixed << std::setprecision(4) << rotation_angle << "   (should be: " << i << " )" << std::endl;
    }
    return 0;
}
*/