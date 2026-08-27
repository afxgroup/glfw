//========================================================================
// GLFW 3.4 WGL - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2019 Camilla Löwy <elmindreda@glfw.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================
// Please use C89 style variable declarations in this file because VS 2010
//========================================================================

#include "internal.h"

#include <stdlib.h>
#include <stdio.h>

#include <proto/mesa.h>
#include <proto/graphics.h>
#include <mesa/mesa_defs.h>

static const char* getStatusString(MesaStatus status)
{
    switch (status)
    {
        case MESA_STATUS_OK:                   return "OK";
        case MESA_STATUS_BAD_ARGUMENT:         return "bad argument";
        case MESA_STATUS_NO_MEMORY:            return "out of memory";
        case MESA_STATUS_INVALID_STATE:        return "invalid state";
        case MESA_STATUS_BUSY:                 return "busy";
        case MESA_STATUS_UNSUPPORTED:          return "unsupported";
        case MESA_STATUS_NATIVE_QUERY_FAILED:  return "native query failed";
        case MESA_STATUS_SCREEN_UNAVAILABLE:   return "screen unavailable";
        case MESA_STATUS_SCREEN_MISMATCH:      return "screen mismatch";
        case MESA_STATUS_CONTEXT_FAILED:       return "context creation failed";
        case MESA_STATUS_MAKE_CURRENT_FAILED:  return "make current failed";
        case MESA_STATUS_DRAWABLE_CHANGED:     return "drawable changed";
        case MESA_STATUS_RESOURCE_FAILED:      return "resource allocation failed";
        case MESA_STATUS_GPU_FAILED:           return "GPU failure";
        case MESA_STATUS_PRESENT_FAILED:       return "present failed";
        default:                               return "unknown error";
    }
}

// Reports a failed mesa.library call through the GLFW error path
//
static void reportStatus(const char* operation, MesaStatus status)
{
    dprintf("%s failed: %s (%d)\n", operation, getStatusString(status), (int) status);

    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "OS4: %s failed: %s",
                    operation, getStatusString(status));
}

// Destroys the drawable owned by the window, if any
//
static void destroyDrawable(_GLFWwindow* window)
{
    if (window->context.gl.drawable)
    {
        const MesaStatus status =
            IMesa->MesaDestroyDrawable(window->context.gl.drawable);

        if (status != MESA_STATUS_OK)
            reportStatus("MesaDestroyDrawable", status);

        window->context.gl.drawable = NULL;
        window->context.gl.drawableWindow = NULL;
    }
}

