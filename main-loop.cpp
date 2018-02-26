#include "main-loop.h"

#include "imgui.h"
#include "shader.h"
#include "bitmap.h"
#include "perlin.h"

#include "ccVector.h"

#include <GL/gl3w.h>
#include <SDL.h>
#include <sys/time.h>
#include <unistd.h>

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

const int VERTEX_POSITION_ATTRIBUTE = 0;
const int VERTEX_COLOUR_ATTRIBUTE = 1;
const int VERTEX_UV_ATTRIBUTE = 2;
const int VERTEX_NORMAL_ATTRIBUTE = 3;
const int VERTEX_TANGENT_ATTRIBUTE = 4;
const int VERTEX_BITANGENT_ATTRIBUTE = 5;


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


#define opengl_print_errors() _opengl_print_errors(__FILE__, __LINE__)

void
opengl_print_error(GLenum error_code, const char *file, int line)
{
  const char *error;
  switch (error_code)
  {
    case (GL_INVALID_ENUM):
    {
      error = "GL_INVALID_ENUM";
    } break;
    case (GL_INVALID_VALUE):
    {
      error = "GL_INVALID_VALUE";
    } break;
    case (GL_INVALID_OPERATION):
    {
      error = "GL_INVALID_OPERATION";
    } break;
    case (GL_STACK_OVERFLOW):
    {
      error = "GL_STACK_OVERFLOW";
    } break;
    case (GL_STACK_UNDERFLOW):
    {
      error = "GL_STACK_UNDERFLOW";
    } break;
    case (GL_OUT_OF_MEMORY):
    {
      error = "GL_OUT_OF_MEMORY";
    } break;
    case (GL_INVALID_FRAMEBUFFER_OPERATION):
    {
      error = "GL_INVALID_FRAMEBUFFER_OPERATION";
    } break;
  }
  printf("OpenGL error: %s at %s:%d\n", error, file, line);
}


/// Get all errors from OpenGL, and print them.
bool
_opengl_print_errors(const char *file, int line)
{
  bool success = true;

  GLenum error = glGetError();
  while (error != GL_NO_ERROR)
  {
    success &= false;
    opengl_print_error(error, file, line);

    error = glGetError();
  }

  return success;
}


TerrainChunk *
get_terrain_chunk_slot(GameState *game_state, vec2 position)
{
  int hash = 0;
  hash = ((hash<<5)^(hash>>29))^(int)position.x;
  hash = ((hash<<7)^(hash>>31))^(int)position.y;
  hash = (hash % 3203) % ARRAY_COUNT(game_state->terrain_chunk_hashmap);
  return game_state->terrain_chunk_hashmap + hash;
}


TerrainChunk &
get_terrain_chunk(GameState *game_state, vec2 position)
{
  TerrainChunk *initial_slot = get_terrain_chunk_slot(game_state, position);
  TerrainChunk *slot = initial_slot;
  while (slot->terrain_gen_id == game_state->terrain_gen_id && !vec2Equal(slot->position, position))
  {
    slot++;
    if (slot == game_state->terrain_chunk_hashmap + ARRAY_COUNT(game_state->terrain_chunk_hashmap))
    {
      slot = game_state->terrain_chunk_hashmap;
    }
    assert(slot != initial_slot);
  }

  if (slot->terrain_gen_id != game_state->terrain_gen_id)
  {
    slot->position = position;
    slot->terrain_gen_id = game_state->terrain_gen_id;
  }
  else
  {
    assert(vec2Equal(slot->position, position));
  }

  return *slot;
}


float &
get_height_from_chunk(TerrainChunk &terrain_chunk, vec2 position)
{
  return terrain_chunk.height_map[int(position.y) * CHUNK_SIZE + int(position.x)];
}


