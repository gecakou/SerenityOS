/*
 * Copyright (c) 2021, Jesse Buhagiar <jooster669@gmail.com>
 * Copyright (c) 2021, Mathieu Gaillard <gaillard.mathieu.39@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGL/GL/gl.h>

// Point in 3D space
struct Vertex {
    GLfloat x;
    GLfloat y;
    GLfloat z;
};

// A triangle defines a single "face" of a mesh
struct Triangle {
    GLuint a;
    GLuint b;
    GLuint c;
};
