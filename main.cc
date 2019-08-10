#include "panic.h"

#include "geometry.h"
#include "shader_program.h"
#include "util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <unistd.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <map>

namespace glm
{
bool operator<(const vec3 &a, const vec3 &b)
{
    return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
}
}

class sphere_geometry
{
public:
    sphere_geometry(int max_subdivisions, bool dual)
        : max_subdivisions_(max_subdivisions)
        , dual_(dual)
    {
        initialize_geometry();
    }

    void render() const
    {
        geometry_.bind();
        glDrawArrays(GL_TRIANGLES, 0, verts_.size());
    }

private:
    void initialize_geometry()
    {
        static const glm::vec3 icosahedron_verts[] = {
            {0, -0.525731, 0.850651},  {0.850651, 0, 0.525731},   {0.850651, 0, -0.525731}, {-0.850651, 0, -0.525731},
            {-0.850651, 0, 0.525731},  {-0.525731, 0.850651, 0},  {0.525731, 0.850651, 0},  {0.525731, -0.850651, 0},
            {-0.525731, -0.850651, 0}, {0, -0.525731, -0.850651}, {0, 0.525731, -0.850651}, {0, 0.525731, 0.850651}};
        static const int icosahedron_tris[][3] = {{2, 3, 7},  {2, 8, 3},   {4, 5, 6},   {5, 4, 9},  {7, 6, 12},
                                                  {6, 7, 11}, {10, 11, 3}, {11, 10, 4}, {8, 9, 10}, {9, 8, 1},
                                                  {12, 1, 2}, {1, 12, 5},  {7, 3, 11},  {2, 7, 12}, {4, 6, 11},
                                                  {6, 5, 12}, {3, 8, 10},  {8, 2, 1},   {4, 10, 9}, {5, 9, 1}};

        for (const auto &vertex : icosahedron_verts)
        {
            maybe_add_vertex(vertex);
        }

        for (const auto &triangle : icosahedron_tris)
        {
            const auto i0 = triangle[0] - 1;
            const auto i1 = triangle[1] - 1;
            const auto i2 = triangle[2] - 1;
            subdivide_triangle(i0, i1, i2, 0);
        }

        if (dual_)
            initialize_dual();

        geometry_.set_data(verts_);
    }

    void initialize_dual()
    {
        for (const auto &source_vert : source_verts_)
        {
            std::vector<glm::vec3> verts;
            auto center = glm::vec3(0);
            for (const auto &tri : source_vert.adjacent_triangles)
            {
                const auto vn =
                    glm::normalize((1.0f / 3) * (source_verts_[tri.i0].position + source_verts_[tri.i1].position +
                                                 source_verts_[tri.i2].position));
                center += vn;
                verts.push_back(vn);
            }
            for (size_t i = 1; i < verts.size() - 1; ++i)
            {
                auto it = std::min_element(std::next(verts.begin(), i), verts.end(),
                                           [o = verts[i - 1]](const auto &a, const auto &b) {
                                               return glm::length(a - o) < glm::length(b - o);
                                           });
                std::swap(verts[i], *it);
            }
            center *= (1.0f / verts.size());
            const auto normal = glm::normalize(center);

            // flip verts if wrong orientation
            const auto r = glm::cross(verts[0] - center, verts[1] - center);
            if (glm::dot(r, normal) < 0)
                std::reverse(verts.begin(), verts.end());

            for (size_t i = 0; i < verts.size(); ++i)
            {
                const auto &a = verts[i];
                const auto &b = verts[(i + 1) % verts.size()];
                verts_.push_back({center, normal});
                verts_.push_back({a, normal});
                verts_.push_back({b, normal});
            }
        }
    }

    void subdivide_triangle(int i0, int i1, int i2, int level)
    {
        if (level == max_subdivisions_)
        {
            if (dual_)
            {
                source_verts_[i0].adjacent_triangles.push_back({i0, i1, i2});
                source_verts_[i1].adjacent_triangles.push_back({i0, i1, i2});
                source_verts_[i2].adjacent_triangles.push_back({i0, i1, i2});
            }
            else
            {
                const auto v0 = source_verts_[i0].position;
                const auto v1 = source_verts_[i1].position;
                const auto v2 = source_verts_[i2].position;
                const auto vn = (1.0f / 3) * (v0 + v1 + v2);
                verts_.push_back({v0, vn});
                verts_.push_back({v1, vn});
                verts_.push_back({v2, vn});
            }
        }
        else
        {
            const auto v0 = source_verts_[i0].position;
            const auto v1 = source_verts_[i1].position;
            const auto v2 = source_verts_[i2].position;

            const auto i01 = maybe_add_vertex(glm::normalize(0.5f * (v0 + v1)));
            const auto i12 = maybe_add_vertex(glm::normalize(0.5f * (v1 + v2)));
            const auto i20 = maybe_add_vertex(glm::normalize(0.5f * (v2 + v0)));

            subdivide_triangle(i0, i01, i20, level + 1);
            subdivide_triangle(i01, i1, i12, level + 1);
            subdivide_triangle(i20, i12, i2, level + 1);
            subdivide_triangle(i01, i12, i20, level + 1);
        }
    }