void
generate_terrain(GameState *game_state)
{
  game_state->terrain_gen_id++;
  game_state->current_terrain_dim = game_state->user_terrain_dim;

  vec2 chunk_position;
  for (chunk_position.x = -floorf(game_state->current_terrain_dim.x*0.5);
       chunk_position.x < floorf(game_state->current_terrain_dim.x*0.5);
       ++chunk_position.x)
  for (chunk_position.y = -floorf(game_state->current_terrain_dim.y*0.5);
       chunk_position.y < floorf(game_state->current_terrain_dim.y*0.5);
       ++chunk_position.y)
  {
    TerrainChunk &terrain_chunk = get_terrain_chunk(game_state, chunk_position);

    vec2 translation;
    for (translation.x = 0;
         translation.x < CHUNK_SIZE;
         ++translation.x)
    for (translation.y = 0;
         translation.y < CHUNK_SIZE;
         ++translation.y)
    {
      vec2 global_position = vec2Add(vec2Multiply(chunk_position, (float)CHUNK_SIZE), translation);
      float terrain_offset = 0;
      for (int perlin_n = 0;
           perlin_n < game_state->n_perlins;
           ++perlin_n)
      {
        terrain_offset += game_state->perlin_amplitudes[perlin_n] * perlin(global_position, game_state->perlin_periods[perlin_n]);
      }
      get_height_from_chunk(terrain_chunk, translation) = terrain_offset;
    }
  }
}


vec2
get_chunk_position(vec2 position)
{
  vec2 chunk_position = vec2Multiply(position, 1.0/CHUNK_SIZE);
  chunk_position = {floorf(chunk_position.x), floorf(chunk_position.y)};
  return chunk_position;
}


float
get_terrain_height_for_global_position(GameState *game_state, vec2 position)
{
  vec2 chunk_position = get_chunk_position(position);
  vec2 chunk_offset = vec2Subtract(position, vec2Multiply(chunk_position, CHUNK_SIZE));

  return get_height_from_chunk(get_terrain_chunk(game_state, chunk_position), chunk_offset);
}


void
ToggleButton(const char* str_id, bool* v)
{
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  float height = ImGui::GetFrameHeight();
  float width = height * 1.55f;
  float radius = height * 0.50f;

  if (ImGui::InvisibleButton(str_id, ImVec2(width, height)))
  {
    *v = !*v;
  }

  ImU32 col_bg;
  if (ImGui::IsItemHovered())
  {
    col_bg = *v ? IM_COL32(145+20, 211, 68+20, 255) : IM_COL32(218-20, 218-20, 218-20, 255);
  }
  else
  {
    col_bg = *v ? IM_COL32(145, 211, 68, 255) : IM_COL32(218, 218, 218, 255);
  }

  draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), col_bg, height * 0.5f);
  draw_list->AddCircleFilled(ImVec2(*v ? (p.x + width - radius) : (p.x + radius), p.y + radius), radius - 1.5f, IM_COL32(255, 255, 255, 255));

  ImGui::SameLine();
  ImGui::Text("%s", str_id);
}


