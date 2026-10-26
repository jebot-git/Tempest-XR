// SPDX-License-Identifier: MIT
// Test-only API runtime adapted from de-xr Comic Chat's isolated mock.
// LD_PRELOAD applies only to the test child; no system runtime is contacted.
#define XR_USE_PLATFORM_EGL
#define XR_USE_GRAPHICS_API_OPENGL
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <GL/gl.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <fstream>
namespace {
struct Chain { GLuint texture=0;int width=0,height=0; };
struct Space { const char *action;int hand; };
unsigned frame=0;int pollStage=0,stereoFrames=0;
XrPosef tracked(int){return {{0,0,0,1},{0,1.6f,0}};}
XrResult requirements(XrInstance,XrSystemId,XrGraphicsRequirementsOpenGLKHR *out){
 out->minApiVersionSupported=XR_MAKE_VERSION(3,3,0);out->maxApiVersionSupported=XR_MAKE_VERSION(4,6,0);return XR_SUCCESS;
}
void state(XrEventDataBuffer *out,XrSessionState state){
 auto e=reinterpret_cast<XrEventDataSessionStateChanged *>(out);*e={XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};
 e->session=reinterpret_cast<XrSession>(2);e->state=state;
}
}
extern "C" {
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo *,
                                                XrInstance *out) {
  frame=0;pollStage=0;
  *out = reinterpret_cast<XrInstance>(1);
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyInstance(XrInstance) {
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
xrGetInstanceProperties(XrInstance, XrInstanceProperties *out) {
  strcpy(out->runtimeName, "Tempest isolated XR test");
  out->runtimeVersion = 1;
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance,
                                                     const char *name,
                                                     PFN_xrVoidFunction *out) {
  if (!strcmp(name, "xrGetOpenGLGraphicsRequirementsKHR")) {
    *out = reinterpret_cast<PFN_xrVoidFunction>(requirements);
    return XR_SUCCESS;
  }
  *out = nullptr;
  return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(XrInstance, const XrSystemGetInfo *,
                                           XrSystemId *out) {
  *out = 1;
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(XrInstance,
                                               const XrSessionCreateInfo *info,
                                               XrSession *out) {
  auto binding = static_cast<const XrGraphicsBindingEGLMNDX *>(info->next);
  if (!binding || binding->type != XR_TYPE_GRAPHICS_BINDING_EGL_MNDX)
    return XR_ERROR_GRAPHICS_DEVICE_INVALID;
  *out = reinterpret_cast<XrSession>(2);
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySession(XrSession) {
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateReferenceSpace(
    XrSession, const XrReferenceSpaceCreateInfo *, XrSpace *out) {
  *out = reinterpret_cast<XrSpace>(new Space{"head", -1});
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySpace(XrSpace space) {
  delete reinterpret_cast<Space *>(space);
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateEnvironmentBlendModes(
    XrInstance, XrSystemId, XrViewConfigurationType, uint32_t capacity,
    uint32_t *count, XrEnvironmentBlendMode *out) {
  *count = 1;
  if (capacity)
    out[0] = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurationViews(
    XrInstance, XrSystemId, XrViewConfigurationType, uint32_t capacity,
    uint32_t *count, XrViewConfigurationView *out) {
  *count = 2;
  if (capacity < 2)
    return capacity ? XR_ERROR_SIZE_INSUFFICIENT : XR_SUCCESS;
  for (int i = 0; i < 2; ++i) {
    out[i] = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
    out[i].recommendedImageRectWidth = out[i].recommendedImageRectHeight = 128;
    out[i].maxImageRectWidth = out[i].maxImageRectHeight = 128;
    out[i].recommendedSwapchainSampleCount = out[i].maxSwapchainSampleCount = 1;
  }
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainFormats(XrSession,
                                                           uint32_t capacity,
                                                           uint32_t *count,
                                                           int64_t *out) {
  *count = 1;
  if (capacity)
    out[0] = GL_SRGB8_ALPHA8;
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(
    XrSession, const XrSwapchainCreateInfo *info, XrSwapchain *out) {
  auto c = new Chain;
  c->width = info->width;
  c->height = info->height;
  glGenTextures(1, &c->texture);
  glBindTexture(GL_TEXTURE_2D, c->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, c->width, c->height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  *out = reinterpret_cast<XrSwapchain>(c);
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain value) {
  auto c = reinterpret_cast<Chain *>(value);
  glDeleteTextures(1, &c->texture);
  delete c;
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
xrEnumerateSwapchainImages(XrSwapchain value, uint32_t capacity,
                           uint32_t *count, XrSwapchainImageBaseHeader *out) {
  *count = 1;
  if (capacity)
    reinterpret_cast<XrSwapchainImageOpenGLKHR *>(out)->image =
        reinterpret_cast<Chain *>(value)->texture;
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrPollEvent(XrInstance, XrEventDataBuffer *out) {
  if (pollStage == 0) {
    ++pollStage;
    state(out, XR_SESSION_STATE_READY);
    return XR_SUCCESS;
  }
  if (pollStage == 1) {
    ++pollStage;
    state(out, XR_SESSION_STATE_FOCUSED);
    return XR_SUCCESS;
  }
  return XR_EVENT_UNAVAILABLE;
}

XRAPI_ATTR XrResult XRAPI_CALL xrBeginSession(XrSession,
                                              const XrSessionBeginInfo *) {
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEndSession(XrSession) { return XR_SUCCESS; }

XRAPI_ATTR XrResult XRAPI_CALL xrWaitFrame(XrSession, const XrFrameWaitInfo *,
                                           XrFrameState *out) {
  std::this_thread::sleep_for(std::chrono::milliseconds(8));
  ++frame;
  out->predictedDisplayTime = frame * 11111111LL;
  out->predictedDisplayPeriod = 11111111;
  out->shouldRender = XR_TRUE;
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrBeginFrame(XrSession,
                                            const XrFrameBeginInfo *) {
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrLocateViews(XrSession,
                                             const XrViewLocateInfo *,
                                             XrViewState *out,
                                             uint32_t capacity, uint32_t *count,
                                             XrView *views) {
  *count = 2;
  if (capacity < 2)
    return XR_ERROR_SIZE_INSUFFICIENT;
  out->viewStateFlags =
      XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
  for (int i = 0; i < 2; ++i) {
    views[i].pose = tracked(-1);
    views[i].pose.position.x = i ? .032f : -.032f;
    views[i].fov = {-.65f, .65f, .65f, -.65f};
  }
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(
    XrSwapchain, const XrSwapchainImageAcquireInfo *, uint32_t *out) {
  *out = 0;
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
xrWaitSwapchainImage(XrSwapchain, const XrSwapchainImageWaitInfo *) {
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
xrReleaseSwapchainImage(XrSwapchain, const XrSwapchainImageReleaseInfo *) {
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEndFrame(XrSession,
                                          const XrFrameEndInfo *info) {
  if (const char *path = std::getenv("TEMPEST_TEST_FRAMES")) {
    static std::ofstream log(path, std::ios::app);
    log.precision(16);
    log << std::chrono::duration<double>(
               std::chrono::steady_clock::now().time_since_epoch())
               .count()
        << " " << info->layerCount << "\n";
    log.flush();
  }
  if (info->layerCount) {
    auto layer =
        reinterpret_cast<const XrCompositionLayerProjection *>(info->layers[0]);
    if (layer->viewCount != 2)
      return XR_ERROR_VALIDATION_FAILURE;
    ++stereoFrames;
  }
  return XR_SUCCESS;
}
}
