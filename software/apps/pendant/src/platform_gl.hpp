#pragma once

#if defined(ROBERTA_PENDANT_USE_GLES2)
#include <SDL_opengles2.h>
#else
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <SDL_opengl.h>
#endif
