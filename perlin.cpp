#include "random.h"
#include "ccVector.h"


vec2
random_unit_vector(int seed)
{
  float theta = random_0_1_float(seed) * 2.0*M_PI;
  vec2 result;
  result.x = cos(theta);
  result.y = sin(theta);
  return result;
}


int
hash_vector(vec2 vector)
{
  int result = vector.x*2521 + vector.y*7043;
  return result;
}


float
lerp(float a, float b, float t)
{
  float result = (t * a) + ((1-t) * b);
  return result;
}


float
perlin(vec2 input, float period)
{
  float result;

  input = vec2Multiply(input, 1.0/period);

  vec2 integer_coord = {(float)floor(input.x), (float)floor(input.y)};
  vec2 fractional = vec2Subtract(input, integer_coord);

  vec2 p00 = vec2Add(integer_coord, {0, 0});
  vec2 p10 = vec2Add(integer_coord, {1, 0});
  vec2 p01 = vec2Add(integer_coord, {0, 1});
  vec2 p11 = vec2Add(integer_coord, {1, 1});

  vec2 gradient00 = random_unit_vector(hash_vector(p00));
  vec2 gradient10 = random_unit_vector(hash_vector(p10));
  vec2 gradient01 = random_unit_vector(hash_vector(p01));
  vec2 gradient11 = random_unit_vector(hash_vector(p11));

  float s = vec2DotProduct(gradient00, vec2Subtract(input, p00));
  float t = vec2DotProduct(gradient10, vec2Subtract(input, p10));
  float u = vec2DotProduct(gradient01, vec2Subtract(input, p01));
  float v = vec2DotProduct(gradient11, vec2Subtract(input, p11));

  float Sx = 3.0*pow(fractional.x, 2.0f) - 2.0*pow(fractional.x, 3.0f);
  float a = s + Sx*(t - s);
  float b = u + Sx*(v - u);

  float Sy = 3.0*pow(fractional.y, 2.0f) - 2.0*pow(fractional.y, 3.0f);
  result = a + Sy*(b - a);

  return result;
}