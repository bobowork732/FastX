#ifdef FASTX_USE_SDL
#include <SDL2/SDL.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace fastx {

constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr float kPi = 3.14159265359f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }
Vec3 Normalize(const Vec3& v) {
    float len = Length(v);
    return len > 0.0f ? v / len : Vec3{};
}

struct Mat4 {
    std::array<float, 16> m{};

    static Mat4 Identity() {
        Mat4 r;
        r.m = {1, 0, 0, 0,
               0, 1, 0, 0,
               0, 0, 1, 0,
               0, 0, 0, 1};
        return r;
    }

    static Mat4 Translation(const Vec3& t) {
        Mat4 r = Identity();
        r.m[3] = t.x;
        r.m[7] = t.y;
        r.m[11] = t.z;
        return r;
    }

    static Mat4 RotationY(float a) {
        Mat4 r = Identity();
        float c = std::cos(a);
        float s = std::sin(a);
        r.m[0] = c;
        r.m[2] = s;
        r.m[8] = -s;
        r.m[10] = c;
        return r;
    }

    static Mat4 Perspective(float fovY, float aspect, float zNear, float zFar) {
        Mat4 r{};
        float f = 1.0f / std::tan(fovY * 0.5f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zFar + zNear) / (zNear - zFar);
        r.m[11] = (2.0f * zFar * zNear) / (zNear - zFar);
        r.m[14] = -1.0f;
        return r;
    }

    static Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = Normalize(center - eye);
        Vec3 s = Normalize(Cross(f, up));
        Vec3 u = Cross(s, f);

        Mat4 r = Identity();
        r.m[0] = s.x;
        r.m[1] = s.y;
        r.m[2] = s.z;
        r.m[3] = -Dot(s, eye);

        r.m[4] = u.x;
        r.m[5] = u.y;
        r.m[6] = u.z;
        r.m[7] = -Dot(u, eye);

        r.m[8] = -f.x;
        r.m[9] = -f.y;
        r.m[10] = -f.z;
        r.m[11] = Dot(f, eye);
        return r;
    }
};

Vec4 Mul(const Mat4& m, const Vec4& v) {
    return {
        m.m[0] * v.x + m.m[1] * v.y + m.m[2] * v.z + m.m[3] * v.w,
        m.m[4] * v.x + m.m[5] * v.y + m.m[6] * v.z + m.m[7] * v.w,
        m.m[8] * v.x + m.m[9] * v.y + m.m[10] * v.z + m.m[11] * v.w,
        m.m[12] * v.x + m.m[13] * v.y + m.m[14] * v.z + m.m[15] * v.w};
}

Mat4 Mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            r.m[row * 4 + col] =
                a.m[row * 4 + 0] * b.m[0 * 4 + col] + a.m[row * 4 + 1] * b.m[1 * 4 + col] +
                a.m[row * 4 + 2] * b.m[2 * 4 + col] + a.m[row * 4 + 3] * b.m[3 * 4 + col];
        }
    }
    return r;
}

struct Vertex {
    Vec3 position;
    Vec3 normal;
};

struct Triangle {
    int i0 = 0;
    int i1 = 0;
    int i2 = 0;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
};

struct Object {
    Mesh mesh;
    Vec3 position{0.0f, 0.0f, 0.0f};
    float rotationY = 0.0f;
    Vec3 color{0.7f, 0.7f, 0.7f};
};

Mesh CreateCube() {
    Mesh m;
    std::array<Vec3, 8> p = {{{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                              {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}}};
    std::array<Triangle, 12> t = {{{0, 1, 2}, {0, 2, 3}, {1, 5, 6}, {1, 6, 2}, {5, 4, 7}, {5, 7, 6},
                                    {4, 0, 3}, {4, 3, 7}, {3, 2, 6}, {3, 6, 7}, {4, 5, 1}, {4, 1, 0}}};
    m.vertices.resize(8);
    for (int i = 0; i < 8; ++i) {
        m.vertices[i].position = p[i];
        m.vertices[i].normal = Normalize(p[i]);
    }
    m.triangles.assign(t.begin(), t.end());
    return m;
}

Mesh CreateSphere(int segments = 16, int rings = 12) {
    Mesh m;
    for (int y = 0; y <= rings; ++y) {
        float v = static_cast<float>(y) / static_cast<float>(rings);
        float phi = v * kPi;
        for (int x = 0; x <= segments; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(segments);
            float theta = u * 2.0f * kPi;
            Vec3 p{std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)};
            m.vertices.push_back({p, Normalize(p)});
        }
    }
    int row = segments + 1;
    for (int y = 0; y < rings; ++y) {
        for (int x = 0; x < segments; ++x) {
            int i0 = y * row + x;
            int i1 = i0 + 1;
            int i2 = i0 + row;
            int i3 = i2 + 1;
            m.triangles.push_back({i0, i2, i1});
            m.triangles.push_back({i1, i2, i3});
        }
    }
    return m;
}

