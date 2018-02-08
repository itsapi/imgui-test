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
  GLuint program_id;
  GLuint mvp_matrix_id;
  GLuint model_matrix_id;

  GLuint index_buffer;
  GLuint vertex_buffer;
  GLuint color_buffer;
  GLuint uv_buffer;

  GLuint grass_texture_id;

  int n_indices;

  bool init;

  int fps;
  uint64_t game_start_time;

  float fov;
  vec3 terrain_rotation;
  SineOffsetType sine_offset_type;
  float bounces_per_second;
  float bounce_height;
  float oscillation_frequency;

  vec4 colours[2];
  int colour_picker_n;

  vec2 last_frame_mouse;

  vec3 camera_velocity;
  vec3 camera_position;

  vec3 camera_direction_velocity;
  vec3 camera_direction;
};

void
main_loop(GameState *game_stae);

void
shutdown(GameState *game_stae);


#endif