void
render_window(GameState *game_state, float surface_height)
{
  if (ImGui::Begin("Render parameters"))
  {

    ImGui::Value("Surface Height", surface_height);

    vec2 chunk_position = get_chunk_position({game_state->camera_position.x, game_state->camera_position.z});
    ImGui::Text("Chunk position: %f  %f", chunk_position.x, chunk_position.y);

    ImGui::DragInt("FPS", &game_state->fps, 1, 1, 120);
    ImGui::Value("Last Frame Delta", game_state->last_frame_delta);
    ImGui::Value("Last FPS", 1000000.0f/game_state->last_frame_total);

    ImGui::DragFloat("FOV", &game_state->fov, 1, 1, 180);
    ImGui::DragFloat3("Camera position", (float *)&game_state->camera_position.v);

    vec3 camera_direction_deg = vec3Multiply(game_state->camera_direction, 180.0/M_PI);
    if (ImGui::DragFloat3("Camera direction", (float *)&camera_direction_deg.v))
    {
      game_state->camera_direction = vec3Multiply(camera_direction_deg, M_PI/180.0);
    }
    ImGui::Text("Camera Velocity: %f  %f  %f", game_state->camera_velocity.x, game_state->camera_velocity.y, game_state->camera_velocity.z);

    ToggleButton("Capture Mouse", &game_state->capture_mouse);
    ToggleButton("Debug Camera", &game_state->debug_camera);

    if (ImGui::Button("Top down view"))
    {
      game_state->debug_camera = true;
      game_state->camera_position = {0, 20, 0};
      game_state->camera_direction = {0.5*M_PI, 0, 0};
      game_state->camera_velocity = {};
      game_state->camera_direction_velocity = {};
    }

    ImGui::DragFloat("Player speed", &game_state->player_speed);
    ImGui::DragFloat("Mouse speed", &game_state->drag_radians_per_second, M_PI/180.0);
    ImGui::DragFloat("Player Feet", &game_state->player_feet);

    ImGui::DragFloat3("Light position", (float *)&game_state->light_position.v);

    if (ImGui::ColorButton("Light colour", {game_state->light_colour.x, game_state->light_colour.y, game_state->light_colour.z, 1}))
    {
      ImGui::OpenPopup("Change light colour");
    }
    if (ImGui::BeginPopup("Change light colour"))
    {
      ImGui::ColorPicker3("##light colour picker", (float *)&game_state->light_colour.v);
      ImGui::EndPopup();
    }

    if (ImGui::ColorButton("Ambient light colour", {game_state->ambient_light_colour.x, game_state->ambient_light_colour.y, game_state->ambient_light_colour.z, 1}))
    {
      ImGui::OpenPopup("Change ambient light colour");
    }
    if (ImGui::BeginPopup("Change ambient light colour"))
    {
      ImGui::ColorPicker3("##ambient light colour picker", (float *)&game_state->ambient_light_colour.v);
      ImGui::EndPopup();
    }

    ImGui::DragFloat2("Terrain dim", (float *)&game_state->user_terrain_dim.v);
    ImGui::Value("N chunks", game_state->user_terrain_dim.x * game_state->user_terrain_dim.y);
    ImGui::Value("N cubes", game_state->user_terrain_dim.x * game_state->user_terrain_dim.y * CHUNK_SIZE * CHUNK_SIZE);

    ImGui::DragFloat3("Terrain Rotation", (float *)&game_state->terrain_rotation, 1, -360, 360);

    if (ImGui::Button("Re-generate terrain"))
    {
      generate_terrain(game_state);
    }

    ImGui::DragInt("Number of Perlins", &game_state->n_perlins, 0.2, 0, ARRAY_COUNT(game_state->perlin_periods));
    for (int perlin_n = 0;
         perlin_n < game_state->n_perlins;
         ++perlin_n)
    {
      ImGui::PushID(perlin_n);
      ImGui::DragInt("Perlin period", &game_state->perlin_periods[perlin_n], 1, 1, 1024);
      ImGui::DragFloat("Perlin amplitude", &game_state->perlin_amplitudes[perlin_n], 0.1, 0, 1024);
      ImGui::PopID();
    }

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
}


void
init_game_state(GameState *game_state)
{
  // Initialise GameState
  //

  game_state->game_start_time = get_us();
  game_state->fps = 60;
  game_state->fov = 45.0f;
  game_state->terrain_rotation = {};
  game_state->sine_offset_type = SineOffsetType::Concentric;
  game_state->bounces_per_second = 0;
  game_state->oscillation_frequency = 0;
  game_state->bounce_height = 1;
  game_state->camera_velocity = {};
  game_state->camera_position = {2.0f, 2.0f, 2.0f};
  game_state->camera_direction_velocity = {};
  game_state->camera_direction = { (float)atan2(game_state->camera_position.y, game_state->camera_position.z),
                                  -(float)atan2(game_state->camera_position.x, game_state->camera_position.z), 0.0f};
  game_state->player_speed = 0.5; // m/s
  game_state->drag_radians_per_second = 0.5 * M_PI;
  game_state->player_feet = -4;

  game_state->colour_picker_n = 0;
  game_state->colours[0] = {0.15f, 0.7f, 0.1f, 1};
  game_state->colours[1] = {0.5f, 0.5f, 0.5f, 1};

  game_state->user_terrain_dim = {5, 5};
  game_state->n_perlins = 15;
  for (int perlin_n = 0;
       perlin_n < ARRAY_COUNT(game_state->perlin_periods);
       ++perlin_n)
  {
    game_state->perlin_periods[perlin_n] = 16;
    game_state->perlin_amplitudes[perlin_n] = 1;
  }

  game_state->terrain_gen_id = 0;
  generate_terrain(game_state);

  game_state->light_position = {0, 10, 0};
  game_state->light_colour = {1, 1, 1};
  game_state->ambient_light_colour = {0.3, 0.3, 0.3};

  game_state->capture_mouse = true;

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

  game_state->world_view_projection_matrix_uniform = glGetUniformLocation(game_state->program_id, "WORLD_VIEW_PROJECTION");
  game_state->model_matrix_uniform = glGetUniformLocation(game_state->program_id, "MODEL");
  game_state->light_position_uniform = glGetUniformLocation(game_state->program_id, "LIGHT_POSITION");
  game_state->light_colour_uniform = glGetUniformLocation(game_state->program_id, "LIGHT_COLOUR");
  game_state->ambient_light_uniform = glGetUniformLocation(game_state->program_id, "AMBIENT_LIGHT_COLOUR");
  game_state->grass_texture_uniform = glGetUniformLocation(game_state->program_id, "GRASS_TEXTURE");
  game_state->normal_map_texture_uniform = glGetUniformLocation(game_state->program_id, "NORMAL_MAP_TEXTURE");

  // Vertex data
  //

  const GLubyte index_buffer_data[] = {
    // Top
    0, 1, 2,
    0, 2, 3,

    // Bottom
    7, 5, 4,
    5, 7, 6,

    // Front face
    8, 9, 10,
    11, 9, 8,

    // Back face
    12, 13, 14,
    15, 13, 12,

    // Side face
    10, 9, 12,
    9, 15, 12,

    // Side face
    11, 8, 13,
    8, 14, 13
  };

  game_state->n_indices = ARRAY_COUNT(index_buffer_data);

  const vec3 vertex_buffer_data[] = {
    // Top face
    { 1.0f,  1.0f,  1.0f},
    { 1.0f,  1.0f, -1.0f},
    {-1.0f,  1.0f, -1.0f},
    {-1.0f,  1.0f,  1.0f},

    // Bottom face
    { 1.0f, -1.0f,  1.0f},
    { 1.0f, -1.0f, -1.0f},
    {-1.0f, -1.0f, -1.0f},
    {-1.0f, -1.0f,  1.0f},

    // Front face
    { 1.0f,  1.0f,  1.0f},
    { 1.0f, -1.0f, -1.0f},
    { 1.0f,  1.0f, -1.0f},
    { 1.0f, -1.0f,  1.0f},

    // Back face
    {-1.0f,  1.0f, -1.0f},
    {-1.0f, -1.0f,  1.0f},
    {-1.0f,  1.0f,  1.0f},
    {-1.0f, -1.0f, -1.0f}
  };

  const int n_vertices = ARRAY_COUNT(vertex_buffer_data);

  const vec2 uv_buffer_data[n_vertices] = {
    // Top face
    {0,   1},
    {0.5, 1},
    {0.5, 0.5},
    {0,   0.5},

    // Bottom face
    {0.5, 1},
    {1,   1},
    {1,   0.5},
    {0.5, 0.5},

    // Front face
    {0,   0.5},
    {0.5, 0},
    {0.5, 0.5},
    {0,   0},

    // Back face
    {0,   0.5},
    {0.5, 0},
    {0.5, 0.5},
    {0,   0}
  };

  const vec3 normal_buffer_data[n_vertices] = {
    // Top face
    {0, 1, 0},
    {0, 1, 0},
    {0, 1, 0},
    {0, 1, 0},

    // Bottom face
    {0, -1, 0},
    {0, -1, 0},
    {0, -1, 0},
    {0, -1, 0},

    // Front face
    vec3Normalize({1, 0,  1}),
    vec3Normalize({1, 0, -1}),
    vec3Normalize({1, 0, -1}),
    vec3Normalize({1, 0,  0}),

    // Back face
    vec3Normalize({-1, 0, -1}),
    vec3Normalize({-1, 0,  1}),
    vec3Normalize({-1, 0,  1}),
    vec3Normalize({-1, 0, -1})
  };

  vec3 tangent_buffer_data[n_vertices] = {};
  vec3 bitangent_buffer_data[n_vertices] = {};

  // Generate tangents and bitangents
  //

  for (int index_index = 0;
       index_index < game_state->n_indices;
       index_index += 3)
  {
    GLubyte index0 = index_buffer_data[index_index+0];
    GLubyte index1 = index_buffer_data[index_index+1];
    GLubyte index2 = index_buffer_data[index_index+2];

    const vec3& vertex0 = vertex_buffer_data[index0];
    const vec3& vertex1 = vertex_buffer_data[index1];
    const vec3& vertex2 = vertex_buffer_data[index2];

    const vec2& uv0 = uv_buffer_data[index0];
    const vec2& uv1 = uv_buffer_data[index1];
    const vec2& uv2 = uv_buffer_data[index2];

    // Edges of the triangle - position delta
    vec3 delta_pos1 = vec3Subtract(vertex1, vertex0);
    vec3 delta_pos2 = vec3Subtract(vertex2, vertex0);

    // UV delta
    vec2 delta_UV1 = vec2Subtract(uv1, uv0);
    vec2 delta_UV2 = vec2Subtract(uv2, uv0);

    float r = 1.0f / (delta_UV1.x * delta_UV2.y - delta_UV1.y * delta_UV2.x);

    vec3 tangent = vec3Multiply(vec3Subtract(vec3Multiply(delta_pos1, delta_UV2.y), vec3Multiply(delta_pos2, delta_UV1.y)), r);
    vec3 bitangent = vec3Multiply(vec3Subtract(vec3Multiply(delta_pos2, delta_UV1.x), vec3Multiply(delta_pos1, delta_UV2.x)), r);

    tangent_buffer_data[index0] = tangent;
    tangent_buffer_data[index1] = tangent;
    tangent_buffer_data[index2] = tangent;
    bitangent_buffer_data[index0] = bitangent;
    bitangent_buffer_data[index1] = bitangent;
    bitangent_buffer_data[index2] = bitangent;
  }

  // Initialise OpenGL buffers
  //

  glGenBuffers(1, &game_state->index_buffer);
  glGenBuffers(1, &game_state->vertex_buffer);
  glGenBuffers(1, &game_state->color_buffer);
  glGenBuffers(1, &game_state->uv_buffer);
  glGenBuffers(1, &game_state->normal_buffer);
  glGenBuffers(1, &game_state->tangent_buffer);
  glGenBuffers(1, &game_state->bitangent_buffer);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, game_state->index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, game_state->vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, game_state->uv_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), uv_buffer_data, GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, game_state->normal_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(normal_buffer_data), normal_buffer_data, GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, game_state->tangent_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(tangent_buffer_data), tangent_buffer_data, GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, game_state->bitangent_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(bitangent_buffer_data), bitangent_buffer_data, GL_STATIC_DRAW);

  // Initialise OpenGL textures
  //

  glActiveTexture(GL_TEXTURE0);
  game_state->grass_texture_id = loadBMP_custom("textures/block-texture.bmp");

  glActiveTexture(GL_TEXTURE1);
  game_state->normal_map_texture_id = loadBMP_custom("textures/normal-map-texture.bmp");

  opengl_print_errors();
}