std::optional<Mesh> LoadOBJ(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return std::nullopt;
    }

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    Mesh m;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream ss(line);
        std::string type;
        ss >> type;
        if (type == "v") {
            Vec3 p;
            ss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (type == "vn") {
            Vec3 n;
            ss >> n.x >> n.y >> n.z;
            normals.push_back(Normalize(n));
        } else if (type == "f") {
            std::array<std::string, 4> parts;
            int count = 0;
            while (ss >> parts[count]) {
                ++count;
                if (count == 4) {
                    break;
                }
            }
            if (count < 3) {
                continue;
            }

            auto parseIndex = [&](const std::string& token) {
                int vi = -1;
                int ni = -1;
                size_t s1 = token.find('/');
                if (s1 == std::string::npos) {
                    vi = std::stoi(token) - 1;
                } else {
                    vi = std::stoi(token.substr(0, s1)) - 1;
                    size_t s2 = token.find('/', s1 + 1);
                    if (s2 != std::string::npos && s2 + 1 < token.size()) {
                        ni = std::stoi(token.substr(s2 + 1)) - 1;
                    }
                }
                return std::pair<int, int>{vi, ni};
            };

            std::array<int, 4> idx{};
            for (int i = 0; i < count; ++i) {
                auto [vi, ni] = parseIndex(parts[i]);
                if (vi < 0 || vi >= static_cast<int>(positions.size())) {
                    continue;
                }
                Vec3 n = ni >= 0 && ni < static_cast<int>(normals.size()) ? normals[ni] : Normalize(positions[vi]);
                m.vertices.push_back({positions[vi], n});
                idx[i] = static_cast<int>(m.vertices.size() - 1);
            }
            m.triangles.push_back({idx[0], idx[1], idx[2]});
            if (count == 4) {
                m.triangles.push_back({idx[0], idx[2], idx[3]});
            }
        }
    }

    if (m.vertices.empty() || m.triangles.empty()) {
        return std::nullopt;
    }
    return m;
}

struct Camera {
    Vec3 pos{0.0f, 1.5f, 6.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
};

struct RasterVertex {
    Vec3 world;
    Vec3 normal;
    float sx = 0.0f;
    float sy = 0.0f;
    float z = 0.0f;
    float invW = 1.0f;
};

class Renderer {
public:
    Renderer(int w, int h) : width_(w), height_(h), color_(w * h), depth_(w * h), normal_(w * h) {}

    void Clear() {
        std::fill(color_.begin(), color_.end(), 0x11161DFFu);
        std::fill(depth_.begin(), depth_.end(), std::numeric_limits<float>::infinity());
        std::fill(normal_.begin(), normal_.end(), Vec3{});
    }

    void Draw(const std::vector<Object>& objects, const Camera& cam) {
        Mat4 proj = Mat4::Perspective(70.0f * kPi / 180.0f, static_cast<float>(width_) / static_cast<float>(height_), 0.1f,
                                      200.0f);
        Vec3 forward{std::cos(cam.pitch) * std::sin(cam.yaw), std::sin(cam.pitch), std::cos(cam.pitch) * std::cos(cam.yaw)};
        Mat4 view = Mat4::LookAt(cam.pos, cam.pos + forward, {0, 1, 0});

        for (const auto& obj : objects) {
            Mat4 model = Mul(Mat4::Translation(obj.position), Mat4::RotationY(obj.rotationY));
            Mat4 mvp = Mul(proj, Mul(view, model));
            DrawObject(obj, model, mvp, cam.pos);
        }

        ScreenSpaceRayTrace(forward);
    }