// Creates a mesa.library drawable for the Intuition window
//
static GLFWbool createDrawable(_GLFWwindow* window,
                               const _GLFWfbconfig* fbconfig)
{
    struct TagItem drawableTags[10];
    MesaDrawable drawable = NULL;
    MesaStatus status;
    int count = 0;

    if (!window->os4.handle)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "OS4: Cannot create a drawable without a native window");
        return GLFW_FALSE;
    }

    // A bit count of zero (or GLFW_DONT_CARE) means "let mesa.library pick the
    // format that suits this window", so only forward the values the
    // application actually asked for.
    if (fbconfig)
    {
        if (fbconfig->redBits != GLFW_DONT_CARE && fbconfig->redBits > 0)
        {
            drawableTags[count].ti_Tag  = MESA_DRAWABLE_RED_BITS;
            drawableTags[count].ti_Data = (ULONG) fbconfig->redBits;
            count++;
        }
        if (fbconfig->greenBits != GLFW_DONT_CARE && fbconfig->greenBits > 0)
        {
            drawableTags[count].ti_Tag  = MESA_DRAWABLE_GREEN_BITS;
            drawableTags[count].ti_Data = (ULONG) fbconfig->greenBits;
            count++;
        }
        if (fbconfig->blueBits != GLFW_DONT_CARE && fbconfig->blueBits > 0)
        {
            drawableTags[count].ti_Tag  = MESA_DRAWABLE_BLUE_BITS;
            drawableTags[count].ti_Data = (ULONG) fbconfig->blueBits;
            count++;
        }
        if (fbconfig->alphaBits != GLFW_DONT_CARE && fbconfig->alphaBits > 0)
        {
            drawableTags[count].ti_Tag  = MESA_DRAWABLE_ALPHA_BITS;
            drawableTags[count].ti_Data = (ULONG) fbconfig->alphaBits;
            count++;
        }
        if (fbconfig->depthBits != GLFW_DONT_CARE && fbconfig->depthBits > 0)
        {
            drawableTags[count].ti_Tag  = MESA_DRAWABLE_DEPTH_BITS;
            drawableTags[count].ti_Data = (ULONG) fbconfig->depthBits;
            count++;
        }
        if (fbconfig->stencilBits != GLFW_DONT_CARE && fbconfig->stencilBits > 0)
        {
            drawableTags[count].ti_Tag  = MESA_DRAWABLE_STENCIL_BITS;
            drawableTags[count].ti_Data = (ULONG) fbconfig->stencilBits;
            count++;
        }
        if (fbconfig->samples != GLFW_DONT_CARE && fbconfig->samples > 0)
        {
            drawableTags[count].ti_Tag  = MESA_DRAWABLE_SAMPLES;
            drawableTags[count].ti_Data = (ULONG) fbconfig->samples;
            count++;
        }

        drawableTags[count].ti_Tag  = MESA_DRAWABLE_DOUBLE_BUFFERED;
        drawableTags[count].ti_Data = fbconfig->doublebuffer ? TRUE : FALSE;
        count++;
    }

    drawableTags[count].ti_Tag  = TAG_DONE;
    drawableTags[count].ti_Data = 0;

    status = IMesa->MesaCreateWindowDrawable(window->os4.handle,
                                             drawableTags,
                                             &drawable);
    if (status != MESA_STATUS_OK)
    {
        reportStatus("MesaCreateWindowDrawable", status);
        return GLFW_FALSE;
    }

    window->context.gl.drawable = drawable;
    window->context.gl.drawableWindow = window->os4.handle;

    dprintf("Created drawable %p for window handle %p\n",
            drawable, window->os4.handle);

    return GLFW_TRUE;
}

// Translates the GLFW context request into the mesa.library API and profile
//
static GLFWbool translateContextRequest(const _GLFWctxconfig* ctxconfig,
                                        MesaAPI* api,
                                        MesaProfile* profile)
{
    if (ctxconfig->client == GLFW_OPENGL_ES_API)
    {
        if (!((ctxconfig->major == 2 && ctxconfig->minor == 0) ||
              (ctxconfig->major == 3 && ctxconfig->minor >= 0 && ctxconfig->minor <= 2)))
        {
            _glfwInputError(GLFW_VERSION_UNAVAILABLE,
                            "OS4: mesa.library does not support OpenGL ES %i.%i",
                            ctxconfig->major, ctxconfig->minor);
            return GLFW_FALSE;
        }

        *api = MESA_API_GLES2;
        *profile = MESA_PROFILE_ES;
        return GLFW_TRUE;
    }

    if (ctxconfig->profile == GLFW_OPENGL_CORE_PROFILE)
    {
        if (ctxconfig->major < 3 || (ctxconfig->major == 3 && ctxconfig->minor < 2))
        {
            _glfwInputError(GLFW_VERSION_UNAVAILABLE,
                            "OS4: Core profile requires OpenGL 3.2 or greater");
            return GLFW_FALSE;
        }

        *profile = MESA_PROFILE_CORE;
    }
    else
        *profile = MESA_PROFILE_COMPATIBILITY;

    *api = MESA_API_OPENGL;
    return GLFW_TRUE;
}

static void makeContextCurrentGL(_GLFWwindow* window)
{
    MesaStatus status;

    if (window)
    {
        if (!window->context.gl.drawable)
        {
            _glfwInputError(GLFW_PLATFORM_ERROR,
                            "OS4: Window has no mesa.library drawable");
            return;
        }

        status = IMesa->MesaMakeCurrent(window->context.gl.glContext,
                                        window->context.gl.drawable,
                                        window->context.gl.drawable);
        if (status != MESA_STATUS_OK)
        {
            reportStatus("MesaMakeCurrent", status);
            return;
        }

        _glfwPlatformSetTls(&_glfw.contextSlot, window);
    }
    else
    {
        status = IMesa->MesaUnbindCurrent();
        if (status != MESA_STATUS_OK)
            reportStatus("MesaUnbindCurrent", status);

        _glfwPlatformSetTls(&_glfw.contextSlot, NULL);
    }
}

