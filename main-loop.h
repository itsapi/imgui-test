#ifndef MAIN_LOOP_H_DEF
#define MAIN_LOOP_H_DEF

#include "ccVector.h"
#include <GL/gl3w.h>


enum struct SineOffsetType
{
  Diagonal,
  Concentric
};

struct GameState
{
  GLint program_id;

  GLint world_view_projection_matrix_uniform;
  GLint model_matrix_uniform;
  GLint light_position_uniform;
  GLint light_colour_uniform;
  GLint ambient_light_uniform;
  GLint grass_texture_uniform;
  GLint normal_map_texture_uniform;

  GLuint index_buffer;
  GLuint vertex_buffer;
  GLuint color_buffer;
  GLuint uv_buffer;
  GLuint normal_buffer;
  GLuint tangent_buffer;
  GLuint bitangent_buffer;

  GLuint grass_texture_id;
  GLuint normal_map_texture_id;

  int n_indices;

  bool init;
  bool capture_mouse;

  int fps;
  uint64_t game_start_time;
  float last_frame_delta;
  float last_frame_total;

  vec2 terrain_dim;

  int n_perlins;
  int perlin_periods[16];
  float perlin_amplitudes[16];

  float fov;
  vec3 terrain_rotation;
  SineOffsetType sine_offset_type;
  float bounces_per_second;
  float bounce_height;
  float oscillation_frequency;

  vec4 colours[2];
  int colour_picker_n;

  vec3 light_position;
  vec3 light_colour;

  vec3 ambient_light_colour;

  vec2 last_frame_mouse;

  float player_speed;
  float drag_radians_per_second;

  vec3 camera_velocity;
  vec3 camera_position;

  vec3 camera_direction_velocity;
  vec3 camera_direction;
};

void
main_loop(GameState *game_state, vec2  mouse_delta);

void
shutdown(GameState *game_state);


#endif