    int maybe_add_vertex(const glm::vec3 &v)
    {
        auto it = vert_indices_.find(v);
        if (it != vert_indices_.end())
            return it->second;
        auto index = source_verts_.size();
        source_verts_.push_back({v, {}});
        vert_indices_.insert(it, {v, index});
        return index;
    }

    int max_subdivisions_;
    bool dual_;
    struct triangle
    {
        int i0, i1, i2;
    };
    struct vertex_triangles
    {
        glm::vec3 position;
        std::vector<triangle> adjacent_triangles;
    };
    std::vector<vertex_triangles> source_verts_;
    std::map<glm::vec3, std::size_t> vert_indices_;

    using gl_vertex = std::tuple<glm::vec3, glm::vec3>;
    std::vector<gl_vertex> verts_;
    geometry geometry_;
};

class demo
{
public:
    demo(int window_width, int window_height, int subdivisions, bool dual)
        : window_width_(window_width)
        , window_height_(window_height)
        , sphere_(new sphere_geometry(subdivisions, dual))
    {
        initialize_shader();
    }

    void render_and_step(float dt)
    {
        render(program_);
        cur_time_ += dt;
    }

private:
    void initialize_shader()
    {
        program_.add_shader(GL_VERTEX_SHADER, "shaders/sphere.vert");
        program_.add_shader(GL_FRAGMENT_SHADER, "shaders/sphere.frag");
        program_.link();
    }

    void render(const shader_program &program) const
    {
        glViewport(0, 0, window_width_, window_height_);
        glClearColor(0.5, 0.5, 0.5, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_MULTISAMPLE);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        const auto projection =
            glm::perspective(glm::radians(45.0f), static_cast<float>(window_width_) / window_height_, 0.1f, 100.f);
        const auto view_pos = glm::vec3(0, 0, 3);
        const auto view_up = glm::vec3(0, 1, 0);
        const auto view = glm::lookAt(view_pos, glm::vec3(0, 0, 0), view_up);

        const auto angle = 0.5f * sinf(cur_time_);
        const auto model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1, 0));
        const auto mvp = projection * view * model;

        glm::mat3 model_normal(model);
        model_normal = glm::inverse(model_normal);
        model_normal = glm::transpose(model_normal);

        program.bind();
        program.set_uniform(program.uniform_location("mvp"), mvp);
        program.set_uniform(program.uniform_location("normalMatrix"), model_normal);
        program.set_uniform(program.uniform_location("modelMatrix"), model * view);
        program.set_uniform(program.uniform_location("eyePosition"), view_pos);

        sphere_->render();
    }

    int window_width_;
    int window_height_;
    float cur_time_ = 0;
    shader_program program_;
    std::unique_ptr<sphere_geometry> sphere_;
};

void usage(const char *argv0)
{
    std::cerr << "Usage: " << argv0 << " [-w width] [-h height] [-s subdivisions]\n";
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    int window_width = 512;
    int window_height = 512;
    int subdivisions = 3;
    bool dual = true;
    bool dump_frames = false;

    int opt;
    while ((opt = getopt(argc, argv, "w:h:s:td")) != -1)
    {
        switch (opt)
        {
        case 'w':
            window_width = std::stoi(optarg);
            break;
        case 'h':
            window_height = std::stoi(optarg);
            break;
        case 's':
            subdivisions = std::stoi(optarg);
            break;
        case 't':
            dual = false;
            break;
        case 'd':
            dump_frames = true;
            break;
        default:
            usage(argv[0]);
            break;
        }
    }

    if (!glfwInit())
        panic("glfwInit failed\n");

    glfwSetErrorCallback([](int error, const char *description) { panic("GLFW error: %s\n", description); });

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 16);
    auto *window = glfwCreateWindow(window_width, window_height, "demo", nullptr, nullptr);
    if (!window)
        panic("glfwCreateWindow failed\n");

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewInit();

    glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mode) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GL_TRUE);
    });

    constexpr auto total_frames = 3 * 40;
    auto frame_num = 0;

    {
        demo d(window_width, window_height, subdivisions, dual);

        while (!glfwWindowShouldClose(window))
        {
            const auto dt = dump_frames ? 1.0f / 40 : 1.0f / 60;
            d.render_and_step(dt);

            if (dump_frames)
            {
                char path[80];
                std::sprintf(path, "%05d.ppm", frame_num);
                dump_frame_to_file(path, window_width, window_height);
                if (++frame_num == total_frames)
                    break;
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
