#include "main-loop.h"

#include "imgui.h"
#include "shader.h"
#include <GL/gl3w.h>
#include <SDL.h>
#include "ccVector.h"


void
main_loop()
{
  static bool init = false;
  static float fov = 45.0f;
  static float rotate_x_deg = 0;
  static float rotate_y_deg = 0;
  static float rotate_z_deg = 0;

  static vec3 colours[] = {
    {1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f},
    {1.0f, 1.0f, 1.0f},
    {0.5f, 0.5f, 0.5f},
  };

  ImGuiIO& io = ImGui::GetIO();
  // ImGui::ShowDemoWindow();

  if (ImGui::Begin("Render parameters"))
  {
    ImGui::DragFloat("FOV", &fov, 1, 1, 180);
    ImGui::DragFloat("Rotate X", &rotate_x_deg, 1, -360, 360);
    ImGui::DragFloat("Rotate Y", &rotate_y_deg, 1, -360, 360);
    ImGui::DragFloat("Rotate Z", &rotate_z_deg, 1, -360, 360);

    static int colour_picker_n = 0;

    for (int colour_n = 0; colour_n < 6; ++colour_n)
    {
      ImGui::PushID(colour_n);
      vec4 c = {colours[colour_n].x, colours[colour_n].y, colours[colour_n].z, 0.0f};
      bool pushed = ImGui::ColorButton("##change colour", c);
      ImGui::PopID();

      if (pushed) {
        colour_picker_n = colour_n;
        ImGui::OpenPopup("Change Colour");
      }
      ImGui::SameLine();
      ImGui::Text("Side %d", colour_n + 1);
    }

    if (ImGui::BeginPopup("Change Colour"))
    {
      ImGui::ColorPicker3("##picker", (float*)&colours[colour_picker_n]);
      ImGui::EndPopup();
    }
  }

  static GLuint programID;
  static GLuint MatrixID;
  static mat4x4 MVP;
  static GLuint vertexbuffer;
  static GLuint colorbuffer;

  if (!init)
  {
    init = true;

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);

    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);

    // Create and compile our GLSL program from the shaders
    programID = LoadShaders( "TransformVertexShader.vertexshader", "ColorFragmentShader.fragmentshader" );

  }

  // Get a handle for our "MVP" uniform
  MatrixID = glGetUniformLocation(programID, "MVP");

  // Projection matrix : 45� Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
  mat4x4 Projection;
  mat4x4Perspective(Projection, (fov*(M_PI/180.0f)), io.DisplaySize.x / io.DisplaySize.y, 0.1f, 100.0f);
  // Model matrix : an identity matrix (model will be at the origin)
  mat4x4 Model;
  mat4x4Identity(Model);
  mat4x4RotateX(Model, rotate_x_deg*(M_PI/180.0f));
  mat4x4RotateY(Model, rotate_y_deg*(M_PI/180.0f));
  mat4x4RotateZ(Model, rotate_z_deg*(M_PI/180.0f));
  // Camera matrix
  mat4x4 View;
  mat4x4LookAt(View, (vec3){4.0f,3.0f,-3.0f}, // Camera is at (4,3,-3), in World Space
                     (vec3){0.0,0,0.0}, // and looks at the origin
                     (vec3){0.0,1.0,0.0}  // Head is up (set to 0,-1,0 to look upside-down)
  );

  // Our ModelViewProjection : multiplication of our 3 matrices
  mat4x4 VP;
  mat4x4MultiplyMatrix(VP, View, Projection); // Remember, matrix multiplication is the other way around
  mat4x4MultiplyMatrix(MVP, Model, VP); // Remember, matrix multiplication is the other way around

  const int n_vertices = 36;

  // Our vertices. Tree consecutive floats give a 3D vertex; Three consecutive vertices give a triangle.
  // A cube has 6 faces with 2 triangles each, so this makes 6*2=12 triangles, and 12*3 vertices
  static const GLfloat g_vertex_buffer_data[n_vertices*3] = {
    -1.0f,-1.0f,-1.0f,
    -1.0f,-1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,

    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,-1.0f,


     1.0f, 1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f,-1.0f,

     1.0f, 1.0f,-1.0f,
     1.0f,-1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,


     1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f,-1.0f,
     1.0f,-1.0f,-1.0f,

     1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f,-1.0f,


    -1.0f, 1.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,
     1.0f,-1.0f, 1.0f,

     1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
     1.0f,-1.0f, 1.0f,


     1.0f, 1.0f, 1.0f,
     1.0f,-1.0f,-1.0f,
     1.0f, 1.0f,-1.0f,

     1.0f,-1.0f,-1.0f,
     1.0f, 1.0f, 1.0f,
     1.0f,-1.0f, 1.0f,


     1.0f, 1.0f, 1.0f,
     1.0f, 1.0f,-1.0f,
    -1.0f, 1.0f,-1.0f,

     1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,-1.0f,
    -1.0f, 1.0f, 1.0f
  };

  // One color for each vertex. They were generated randomly.
  GLfloat g_color_buffer_data[n_vertices*3];

  for (int colour_n = 0; colour_n < n_vertices; ++colour_n)
  {
    g_color_buffer_data[(colour_n*3)+0] = colours[colour_n/6].x;
    g_color_buffer_data[(colour_n*3)+1] = colours[colour_n/6].y;
    g_color_buffer_data[(colour_n*3)+2] = colours[colour_n/6].z;
  }

  glGenBuffers(1, &vertexbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

  glGenBuffers(1, &colorbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_color_buffer_data), g_color_buffer_data, GL_STATIC_DRAW);

  // Clear the screen
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Use our shader
  glUseProgram(programID);

  // Send our transformation to the currently bound shader,
  // in the "MVP" uniform
  glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

  // 1rst attribute buffer : vertices
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
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
  glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
  glVertexAttribPointer(
    1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
    3,                                // size
    GL_FLOAT,                         // type
    GL_FALSE,                         // normalized?
    0,                                // stride
    (void*)0                          // array buffer offset
  );

  // Draw the triangle !
  glDrawArrays(GL_TRIANGLES, 0, 12*3); // 12*3 indices starting at 0 -> 12 triangles

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);


  // Cleanup VBO and shader
  glDeleteBuffers(1, &vertexbuffer);
  glDeleteBuffers(1, &colorbuffer);

  ImGui::End();
}