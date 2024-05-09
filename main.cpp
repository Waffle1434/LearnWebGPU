#include <iostream>
#include <cassert>
//#include <vector>
#include <GLFW/glfw3.h> // Native Window
#include <webgpu/webgpu.h>
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>
#include <glfw3webgpu.h>

using namespace wgpu;

Adapter requestAdapter(Instance instance, RequestAdapterOptions const * options) { // https://eliemichel.github.io/LearnWebGPU/getting-started/the-adapter.html#request
    struct UserData {
        WGPUAdapter adapter = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * pUserData) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestAdapterStatus_Success) {
            userData.adapter = adapter;
        } else {
            std::cout << "Could not get WebGPU adapter: " << message << std::endl;
        }
        userData.requestEnded = true;
    };

    wgpuInstanceRequestAdapter(instance, options, onAdapterRequestEnded, (void*)&userData);
    assert(userData.requestEnded);
    return userData.adapter;
}
Device requestDevice(Adapter adapter, DeviceDescriptor const * descriptor) {
    struct UserData {
        WGPUDevice device = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const * message, void * pUserData) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestDeviceStatus_Success) {
            userData.device = device;
        } else {
            std::cout << "Could not get WebGPU device: " << message << std::endl;
        }
        userData.requestEnded = true;
    };

    wgpuAdapterRequestDevice(adapter, descriptor, onDeviceRequestEnded, (void*)&userData);
    assert(userData.requestEnded);
    return userData.device;
}

int main (int, char**) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    int w = 800;
    int h = 600;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(w, h, "WebGPU", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return 1;
    }

    InstanceDescriptor desc = {};
    desc.nextInChain = nullptr;
    Instance instance = createInstance(desc);
    if (!instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        return 1;
    }

    Surface surface = glfwGetWGPUSurface(instance, window);

    RequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain       = nullptr;
    adapterOpts.compatibleSurface = surface;
    Adapter adapter = instance.requestAdapter(adapterOpts); // https://eliemichel.github.io/LearnWebGPU/getting-started/the-adapter.html

    DeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "Device"; // anything works here, that's your call
    deviceDesc.requiredFeaturesCount = 0; // we do not require any specific feature
    deviceDesc.requiredLimits = nullptr; // we do not require any specific limit
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "Default Queue";
    Device device = adapter.requestDevice(deviceDesc); // https://eliemichel.github.io/LearnWebGPU/getting-started/the-device.html
    device.setUncapturedErrorCallback([](WGPUErrorType type, char const* message) {
        std::cout << "Uncaptured device error: type " << type;
        if (message) std::cout << " (" << message << ")";
        std::cout << std::endl;
    });

    Queue queue = device.getQueue(); // https://eliemichel.github.io/LearnWebGPU/getting-started/the-command-queue.html#queue-operations

    TextureFormat swapChainFormat = surface.getPreferredFormat(adapter);
    SwapChainDescriptor swapChainDesc = {};
    swapChainDesc.nextInChain = nullptr;
    swapChainDesc.width       = w;
    swapChainDesc.height      = h;
    swapChainDesc.format      = swapChainFormat;
    swapChainDesc.usage       = WGPUTextureUsage_RenderAttachment;
    swapChainDesc.presentMode = WGPUPresentMode_Immediate;
    SwapChain swapChain = device.createSwapChain(surface, swapChainDesc);

    // Main Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        TextureView RT = swapChain.getCurrentTextureView();
        if (!RT) {
            std::cerr << "Cannot acquire next swap chain texture" << std::endl;
            break;
        }

        {
            // Create Command Encoder
            CommandEncoderDescriptor encoderDesc = {};
            encoderDesc.nextInChain = nullptr;
            encoderDesc.label       = "Command Encoder";
            CommandEncoder encoder = device.createCommandEncoder(encoderDesc);
            // https://eliemichel.github.io/LearnWebGPU/getting-started/the-command-queue.html#command-encoder

            RenderPassColorAttachment renderPassColorAttachment = {};
            renderPassColorAttachment.view          = RT;
            renderPassColorAttachment.resolveTarget = nullptr; // For MSAA
            renderPassColorAttachment.loadOp        = WGPULoadOp_Clear;
            renderPassColorAttachment.storeOp       = WGPUStoreOp_Store;
            renderPassColorAttachment.clearValue    = WGPUColor{ 0.0, 1.0, 0.0, 1.0 };

            // Encode Render Pass
            RenderPassDescriptor renderPassDesc = {};
            renderPassDesc.colorAttachmentCount   = 1;
            renderPassDesc.colorAttachments       = &renderPassColorAttachment;
            renderPassDesc.depthStencilAttachment = nullptr;
            renderPassDesc.timestampWriteCount    = 0;
            renderPassDesc.timestampWrites        = nullptr;
            renderPassDesc.nextInChain            = nullptr;

            RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
            renderPass.end();
            renderPass.release();

            // Finish encoding and submit
            CommandBufferDescriptor cmdBufferDescriptor = {};
            cmdBufferDescriptor.nextInChain = nullptr;
            cmdBufferDescriptor.label = "Command buffer";
            CommandBuffer command = encoder.finish(cmdBufferDescriptor);
            encoder.release();
            queue.submit(1, &command);
    #ifdef WEBGPU_BACKEND_DAWN
            encoder.release();
            command.release();
    #endif
        }

        RT.release();
        swapChain.present();
    }

    swapChain.release();
    queue.release();
    device.release();
    adapter.release();
    surface.release();
    instance.release();
    glfwTerminate();

    return 0;
}