    const uint32_t* ColorBuffer() const { return color_.data(); }

private:
    void DrawObject(const Object& obj, const Mat4& model, const Mat4& mvp, const Vec3& camPos) {
        std::vector<RasterVertex> verts(obj.mesh.vertices.size());
        for (size_t i = 0; i < obj.mesh.vertices.size(); ++i) {
            const auto& v = obj.mesh.vertices[i];
            Vec4 world4 = Mul(model, Vec4{v.position.x, v.position.y, v.position.z, 1.0f});
            Vec4 clip = Mul(mvp, Vec4{v.position.x, v.position.y, v.position.z, 1.0f});
            if (clip.w <= 0.0001f) {
                verts[i].z = -1.0f;
                continue;
            }
            float invW = 1.0f / clip.w;
            float nx = clip.x * invW;
            float ny = clip.y * invW;
            float nz = clip.z * invW;
            verts[i].world = {world4.x, world4.y, world4.z};
            verts[i].normal = Normalize(v.normal);
            verts[i].sx = (nx * 0.5f + 0.5f) * static_cast<float>(width_);
            verts[i].sy = (1.0f - (ny * 0.5f + 0.5f)) * static_cast<float>(height_);
            verts[i].z = nz;
            verts[i].invW = invW;
        }

        Vec3 lightDir = Normalize(Vec3{-0.4f, -1.0f, -0.3f});
        for (const auto& tri : obj.mesh.triangles) {
            const RasterVertex& a = verts[tri.i0];
            const RasterVertex& b = verts[tri.i1];
            const RasterVertex& c = verts[tri.i2];
            if (a.z < 0.0f || b.z < 0.0f || c.z < 0.0f) {
                continue;
            }

            Vec3 ab{b.sx - a.sx, b.sy - a.sy, b.z - a.z};
            Vec3 ac{c.sx - a.sx, c.sy - a.sy, c.z - a.z};
            float facing = ab.x * ac.y - ab.y * ac.x;
            if (facing <= 0.0f) {
                continue;
            }

            int minX = static_cast<int>(std::max(0.0f, std::floor(std::min({a.sx, b.sx, c.sx}))));
            int minY = static_cast<int>(std::max(0.0f, std::floor(std::min({a.sy, b.sy, c.sy}))));
            int maxX = static_cast<int>(std::min(static_cast<float>(width_ - 1), std::ceil(std::max({a.sx, b.sx, c.sx}))));
            int maxY = static_cast<int>(std::min(static_cast<float>(height_ - 1), std::ceil(std::max({a.sy, b.sy, c.sy}))));

            auto edge = [](float ax, float ay, float bx, float by, float px, float py) {
                return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
            };
            float area = edge(a.sx, a.sy, b.sx, b.sy, c.sx, c.sy);
            if (std::abs(area) < 1e-5f) {
                continue;
            }

            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    float px = x + 0.5f;
                    float py = y + 0.5f;
                    float w0 = edge(b.sx, b.sy, c.sx, c.sy, px, py);
                    float w1 = edge(c.sx, c.sy, a.sx, a.sy, px, py);
                    float w2 = edge(a.sx, a.sy, b.sx, b.sy, px, py);
                    if ((w0 < 0) || (w1 < 0) || (w2 < 0)) {
                        continue;
                    }

                    w0 /= area;
                    w1 /= area;
                    w2 /= area;

                    float depth = a.z * w0 + b.z * w1 + c.z * w2;
                    int idx = y * width_ + x;
                    if (depth >= depth_[idx]) {
                        continue;
                    }

                    Vec3 n = Normalize(a.normal * w0 + b.normal * w1 + c.normal * w2);
                    Vec3 world = a.world * w0 + b.world * w1 + c.world * w2;
                    Vec3 vdir = Normalize(camPos - world);
                    float diff = std::max(0.05f, -Dot(n, lightDir));
                    float spec = std::pow(std::max(0.0f, Dot(Normalize(lightDir + vdir), n)), 30.0f);
                    float shadow = world.y < -0.95f ? 0.6f : 1.0f;

                    Vec3 col = obj.color * (diff * shadow) + Vec3{1, 1, 1} * (0.2f * spec);
                    col.x = std::clamp(col.x, 0.0f, 1.0f);
                    col.y = std::clamp(col.y, 0.0f, 1.0f);
                    col.z = std::clamp(col.z, 0.0f, 1.0f);

                    depth_[idx] = depth;
                    normal_[idx] = n;
                    color_[idx] = Pack(col.x, col.y, col.z);
                }
            }
        }
    }

    void ScreenSpaceRayTrace(const Vec3& viewDir) {
        std::vector<uint32_t> post = color_;
        for (int y = 1; y < height_ - 1; ++y) {
            for (int x = 1; x < width_ - 1; ++x) {
                int idx = y * width_ + x;
                if (!std::isfinite(depth_[idx])) {
                    continue;
                }
                Vec3 n = Normalize(normal_[idx]);
                Vec3 refl = Normalize(viewDir - n * (2.0f * Dot(viewDir, n)));

                float sx = static_cast<float>(x);
                float sy = static_cast<float>(y);
                bool hit = false;
                for (int step = 0; step < 20; ++step) {
                    sx += refl.x * 3.0f;
                    sy -= refl.y * 3.0f;
                    int ix = static_cast<int>(sx);
                    int iy = static_cast<int>(sy);
                    if (ix < 1 || iy < 1 || ix >= width_ - 1 || iy >= height_ - 1) {
                        break;
                    }
                    int ri = iy * width_ + ix;
                    if (std::isfinite(depth_[ri]) && depth_[ri] < depth_[idx] - 0.002f) {
                        uint32_t rc = color_[ri];
                        post[idx] = Blend(color_[idx], rc, 0.22f);
                        hit = true;
                        break;
                    }
                }
                if (!hit) {
                    post[idx] = Blend(color_[idx], 0x0E1218FFu, 0.08f);
                }
            }
        }
        color_.swap(post);
    }

    static uint32_t Pack(float r, float g, float b) {
        uint8_t R = static_cast<uint8_t>(r * 255.0f);
        uint8_t G = static_cast<uint8_t>(g * 255.0f);
        uint8_t B = static_cast<uint8_t>(b * 255.0f);
        return (R << 24) | (G << 16) | (B << 8) | 0xFFu;
    }

    static uint32_t Blend(uint32_t a, uint32_t b, float t) {
        auto ch = [](uint32_t c, int s) { return static_cast<float>((c >> s) & 0xFFu); };
        float ar = ch(a, 24), ag = ch(a, 16), ab = ch(a, 8);
        float br = ch(b, 24), bg = ch(b, 16), bb = ch(b, 8);
        uint32_t r = static_cast<uint32_t>(std::clamp(ar + (br - ar) * t, 0.0f, 255.0f));
        uint32_t g = static_cast<uint32_t>(std::clamp(ag + (bg - ag) * t, 0.0f, 255.0f));
        uint32_t bl = static_cast<uint32_t>(std::clamp(ab + (bb - ab) * t, 0.0f, 255.0f));
        return (r << 24) | (g << 16) | (bl << 8) | 0xFFu;
    }

    int width_;
    int height_;
    std::vector<uint32_t> color_;
    std::vector<float> depth_;
    std::vector<Vec3> normal_;
};