static void destroyContextGL(_GLFWwindow* window)
{
    // The context must go away before the drawable it was created from, and
    // both must go away before the Intuition window is closed.
    if (window->context.gl.glContext)
    {
        const MesaStatus status =
            IMesa->MesaDestroyContext(window->context.gl.glContext);

        if (status != MESA_STATUS_OK)
            reportStatus("MesaDestroyContext", status);

        window->context.gl.glContext = NULL;
    }

    destroyDrawable(window);
}

static void swapBuffersGL(_GLFWwindow* window)
{
    MesaStatus status;

    if (!window->context.gl.drawable)
        return;

    // First flush the render pipeline, so that everything gets drawn
    glFinish();

    if (window->context.gl.vsyncEnabled) {
        IGraphics->WaitTOF();
    }

    // Swap the buffers (if any)
    status = IMesa->MesaSwapBuffers(window->context.gl.drawable);
    if (status != MESA_STATUS_OK)
        reportStatus("MesaSwapBuffers", status);
}

static GLFWglproc getProcAddressGL(const char* procname)
{
    dprintf("Searching for %s\n", procname);
    const GLFWglproc proc = (GLFWglproc) IMesa->MesaGetProcAddress(procname);
    return proc;
}

static int extensionSupportedGL(const char* extension)
{
    // TODO - Implement this
    return GLFW_FALSE;
}

static void swapIntervalGL(int interval)
{
    _GLFWwindow* window = _glfwPlatformGetTls(&_glfw.contextSlot);

    if (!window)
        return;

    switch (interval) {
        case 0:
        case 1:
            window->context.gl.vsyncEnabled = interval ? TRUE : FALSE;
            dprintf("VSYNC %d\n", interval);
            break;
        default:
            dprintf("Unsupported interval %d\n", interval);
            break;
    }
}

// Recreates the drawable after the Intuition window pointer has changed
//
GLFWbool _glfwResizeContextGL(_GLFWwindow* window)
{
    MesaStatus status;

    if (!window->context.gl.glContext)
        return GLFW_TRUE;

    // mesa.library revalidates the native window size on its next use of the
    // drawable, so only an actual window replacement needs handling here.
    if (window->context.gl.drawableWindow == window->os4.handle)
        return GLFW_TRUE;

    destroyDrawable(window);

    if (!createDrawable(window, NULL))
        return GLFW_FALSE;

    status = IMesa->MesaMakeCurrent(window->context.gl.glContext,
                                    window->context.gl.drawable,
                                    window->context.gl.drawable);
    if (status != MESA_STATUS_OK)
    {
        reportStatus("MesaMakeCurrent", status);
        return GLFW_FALSE;
    }

    return GLFW_TRUE;
}