void
main_loop(GameState *game_state, vec2 mouse_delta)
{
  if (!game_state->init)
  {
    init_game_state(game_state);
  }

  uint64_t frame_start_time = get_us();
  unsigned int frame_time = frame_start_time - game_state->game_start_time;

  // User inputs
  //

  ImGuiIO& io = ImGui::GetIO();

  vec3 camera_rotation_acceleration = {0, 0, 0};
  vec4 camera_acceleration_direction = {0, 0, 0, 1};

  float this_frame_player_horizontal_speed = game_state->player_speed;
  float this_frame_player_vertical_speed = 0;
  bool jump = false;

  if (!io.WantCaptureKeyboard)
  {
    if (ImGui::IsKeyPressed(SDL_SCANCODE_E))
    {
      game_state->debug_camera = !game_state->debug_camera;
    }
    if (ImGui::IsKeyPressed(SDL_SCANCODE_Q))
    {
      game_state->capture_mouse = !game_state->capture_mouse;
    }

    if (ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT))
    {
      this_frame_player_horizontal_speed += 1;
    }
    if (ImGui::IsKeyDown(SDL_SCANCODE_D))
    {
      camera_acceleration_direction.x += 1;
    }
    if (ImGui::IsKeyDown(SDL_SCANCODE_A))
    {
      camera_acceleration_direction.x -= 1;
    }
    if (ImGui::IsKeyDown(SDL_SCANCODE_S))
    {
      camera_acceleration_direction.z += 1;
    }
    if (ImGui::IsKeyDown(SDL_SCANCODE_W))
    {
      camera_acceleration_direction.z -= 1;
    }
    if (ImGui::IsKeyDown(SDL_SCANCODE_SPACE))
    {
      jump = true;
    }

    if (game_state->debug_camera)
    {
      this_frame_player_vertical_speed = game_state->player_speed;
      if (ImGui::IsKeyDown(SDL_SCANCODE_SPACE))
      {
        camera_acceleration_direction.y += 1;
      }
      if (ImGui::IsKeyDown(SDL_SCANCODE_F))
      {
        camera_acceleration_direction.y -= 1;
      }
      if (ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT))
      {
        this_frame_player_vertical_speed += 1;
      }
    }
  }

  // Mouse movement
  //

  SDL_SetRelativeMouseMode(game_state->capture_mouse ? SDL_TRUE : SDL_FALSE);

  if (game_state->capture_mouse)
  {
    vec2 drag_delta = vec2Multiply(mouse_delta, game_state->last_frame_total * game_state->drag_radians_per_second / 1000000.0);
    game_state->camera_direction.x += drag_delta.y;
    game_state->camera_direction.x = fmin(fmax(game_state->camera_direction.x, -0.5*M_PI), 0.5*M_PI);
    game_state->camera_direction.y += drag_delta.x;
  }

  // Gravity
  //

  vec3 camera_gravity_acceleration = {0, 0, 0};

  float surface_height;
  if (!game_state->debug_camera)
  {
    surface_height = get_terrain_height_for_global_position(game_state, {game_state->camera_position.x, game_state->camera_position.z}) + 0.5;

    if (game_state->camera_position.y + game_state->player_feet > surface_height)
    {
      camera_gravity_acceleration.y = -9.8;
    }
    else if (jump)
    {
      this_frame_player_vertical_speed = 50;
      camera_acceleration_direction.y = 1;
    }
  }

  // Update camera direction
  //

  vec2 camera_horizontal_acceleration = {camera_acceleration_direction.x, camera_acceleration_direction.z};
  float unnormalised_camera_horizontal_acceleration_length = vec2Length(camera_horizontal_acceleration);
  if (unnormalised_camera_horizontal_acceleration_length > 0)
  {
    camera_horizontal_acceleration = vec2Multiply(camera_horizontal_acceleration, 1.0f / unnormalised_camera_horizontal_acceleration_length);
  }

  camera_horizontal_acceleration = vec2Multiply(camera_horizontal_acceleration, this_frame_player_horizontal_speed);

  vec4 camera_acceleration = {camera_horizontal_acceleration.x,
                              camera_acceleration_direction.y * this_frame_player_vertical_speed,
                              camera_horizontal_acceleration.y};

  camera_acceleration = vec4Multiply(camera_acceleration, game_state->last_frame_total / 1000000.0);

  camera_rotation_acceleration = vec3Multiply(camera_rotation_acceleration, 0.001 * 2.0 * M_PI);

  game_state->camera_direction_velocity = vec3Add(game_state->camera_direction_velocity, camera_rotation_acceleration);
  game_state->camera_direction = vec3Add(game_state->camera_direction, game_state->camera_direction_velocity);
  game_state->camera_direction_velocity = vec3Multiply(game_state->camera_direction_velocity, 0.8);

  // Update camera position
  //

  mat4x4 camera_y_orientation;
  mat4x4Identity(camera_y_orientation);
  mat4x4RotateY(camera_y_orientation, -game_state->camera_direction.y);

  vec3 camera_gravity_acceleration_frame = vec3Multiply(camera_gravity_acceleration, game_state->last_frame_total/1000000.0);

  vec3 camera_world_acceleration = mat4x4MultiplyVector(camera_y_orientation, camera_acceleration).xyz;
  camera_world_acceleration = vec3Add(camera_world_acceleration, camera_gravity_acceleration_frame);

  game_state->camera_velocity = vec3Add(game_state->camera_velocity, camera_world_acceleration);
  game_state->camera_position = vec3Add(game_state->camera_position, game_state->camera_velocity);
  game_state->camera_position.x = game_state->camera_position.x * 0.8;
  game_state->camera_position.z = game_state->camera_position.z * 0.8;

  if (!game_state->debug_camera)
  {
    if (game_state->camera_position.y + game_state->player_feet < surface_height)
    {
      game_state->camera_velocity.y = 0;
      game_state->camera_position.y = surface_height - game_state->player_feet;
    }
  }
  else
  {
    game_state->camera_velocity.y *= 0.8;
  }

  // ImGui window
  //

  render_window(game_state, surface_height);

  // Generate new chunks
  //

  bool regenerate = false;

  vec2 position = {game_state->camera_position.x, game_state->camera_position.z};
  vec2 chunk_position = get_chunk_position(position);

  if (chunk_position.x < -floorf(game_state->current_terrain_dim.x*0.5) ||
      chunk_position.x >= floorf(game_state->current_terrain_dim.x*0.5))
  {
    game_state->user_terrain_dim.x = game_state->current_terrain_dim.x + 1;
    regenerate = true;
  }

  if (chunk_position.y < -floorf(game_state->current_terrain_dim.y*0.5) ||
      chunk_position.y >= floorf(game_state->current_terrain_dim.y*0.5))
  {
    game_state->user_terrain_dim.y = game_state->current_terrain_dim.y + 1;
    regenerate = true;
  }

  if (regenerate)
  {
    generate_terrain(game_state);
  }

  // Create world, view, projection matrices
  //

  mat4x4 world;
  mat4x4Identity(world);
  mat4x4RotateY(world, game_state->terrain_rotation.y*(M_PI/180.0f));
  mat4x4RotateX(world, game_state->terrain_rotation.x*(M_PI/180.0f));
  mat4x4RotateZ(world, game_state->terrain_rotation.z*(M_PI/180.0f));

  mat4x4 view;
  mat4x4Identity(view);
  mat4x4Translate(view, vec3Multiply(game_state->camera_position, -1));
  mat4x4RotateY(view, game_state->camera_direction.y);
  mat4x4RotateX(view, game_state->camera_direction.x);
  mat4x4RotateZ(view, game_state->camera_direction.z);

  mat4x4 projection;
  mat4x4Perspective(projection, (game_state->fov*(M_PI/180.0f)), io.DisplaySize.x / io.DisplaySize.y, 0.1f, 100000.0f);

  mat4x4 view_projection;
  mat4x4MultiplyMatrix(view_projection, view, projection);
  mat4x4 world_view_projection;
  mat4x4MultiplyMatrix(world_view_projection, world, view_projection);

  // Upload colours to colour buffer
  //

  // Duplicate the two colours, one colour per vertex
  int n_colours = 16;
  vec4 color_buffer_data[n_colours];
  for (int colour_n = 0; colour_n < n_colours; ++colour_n)
  {
    color_buffer_data[colour_n] = game_state->colours[int(colour_n * 2.0/n_colours)];
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

  glEnableVertexAttribArray(VERTEX_NORMAL_ATTRIBUTE);
  glBindBuffer(GL_ARRAY_BUFFER, game_state->normal_buffer);
  glVertexAttribPointer(
    VERTEX_NORMAL_ATTRIBUTE,
    3,         // size
    GL_FLOAT,  // type
    GL_FALSE,  // normalized?
    0,         // stride
    (void*)0   // array buffer offset
  );

  glEnableVertexAttribArray(VERTEX_TANGENT_ATTRIBUTE);
  glBindBuffer(GL_ARRAY_BUFFER, game_state->tangent_buffer);
  glVertexAttribPointer(
    VERTEX_TANGENT_ATTRIBUTE,
    3,         // size
    GL_FLOAT,  // type
    GL_FALSE,  // normalized?
    0,         // stride
    (void*)0   // array buffer offset
  );

  glEnableVertexAttribArray(VERTEX_BITANGENT_ATTRIBUTE);
  glBindBuffer(GL_ARRAY_BUFFER, game_state->tangent_buffer);
  glVertexAttribPointer(
    VERTEX_BITANGENT_ATTRIBUTE,
    3,         // size
    GL_FLOAT,  // type
    GL_FALSE,  // normalized?
    0,         // stride
    (void*)0   // array buffer offset
  );

  // Render
  //

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(game_state->program_id);

  glUniformMatrix4fv(game_state->world_view_projection_matrix_uniform, 1, GL_FALSE, &world_view_projection[0][0]);
  glUniform3fv(game_state->light_position_uniform, 1, (float *)&game_state->light_position.v);
  glUniform3fv(game_state->light_colour_uniform, 1, (float *)&game_state->light_colour.v);
  glUniform3fv(game_state->ambient_light_uniform, 1, (float *)&game_state->ambient_light_colour.v);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, game_state->grass_texture_id);
  glUniform1i(game_state->grass_texture_uniform, 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, game_state->normal_map_texture_id);
  glUniform1i(game_state->normal_map_texture_uniform, 1);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, game_state->index_buffer);

  float bounces_per_us = game_state->bounces_per_second / 1000000.0;

  for (chunk_position.x = -floorf(game_state->current_terrain_dim.x*0.5);
       chunk_position.x < floorf(game_state->current_terrain_dim.x*0.5);
       ++chunk_position.x)
  for (chunk_position.y = -floorf(game_state->current_terrain_dim.y*0.5);
       chunk_position.y < floorf(game_state->current_terrain_dim.y*0.5);
       ++chunk_position.y)
  {
    TerrainChunk &terrain_chunk = get_terrain_chunk(game_state, chunk_position);

    vec2 translation;
    for (translation.x = 0;
         translation.x < CHUNK_SIZE;
         ++translation.x)
    for (translation.y = 0;
         translation.y < CHUNK_SIZE;
         ++translation.y)
    {
      mat4x4 cube;
      mat4x4Identity(cube);
      mat4x4Scale(cube, 0.5);
      vec2 global_position = vec2Add(vec2Multiply(chunk_position, CHUNK_SIZE), translation);
      mat4x4Translate(cube, {global_position.x, 0, global_position.y});

      float terrain_offset = get_height_from_chunk(terrain_chunk, translation);
      mat4x4Translate(cube, {0, terrain_offset, 0});

      float sine_offset = 0;
      switch (game_state->sine_offset_type)
      {
        case (SineOffsetType::Diagonal):
        {
          sine_offset = (translation.x/game_state->current_terrain_dim.x + translation.y/game_state->current_terrain_dim.y) * game_state->oscillation_frequency*2*M_PI;
        } break;
        case (SineOffsetType::Concentric):
        {
          sine_offset = vec2Length(translation) / (0.5 * vec2Length(game_state->current_terrain_dim)) * game_state->oscillation_frequency*2*M_PI;
        } break;
      }

      float offset = sin(frame_time * bounces_per_us * 2*M_PI + sine_offset) * game_state->bounce_height;

      mat4x4Translate(cube, {0, offset, 0});
      glUniformMatrix4fv(game_state->model_matrix_uniform, 1, GL_FALSE, &cube[0][0]);
      glDrawElements(GL_TRIANGLES, game_state->n_indices, GL_UNSIGNED_BYTE, 0);
    }

  }

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  // Lock frame-rate
  //

  float frame_delta_us = 1000000.0/game_state->fps;

  uint64_t frame_end_time = get_us();
  float this_frame_delta = (float)(frame_end_time - frame_start_time);
  float frame_delta_left = frame_delta_us - this_frame_delta;

  if (frame_delta_left > 0)
  {
    usleep(frame_delta_left);
  }

  game_state->last_frame_total = this_frame_delta + fmax(frame_delta_left, 0);
  game_state->last_frame_delta = this_frame_delta;
}


void
shutdown(GameState *game_state)
{
  // Cleanup VBO and shader
  glDeleteBuffers(1, &game_state->vertex_buffer);
  glDeleteBuffers(1, &game_state->index_buffer);
  glDeleteBuffers(1, &game_state->color_buffer);
}