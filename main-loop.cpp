#include "main-loop.h"

#include "imgui.h"
#include "shader.h"
#include "textures/block-texture.h"

#include "ccVector.h"

#include <GL/gl3w.h>
#include <SDL.h>
#include <sys/time.h>
#include <unistd.h>

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

const int VERTEX_POSITION_ATTRIBUTE = 0;
const int VERTEX_COLOUR_ATTRIBUTE = 1;
const int VERTEX_UV_ATTRIBUTE = 2;


uint64_t
get_us()
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  return ((uint64_t)tv.tv_sec * (uint64_t)1000000) + (uint64_t)tv.tv_usec;
}


bool
sleep_us(int us)
{
  int error = usleep(us);
  return error;
}


void
main_loop(GameState *game_state)
{
  if (!game_state->init)
  {
    // Initialise GameState
    //

    game_state->game_start_time = get_us();
    game_state->fps = 60;
    game_state->fov = 45.0f;
    game_state->terrain_rotation = {};
    game_state->sine_offset_type = SineOffsetType::Concentric;
    game_state->bounces_per_second = 1;
    game_state->oscillation_frequency = 1;
    game_state->bounce_height = 1;
    game_state->camera_velocity = {};
    game_state->camera_position = {15.0f,10.0f,15.0f};
    game_state->camera_direction_velocity = {};
    game_state->camera_direction = { (float)atan2(game_state->camera_position.y, game_state->camera_position.z),
                                    -(float)atan2(game_state->camera_position.x, game_state->camera_position.z), 0.0f};

    game_state->colour_picker_n = 0;
    game_state->colours[0] = {0.15f, 0.7f, 0.1f, 1};
    game_state->colours[1] = {0.2f, 0.1f, 0.1f, 1};

    game_state->init = true;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);

    GLuint vertex_array_id;
    glGenVertexArrays(1, &vertex_array_id);
    glBindVertexArray(vertex_array_id);

    // Initialise OpenGL shaders
    //

    game_state->program_id = LoadShaders( "TransformVertexShader.vertexshader", "ColorFragmentShader.fragmentshader" );

    game_state->mvp_matrix_id = glGetUniformLocation(game_state->program_id, "MVP");
    game_state->model_matrix_id = glGetUniformLocation(game_state->program_id, "M");

    // Initialise OpenGL buffers
    //

    const vec3 vertex_buffer_data[] = {
      { 1.0f,  1.0f,  1.0f},
      { 1.0f,  1.0f, -1.0f},
      {-1.0f,  1.0f, -1.0f},
      {-1.0f,  1.0f,  1.0f},

      { 1.0f, -1.0f,  1.0f},
      { 1.0f, -1.0f, -1.0f},
      {-1.0f, -1.0f, -1.0f},
      {-1.0f, -1.0f,  1.0f}
    };

    const GLubyte index_buffer_data[] = {
      // Top
      0, 1, 2,
      0, 2, 3,

      0, 4, 1,
      5, 1, 4,

      1, 5, 2,
      5, 6, 2,

      2, 6, 3,
      6, 7, 3,

      3, 7, 4,
      3, 4, 0,

      // Bottom
      7, 5, 4,
      5, 7, 6
    };

    const vec2 uv_buffer_data[] = {
      0,   0,
      0.5, 0,
      0.5, 1,
      0,   1,

      0.5, 0,
      1,   0,
      1,   1,
      0.5, 1,
    };

    game_state->n_indices = ARRAY_COUNT(index_buffer_data);

    glGenBuffers(1, &game_state->index_buffer);
    glGenBuffers(1, &game_state->vertex_buffer);
    glGenBuffers(1, &game_state->color_buffer);
    glGenBuffers(1, &game_state->uv_buffer);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, game_state->index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, game_state->vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, game_state->uv_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), uv_buffer_data, GL_STATIC_DRAW);

    // Initialise OpenGL textures
    //

    glGenTextures(1, &game_state->grass_texture_id);
    glBindTexture(GL_TEXTURE_2D, game_state->grass_texture_id);

    assert(BLOCK_TEXTURE.bytes_per_pixel == 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, BLOCK_TEXTURE.width, BLOCK_TEXTURE.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, BLOCK_TEXTURE.pixel_data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  uint64_t frame_start_time = get_us();
  unsigned int frame_time = frame_start_time - game_state->game_start_time;

  // User inputs
  //

  ImGuiIO& io = ImGui::GetIO();

  vec3 camera_rotation_acceleration = {};
  if (ImGui::IsKeyDown(SDL_SCANCODE_RIGHT))
  {
    camera_rotation_acceleration.y += 1;
  }
  if (ImGui::IsKeyDown(SDL_SCANCODE_LEFT))
  {
    camera_rotation_acceleration.y -= 1;
  }
  if (ImGui::IsKeyDown(SDL_SCANCODE_END))
  {
    camera_rotation_acceleration.x += 1;
  }
  if (ImGui::IsKeyDown(SDL_SCANCODE_HOME))
  {
    camera_rotation_acceleration.x -= 1;
  }

  vec4 camera_acceleration = {0, 0, 0, 1};
  if (ImGui::IsKeyDown(SDL_SCANCODE_INSERT))
  {
    camera_acceleration.x += 1;
  }
  if (ImGui::IsKeyDown(SDL_SCANCODE_PAGEUP))
  {
    camera_acceleration.x -= 1;
  }
  if (ImGui::IsKeyDown(SDL_SCANCODE_UP))
  {
    camera_acceleration.z += 1;
  }
  if (ImGui::IsKeyDown(SDL_SCANCODE_DOWN))
  {
    camera_acceleration.z -= 1;
  }
  if (ImGui::IsKeyDown(SDL_SCANCODE_RCTRL))
  {
    camera_acceleration.y += 1;
  }
  if (ImGui::IsKeyDown(SDL_SCANCODE_RSHIFT))
  {
    camera_acceleration.y -= 1;
  }

  vec2 mouse_pos = ImGui::GetMousePos();
  if ((mouse_pos.x < 0 || mouse_pos.x >= io.DisplaySize.x) ||
      (mouse_pos.y < 0 || mouse_pos.y >= io.DisplaySize.y) ||
      io.WantCaptureMouse)
  {
    mouse_pos = game_state->last_frame_mouse;
  }

  if (!io.WantCaptureMouse)
  {
    if (ImGui::IsMouseDragging())
    {
      vec2 mouse_drag_delta = vec2Subtract(mouse_pos, game_state->last_frame_mouse);
      game_state->camera_direction.y += -(mouse_drag_delta.x / io.DisplaySize.x);
      game_state->camera_direction.x += -(mouse_drag_delta.y / io.DisplaySize.y);
    }

    camera_acceleration.z += io.MouseWheel;
  }

  // Update camera direction
  //

  camera_rotation_acceleration = vec3Multiply(camera_rotation_acceleration, 0.001 * 2.0*M_PI);
  camera_acceleration = vec4Multiply(camera_acceleration, -0.2);

  game_state->camera_direction_velocity = vec3Add(game_state->camera_direction_velocity, camera_rotation_acceleration);
  game_state->camera_direction = vec3Add(game_state->camera_direction, game_state->camera_direction_velocity);
  game_state->camera_direction_velocity = vec3Multiply(game_state->camera_direction_velocity, 0.8);

  // Update camera position
  //

  mat4x4 camera_orientation;
  mat4x4Identity(camera_orientation);
  mat4x4RotateZ(camera_orientation, -game_state->camera_direction.z);
  mat4x4RotateX(camera_orientation, -game_state->camera_direction.x);
  mat4x4RotateY(camera_orientation, -game_state->camera_direction.y);

  vec3 camera_world_acceleration = mat4x4MultiplyVector(camera_orientation, camera_acceleration).xyz;

  game_state->camera_velocity = vec3Add(game_state->camera_velocity, camera_world_acceleration);
  game_state->camera_position = vec3Add(game_state->camera_position, game_state->camera_velocity);
  game_state->camera_velocity = vec3Multiply(game_state->camera_velocity, 0.8);

  // ImGui window
  //

  if (ImGui::Begin("Render parameters"))
  {
    ImGui::DragInt("FPS", &game_state->fps, 1, 1, 120);

    ImGui::DragFloat("FOV", &game_state->fov, 1, 1, 180);
    ImGui::DragFloat3("Camera position", (float *)&game_state->camera_position.v);

    vec3 camera_direction_deg = vec3Multiply(game_state->camera_direction, 180.0/M_PI);
    if (ImGui::DragFloat3("Camera direction", (float *)&camera_direction_deg.v))
    {
      game_state->camera_direction = vec3Multiply(camera_direction_deg, M_PI/180.0);
    }

    ImGui::DragFloat3("Terrain Rotation", (float *)&game_state->terrain_rotation, 1, -360, 360);

    ImGui::Combo("Sine Offset Type", (int*)&game_state->sine_offset_type, "Diagonal\0Concentric\0\0");
    ImGui::DragFloat("Bounces Per Second", &game_state->bounces_per_second, 0.01, 0, 10);
    ImGui::DragFloat("Oscillation Frequency", &game_state->oscillation_frequency, 0.01, 0, 10);
    ImGui::DragFloat("Bounce Height", &game_state->bounce_height, 0.1, 0, 100);

    for (int colour_n = 0; colour_n < 2; ++colour_n)
    {
      ImGui::PushID(colour_n);
      vec4 c = {game_state->colours[colour_n].x, game_state->colours[colour_n].y, game_state->colours[colour_n].z, 0.0f};
      bool pushed = ImGui::ColorButton("##change colour", c);
      ImGui::PopID();

      if (pushed) {
        game_state->colour_picker_n = colour_n;
        ImGui::OpenPopup("Change Colour");
      }
      ImGui::SameLine();
      ImGui::Text("Side %d", colour_n + 1);
    }

    if (ImGui::BeginPopup("Change Colour"))
    {
      ImGui::ColorPicker3("##picker", (float*)&game_state->colours[game_state->colour_picker_n]);
      ImGui::EndPopup();
    }
  }

  ImGui::End();

  // Create projection, model, view matrices
  //

  mat4x4 projection;
  mat4x4Perspective(projection, (game_state->fov*(M_PI/180.0f)), io.DisplaySize.x / io.DisplaySize.y, 0.1f, 100.0f);

  mat4x4 model;
  mat4x4Identity(model);
  mat4x4RotateY(model, game_state->terrain_rotation.y*(M_PI/180.0f));
  mat4x4RotateX(model, game_state->terrain_rotation.x*(M_PI/180.0f));
  mat4x4RotateZ(model, game_state->terrain_rotation.z*(M_PI/180.0f));

  mat4x4 view;
  mat4x4Identity(view);
  mat4x4Translate(view, vec3Multiply(game_state->camera_position, -1));
  mat4x4RotateY(view, game_state->camera_direction.y);
  mat4x4RotateX(view, game_state->camera_direction.x);
  mat4x4RotateZ(view, game_state->camera_direction.z);

  mat4x4 view_projection;
  mat4x4MultiplyMatrix(view_projection, view, projection);
  mat4x4 model_view_projection;
  mat4x4MultiplyMatrix(model_view_projection, model, view_projection);

  // Upload colours to colour buffer
  //

  // Duplicate the two colours, one colour per vertex
  int n_colours = 8;
  vec4 color_buffer_data[n_colours];
  for (int colour_n = 0; colour_n < n_colours; ++colour_n)
  {
    color_buffer_data[colour_n] = game_state->colours[int(colour_n*(2.0/n_colours))];
  }

  glBindBuffer(GL_ARRAY_BUFFER, game_state->color_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(color_buffer_data), color_buffer_data, GL_STATIC_DRAW);

  // Initialise buffer attributes
  //

  glEnableVertexAttribArray(VERTEX_POSITION_ATTRIBUTE);
  glBindBuffer(GL_ARRAY_BUFFER, game_state->vertex_buffer);
  glVertexAttribPointer(
    VERTEX_POSITION_ATTRIBUTE,
    3,         // size
    GL_FLOAT,  // type
    GL_FALSE,  // normalized?
    0,         // stride
    (void*)0   // array buffer offset
  );

  glEnableVertexAttribArray(VERTEX_COLOUR_ATTRIBUTE);
  glBindBuffer(GL_ARRAY_BUFFER, game_state->color_buffer);
  glVertexAttribPointer(
    VERTEX_COLOUR_ATTRIBUTE,
    4,         // size
    GL_FLOAT,  // type
    GL_FALSE,  // normalized?
    0,         // stride
    (void*)0   // array buffer offset
  );

  glEnableVertexAttribArray(VERTEX_UV_ATTRIBUTE);
  glBindBuffer(GL_ARRAY_BUFFER, game_state->uv_buffer);
  glVertexAttribPointer(
    VERTEX_UV_ATTRIBUTE,
    2,         // size
    GL_FLOAT,  // type
    GL_FALSE,  // normalized?
    0,         // stride
    (void*)0   // array buffer offset
  );

  // Render
  //

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(game_state->program_id);

  glUniformMatrix4fv(game_state->mvp_matrix_id, 1, GL_FALSE, &model_view_projection[0][0]);

  glBindTexture(GL_TEXTURE_2D, game_state->grass_texture_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, game_state->index_buffer);

  float bounces_per_us = game_state->bounces_per_second / 1000000.0;

  vec2 terrain_dim = {20, 20};
  vec2 translation;
  for (translation.x = -terrain_dim.x*0.5;
       translation.x <= terrain_dim.x*0.5;
       ++translation.x)
  for (translation.y = -terrain_dim.y*0.5;
       translation.y <= terrain_dim.y*0.5;
       ++translation.y)
  {
    mat4x4 cube;
    mat4x4Identity(cube);
    mat4x4Scale(cube, 0.5);
    mat4x4Translate(cube, {translation.x, 0, translation.y});

    float sine_offset = 0;
    switch (game_state->sine_offset_type)
    {
      case (SineOffsetType::Diagonal):
      {
        sine_offset = (translation.x/terrain_dim.x + translation.y/terrain_dim.y) * game_state->oscillation_frequency*2*M_PI;
      } break;
      case (SineOffsetType::Concentric):
      {
        sine_offset = vec2Length(translation) / (0.5 * vec2Length(terrain_dim)) * game_state->oscillation_frequency*2*M_PI;
      } break;
    }

    float offset = sin(frame_time * bounces_per_us * 2*M_PI + sine_offset) * game_state->bounce_height;

    mat4x4Translate(cube, {0, offset, 0});
    glUniformMatrix4fv(game_state->model_matrix_id, 1, GL_FALSE, &cube[0][0]);
    glDrawElements(GL_TRIANGLES, game_state->n_indices, GL_UNSIGNED_BYTE, 0);
  }

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  // Lock frame-rate
  //

  game_state->last_frame_mouse = mouse_pos;

  float frame_delta_us = 1000000.0/game_state->fps;

  uint64_t frame_end_time = get_us();
  float this_frame_delta = (float)(frame_end_time - frame_start_time);
  float frame_delta_left = frame_delta_us - this_frame_delta;

  if (frame_delta_left)
  {
    usleep(frame_delta_left);
  }
}


void
shutdown(GameState *game_state)
{
  // Cleanup VBO and shader
  glDeleteBuffers(1, &game_state->vertex_buffer);
  glDeleteBuffers(1, &game_state->index_buffer);
  glDeleteBuffers(1, &game_state->color_buffer);
}