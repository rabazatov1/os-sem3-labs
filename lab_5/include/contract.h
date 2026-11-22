#pragma once

typedef float (*sin_integral_fn)(float A, float B, float e);
typedef float (*derivative_fn)(float A, float delta_x);

typedef struct {
    sin_integral_fn integral;
    derivative_fn derivative;
    int implementation;
} math_api;

float SinIntegral(float A, float B, float e);
float Derivative(float A, float delta_x);
