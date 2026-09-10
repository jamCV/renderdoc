// Standalone Windows smoke test for Vulkan bridge captures (no window or Vulkan SDK needed).
// See vulkan_bridge_smoke.md for build/run instructions and expected results.
#define VK_NO_PROTOTYPES
#include "../../noobdawn/driver/vulkan/official/vulkan.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../../noobdawn/api/app/noobdawn_app.h"

#define CHECK(call)                                           \
  do                                                          \
  {                                                           \
    VkResult r = (call);                                      \
    if(r != VK_SUCCESS)                                       \
    {                                                         \
      std::fprintf(stderr, "%s failed: %d\n", #call, int(r)); \
      std::exit(1);                                           \
    }                                                         \
  } while(0)
#define LOAD(name) auto name = (PFN_##name)gpa(instance, #name)

static PFN_vkGetInstanceProcAddr gpa;

struct Context
{
  VkInstance instance = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;

  void init(bool createDevice)
  {
    const char *extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Vulkan bridge smoke test";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = &extension;
    LOAD(vkCreateInstance);
    CHECK(vkCreateInstance(&info, NULL, &instance));
    if(!createDevice)
      return;

    LOAD(vkEnumeratePhysicalDevices);
    uint32_t count = 0;
    CHECK(vkEnumeratePhysicalDevices(instance, &count, NULL));
    if(count == 0)
      std::exit(2);
    std::vector<VkPhysicalDevice> physical(count);
    CHECK(vkEnumeratePhysicalDevices(instance, &count, physical.data()));
    LOAD(vkGetPhysicalDeviceQueueFamilyProperties);
    vkGetPhysicalDeviceQueueFamilyProperties(physical[0], &count, NULL);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical[0], &count, families.data());
    uint32_t family = 0;
    while(family < count && !(families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT))
      family++;
    if(family == count)
      std::exit(2);
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qi = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = family;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    VkDeviceCreateInfo di = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    LOAD(vkCreateDevice);
    CHECK(vkCreateDevice(physical[0], &di, NULL, &device));
    LOAD(vkGetDeviceQueue);
    vkGetDeviceQueue(device, family, 0, &queue);
    VkCommandPoolCreateInfo pi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.queueFamilyIndex = family;
    LOAD(vkCreateCommandPool);
    CHECK(vkCreateCommandPool(device, &pi, NULL, &pool));
    VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool;
    ai.commandBufferCount = 1;
    LOAD(vkAllocateCommandBuffers);
    CHECK(vkAllocateCommandBuffers(device, &ai, &cmd));
    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = 256;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    LOAD(vkCreateBuffer);
    CHECK(vkCreateBuffer(device, &bi, NULL, &buffer));
    VkMemoryRequirements requirements;
    LOAD(vkGetBufferMemoryRequirements);
    vkGetBufferMemoryRequirements(device, buffer, &requirements);
    VkMemoryAllocateInfo mi = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mi.allocationSize = requirements.size;
    while(!(requirements.memoryTypeBits & (1U << mi.memoryTypeIndex)))
      mi.memoryTypeIndex++;
    LOAD(vkAllocateMemory);
    CHECK(vkAllocateMemory(device, &mi, NULL, &memory));
    LOAD(vkBindBufferMemory);
    CHECK(vkBindBufferMemory(device, buffer, memory, 0));
    LOAD(vkBeginCommandBuffer);
    VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CHECK(vkBeginCommandBuffer(cmd, &begin));
    LOAD(vkCmdFillBuffer);
    vkCmdFillBuffer(cmd, buffer, 0, 256, 0x12345678);
    LOAD(vkEndCommandBuffer);
    CHECK(vkEndCommandBuffer(cmd));
  }

  void marker(const char *name)
  {
    LOAD(vkQueueInsertDebugUtilsLabelEXT);
    VkDebugUtilsLabelEXT label = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    vkQueueInsertDebugUtilsLabelEXT(queue, &label);
  }

  void submit()
  {
    LOAD(vkQueueSubmit);
    VkSubmitInfo info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    info.commandBufferCount = 1;
    info.pCommandBuffers = &cmd;
    CHECK(vkQueueSubmit(queue, 1, &info, VK_NULL_HANDLE));
    LOAD(vkQueueWaitIdle);
    CHECK(vkQueueWaitIdle(queue));
  }

  void destroy()
  {
    if(device)
    {
      LOAD(vkDestroyBuffer);
      vkDestroyBuffer(device, buffer, NULL);
      LOAD(vkFreeMemory);
      vkFreeMemory(device, memory, NULL);
      LOAD(vkDestroyCommandPool);
      vkDestroyCommandPool(device, pool, NULL);
      LOAD(vkDestroyDevice);
      vkDestroyDevice(device, NULL);
    }
    LOAD(vkDestroyInstance);
    vkDestroyInstance(instance, NULL);
    device = VK_NULL_HANDLE;
  }
};

int main(int argc, char **argv)
{
  if(argc != 2)
  {
    std::fprintf(stderr, "Usage: vulkan_bridge_smoke capture-path-prefix\n");
    return 2;
  }
  HMODULE loader = LoadLibraryA("vulkan-1.dll");
  if(!loader)
    return 2;
  gpa = (PFN_vkGetInstanceProcAddr)GetProcAddress(loader, "vkGetInstanceProcAddr");
  Context owner, peer, enumerationOnly;
  owner.init(true);
  peer.init(true);
  enumerationOnly.init(false);
  HMODULE capture = GetModuleHandleA("noobdawn.dll");
  if(!capture)
    return 2;
  auto getAPI = (pNOOBDAWN_GetAPI)GetProcAddress(capture, "NOOBDAWN_GetAPI");
  NOOBDAWN_API_1_6_0 *api = NULL;
  if(!getAPI || !getAPI(eNOOBDAWN_API_Version_1_6_0, (void **)&api))
    return 2;
  api->SetCaptureFilePathTemplate(argv[1]);
  uint32_t before = api->GetNumCaptures();
  owner.marker("capture-marker,begin_capture");
  peer.submit();    // The presenting/triggering instance has no GPU work at all.
  owner.marker("capture-marker,end_capture");
  uint32_t after = api->GetNumCaptures();
  std::printf("Multi-instance capture: %u files (expected 2)\n", after - before);
  if(after - before != 2)
    return 1;

  // A capture independently started through the application API must not be ended by the owner.
  void *peerDevice = NOOBDAWN_DEVICEPOINTER_FROM_VKINSTANCE(peer.instance);
  api->StartFrameCapture(peerDevice, NULL);
  before = api->GetNumCaptures();
  owner.marker("capture-marker,begin_capture");
  peer.submit();
  owner.marker("capture-marker,end_capture");
  after = api->GetNumCaptures();
  std::printf("Independent capture preserved: %u files (expected 1)\n", after - before);
  if(after - before != 1 || !api->IsFrameCapturing())
    return 1;
  if(!api->EndFrameCapture(peerDevice, NULL) || api->GetNumCaptures() != after + 1)
    return 1;

  peer.destroy();
  before = api->GetNumCaptures();
  owner.marker("capture-marker,begin_capture");
  owner.submit();
  owner.marker("capture-marker,end_capture");
  after = api->GetNumCaptures();
  std::printf("After peer destruction: %u files (expected 1)\n", after - before);
  if(after - before != 1)
    return 1;
  enumerationOnly.destroy();
  owner.destroy();
  return 0;
}
