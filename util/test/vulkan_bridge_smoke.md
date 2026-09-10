# Vulkan multi-instance capture backport

Source: Yi-Meng/renderdoc commit
[`8f4c7c48cace946f8139e0d2d50e893a97c880e7`](https://github.com/Yi-Meng/renderdoc/commit/8f4c7c48cace946f8139e0d2d50e893a97c880e7),
included in [v1.46](https://github.com/Yi-Meng/renderdoc/releases/tag/v1.46).
The separate `236c57f` GUI active-file diagnostic change is not needed for capture and is not included.

## Behavior and adaptations

- A Vulkan UI/hotkey capture, frame-zero capture, or capture marker starts captures on the other
  registered Vulkan instances. The triggering instance's end-of-frame/marker ends those captures.
  Each instance writes a separate `.nbd` file; inspect the extra files for rendering commands.
- Ported RenderDoc/RDC/rdcarray names to NoobDawn/NBD/nbdarray.
- Register after successful device initialization, not instance creation: an enumeration-only
  instance has no device/command pool and cannot safely start capturing.
- Unregister at device destruction, with a destructor fallback. Finish associated bridge captures
  before tearing down internal Vulkan resources.
- Track the initiator of each peer capture. Skip peers already capturing, and only end captures
  started by this initiator, preserving independent application API captures.
- Use the initiator's frame counter for peer files and cover command-buffer capture markers in
  both QueueSubmit and QueueSubmit2 paths as well as queue markers.

This addresses rendering split across Vulkan instances. It does not add support for multiple
logical devices within one instance (the existing `MULTIDEVICE` restriction remains), or capture
other graphics APIs. Direct application API StartFrameCapture/EndFrameCapture keeps its existing
device selection. Devices created after a bridge capture starts participate in subsequent triggers.
The registry mutex protects list access, as in the source patch; it does not make concurrent
device teardown and capture transitions a newly supported operation.

## Standalone smoke test

From an x64 Visual Studio developer command prompt in the repository root:

```bat
cl /nologo /EHsc /W3 util\test\vulkan_bridge_smoke.cpp /Fe:x64\Release\vulkan_bridge_smoke.exe /Fo:x64\Release\vulkan_bridge_smoke.obj
set VK_IMPLICIT_LAYER_PATH=%CD%\x64\Release
set ENABLE_VULKAN_NOOBDAWN_CAPTURE=1
mkdir analysis\vulkan_bridge_smoke
x64\Release\vulkan_bridge_smoke.exe %CD%\analysis\vulkan_bridge_smoke\verified
```

Use a Vulkan loader supporting `VK_IMPLICIT_LAYER_PATH`. Loader diagnostics should show the
layer DLL coming from `x64\Release`; a globally registered older `dist` build must not be used.
Do not explicitly add the capture layer to `VkInstanceCreateInfo`.

Expected output (exit code 0):

```text
Multi-instance capture: 2 files (expected 2)
Independent capture preserved: 1 files (expected 1)
After peer destruction: 1 files (expected 1)
```

The test creates two devices in separate instances plus an instance with no device. Only the peer
submits `vkCmdFillBuffer` during the first capture. It then tests independent capture ownership and
device removal. Five capture files are produced in total. An XML conversion of the first peer
capture verifies it contains `vkCmdFillBuffer` and `vkQueueSubmit`:

```bat
x64\Release\noobdawncmd.exe convert -f analysis\vulkan_bridge_smoke\verified_frame0.nbd -o analysis\vulkan_bridge_smoke\peer.xml -c xml
```

Validated on 2026-09-10: x64 Release DLL build with VS 2022/v143, smoke test, XML command inspection,
and `git diff --check`. Emulator presentation/hotkey behavior, GPU replay, Win32, and Android still
need testing in their target environments.
