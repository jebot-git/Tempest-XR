#include "xr.hpp"
#include "render.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#ifdef TEMPEST_OPENXR
#include <SDL_syswm.h>
#define XR_USE_PLATFORM_XLIB
#define XR_USE_PLATFORM_EGL
#define XR_USE_GRAPHICS_API_OPENGL
#include <EGL/egl.h>
#include <GL/glx.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace tempest {
static void check(XrResult r, const char *name) {
    if (XR_FAILED(r))
        throw std::runtime_error(std::string(name) + " (OpenXR " + std::to_string(r) + ")");
}
#define XR_CHECK(call) check(call, #call)
struct XR::Impl {
    XrInstance instance = XR_NULL_HANDLE;
    XrSession session = XR_NULL_HANDLE;
    XrSpace space = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    bool running = false, hasAnchor = false, quit = false, isFocused = false, visible = true;
    XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    struct Eye {
        XrSwapchain chain = XR_NULL_HANDLE;
        int width = 0, height = 0;
        std::vector<XrSwapchainImageOpenGLKHR> images;
        GLuint depth = 0;
    };
    std::vector<Eye> eyes;
    GLuint fbo = 0;
    XrVector3f anchor{};
    float yaw = 0;
    std::string runtime;
    uint64_t frames = 0, stereoFrames = 0;
    uint64_t warpFrames = 0, jumpFrames = 0;
    float maxWarpDistance = 0;
    ~Impl() {
        if (instance)
            std::cerr << "OpenXR frames=" << frames << " stereo_frames=" << stereoFrames
                      << " warp_frames=" << warpFrames << " max_warp_distance=" << maxWarpDistance
                      << " jump_frames=" << jumpFrames << '\n';
        if (fbo)
            glDeleteFramebuffers(1, &fbo);
        for (auto &e : eyes) {
            if (e.depth)
                glDeleteRenderbuffers(1, &e.depth);
            if (e.chain)
                xrDestroySwapchain(e.chain);
        }
        if (space)
            xrDestroySpace(space);
        if (session)
            xrDestroySession(session);
        if (instance)
            xrDestroyInstance(instance);
    }
};
XR::XR() : p(std::make_unique<Impl>()) {}
XR::~XR() = default;
void XR::initialize(SDL_Window *window) {
    SDL_SysWMinfo wm{};
    SDL_VERSION(&wm.version);
    auto &x = *p;
    bool useEgl = eglGetCurrentContext() != EGL_NO_CONTEXT;
    bool hasWindowInfo = SDL_GetWindowWMInfo(window, &wm);
    if (!useEgl && !hasWindowInfo)
        throw std::runtime_error("SDL could not identify the native window");
    if (!useEgl && wm.subsystem != SDL_SYSWM_X11)
        throw std::runtime_error("XR requires a native EGL or X11/GLX OpenGL context");
    const char *extensions[] = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
                                XR_MNDX_EGL_ENABLE_EXTENSION_NAME};
    XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strcpy(ci.applicationInfo.applicationName, "Tempest XR");
    ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    ci.enabledExtensionCount = useEgl ? 2 : 1;
    ci.enabledExtensionNames = extensions;
    XR_CHECK(xrCreateInstance(&ci, &x.instance));
    XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
    XR_CHECK(xrGetInstanceProperties(x.instance, &properties));
    x.runtime = properties.runtimeName;
    XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};
    si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XR_CHECK(xrGetSystem(x.instance, &si, &x.system));
    PFN_xrGetOpenGLGraphicsRequirementsKHR requirementsFn = nullptr;
    XR_CHECK(xrGetInstanceProcAddr(x.instance, "xrGetOpenGLGraphicsRequirementsKHR",
                                   reinterpret_cast<PFN_xrVoidFunction *>(&requirementsFn)));
    XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    XR_CHECK(requirementsFn(x.instance, x.system, &requirements));
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    auto version = XR_MAKE_VERSION(major, minor, 0);
    if (version < requirements.minApiVersionSupported ||
        version > requirements.maxApiVersionSupported)
        throw std::runtime_error("OpenGL version is outside the OpenXR runtime's supported range");
    XrGraphicsBindingOpenGLXlibKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    XrGraphicsBindingEGLMNDX eglBinding{XR_TYPE_GRAPHICS_BINDING_EGL_MNDX};
    if (useEgl) {
        eglBinding.display = eglGetCurrentDisplay();
        eglBinding.context = eglGetCurrentContext();
        eglBinding.getProcAddress =
            reinterpret_cast<PFN_xrEglGetProcAddressMNDX>(eglGetProcAddress);
        EGLint configId = 0, found = 0;
        eglQueryContext(eglBinding.display, eglBinding.context, EGL_CONFIG_ID, &configId);
        EGLint attributes[] = {EGL_CONFIG_ID, configId, EGL_NONE};
        if (!eglChooseConfig(eglBinding.display, attributes, &eglBinding.config, 1, &found) ||
            !found)
            throw std::runtime_error("Could not identify SDL's EGL configuration");
    } else {
        Display *display = wm.info.x11.display;
        GLXContext context = glXGetCurrentContext();
        int id = 0;
        glXQueryContext(display, context, GLX_FBCONFIG_ID, &id);
        int count = 0;
        auto configs = glXGetFBConfigs(display, DefaultScreen(display), &count);
        GLXFBConfig config = nullptr;
        VisualID visual = 0;
        for (int i = 0; i < count; i++) {
            int candidate = 0;
            glXGetFBConfigAttrib(display, configs[i], GLX_FBCONFIG_ID, &candidate);
            if (candidate == id) {
                config = configs[i];
                auto vi = glXGetVisualFromFBConfig(display, config);
                if (vi) {
                    visual = vi->visualid;
                    XFree(vi);
                }
                break;
            }
        }
        XFree(configs);
        if (!config || !context)
            throw std::runtime_error("Could not identify the SDL GLX context");
        binding.xDisplay = display;
        binding.visualid = visual;
        binding.glxFBConfig = config;
        binding.glxDrawable = glXGetCurrentDrawable();
        binding.glxContext = context;
    }
    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = useEgl ? static_cast<void *>(&eglBinding) : static_cast<void *>(&binding);
    sessionInfo.systemId = x.system;
    XR_CHECK(xrCreateSession(x.instance, &sessionInfo, &x.session));
    XrReferenceSpaceCreateInfo ri{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    ri.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    ri.poseInReferenceSpace.orientation.w = 1;
    XR_CHECK(xrCreateReferenceSpace(x.session, &ri, &x.space));
    uint32_t n = 0;
    XR_CHECK(xrEnumerateEnvironmentBlendModes(
        x.instance, x.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &n, nullptr));
    std::vector<XrEnvironmentBlendMode> blends(n);
    XR_CHECK(xrEnumerateEnvironmentBlendModes(
        x.instance, x.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, n, &n, blends.data()));
    if (blends.empty())
        throw std::runtime_error("No OpenXR blend modes");
    x.blend = blends.front();
    XR_CHECK(xrEnumerateSwapchainFormats(x.session, 0, &n, nullptr));
    std::vector<int64_t> formats(n);
    XR_CHECK(xrEnumerateSwapchainFormats(x.session, n, &n, formats.data()));
    int64_t format = 0;
    for (auto wanted : {GL_RGBA8, GL_SRGB8_ALPHA8})
        if (std::find(formats.begin(), formats.end(), wanted) != formats.end()) {
            format = wanted;
            break;
        }
    if (!format)
        throw std::runtime_error("No supported OpenGL RGBA swapchain format");
    XR_CHECK(xrEnumerateViewConfigurationViews(
        x.instance, x.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &n, nullptr));
    std::vector<XrViewConfigurationView> views(n, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    XR_CHECK(xrEnumerateViewConfigurationViews(
        x.instance, x.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, n, &n, views.data()));
    if (n != 2)
        throw std::runtime_error("Prototype requires stereo OpenXR views");
    x.eyes.resize(n);
    for (size_t i = 0; i < x.eyes.size(); i++) {
        auto &e = x.eyes[i];
        e.width = int(views[i].recommendedImageRectWidth);
        e.height = int(views[i].recommendedImageRectHeight);
        XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        sc.format = format;
        sc.sampleCount = 1;
        sc.width = e.width;
        sc.height = e.height;
        sc.faceCount = 1;
        sc.arraySize = 1;
        sc.mipCount = 1;
        XR_CHECK(xrCreateSwapchain(x.session, &sc, &e.chain));
        XR_CHECK(xrEnumerateSwapchainImages(e.chain, 0, &n, nullptr));
        e.images.resize(n, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
        XR_CHECK(xrEnumerateSwapchainImages(
            e.chain, n, &n, reinterpret_cast<XrSwapchainImageBaseHeader *>(e.images.data())));
        glGenRenderbuffers(1, &e.depth);
        glBindRenderbuffer(GL_RENDERBUFFER, e.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, e.width, e.height);
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glGenFramebuffers(1, &x.fbo);
    std::cerr << "OpenXR initialized: " << x.runtime << " / " << x.eyes[0].width << "x"
              << x.eyes[0].height << " per eye\n";
}
// Inverse rigid pose, OpenGL column-major.
static void inversePose(const XrPosef &pose) {
    auto q = pose.orientation;
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, xy = q.x * q.y, xz = q.x * q.z,
          yz = q.y * q.z, wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    GLfloat m[] = {1 - 2 * (yy + zz),
                   2 * (xy - wz),
                   2 * (xz + wy),
                   0,
                   2 * (xy + wz),
                   1 - 2 * (xx + zz),
                   2 * (yz - wx),
                   0,
                   2 * (xz - wy),
                   2 * (yz + wx),
                   1 - 2 * (xx + yy),
                   0,
                   0,
                   0,
                   0,
                   1};
    glMultMatrixf(m);
    glTranslatef(-pose.position.x, -pose.position.y, -pose.position.z);
}
bool XR::frame(Renderer &renderer, const Game &game) {
    auto &x = *p;
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    XrResult poll;
    while ((poll = xrPollEvent(x.instance, &event)) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto state = reinterpret_cast<XrEventDataSessionStateChanged &>(event).state;
            x.isFocused = state == XR_SESSION_STATE_FOCUSED;
            if (state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                XR_CHECK(xrBeginSession(x.session, &bi));
                x.running = true;
            }
            if (state == XR_SESSION_STATE_STOPPING) {
                x.running = false;
                XR_CHECK(xrEndSession(x.session));
            }
            if (state == XR_SESSION_STATE_EXITING || state == XR_SESSION_STATE_LOSS_PENDING)
                x.quit = true;
        } else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
            x.quit = true;
        else if (event.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING)
            x.hasAnchor = false;
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
    if (poll != XR_EVENT_UNAVAILABLE)
        check(poll, "xrPollEvent");
    if (x.quit)
        return false;
    if (!x.running)
        return true;
    XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState fs{XR_TYPE_FRAME_STATE};
    XR_CHECK(xrWaitFrame(x.session, &wi, &fs));
    XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
    XR_CHECK(xrBeginFrame(x.session, &bi));
    std::array<XrView, 2> views{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
    std::array<XrCompositionLayerProjectionView, 2> projectionViews{
        {{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
    XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
    li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    li.displayTime = fs.predictedDisplayTime;
    li.space = x.space;
    XrViewState vs{XR_TYPE_VIEW_STATE};
    uint32_t n = 0;
    XR_CHECK(xrLocateViews(x.session, &li, &vs, 2, &n, views.data()));
    bool render = x.visible && fs.shouldRender && n == 2 &&
                  (vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) &&
                  (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);
    if (render) {
        if (game.transitionDepth() > 0)
            x.warpFrames++;
        if (game.jumpTime > 0)
            x.jumpFrames++;
        x.maxWarpDistance = std::max(x.maxWarpDistance, game.warpDistance());
        if (!x.hasAnchor) {
            x.anchor = {(views[0].pose.position.x + views[1].pose.position.x) * .5f,
                        (views[0].pose.position.y + views[1].pose.position.y) * .5f,
                        (views[0].pose.position.z + views[1].pose.position.z) * .5f};
            auto q = views[0].pose.orientation;
            x.yaw = std::atan2(2 * (q.w * q.y + q.x * q.z), 1 - 2 * (q.x * q.x + q.y * q.y)) * 180 /
                    3.14159265f;
            x.hasAnchor = true;
        }
        for (int i = 0; i < 2; i++) {
            auto &e = x.eyes[i];
            uint32_t index = 0;
            XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            XR_CHECK(xrAcquireSwapchainImage(e.chain, &ai, &index));
            XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            wait.timeout = XR_INFINITE_DURATION;
            XR_CHECK(xrWaitSwapchainImage(e.chain, &wait));
            glBindFramebuffer(GL_FRAMEBUFFER, x.fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   e.images[index].image, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                      e.depth);
            auto framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error(
                    "OpenXR eye framebuffer is incomplete: status=" +
                    std::to_string(framebufferStatus) + " glError=" + std::to_string(glGetError()) +
                    " texture=" + std::to_string(e.images[index].image) +
                    " isTexture=" + std::to_string(glIsTexture(e.images[index].image)));
            glViewport(0, 0, e.width, e.height);
            glClearColor(.006f, .009f, .025f, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            auto f = views[i].fov;
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glFrustum(std::tan(f.angleLeft) * .05, std::tan(f.angleRight) * .05,
                      std::tan(f.angleDown) * .05, std::tan(f.angleUp) * .05, .05, 150);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            inversePose(views[i].pose);
            glTranslatef(x.anchor.x, x.anchor.y, x.anchor.z);
            glRotatef(x.yaw, 0, 1, 0);
            renderer.world(game, true);
            renderer.finishFrame(game, i, 0, 0, e.width, e.height);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glFlush();
            XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            XR_CHECK(xrReleaseSwapchainImage(e.chain, &release));
            auto &pv = projectionViews[i];
            pv.pose = views[i].pose;
            pv.fov = views[i].fov;
            pv.subImage.swapchain = e.chain;
            pv.subImage.imageRect.extent = {e.width, e.height};
        }
    }
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = x.space;
    layer.viewCount = 2;
    layer.views = projectionViews.data();
    const XrCompositionLayerBaseHeader *layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader *>(&layer)};
    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
    end.displayTime = fs.predictedDisplayTime;
    end.environmentBlendMode = x.blend;
    end.layerCount = render ? 1 : 0;
    end.layers = render ? layers : nullptr;
    XR_CHECK(xrEndFrame(x.session, &end));
    x.frames++;
    if (render)
        x.stereoFrames++;
    return true;
}
void XR::setVisible(bool visible) { p->visible=visible; }
void XR::recenter() {
    p->hasAnchor = false;
}
bool XR::focused() const {
    return p->isFocused;
}
std::string XR::description() const {
    return p->runtime;
}
} // namespace tempest
#else
namespace tempest {
struct XR::Impl {};
XR::XR() : p(std::make_unique<Impl>()) {}
XR::~XR() = default;
void XR::initialize(SDL_Window *) {
    throw std::runtime_error("Built without OpenXR. Rebuild with -DTEMPEST_OPENXR=ON.");
}
bool XR::frame(Renderer &, const Game &) {
    return false;
}
void XR::setVisible(bool) {}
void XR::recenter() {}
bool XR::focused() const {
    return false;
}
std::string XR::description() const {
    return "unavailable";
}
} // namespace tempest
#endif
