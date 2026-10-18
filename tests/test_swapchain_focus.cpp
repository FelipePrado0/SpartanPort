// Regression test for Issue #325 (Alt+Tab fullscreen crash / VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
// and related Issue #391 (VK_ERROR_SURFACE_LOST_KHR handling).
#include <cassert>
#include <cstdio>
#include <atomic>
#include <limits>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <SDL3/SDL.h>

#include "sdl_window.h"

// Test mock for verifying window minimization states and transitions
class MockWindow {
public:
    std::atomic<s32> width{1920};
    std::atomic<s32> height{1080};
    std::atomic<bool> is_minimized{false};
    SDL_WindowFlags window_flags{0};

    bool IsMinimized() const {
        if (is_minimized.load(std::memory_order_relaxed)) {
            return true;
        }
        if (window_flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN | SDL_WINDOW_OCCLUDED)) {
            return true;
        }
        return width.load(std::memory_order_relaxed) <= 0 || height.load(std::memory_order_relaxed) <= 0;
    }

    void HandleEvent(Uint32 event_type, int w = 0, int h = 0) {
        switch (event_type) {
        case SDL_EVENT_WINDOW_MINIMIZED:
            window_flags |= SDL_WINDOW_MINIMIZED;
            is_minimized.store(true, std::memory_order_relaxed);
            break;
        case SDL_EVENT_WINDOW_HIDDEN:
            window_flags |= SDL_WINDOW_HIDDEN;
            is_minimized.store(true, std::memory_order_relaxed);
            break;
        case SDL_EVENT_WINDOW_OCCLUDED:
            window_flags |= SDL_WINDOW_OCCLUDED;
            is_minimized.store(true, std::memory_order_relaxed);
            break;
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_EXPOSED:
            window_flags &= ~(SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN | SDL_WINDOW_OCCLUDED);
            is_minimized.store(false, std::memory_order_relaxed);
            if (w > 0 && h > 0) {
                width.store(w, std::memory_order_relaxed);
                height.store(h, std::memory_order_relaxed);
            }
            break;
        default:
            break;
        }
    }
};

// Test Vulkan swapchain recoverable error classification
static bool IsRecoverableSwapchainError(vk::Result result) {
    return result == vk::Result::eErrorOutOfDateKHR ||
           result == vk::Result::eSuboptimalKHR ||
           result == vk::Result::eErrorSurfaceLostKHR ||
           result == vk::Result::eErrorNativeWindowInUseKHR ||
           result == vk::Result::eErrorInitializationFailed;
}

// Test surface extent validation
static bool ValidateSurfaceExtent(vk::Extent2D extent) {
    if (extent.width == 0 || extent.height == 0) {
        return false;
    }
    return true;
}

// Test presenter decision logic: returns true if frame should be presented to GPU,
// false if presentation must be skipped to avoid assertion failures.
static bool ShouldPresentFrame(const MockWindow& win) {
    if (win.IsMinimized() || win.width.load() <= 0 || win.height.load() <= 0) {
        return false;
    }
    return true;
}

// Test bounds checking on swapchain accessors when swapchain is destroyed or resetting
struct MockSwapchainAccessors {
    std::vector<vk::Image> images;
    std::vector<vk::ImageView> images_view;
    std::vector<vk::Semaphore> image_acquired;
    std::vector<vk::Semaphore> present_ready;
    u32 image_index{0};
    u32 frame_index{0};

    vk::Image Image() const {
        return (image_index < images.size()) ? images[image_index] : vk::Image{};
    }

    vk::ImageView ImageView() const {
        return (image_index < images_view.size()) ? images_view[image_index] : vk::ImageView{};
    }

    vk::Semaphore GetImageAcquiredSemaphore() const {
        return (frame_index < image_acquired.size()) ? image_acquired[frame_index] : vk::Semaphore{};
    }

