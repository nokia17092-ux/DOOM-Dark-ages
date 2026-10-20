#include <SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {
constexpr int Width = 320;
constexpr int Height = 200;
constexpr int HudHeight = 24;
constexpr float Pi = 3.14159265358979323846f;
constexpr float Fov = Pi / 3.0f;
constexpr float CameraHeight = 1.4f;

struct Color { Uint8 r, g, b; };
struct Point { float x, y, z, u, v; };
struct Level { std::string name; std::vector<std::vector<int>> map; int width = 0; int height = 0; };
struct Enemy { float x, y; int health = 2; float cooldown = 0; bool alive = true; };
struct Texture { std::array<Color, 4096> pixels{}; };
enum class State { Menu, Playing, Paused, Complete, Dead };

fs::path appDirectory() {
    char* base = SDL_GetBasePath();
    if (!base) return fs::current_path();
    fs::path path(base);
    SDL_free(base);
    return path;
}

std::array<std::array<std::uint8_t, 7>, 40> createFont() {
    return {{
        {{14,17,17,31,17,17,17}},{{30,17,17,30,17,17,30}},{{14,17,16,16,16,17,14}},{{30,17,17,17,17,17,30}},
        {{31,16,16,30,16,16,31}},{{31,16,16,30,16,16,16}},{{14,17,16,23,17,17,15}},{{17,17,17,31,17,17,17}},
        {{14,4,4,4,4,4,14}},{{7,2,2,2,18,18,12}},{{17,18,20,24,20,18,17}},{{16,16,16,16,16,16,31}},
        {{17,27,21,21,17,17,17}},{{17,25,21,19,17,17,17}},{{14,17,17,17,17,17,14}},{{30,17,17,30,16,16,16}},
        {{14,17,17,17,21,18,13}},{{30,17,17,30,20,18,17}},{{15,16,16,14,1,1,30}},{{31,4,4,4,4,4,4}},
        {{17,17,17,17,17,17,14}},{{17,17,17,17,17,10,4}},{{17,17,17,21,21,21,10}},{{17,17,10,4,10,17,17}},
        {{17,17,10,4,4,4,4}},{{31,1,2,4,8,16,31}},{{14,17,19,21,25,17,14}},{{4,12,4,4,4,4,14}},
        {{14,17,1,2,4,8,31}},{{30,1,1,14,1,1,30}},{{2,6,10,18,31,2,2}},{{31,16,16,30,1,1,30}},
        {{14,16,16,30,17,17,14}},{{31,1,2,4,8,8,8}},{{14,17,17,14,17,17,14}},{{14,17,17,15,1,1,14}},
        {{0,0,4,0,0,4,0}},{{0,0,4,0,0,4,8}},{{0,0,0,31,0,0,0}},{{0,0,0,0,0,0,0}}
    }};
}

int glyph(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + c - '0';
    if (c == ':') return 36;
    if (c == '.') return 37;
    if (c == '-') return 38;
    return 39;
}