std::string PickOBJFromCurrentDirectory() {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path())) {
        if (entry.is_regular_file() && entry.path().extension() == ".obj") {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());
    std::cout << "FastX file explorer (.obj in current folder)\n";
    if (files.empty()) {
        std::cout << "No OBJ files found. Press Enter to skip import.\n";
        std::string dummy;
        std::getline(std::cin, dummy);
        return {};
    }

    for (size_t i = 0; i < files.size(); ++i) {
        std::cout << "  [" << i << "] " << files[i].filename().string() << '\n';
    }
    std::cout << "Select index (or blank for none): ";
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) {
        return {};
    }
    try {
        size_t idx = static_cast<size_t>(std::stoul(line));
        if (idx < files.size()) {
            return files[idx].string();
        }
    } catch (...) {
    }
    return {};
}

}  // namespace fastx

int main() {
    using namespace fastx;

    std::string objPath = PickOBJFromCurrentDirectory();

    Camera cam;
    std::vector<Object> objects;
    if (!objPath.empty()) {
        auto loaded = LoadOBJ(objPath);
        if (loaded) {
            objects.push_back({*loaded, {0.0f, 0.0f, 0.0f}, 0.0f, {0.55f, 0.72f, 0.95f}});
            std::cout << "Imported OBJ: " << objPath << '\n';
        } else {
            std::cerr << "Failed to import OBJ: " << objPath << '\n';
        }
    }

    objects.push_back({CreateCube(), {-2.3f, 0.0f, 0.0f}, 0.0f, {0.95f, 0.35f, 0.32f}});
    objects.push_back({CreateSphere(), {2.3f, 0.0f, 0.0f}, 0.0f, {0.25f, 0.85f, 0.48f}});

#ifdef FASTX_USE_SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* win = SDL_CreateWindow("FastX - CPU Real Time Renderer (No OpenGL)", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, kWidth, kHeight, SDL_WINDOW_SHOWN);
    if (!win) {
        std::cerr << "Window create failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* render = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture* tex = SDL_CreateTexture(render, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight);

    Renderer engine(kWidth, kHeight);
    bool running = true;
    bool mouseLook = false;
    auto last = std::chrono::high_resolution_clock::now();

    while (running) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        dt = std::min(dt, 0.033f);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
                mouseLook = true;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) {
                mouseLook = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            if (mouseLook && e.type == SDL_MOUSEMOTION) {
                cam.yaw += static_cast<float>(e.motion.xrel) * 0.002f;
                cam.pitch -= static_cast<float>(e.motion.yrel) * 0.002f;
                cam.pitch = std::clamp(cam.pitch, -1.4f, 1.4f);
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        Vec3 forward{std::cos(cam.pitch) * std::sin(cam.yaw), 0.0f, std::cos(cam.pitch) * std::cos(cam.yaw)};
        forward = Normalize(forward);
        Vec3 right = Normalize(Cross(forward, {0, 1, 0}));
        float speed = 4.0f * dt;
        if (keys[SDL_SCANCODE_W]) cam.pos += forward * speed;
        if (keys[SDL_SCANCODE_S]) cam.pos += forward * -speed;
        if (keys[SDL_SCANCODE_A]) cam.pos += right * -speed;
        if (keys[SDL_SCANCODE_D]) cam.pos += right * speed;
        if (keys[SDL_SCANCODE_Q]) cam.pos.y -= speed;
        if (keys[SDL_SCANCODE_E]) cam.pos.y += speed;

        for (size_t i = 0; i < objects.size(); ++i) {
            objects[i].rotationY += dt * (0.2f + 0.1f * static_cast<float>(i));
        }

        engine.Clear();
        engine.Draw(objects, cam);

        SDL_UpdateTexture(tex, nullptr, engine.ColorBuffer(), kWidth * static_cast<int>(sizeof(uint32_t)));
        SDL_RenderClear(render);
        SDL_RenderCopy(render, tex, nullptr, nullptr);
        SDL_RenderPresent(render);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(render);
    SDL_DestroyWindow(win);
    SDL_Quit();
#else
    Renderer engine(kWidth, kHeight);
    for (size_t i = 0; i < objects.size(); ++i) {
        objects[i].rotationY = 0.6f * static_cast<float>(i);
    }
    engine.Clear();
    engine.Draw(objects, cam);

    std::ofstream ppm("output.ppm", std::ios::binary);
    ppm << "P6\n" << kWidth << " " << kHeight << "\n255\n";
    const uint32_t* buf = engine.ColorBuffer();
    for (int i = 0; i < kWidth * kHeight; ++i) {
        uint8_t r = static_cast<uint8_t>((buf[i] >> 24) & 0xFFu);
        uint8_t g = static_cast<uint8_t>((buf[i] >> 16) & 0xFFu);
        uint8_t b = static_cast<uint8_t>((buf[i] >> 8) & 0xFFu);
        ppm.write(reinterpret_cast<char*>(&r), 1);
        ppm.write(reinterpret_cast<char*>(&g), 1);
        ppm.write(reinterpret_cast<char*>(&b), 1);
    }
    std::cout << "SDL2 not available. Rendered single frame to output.ppm\n";
#endif

    return 0;
}
