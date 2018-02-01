#ifndef MAIN_LOOP_H_DEF
#define MAIN_LOOP_H_DEF

#include "ccVector.h"
#include <GL/gl3w.h>


struct GameState
{
 	GLuint program_id;
  GLuint vp_matrix_id;
	GLuint m_matrix_id;
  mat4x4 model_view_projection;
  GLuint vertex_buffer;
  GLuint index_buffer;
  GLuint color_buffer;

  bool init;
  float fov;
  float rotate_x_deg;
  float rotate_y_deg;
  float rotate_z_deg;

  vec3 camera_velocity;
  vec3 camera_position;
};

void
main_loop(GameState *game_stae);

void
shutdown(GameState *game_stae);


#endif