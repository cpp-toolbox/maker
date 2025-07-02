#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/fwd.hpp>

// REMOVE this one day.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "graphics/draw_info/draw_info.hpp"
#include "graphics/input_graphics_sound_menu/input_graphics_sound_menu.hpp"

#include "input/glfw_lambda_callback_manager/glfw_lambda_callback_manager.hpp"
#include "input/input_state/input_state.hpp"

#include "utility/fixed_frequency_loop/fixed_frequency_loop.hpp"

#include "graphics/ui_render_suite_implementation/ui_render_suite_implementation.hpp"
#include "graphics/input_graphics_sound_menu/input_graphics_sound_menu.hpp"
#include "graphics/vertex_geometry/vertex_geometry.hpp"
#include "graphics/shader_standard/shader_standard.hpp"
#include "graphics/batcher/generated/batcher.hpp"
#include "graphics/shader_cache/shader_cache.hpp"
#include "graphics/fps_camera/fps_camera.hpp"
#include "graphics/window/window.hpp"
#include "graphics/colors/colors.hpp"
#include "graphics/ui/ui.hpp"

#include "system_logic/toolbox_engine/toolbox_engine.hpp"

#include "utility/unique_id_generator/unique_id_generator.hpp"
#include "utility/logger/logger.hpp"

#include <iostream>

glm::vec2 get_ndc_mouse_pos1(GLFWwindow *window, double xpos, double ypos) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    return {(2.0f * xpos) / width - 1.0f, 1.0f - (2.0f * ypos) / height};
}

glm::vec2 aspect_corrected_ndc_mouse_pos1(const glm::vec2 &ndc_mouse_pos, float x_scale) {
    return {ndc_mouse_pos.x * x_scale, ndc_mouse_pos.y};
}

class Hud3D {
  private:
    Batcher &batcher;
    InputState &input_state;
    Configuration &configuration;
    FPSCamera &fps_camera;
    UIRenderSuiteImpl &ui_render_suite;
    Window &window;

    const glm::vec3 crosshair_color = colors::green;

    const std::string crosshair = R"(
--*--
--*--
*****
--*--
--*--
)";

    vertex_geometry::Rectangle crosshair_rect = vertex_geometry::Rectangle(glm::vec3(0), 0.1, 0.1);

    draw_info::IVPSolidColor crosshair_ivpsc;

    int crosshair_batcher_object_id;

  public:
    Hud3D(Configuration &configuration, InputState &input_state, Batcher &batcher, FPSCamera &fps_camera,
          UIRenderSuiteImpl &ui_render_suite, Window &window)
        : batcher(batcher), input_state(input_state), configuration(configuration), fps_camera(fps_camera),
          ui_render_suite(ui_render_suite), window(window), ui(create_ui()) {}

    ConsoleLogger logger;

    UI ui;
    int fps_ui_element_id, pos_ui_element_id;
    float average_fps;

    UI create_ui() {
        UI hud_ui(0, batcher.absolute_position_with_colored_vertex_shader_batcher.object_id_generator);
        fps_ui_element_id = hud_ui.add_textbox(
            "FPS", vertex_geometry::create_rectangle_from_top_right(glm::vec3(1, 1, 0), 0.2, 0.2), colors::black);
        pos_ui_element_id = hud_ui.add_textbox(
            "POS", vertex_geometry::create_rectangle_from_bottom_left(glm::vec3(-1, -1, 0), 0.8, 0.4), colors::black);

        crosshair_batcher_object_id =
            batcher.absolute_position_with_colored_vertex_shader_batcher.object_id_generator.get_id();
        auto crosshair_ivp = vertex_geometry::text_grid_to_rect_grid(crosshair, crosshair_rect);
        std::vector<glm::vec3> cs(crosshair_ivp.xyz_positions.size(), crosshair_color);
        crosshair_ivpsc = draw_info::IVPSolidColor(crosshair_ivp, cs, crosshair_batcher_object_id);

        return hud_ui;
    }

    void process_and_queue_render_hud_ui_elements() {

        if (configuration.get_value("graphics", "show_pos").value_or("off") == "on") {
            ui.unhide_textbox(pos_ui_element_id);
            ui.modify_text_of_a_textbox(pos_ui_element_id, vec3_to_string(fps_camera.transform.get_translation()));
        } else {
            ui.hide_textbox(pos_ui_element_id);
        }

        if (configuration.get_value("graphics", "show_fps").value_or("off") == "on") {
            std::ostringstream fps_stream;
            fps_stream << std::fixed << std::setprecision(1) << average_fps;
            ui.modify_text_of_a_textbox(fps_ui_element_id, fps_stream.str());
            ui.unhide_textbox(fps_ui_element_id);
        } else {
            ui.hide_textbox(fps_ui_element_id);
        }

        auto ndc_mouse_pos =
            get_ndc_mouse_pos1(window.glfw_window, input_state.mouse_position_x, input_state.mouse_position_y);
        auto acnmp = aspect_corrected_ndc_mouse_pos1(ndc_mouse_pos, window.width_px / (float)window.height_px);

        batcher.absolute_position_with_colored_vertex_shader_batcher.queue_draw(
            crosshair_batcher_object_id, crosshair_ivpsc.indices, crosshair_ivpsc.xyz_positions,
            crosshair_ivpsc.rgb_colors);

        process_and_queue_render_ui(acnmp, ui, ui_render_suite, input_state.get_just_pressed_key_strings(),
                                    input_state.is_just_pressed(EKey::BACKSPACE),
                                    input_state.is_just_pressed(EKey::ENTER),
                                    input_state.is_just_pressed(EKey::LEFT_MOUSE_BUTTON));
    }
};

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <random>

