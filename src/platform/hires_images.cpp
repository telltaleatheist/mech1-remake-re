#include "platform/hires_images.h"

namespace platform {
namespace {
HiresImages* g_images = nullptr;
}  // namespace

void set_hires_images(HiresImages* images) { g_images = images; }
HiresImages* hires_images() { return g_images; }

}  // namespace platform