// Create the OpenGL or OpenGL ES context
//
GLFWbool _glfwCreateContextGL(_GLFWwindow* window,
                               const _GLFWctxconfig* ctxconfig,
                               const _GLFWfbconfig* fbconfig)
{
    struct TagItem contextTags[8];
    MesaContext context = NULL;
    MesaContext share = NULL;
    MesaAPI api;
    MesaProfile profile;
    MesaStatus status;
    int count = 0;

    dprintf("redBits=%d\n", fbconfig->redBits);
    dprintf("greenBits=%d\n", fbconfig->greenBits);
    dprintf("blueBits=%d\n", fbconfig->blueBits);
    dprintf("alphaBits=%d\n", fbconfig->alphaBits);
    dprintf("depthBits=%d\n", fbconfig->depthBits);
    dprintf("stencilBits=%d\n", fbconfig->stencilBits);
    dprintf("accumRedBits=%d\n", fbconfig->accumRedBits);
    dprintf("accumGreenBits=%d\n", fbconfig->accumGreenBits);
    dprintf("accumBlueBits=%d\n", fbconfig->accumBlueBits);
    dprintf("accumAlphaBits=%d\n", fbconfig->accumAlphaBits);
    dprintf("auxBuffers=%d\n", fbconfig->auxBuffers);

    if (!IMesa)
    {
        _glfwInputError(GLFW_API_UNAVAILABLE, "OS4: mesa.library is not available");
        return GLFW_FALSE;
    }

    if (!translateContextRequest(ctxconfig, &api, &profile))
        return GLFW_FALSE;

    if (ctxconfig->share)
        share = ctxconfig->share->context.gl.glContext;

    dprintf("sharedContext = %p\n", share);

    // The drawable describes the visual and must exist before the context
    if (!createDrawable(window, fbconfig))
        return GLFW_FALSE;

    contextTags[count].ti_Tag  = MESA_CONTEXT_API;
    contextTags[count].ti_Data = (ULONG) api;
    count++;
    contextTags[count].ti_Tag  = MESA_CONTEXT_PROFILE;
    contextTags[count].ti_Data = (ULONG) profile;
    count++;
    contextTags[count].ti_Tag  = MESA_CONTEXT_MAJOR_VERSION;
    contextTags[count].ti_Data = (ULONG) ctxconfig->major;
    count++;
    contextTags[count].ti_Tag  = MESA_CONTEXT_MINOR_VERSION;
    contextTags[count].ti_Data = (ULONG) ctxconfig->minor;
    count++;
    // ABI version 1 only accepts zero here
    contextTags[count].ti_Tag  = MESA_CONTEXT_FLAGS;
    contextTags[count].ti_Data = 0;
    count++;

    if (share)
    {
        contextTags[count].ti_Tag  = MESA_CONTEXT_SHARE_WITH;
        contextTags[count].ti_Data = (ULONG) share;
        count++;
    }

    contextTags[count].ti_Tag  = TAG_DONE;
    contextTags[count].ti_Data = 0;

    status = IMesa->MesaCreateContext(window->context.gl.drawable,
                                      contextTags,
                                      &context);
    if (status != MESA_STATUS_OK)
    {
        reportStatus("MesaCreateContext", status);
        destroyDrawable(window);
        return GLFW_FALSE;
    }

    window->context.gl.glContext = context;
    window->context.client = (api == MESA_API_GLES2) ? GLFW_OPENGL_ES_API
                                                     : GLFW_OPENGL_API;

    dprintf("firstContext = %p\n", context);

    status = IMesa->MesaMakeCurrent(context,
                                    window->context.gl.drawable,
                                    window->context.gl.drawable);
    if (status != MESA_STATUS_OK)
    {
        reportStatus("MesaMakeCurrent", status);
        IMesa->MesaDestroyContext(context);
        window->context.gl.glContext = NULL;
        destroyDrawable(window);
        return GLFW_FALSE;
    }

    _glfwPlatformSetTls(&_glfw.contextSlot, window);

    dprintf("GL Extensions: %s\n", glGetString(GL_EXTENSIONS));

    // Some games (like q3) doesn't clear the z-buffer prior to use. Since we're using a floating-point depth buffer in warp3dnova,
    // that means it may contain illegal floating-point values, which causes some pixels to fail the depth-test when they shouldn't,
    // so we clear the depth buffer to a constant value when it's first created.
    // Pandora may well use an integer depth-buffer, in which case this can't happen.
    // On MiniGL it didn't happens as there is workaround inside of old warp3d (and probabaly inside of MiniGL itself too).
    // in SDL1 with gl4es (so warp3dnova/ogles2, where no such workaround) it didn't happens probabaly because SDL1 doing something like that (but not glClear).

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, window->os4.width, window->os4.height);

    dprintf("Creating context %p for window handle %p\n",
            context, window->os4.handle);

    window->context.makeCurrent = makeContextCurrentGL;
    window->context.swapBuffers = swapBuffersGL;
    window->context.swapInterval = swapIntervalGL;
    window->context.extensionSupported = extensionSupportedGL;
    window->context.getProcAddress = getProcAddressGL;
    window->context.destroy = destroyContextGL;

    return GLFW_TRUE;
}