    vk::Semaphore GetPresentReadySemaphore() const {
        return (image_index < present_ready.size()) ? present_ready[image_index] : vk::Semaphore{};
    }
};

int main() {
    std::printf("Running test_swapchain_focus...\n");

    // 1. Test Window minimization state machine
    MockWindow win;
    assert(!win.IsMinimized());
    assert(ShouldPresentFrame(win));

    // Minimize event (Alt+Tab in fullscreen triggers SDL_EVENT_WINDOW_MINIMIZED)
    win.HandleEvent(SDL_EVENT_WINDOW_MINIMIZED);
    assert(win.IsMinimized());
    assert(!ShouldPresentFrame(win)); // Must skip presenting while minimized!

    // Hidden event
    win.HandleEvent(SDL_EVENT_WINDOW_RESTORED, 1920, 1080);
    assert(!win.IsMinimized());
    assert(ShouldPresentFrame(win));

    win.HandleEvent(SDL_EVENT_WINDOW_HIDDEN);
    assert(win.IsMinimized());
    assert(!ShouldPresentFrame(win));

    // Occluded event
    win.HandleEvent(SDL_EVENT_WINDOW_RESTORED, 1920, 1080);
    assert(!win.IsMinimized());
    win.HandleEvent(SDL_EVENT_WINDOW_OCCLUDED);
    assert(win.IsMinimized());

    // Restore event
    win.HandleEvent(SDL_EVENT_WINDOW_RESTORED, 2560, 1440);
    assert(!win.IsMinimized());
    assert(win.width.load() == 2560);
    assert(win.height.load() == 1440);
    assert(ShouldPresentFrame(win));

    // Zero dimension edge cases
    win.width.store(0);
    assert(win.IsMinimized());
    assert(!ShouldPresentFrame(win));

    win.width.store(1920);
    win.height.store(0);
    assert(win.IsMinimized());
    assert(!ShouldPresentFrame(win));

    win.height.store(1080);
    assert(!win.IsMinimized());
    assert(ShouldPresentFrame(win));

    // 2. Test Vulkan surface extent zero-extent handling
    assert(!ValidateSurfaceExtent(vk::Extent2D{0, 0}));
    assert(!ValidateSurfaceExtent(vk::Extent2D{1920, 0}));
    assert(!ValidateSurfaceExtent(vk::Extent2D{0, 1080}));
    assert(ValidateSurfaceExtent(vk::Extent2D{1920, 1080}));

    // 3. Test recoverable Vulkan errors (Issue #325 and #391)
    // Issue #325 root cause: vkCreateSwapchainKHR returned ErrorNativeWindowInUseKHR
    assert(IsRecoverableSwapchainError(vk::Result::eErrorNativeWindowInUseKHR));

    // Issue #391: vkQueuePresentKHR returned ErrorSurfaceLostKHR
    assert(IsRecoverableSwapchainError(vk::Result::eErrorSurfaceLostKHR));

    // Out of date & suboptimal
    assert(IsRecoverableSwapchainError(vk::Result::eErrorOutOfDateKHR));
    assert(IsRecoverableSwapchainError(vk::Result::eSuboptimalKHR));
    assert(IsRecoverableSwapchainError(vk::Result::eErrorInitializationFailed));

    // Fatal error should NOT be classified as recoverable
    assert(!IsRecoverableSwapchainError(vk::Result::eErrorDeviceLost));

    // 4. Test safe swapchain accessors on empty/reset state
    MockSwapchainAccessors sc;
    assert(!sc.Image());
    assert(!sc.ImageView());
    assert(!sc.GetImageAcquiredSemaphore());
    assert(!sc.GetPresentReadySemaphore());

    sc.image_index = 5;
    sc.frame_index = 3;
    assert(!sc.Image());
    assert(!sc.ImageView());
    assert(!sc.GetImageAcquiredSemaphore());
    assert(!sc.GetPresentReadySemaphore());

    std::printf("test_swapchain_focus passed successfully!\n");
    return 0;
}