// Your Light struct
struct Light {
    glm::vec3 position;
    glm::vec3 color;
};

// Utility: Random float in [min, max]
float rand_float(float min, float max) {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

// Generate NR_LIGHTS random lights
std::vector<Light> generate_random_lights(int count) {
    std::vector<Light> lights(count);

    for (int i = 0; i < count; ++i) {
        // Position in a cube from [-10, 10] on each axis
        glm::vec3 pos = glm::vec3(rand_float(-10.0f, 10.0f), rand_float(0.0f, 5.0f), rand_float(-10.0f, 10.0f));

        // Bright color, each component in [0.5, 1.0]
        glm::vec3 color = glm::vec3(rand_float(0.5f, 1.0f), rand_float(0.5f, 1.0f), rand_float(0.5f, 1.0f));

        lights[i] = Light{pos, color};
    }

    return lights;
}

void upload_lights(GLuint shader_program, const std::vector<Light> &lights) {
    for (int i = 0; i < lights.size(); ++i) {
        std::string index_str = std::to_string(i);

        std::string pos_name = "lights[" + index_str + "].position";
        std::string color_name = "lights[" + index_str + "].color";

        GLint pos_loc = glGetUniformLocation(shader_program, pos_name.c_str());
        GLint color_loc = glGetUniformLocation(shader_program, color_name.c_str());

        if (pos_loc != -1)
            glUniform3fv(pos_loc, 1, glm::value_ptr(lights[i].position));

        if (color_loc != -1)
            glUniform3fv(color_loc, 1, glm::value_ptr(lights[i].color));
    }
}

int main() {

    ToolboxEngine tbx_engine(
        "fps camera with geom",
        {ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX_DEFERED_LIGHTING_FRAMEBUFFERS,
         ShaderType::DEFERRED_LIGHTING, ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX,
         ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX});

    auto light_geom_for_copying = vertex_geometry::generate_cube(0.2);

    const int MAX_LIGHTS = 32;
    const int num_lights = 2;

    std::vector<Light> lights = generate_random_lights(num_lights);
    std::vector<draw_info::IVPNColored> light_meshes;

    for (const auto &light : lights) {
        if (light.position != glm::vec3(0)) {
            auto light_geom = light_geom_for_copying;
            light_geom.transform.set_translation(light.position);

            light_geom.id =
                tbx_engine.batcher.absolute_position_with_colored_vertex_shader_batcher.object_id_generator.get_id();
            std::vector<glm::vec3> colors(light_geom.xyz_positions.size(), light.color);
            draw_info::IVPNColored colored_light(light_geom, colors);
            light_meshes.push_back(colored_light);
        }
    }

    // Fill remaining with "empty" lights
    lights.resize(MAX_LIGHTS, Light{glm::vec3(0.0f), glm::vec3(0.0f)});

    tbx_engine.fps_camera.fov.add_observer([&](const float &new_value) {
        tbx_engine.shader_cache.set_uniform(
            ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX, ShaderUniformVariable::CAMERA_TO_CLIP,
            tbx_engine.fps_camera.get_projection_matrix(tbx_engine.window.width_px, tbx_engine.window.height_px));
    });

    tbx_engine::register_input_graphics_sound_config_handlers(tbx_engine.configuration, tbx_engine.fps_camera,
                                                              tbx_engine.main_loop);

    UIRenderSuiteImpl ui_render_suite(tbx_engine.batcher);
    Hud3D hud(tbx_engine.configuration, tbx_engine.input_state, tbx_engine.batcher, tbx_engine.fps_camera,
              ui_render_suite, tbx_engine.window);
    InputGraphicsSoundMenu input_graphics_sound_menu(tbx_engine.window, tbx_engine.input_state, tbx_engine.batcher,
                                                     tbx_engine.sound_system, tbx_engine.configuration);

    GLFWLambdaCallbackManager glcm = tbx_engine::create_default_glcm_for_input_and_camera(
        tbx_engine.input_state, tbx_engine.fps_camera, tbx_engine.window, tbx_engine.shader_cache);

    // TODO: make a batcher function to create a tig.
    auto torus = vertex_geometry::generate_torus();
    torus.id = tbx_engine.batcher
                   .cwl_v_transformation_ubos_1024_with_colored_vertex_defered_lighting_framebuffers_shader_batcher
                   .object_id_generator.get_id();
    auto floor = vertex_geometry::generate_box(10, 0.1, 10);
    floor.transform.set_translation(0, -1, 0);
    floor.id = tbx_engine.batcher
                   .cwl_v_transformation_ubos_1024_with_colored_vertex_defered_lighting_framebuffers_shader_batcher
                   .object_id_generator.get_id();

    auto cone = vertex_geometry::generate_cone();
    cone.transform.set_translation(0, 1, 0);
    cone.id = tbx_engine.batcher
                  .cwl_v_transformation_ubos_1024_with_colored_vertex_defered_lighting_framebuffers_shader_batcher
                  .object_id_generator.get_id();

    auto ball = vertex_geometry::generate_icosphere(2, 0.5);
    ball.transform.set_translation(1, 0, 2);
    ball.id = tbx_engine.batcher
                  .cwl_v_transformation_ubos_1024_with_colored_vertex_defered_lighting_framebuffers_shader_batcher
                  .object_id_generator.get_id();

    auto cyl = vertex_geometry::generate_cylinder();
    cyl.transform.set_translation(-1, 1, -1);
    cyl.id = tbx_engine.batcher
                 .cwl_v_transformation_ubos_1024_with_colored_vertex_defered_lighting_framebuffers_shader_batcher
                 .object_id_generator.get_id();

    std::vector<draw_info::IVPNColored> objects = {{torus, colors::blue},
                                                   {floor, colors::grey},
                                                   {cone, colors::green},
                                                   {ball, colors::red},
                                                   {cyl, colors::orange}};

    Transform ball_transform;

    auto deferred_lighting_screen = vertex_geometry::Rectangle().get_ivs();

    glm::mat4 identity = glm::mat4(1);
    tbx_engine.shader_cache.set_uniform(
        ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX, ShaderUniformVariable::CAMERA_TO_CLIP,
        tbx_engine.fps_camera.get_projection_matrix(tbx_engine.window.width_px, tbx_engine.window.height_px));

    tbx_engine.shader_cache.set_uniform(ShaderType::DEFERRED_LIGHTING, ShaderUniformVariable::ASPECT_RATIO,
                                        glm::vec2(1, 1));

    upload_lights(tbx_engine.shader_cache.get_shader_program(ShaderType::DEFERRED_LIGHTING).id, lights);

    tbx_engine.shader_cache.set_uniform(
        ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX_DEFERED_LIGHTING_FRAMEBUFFERS,
        ShaderUniformVariable::CAMERA_TO_CLIP,
        tbx_engine.fps_camera.get_projection_matrix(tbx_engine.window.width_px, tbx_engine.window.height_px));

    tbx_engine.shader_cache.set_uniform(ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX,
                                        ShaderUniformVariable::ASPECT_RATIO,
                                        glm::vec2(tbx_engine.window.height_px / (float)tbx_engine.window.width_px, 1));

    unsigned int lighting_frame_buffer;
    glGenFramebuffers(1, &lighting_frame_buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, lighting_frame_buffer);

    unsigned int position_texture, normal_texture, color_texture;

    glGenTextures(1, &position_texture);
    glBindTexture(GL_TEXTURE_2D, position_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, tbx_engine.window.width_px, tbx_engine.window.height_px, 0, GL_RGBA,
                 GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, position_texture, 0);

    glGenTextures(1, &normal_texture);
    glBindTexture(GL_TEXTURE_2D, normal_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, tbx_engine.window.width_px, tbx_engine.window.height_px, 0, GL_RGBA,
                 GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normal_texture, 0);

    glGenTextures(1, &color_texture);
    glBindTexture(GL_TEXTURE_2D, color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tbx_engine.window.width_px, tbx_engine.window.height_px, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, color_texture, 0);

    // - tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    unsigned int attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);

    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, tbx_engine.window.width_px, tbx_engine.window.height_px);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    tbx_engine.shader_cache.set_uniform(ShaderType::DEFERRED_LIGHTING, ShaderUniformVariable::POSITION_TEXTURE, 0);
    tbx_engine.shader_cache.set_uniform(ShaderType::DEFERRED_LIGHTING, ShaderUniformVariable::NORMAL_TEXTURE, 1);
    tbx_engine.shader_cache.set_uniform(ShaderType::DEFERRED_LIGHTING, ShaderUniformVariable::COLOR_TEXTURE, 2);

    // RateLimitedConsoleLogger tick_logger(1);
    ConsoleLogger tick_logger;
    tick_logger.disable_all_levels();
    std::function<void(double)> tick = [&](double dt) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        tbx_engine.shader_cache.set_uniform(
            ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX, ShaderUniformVariable::CAMERA_TO_CLIP,
            tbx_engine.fps_camera.get_projection_matrix(tbx_engine.window.width_px, tbx_engine.window.height_px));

        tbx_engine.shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX,
                                            ShaderUniformVariable::WORLD_TO_CAMERA,
                                            tbx_engine.fps_camera.get_view_matrix());

        tbx_engine.shader_cache.set_uniform(
            ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX_DEFERED_LIGHTING_FRAMEBUFFERS,
            ShaderUniformVariable::CAMERA_TO_CLIP,
            tbx_engine.fps_camera.get_projection_matrix(tbx_engine.window.width_px, tbx_engine.window.height_px));

        tbx_engine.shader_cache.set_uniform(
            ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX_DEFERED_LIGHTING_FRAMEBUFFERS,
            ShaderUniformVariable::WORLD_TO_CAMERA, tbx_engine.fps_camera.get_view_matrix());

        glBindFramebuffer(GL_FRAMEBUFFER, lighting_frame_buffer);
        {

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            for (auto &o : objects) {
                std::vector<unsigned int> ltw_indices(o.xyz_positions.size(), o.id);

                tbx_engine.batcher
                    .cwl_v_transformation_ubos_1024_with_colored_vertex_defered_lighting_framebuffers_shader_batcher
                    .queue_draw(o.id, o.indices, o.xyz_positions, o.colors, o.normals, ltw_indices);

                tbx_engine.batcher
                    .cwl_v_transformation_ubos_1024_with_colored_vertex_defered_lighting_framebuffers_shader_batcher
                    .ltw_matrices[o.id] = o.transform.get_transform_matrix();
            }

            tbx_engine.batcher
                .cwl_v_transformation_ubos_1024_with_colored_vertex_defered_lighting_framebuffers_shader_batcher
                .upload_ltw_matrices();
            tbx_engine.batcher
                .cwl_v_transformation_ubos_1024_with_colored_vertex_defered_lighting_framebuffers_shader_batcher
                .draw_everything();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, position_texture);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, normal_texture);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, color_texture);

            tbx_engine.shader_cache.set_uniform(ShaderType::DEFERRED_LIGHTING, ShaderUniformVariable::CAMERA_POSITION,
                                                tbx_engine.fps_camera.transform.get_translation());

            tbx_engine.batcher.deferred_lighting_shader_batcher.queue_draw(
                0, deferred_lighting_screen.indices, deferred_lighting_screen.vertices,
                vertex_geometry::generate_rectangle_texture_coordinates_flipped_vertically());

            tbx_engine.batcher.deferred_lighting_shader_batcher.draw_everything();
        }

        // 2.5. copy content of geometry's depth buffer to default framebuffer's depth buffer
        // ----------------------------------------------------------------------------------
        glBindFramebuffer(GL_READ_FRAMEBUFFER, lighting_frame_buffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
        // blit to default framebuffer. Note that this may or may not work as the internal formats of both the FBO and
        // default framebuffer have to match. the internal formats are implementation defined. This works on all of my
        // systems, but if it doesn't on yours you'll likely have to write to the depth buffer in another shader stage
        // (or somehow see to match the default framebuffer's internal format with the FBO's internal format).
        glBlitFramebuffer(0, 0, tbx_engine.window.width_px, tbx_engine.window.height_px, 0, 0,
                          tbx_engine.window.width_px, tbx_engine.window.height_px, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        for (auto &lm : light_meshes) {
            // exists.
            std::vector<unsigned int> ltw_indices(lm.xyz_positions.size(), lm.id);
            tbx_engine.batcher.cwl_v_transformation_ubos_1024_with_colored_vertex_shader_batcher.queue_draw(
                lm.id, lm.indices, lm.xyz_positions, lm.colors, ltw_indices);
            tbx_engine.batcher.cwl_v_transformation_ubos_1024_with_colored_vertex_shader_batcher.ltw_matrices[lm.id] =
                lm.transform.get_transform_matrix();
        }

        tbx_engine::potentially_switch_between_menu_and_3d_view(tbx_engine.input_state, input_graphics_sound_menu,
                                                                tbx_engine.fps_camera, tbx_engine.window);

        hud.process_and_queue_render_hud_ui_elements();

        if (input_graphics_sound_menu.enabled) {
            input_graphics_sound_menu.process_and_queue_render_menu(tbx_engine.window, tbx_engine.input_state,
                                                                    ui_render_suite);
        } else {
            tbx_engine::config_x_input_state_x_fps_camera_processing(tbx_engine.fps_camera, tbx_engine.input_state,
                                                                     tbx_engine.configuration, dt);
        }

        tbx_engine.batcher.cwl_v_transformation_ubos_1024_with_colored_vertex_shader_batcher.upload_ltw_matrices();
        tbx_engine.batcher.cwl_v_transformation_ubos_1024_with_colored_vertex_shader_batcher.draw_everything();

        tbx_engine.batcher.absolute_position_with_colored_vertex_shader_batcher.draw_everything();

        tbx_engine.sound_system.play_all_sounds();

        glfwSwapBuffers(tbx_engine.window.glfw_window);
        glfwPollEvents();

        // tick_logger.tick();

        TemporalBinarySignal::process_all();
    };

    std::function<bool()> termination = [&]() { return glfwWindowShouldClose(tbx_engine.window.glfw_window); };

    std::function<void(IterationStats)> loop_stats_function = [&](IterationStats is) {
        hud.average_fps = is.measured_frequency_hz;
    };

    tbx_engine.main_loop.start(tick, termination, loop_stats_function);

    return 0;
}
