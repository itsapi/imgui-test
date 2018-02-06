#include "main-loop.h"

#include "imgui.h"
#include "shader.h"
#include <GL/gl3w.h>
#include <SDL.h>
#include "ccVector.h"
#include <sys/time.h>
#include <unistd.h>


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
  static const int n_indices = 36;

  static vec3 colours[] = {
    {0.15f, 0.7f, 0.1f},
    {0.2f, 0.1f, 0.1f}
  };

  if (!game_state->init)
  {
    game_state->game_start_time = get_us();
    game_state->fps = 60;
    game_state->fov = 45.0f;
    game_state->rotate_x_deg = 0;
    game_state->rotate_y_deg = 0;
    game_state->rotate_z_deg = 0;
    game_state->sine_offset_type = SineOffsetType::Concentric;
    game_state->bounces_per_second = 1;
    game_state->oscillation_frequency = 1;
    game_state->bounce_height = 1;
    game_state->camera_velocity = {};
    game_state->camera_position = {15.0f,10.0f,15.0f};
    game_state->camera_direction_velocity = {};
    game_state->camera_direction = {};
    game_state->colour_picker_n = 0;

    game_state->init = true;

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);

    GLuint vertex_array_id;
    glGenVertexArrays(1, &vertex_array_id);
    glBindVertexArray(vertex_array_id);

    // Create and compile our GLSL program from the shaders
    game_state->program_id = LoadShaders( "TransformVertexShader.vertexshader", "ColorFragmentShader.fragmentshader" );

    // Get a handle for our "MVP" uniform
    game_state->vp_matrix_id = glGetUniformLocation(game_state->program_id, "VP");
    game_state->m_matrix_id = glGetUniformLocation(game_state->program_id, "M");

    // Our vertices. Tree consecutive floats give a 3D vertex; Three consecutive vertices give a triangle.
    // A cube has 6 faces with 2 triangles each, so this makes 6*2=12 triangles, and 12*3 vertices
    const GLfloat g_vertex_buffer_data[] = {
       1.0f,  1.0f,  1.0f,
       1.0f,  1.0f, -1.0f,
      -1.0f,  1.0f, -1.0f,
      -1.0f,  1.0f,  1.0f,

       1.0f, -1.0f,  1.0f,
       1.0f, -1.0f, -1.0f,
      -1.0f, -1.0f, -1.0f,
      -1.0f, -1.0f,  1.0f
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

    glGenBuffers(1, &game_state->vertex_buffer);
    glGenBuffers(1, &game_state->index_buffer);
    glGenBuffers(1, &game_state->color_buffer);

    glBindBuffer(GL_ARRAY_BUFFER, game_state->vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, game_state->index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);
  }

  uint64_t frame_start_time = get_us();
  unsigned int frame_time = frame_start_time - game_state->game_start_time;

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

  camera_rotation_acceleration = vec3Multiply(camera_rotation_acceleration, 0.001 * 2.0*M_PI);
  camera_acceleration = vec4Multiply(camera_acceleration, -0.2);

  game_state->camera_direction_velocity = vec3Add(game_state->camera_direction_velocity, camera_rotation_acceleration);
  game_state->camera_direction = vec3Add(game_state->camera_direction, game_state->camera_direction_velocity);
  game_state->camera_direction_velocity = vec3Multiply(game_state->camera_direction_velocity, 0.8);

  mat4x4 camera_orientation;
  mat4x4Identity(camera_orientation);
  mat4x4RotateZ(camera_orientation, -game_state->camera_direction.z);
  mat4x4RotateX(camera_orientation, -game_state->camera_direction.x);
  mat4x4RotateY(camera_orientation, -game_state->camera_direction.y);

  vec3 camera_world_acceleration = mat4x4MultiplyVector(camera_orientation, camera_acceleration).xyz;

  game_state->camera_velocity = vec3Add(game_state->camera_velocity, camera_world_acceleration);
  game_state->camera_position = vec3Add(game_state->camera_position, game_state->camera_velocity);
  game_state->camera_velocity = vec3Multiply(game_state->camera_velocity, 0.8);

  if (ImGui::Begin("Render parameters"))
  {
    ImGui::DragInt("FPS", &game_state->fps, 1, 1, 120);

    ImGui::DragFloat("FOV", &game_state->fov, 1, 1, 180);
    ImGui::DragFloat3("Camera position", (float *)&game_state->camera_position.v);
    ImGui::DragFloat3("Camera direction", (float *)&game_state->camera_direction.v);

    ImGui::DragFloat("Rotate X", &game_state->rotate_x_deg, 1, -360, 360);
    ImGui::DragFloat("Rotate Y", &game_state->rotate_y_deg, 1, -360, 360);
    ImGui::DragFloat("Rotate Z", &game_state->rotate_z_deg, 1, -360, 360);

    ImGui::Combo("Sine Offset Type", (int*)&game_state->sine_offset_type, "Diagonal\0Concentric\0\0");
    ImGui::DragFloat("Bounces Per Second", &game_state->bounces_per_second, 0.01, 0, 10);
    ImGui::DragFloat("Oscillation Frequency", &game_state->oscillation_frequency, 0.01, 0, 10);
    ImGui::DragFloat("Bounce Height", &game_state->bounce_height, 0.1, 0, 100);

    for (int colour_n = 0; colour_n < 2; ++colour_n)
    {
      ImGui::PushID(colour_n);
      vec4 c = {colours[colour_n].x, colours[colour_n].y, colours[colour_n].z, 0.0f};
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
      ImGui::ColorPicker3("##picker", (float*)&colours[game_state->colour_picker_n]);
      ImGui::EndPopup();
    }
  }

  ImGui::End();

  float frame_delta_us = 1000000.0/game_state->fps;

  // Projection matrix : 45� Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
  mat4x4 projection;
  mat4x4Perspective(projection, (game_state->fov*(M_PI/180.0f)), io.DisplaySize.x / io.DisplaySize.y, 0.1f, 100.0f);
  // Model matrix : an identity matrix (model will be at the origin)
  mat4x4 model;
  mat4x4Identity(model);
  mat4x4RotateY(model, game_state->rotate_y_deg*(M_PI/180.0f));
  mat4x4RotateX(model, game_state->rotate_x_deg*(M_PI/180.0f));
  mat4x4RotateZ(model, game_state->rotate_z_deg*(M_PI/180.0f));
  // Camera matrix
  mat4x4 view;
  mat4x4Identity(view);
  mat4x4Translate(view, vec3Multiply(game_state->camera_position, -1));
  mat4x4RotateY(view, game_state->camera_direction.y);
  mat4x4RotateX(view, game_state->camera_direction.x);
  mat4x4RotateZ(view, game_state->camera_direction.z);

  // Our ModelViewProjection : multiplication of our 3 matrices
  mat4x4 view_projection;
  mat4x4MultiplyMatrix(view_projection, view, projection); // Remember, matrix multiplication is the other way around
  mat4x4 model_view_projection;
  mat4x4MultiplyMatrix(model_view_projection, model, view_projection); // Remember, matrix multiplication is the other way around

  // One color for each vertex. They were generated randomly.
  int n_colours = 8;
  GLfloat g_color_buffer_data[n_colours*3];

  for (int colour_n = 0; colour_n < n_colours; ++colour_n)
  {
    g_color_buffer_data[(colour_n*3)+0] = colours[colour_n/4].x;
    g_color_buffer_data[(colour_n*3)+1] = colours[colour_n/4].y;
    g_color_buffer_data[(colour_n*3)+2] = colours[colour_n/4].z;
  }

  glBindBuffer(GL_ARRAY_BUFFER, game_state->color_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_color_buffer_data), g_color_buffer_data, GL_STATIC_DRAW);

  // Clear the screen
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Use our shader
  glUseProgram(game_state->program_id);

  // Send our transformation to the currently bound shader,
  // in the "MVP" uniform
  glUniformMatrix4fv(game_state->vp_matrix_id, 1, GL_FALSE, &model_view_projection[0][0]);

  // 1rst attribute buffer : vertices
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, game_state->vertex_buffer);
  glVertexAttribPointer(
    0,                  // attribute. No particular reason for 0, but must match the layout in the shader.
    3,                  // size
    GL_FLOAT,           // type
    GL_FALSE,           // normalized?
    0,                  // stride
    (void*)0            // array buffer offset
  );

  // 2nd attribute buffer : colors
  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, game_state->color_buffer);
  glVertexAttribPointer(
    1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
    3,                                // size
    GL_FLOAT,                         // type
    GL_FALSE,                         // normalized?
    0,                                // stride
    (void*)0                          // array buffer offset
  );

  // Draw the triangle !
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
    glUniformMatrix4fv(game_state->m_matrix_id, 1, GL_FALSE, &cube[0][0]);
    glDrawElements(GL_TRIANGLES, n_indices, GL_UNSIGNED_BYTE, 0); // 12*3 indices starting at 0 -> 12 triangles
  }

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  game_state->last_frame_mouse = mouse_pos;

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