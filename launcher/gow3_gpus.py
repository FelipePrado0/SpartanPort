# SPDX-License-Identifier: GPL-2.0-or-later
"""The graphics cards Vulkan reports, as the game sees them (vk_instance.cpp), through ctypes."""
import ctypes
import ctypes.util
import os

# VkPhysicalDeviceType
INTEGRATED, DISCRETE = 1, 2
VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1


class InstanceCreateInfo(ctypes.Structure):
    _fields_ = [('sType', ctypes.c_int32), ('pNext', ctypes.c_void_p), ('flags', ctypes.c_uint32),
                ('pApplicationInfo', ctypes.c_void_p), ('enabledLayerCount', ctypes.c_uint32),
                ('ppEnabledLayerNames', ctypes.c_void_p), ('enabledExtensionCount', ctypes.c_uint32),
                ('ppEnabledExtensionNames', ctypes.c_void_p)]


def load_vulkan():
    names = ['vulkan-1'] if os.name == 'nt' else ['libvulkan.so.1', ctypes.util.find_library('vulkan')]
    for name in filter(None, names):
        try:
            return ctypes.CDLL(name)
        except OSError:
            pass
    return None


def vulkan_gpus():
    """[(name, VkPhysicalDeviceType)] in Vulkan's order; [] when Vulkan is unavailable."""
    vk = load_vulkan()
    if not vk:
        return []
    instance = ctypes.c_void_p()
    info = InstanceCreateInfo(sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO)
    if vk.vkCreateInstance(ctypes.byref(info), None, ctypes.byref(instance)) != 0:
        return []
    try:
        count = ctypes.c_uint32()
        vk.vkEnumeratePhysicalDevices(instance, ctypes.byref(count), None)
        devices = (ctypes.c_void_p * count.value)()
        if vk.vkEnumeratePhysicalDevices(instance, ctypes.byref(count), devices) not in (0, 5):  # VK_INCOMPLETE
            return []
        gpus = []
        for device in devices[:count.value]:
            # VkPhysicalDeviceProperties: four uint32, deviceType, deviceName[256], ... (824 bytes).
            properties = ctypes.create_string_buffer(4096)
            vk.vkGetPhysicalDeviceProperties(ctypes.c_void_p(device), properties)
            kind = int.from_bytes(properties.raw[16:20], 'little')
            gpus.append((properties.raw[20:276].split(b'\0', 1)[0].decode('utf-8', 'replace'), kind))
        return gpus
    finally:
        vk.vkDestroyInstance(instance, None)


if __name__ == '__main__':
    for name, kind in vulkan_gpus():
        print(kind, name)
