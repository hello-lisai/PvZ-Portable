#ifndef __GLPLATFORM_H__
#define __GLPLATFORM_H__

#include <glad/gles2.h>

#define GLSL_VERT_PREAMBLE \
	"#version 100\n" \
	"precision mediump float;\n" \
	"#define VERT_IN attribute\n" \
	"#define V2F varying\n"

#define GLSL_FRAG_PREAMBLE \
	"#version 100\n" \
	"precision mediump float;\n" \
	"#define V2F varying\n" \
	"#define FRAG_OUT gl_FragColor\n" \
	"#define TEX2D texture2D\n"

#ifdef NINTENDO_SWITCH

#include <switch.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

inline void PlatformGLInit()
{
	gladLoadGLES2((GLADloadfunc)eglGetProcAddress);
}

#else

#include <SDL.h>

inline void PlatformGLInit()
{
	gladLoadGLES2((GLADloadfunc)SDL_GL_GetProcAddress);
}

#endif

#endif // __GLPLATFORM_H__