class Game {
public:
    Game() : base(appDirectory()), font(createFont()) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) throw std::runtime_error(SDL_GetError());
        window = SDL_CreateWindow("DOOM: DARK AGES", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  960, 600, SDL_WINDOW_RESIZABLE);
        if (!window) throw std::runtime_error(SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer) throw std::runtime_error(SDL_GetError());
        SDL_RenderSetLogicalSize(renderer, Width, Height);
        frameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING, Width, Height);
        if (!frameTexture) throw std::runtime_error(SDL_GetError());
        pixels.resize(Width * Height);
        depth.resize(Width * Height);
        loadPalette();
        loadTextures();
        loadLevels();
    }

    ~Game() {
        if (frameTexture) SDL_DestroyTexture(frameTexture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    int run() {
        bool running = true;
        Uint64 previous = SDL_GetPerformanceCounter();
        while (running) {
            const Uint64 now = SDL_GetPerformanceCounter();
            const float dt = std::min(0.05f, static_cast<float>(now - previous) /
                                               static_cast<float>(SDL_GetPerformanceFrequency()));
            previous = now;
            running = events();
            update(dt);
            draw();
        }
        return 0;
    }

    int renderSmokeTest() {
        if (levels.empty()) return 2;
        start();
        draw3D();
        const std::size_t visiblePixels = static_cast<std::size_t>(std::count_if(
            depth.begin(), depth.end(), [](float value) { return std::isfinite(value); }));
        std::unordered_set<std::uint32_t> colors(pixels.begin(), pixels.end());
        std::cout << "3D frame rendered: " << visiblePixels << " depth pixels, "
                  << colors.size() << " colors, " << levels.size() << " maps\n";
        return visiblePixels > 100 && colors.size() > 16 ? 0 : 4;
    }

private:
    fs::path base;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* frameTexture = nullptr;
    State state = State::Menu;
    std::vector<Level> levels;
    std::vector<Enemy> enemies;
    std::array<Color, 256> palette{};
    std::array<Texture, 6> textures{};
    std::array<std::array<std::uint8_t, 7>, 40> font;
    std::vector<std::uint32_t> pixels;
    std::vector<float> depth;
    std::size_t selected = 0;
    float x = 2.5f, y = 2.5f, angle = 0, health = 100;
    int ammo = 24, kills = 0;
    std::string status;

    static std::uint32_t rgba(Color c) {
        return 0xff000000u | (static_cast<std::uint32_t>(c.r) << 16) |
               (static_cast<std::uint32_t>(c.g) << 8) | c.b;
    }

    void loadPalette() {
        std::ifstream file(base / "assets/extracted/sprites/PLAYPAL.lmp", std::ios::binary);
        std::array<unsigned char, 768> data{};
        if (file.read(reinterpret_cast<char*>(data.data()), data.size())) {
            for (std::size_t i = 0; i < palette.size(); ++i) palette[i] = {data[i*3], data[i*3+1], data[i*3+2]};
        } else {
            for (std::size_t i = 0; i < palette.size(); ++i) {
                const auto c = static_cast<Uint8>(i);
                palette[i] = {c, c, c};
            }
        }
    }

    void loadTextures() {
        const std::array<std::string, 6> names = {"AQF005", "FLAT1", "FLOOR0_1", "GRNLITE1", "RROCK01", "SWATER1"};
        for (std::size_t texture = 0; texture < names.size(); ++texture) {
            std::ifstream file(base / "assets/extracted/flats" / (names[texture] + ".raw"), std::ios::binary);
            std::array<unsigned char, 4096> data{};
            if (file.read(reinterpret_cast<char*>(data.data()), data.size())) {
                for (std::size_t i = 0; i < data.size(); ++i) textures[texture].pixels[i] = palette[data[i]];
            } else {
                for (std::size_t i = 0; i < data.size(); ++i) {
                    const Uint8 c = static_cast<Uint8>((i / 64 + i % 64) * 2);
                    textures[texture].pixels[i] = {c, static_cast<Uint8>(c / 2), static_cast<Uint8>(c / 3)};
                }
            }
        }
    }

    void loadLevels() {
        for (int number = 1; number <= 25; ++number) {
            const std::string filename = "level_" + std::string(number < 10 ? "0" : "") +
                                         std::to_string(number) + ".json";
            std::ifstream file(base / "levels" / filename);
            if (!file) continue;
            try {
                const Json json = Json::parse(file);
                if (!json.contains("map") || !json["map"].is_array()) continue;
                Level level;
                level.name = "LEVEL " + std::string(number < 10 ? "0" : "") + std::to_string(number);
                for (const auto& row : json["map"]) {
                    std::vector<int> cells;
                    for (const auto& cell : row) cells.push_back(cell.get<int>());
                    if (level.width == 0) level.width = static_cast<int>(cells.size());
                    if (static_cast<int>(cells.size()) != level.width) throw std::runtime_error("invalid map width");
                    level.map.push_back(std::move(cells));
                }
                level.height = static_cast<int>(level.map.size());
                if (level.width > 0 && level.height > 0) levels.push_back(std::move(level));
            } catch (const std::exception& error) {
                std::cerr << filename << ": " << error.what() << '\n';
            }
        }
        if (levels.empty()) status = "NO LEVELS FOUND";
    }

    bool wall(float wx, float wy) const {
        if (levels.empty()) return true;
        const auto& map = levels[selected];
        const int cx = static_cast<int>(std::floor(wx));
        const int cy = static_cast<int>(std::floor(wy));
        return cx < 0 || cy < 0 || cx >= map.width || cy >= map.height || map.map[cy][cx] > 0;
    }

    bool visible(float tx, float ty) const {
        const float dx = tx - x, dy = ty - y;
        const int steps = static_cast<int>(std::hypot(dx, dy) * 20);
        for (int i = 1; i < steps; ++i) {
            const float t = static_cast<float>(i) / steps;
            if (wall(x + dx * t, y + dy * t)) return false;
        }
        return true;
    }

    void start() {
        if (levels.empty()) return;
        const auto& level = levels[selected];
        float best = std::numeric_limits<float>::infinity();
        for (int cy = 0; cy < level.height; ++cy) for (int cx = 0; cx < level.width; ++cx) {
            if (level.map[cy][cx] != 0) continue;
            const float dx = cx + 0.5f - level.width / 2.0f;
            const float dy = cy + 0.5f - level.height / 2.0f;
            const float distance = dx*dx + dy*dy;
            if (distance < best) { best = distance; x = cx + 0.5f; y = cy + 0.5f; }
        }
        health = 100;
        ammo = 24;
        kills = 0;
        angle = 0;
        enemies.clear();
        for (int cy = 1; cy < level.height - 1 && enemies.size() < 8; ++cy) {
            for (int cx = 1; cx < level.width - 1 && enemies.size() < 8; ++cx) {
                const float dx = cx + 0.5f - x, dy = cy + 0.5f - y;
                if (level.map[cy][cx] == 0 && dx*dx + dy*dy > 36 && (cx*17 + cy*31) % 13 == 0)
                    enemies.push_back({cx + 0.5f, cy + 0.5f});
            }
        }
        state = State::Playing;
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    void shoot() {
        if (ammo <= 0) return;
        --ammo;
        Enemy* target = nullptr;
        float nearest = 8.0f;
        for (auto& enemy : enemies) {
            if (!enemy.alive) continue;
            const float dx = enemy.x - x, dy = enemy.y - y;
            const float distance = std::hypot(dx, dy);
            const float difference = std::remainder(std::atan2(dy, dx) - angle, 2 * Pi);
            if (distance < nearest && std::abs(difference) < 0.11f && visible(enemy.x, enemy.y)) {
                target = &enemy;
                nearest = distance;
            }
        }
        if (!target) return;
        if (--target->health <= 0) {
            target->alive = false;
            ++kills;
            if (std::none_of(enemies.begin(), enemies.end(), [](const Enemy& e) { return e.alive; })) {
                state = State::Complete;
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
        }
    }

    bool events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) return false;
            if (event.type == SDL_MOUSEMOTION && state == State::Playing) angle += event.motion.xrel * 0.003f;
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && state == State::Playing) shoot();
            if (event.type != SDL_KEYDOWN || event.key.repeat) continue;
            const auto key = event.key.keysym.sym;
            if (state == State::Menu) {
                if (key == SDLK_ESCAPE) return false;
                if (key == SDLK_DOWN && !levels.empty()) selected = (selected + 1) % levels.size();
                if (key == SDLK_UP && !levels.empty()) selected = (selected + levels.size() - 1) % levels.size();
                if (key == SDLK_RETURN || key == SDLK_SPACE) start();
            } else if (state == State::Playing && key == SDLK_ESCAPE) {
                state = State::Paused;
                SDL_SetRelativeMouseMode(SDL_FALSE);
            } else if (state == State::Paused) {
                if (key == SDLK_RETURN) { state = State::Playing; SDL_SetRelativeMouseMode(SDL_TRUE); }
                if (key == SDLK_ESCAPE) state = State::Menu;
            } else if (state == State::Complete) {
                if (key == SDLK_RETURN) { selected = (selected + 1) % levels.size(); start(); }
                if (key == SDLK_ESCAPE) state = State::Menu;
            } else if (state == State::Dead) {
                if (key == SDLK_r) start();
                if (key == SDLK_ESCAPE) state = State::Menu;
            }
        }
        return true;
    }

    void update(float dt) {
        if (state != State::Playing) return;
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const float forward = static_cast<float>(keys[SDL_SCANCODE_W]) - keys[SDL_SCANCODE_S];
        const float strafe = static_cast<float>(keys[SDL_SCANCODE_D]) - keys[SDL_SCANCODE_A];
        const float turn = static_cast<float>(keys[SDL_SCANCODE_RIGHT]) - keys[SDL_SCANCODE_LEFT];
        angle += turn * dt * 1.8f;
        const float dx = (std::cos(angle)*forward - std::sin(angle)*strafe) * dt * 3.2f;
        const float dy = (std::sin(angle)*forward + std::cos(angle)*strafe) * dt * 3.2f;
        if (!wall(x + dx, y)) x += dx;
        if (!wall(x, y + dy)) y += dy;
        for (auto& enemy : enemies) {
            if (!enemy.alive) continue;
            const float ex = x - enemy.x, ey = y - enemy.y;
            const float distance = std::hypot(ex, ey);
            enemy.cooldown = std::max(0.0f, enemy.cooldown - dt);
            if (distance < 1.25f) {
                if (enemy.cooldown == 0) {
                    health -= 8;
                    enemy.cooldown = 0.75f;
                    if (health <= 0) { state = State::Dead; SDL_SetRelativeMouseMode(SDL_FALSE); break; }
                }
            } else if (distance < 10 && visible(enemy.x, enemy.y)) {
                const float step = std::min(0.65f * dt, distance - 1.0f);
                if (!wall(enemy.x + ex / distance * step, enemy.y)) enemy.x += ex / distance * step;
                if (!wall(enemy.x, enemy.y + ey / distance * step)) enemy.y += ey / distance * step;
            }
        }
    }

    void setColor(Color c) const { SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255); }

    void text(const std::string& value, int px, int py, Color color, int scale = 1) const {
        setColor(color);
        for (char c : value) {
            if (c == ' ') { px += 6 * scale; continue; }
            const auto& bitmap = font[static_cast<std::size_t>(glyph(c))];
            for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col) {
                if ((bitmap[row] & (1u << (4 - col))) != 0) {
                    SDL_Rect pixel{px + col*scale, py + row*scale, scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
            px += 6 * scale;
        }
    }

    void rasterTriangle(const Point& p0, const Point& p1, const Point& p2,
                        std::size_t texture, Color tint, float light) {
        struct Projected { float x, y, z, u, v; };
        const float fx = std::cos(angle), fy = std::sin(angle);
        const float rx = -fy, ry = fx;
        const float focal = Width / (2 * std::tan(Fov / 2));
        const int viewHeight = Height - HudHeight;
        const auto project = [&](const Point& p, Projected& out) {
            const float dx = p.x - x, dy = p.y - y;
            out.z = dx*fx + dy*fy;
            if (out.z <= 0.08f) return false;
            out.x = Width*0.5f + (dx*rx + dy*ry) * focal / out.z;
            out.y = viewHeight*0.5f - (p.z - CameraHeight) * focal / out.z;
            out.u = p.u; out.v = p.v;
            return true;
        };
        Projected a{}, b{}, c{};
        if (!project(p0, a) || !project(p1, b) || !project(p2, c)) return;
        const float area = (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
        if (std::abs(area) < 0.0001f) return;
        const int left = std::max(0, static_cast<int>(std::floor(std::min({a.x,b.x,c.x}))));
        const int right = std::min(Width-1, static_cast<int>(std::ceil(std::max({a.x,b.x,c.x}))));
        const int top = std::max(0, static_cast<int>(std::floor(std::min({a.y,b.y,c.y}))));
        const int bottom = std::min(viewHeight-1, static_cast<int>(std::ceil(std::max({a.y,b.y,c.y}))));
        const auto edge = [](float x0, float y0, float x1, float y1, float px, float py) {
            return (px-x0)*(y1-y0) - (py-y0)*(x1-x0);
        };
        for (int py = top; py <= bottom; ++py) for (int px = left; px <= right; ++px) {
            const float wa = edge(b.x,b.y,c.x,c.y,px+0.5f,py+0.5f) / area;
            const float wb = edge(c.x,c.y,a.x,a.y,px+0.5f,py+0.5f) / area;
            const float wc = 1 - wa - wb;
            if (wa < 0 || wb < 0 || wc < 0) continue;
            const float inverseZ = wa/a.z + wb/b.z + wc/c.z;
            if (inverseZ <= 0) continue;
            const float z = 1 / inverseZ;
            const std::size_t at = static_cast<std::size_t>(py*Width + px);
            if (z >= depth[at]) continue;
            depth[at] = z;
            const float u = (wa*a.u/a.z + wb*b.u/b.z + wc*c.u/c.z) / inverseZ;
            const float v = (wa*a.v/a.z + wb*b.v/b.z + wc*c.v/c.z) / inverseZ;
            const int tx = ((static_cast<int>(std::floor(u)) % 64) + 64) % 64;
            const int ty = ((static_cast<int>(std::floor(v)) % 64) + 64) % 64;
            const Color src = textures[texture].pixels[static_cast<std::size_t>(ty*64 + tx)];
            const float shade = light * std::clamp(1.0f / (1.0f + z*0.055f), 0.25f, 1.0f);
            pixels[at] = rgba({static_cast<Uint8>(src.r*tint.r/255.0f*shade),
                               static_cast<Uint8>(src.g*tint.g/255.0f*shade),
                               static_cast<Uint8>(src.b*tint.b/255.0f*shade)});
        }
    }

    void quad(const std::array<Point,4>& p, std::size_t texture, Color tint = {255,255,255}, float light = 1) {
        rasterTriangle(p[0], p[1], p[2], texture, tint, light);
        rasterTriangle(p[0], p[2], p[3], texture, tint, light);
    }

    void box(float bx, float by, float bz, float sx, float sy, float sz,
             std::size_t tex, Color tint = {255,255,255}) {
        const auto face = [&](Point a, Point b, Point c, Point d, float light) {
            quad({a,b,c,d}, tex, tint, light);
        };
        face({bx,by,bz,0,64},{bx+sx,by,bz,64,64},{bx+sx,by,bz+sz,64,0},{bx,by,bz+sz,0,0},0.9f);
        face({bx+sx,by+sy,bz,0,64},{bx,by+sy,bz,64,64},{bx,by+sy,bz+sz,64,0},{bx+sx,by+sy,bz+sz,0,0},0.72f);
        face({bx,by+sy,bz,0,64},{bx,by,bz,64,64},{bx,by,bz+sz,64,0},{bx,by+sy,bz+sz,0,0},0.82f);
        face({bx+sx,by,bz,0,64},{bx+sx,by+sy,bz,64,64},{bx+sx,by+sy,bz+sz,64,0},{bx+sx,by,bz+sz,0,0},1.0f);
        face({bx,by,bz+sz,0,64},{bx+sx,by,bz+sz,64,64},{bx+sx,by+sy,bz+sz,64,0},{bx,by+sy,bz+sz,0,0},1.0f);
    }

    void draw3D() {
        const auto& level = levels[selected];
        const int horizon = (Height - HudHeight) / 2;
        const float focal = Width / (2 * std::tan(Fov / 2));
        const float fx = std::cos(angle), fy = std::sin(angle), rx = -fy, ry = fx;
        for (int py = 0; py < Height; ++py) for (int px = 0; px < Width; ++px) {
            Color sky = {31, 34, 43};
            if (py >= Height - HudHeight) sky = {17,19,20};
            else if (py < horizon) {
                const Uint8 shade = static_cast<Uint8>(28 + py*18/std::max(1,horizon));
                sky = {shade, static_cast<Uint8>(shade+2), static_cast<Uint8>(shade+10)};
            } else {
                const Uint8 shade = static_cast<Uint8>(54 - (py-horizon)*22/std::max(1,horizon));
                sky = {shade, static_cast<Uint8>(shade-8), static_cast<Uint8>(shade-15)};
            }
            const std::size_t at = static_cast<std::size_t>(py*Width + px);
            pixels[at] = rgba(sky);
            depth[at] = std::numeric_limits<float>::infinity();
            if (py >= horizon && py < Height-HudHeight) {
                const float distance = CameraHeight*focal / std::max(1, py-horizon);
                const float lateral = (px-Width*0.5f)*distance/focal;
                const float wx = x + fx*distance + rx*lateral;
                const float wy = y + fy*distance + ry*lateral;
                const int mx = static_cast<int>(std::floor(wx)), my = static_cast<int>(std::floor(wy));
                if (mx >= 0 && my >= 0 && mx < level.width && my < level.height) {
                    const int tx = ((static_cast<int>(wx*64)%64)+64)%64;
                    const int ty = ((static_cast<int>(wy*64)%64)+64)%64;
                    pixels[at] = rgba(textures[2].pixels[static_cast<std::size_t>(ty*64+tx)]);
                    depth[at] = distance;
                }
            }
        }
        quad({{{0,0,2.7f,0,0},{static_cast<float>(level.width),0,2.7f,level.width*64.0f,0},
               {static_cast<float>(level.width),static_cast<float>(level.height),2.7f,level.width*64.0f,level.height*64.0f},
               {0,static_cast<float>(level.height),2.7f,0,level.height*64.0f}}}, 1, {255,255,255}, 0.65f);
        for (int my=0; my<level.height; ++my) for (int mx=0; mx<level.width; ++mx) {
            const int type = level.map[my][mx];
            if (type <= 0) continue;
            const float dx=mx+0.5f-x, dy=my+0.5f-y;
            if (dx*dx+dy*dy > 24*24) continue;
            box(static_cast<float>(mx),static_cast<float>(my),0,1,1,2.7f,
                static_cast<std::size_t>(std::clamp(type-1,0,5)));
        }
        for (const auto& enemy : enemies) if (enemy.alive) {
            box(enemy.x-0.28f,enemy.y-0.28f,0,0.56f,0.56f,1.2f,3,{255,104,82});
            box(enemy.x-0.18f,enemy.y-0.18f,1.2f,0.36f,0.36f,0.32f,0,{255,196,130});
        }
        SDL_UpdateTexture(frameTexture,nullptr,pixels.data(),Width*sizeof(std::uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer,frameTexture,nullptr,nullptr);
    }

    void drawMenu() {
        setColor({10,12,14}); SDL_RenderClear(renderer);
        setColor({96,34,20}); SDL_Rect band{0,38,Width,3}; SDL_RenderFillRect(renderer,&band);
        text("DOOM DARK AGES",72,49,{232,179,105},3);
        text("SELECT A LEVEL",105,91,{205,211,205});
        if (!levels.empty()) {
            const std::string label = "< " + levels[selected].name + " >";
            text(label,(Width-static_cast<int>(label.size())*12)/2,117,{238,119,65},2);
        } else text("NO LEVELS FOUND",100,121,{230,75,60});
        text("UP DOWN CHANGE LEVEL",81,160,{130,143,146});
        text("ENTER START   ESC QUIT",78,173,{130,143,146});
        if (!status.empty()) text(status,5,5,{230,80,60});
    }

    void draw() {
        if (state == State::Menu) drawMenu();
        else {
            draw3D();
            SDL_Rect hud{0,Height-HudHeight,Width,HudHeight};
            setColor({17,19,20}); SDL_RenderFillRect(renderer,&hud);
            setColor({132,72,44}); SDL_RenderDrawLine(renderer,0,Height-HudHeight,Width,Height-HudHeight);
            text(levels[selected].name,5,Height-HudHeight+7,{226,179,111});
            text("HP",93,Height-HudHeight+7,{180,184,178});
            text(std::to_string(static_cast<int>(health)),111,Height-HudHeight+7,{225,96,76});
            text("AMMO",150,Height-HudHeight+7,{180,184,178});
            text(std::to_string(ammo),180,Height-HudHeight+7,{232,202,105});
            text("K",218,Height-HudHeight+7,{180,184,178});
            text(std::to_string(kills),230,Height-HudHeight+7,{225,96,76});
            setColor({232,211,165});
            SDL_RenderDrawLine(renderer,Width/2-4,Height/2,Width/2+4,Height/2);
            SDL_RenderDrawLine(renderer,Width/2,Height/2-4,Width/2,Height/2+4);
            if (state != State::Playing) {
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer,0,0,0,175);
                SDL_Rect shade{0,0,Width,Height-HudHeight}; SDL_RenderFillRect(renderer,&shade);
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
                if (state == State::Paused) {
                    text("PAUSED",128,77,{238,175,105},2);
                    text("ENTER RESUME  ESC MENU",82,103,{210,210,200});
                } else if (state == State::Complete) {
                    text("LEVEL CLEARED",91,77,{238,175,105},2);
                    text("ENTER NEXT LEVEL  ESC MENU",72,103,{210,210,200});
                } else {
                    text("YOU DIED",116,77,{238,92,68},2);
                    text("R RETRY  ESC MENU",95,103,{210,210,200});
                }
            }
        }
        SDL_RenderPresent(renderer);
    }
};
}

int main(int argc, char** argv) {
    try {
        Game game;
        if (argc > 1 && std::string(argv[1]) == "--render-smoke") return game.renderSmokeTest();
        return game.run();
    }
    catch (const std::exception& error) { std::cerr << "DOOM Dark Ages: " << error.what() << '\n'; return 1; }
}
