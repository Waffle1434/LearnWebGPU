#include <iostream>
#include <cassert>
#include <cmath>
#include <GLFW/glfw3.h> // Native Window
#include <webgpu/webgpu.h>
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>
#include <glfw3webgpu.h>

using namespace wgpu;

// https://eliemichel.github.io/LearnWebGPU

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
    Adapter adapter = instance.requestAdapter(adapterOpts);

    DeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain              = nullptr;
    deviceDesc.label                    = "Device";
    deviceDesc.requiredFeaturesCount    = 0;
    deviceDesc.requiredLimits           = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label       = "Default Queue";
    Device device = adapter.requestDevice(deviceDesc);
    device.setUncapturedErrorCallback([](WGPUErrorType type, char const* message) {
        std::cout << "Uncaptured device error: type " << type;
        if (message) std::cout << " (" << message << ")";
        std::cout << std::endl;
    });

    Queue queue = device.getQueue();

    TextureFormat swapChainFormat = surface.getPreferredFormat(adapter);
    SwapChainDescriptor swapChainDesc = {};
    swapChainDesc.nextInChain = nullptr;
    swapChainDesc.width       = w;
    swapChainDesc.height      = h;
    swapChainDesc.format      = swapChainFormat;
    swapChainDesc.usage       = WGPUTextureUsage_RenderAttachment;
    swapChainDesc.presentMode = WGPUPresentMode_Immediate;
    SwapChain swapChain = device.createSwapChain(surface, swapChainDesc);

    const char* shaderSource = R"(
        @vertex
        fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
            var p = vec2f(0.0, 0.0);
            if (in_vertex_index == 0u) {
                p = vec2f(-0.5, -0.5);
            } else if (in_vertex_index == 1u) {
                p = vec2f(0.5, -0.5);
            } else {
                p = vec2f(0.0, 0.5);
            }
            return vec4f(p, 0.0, 1.0);
        }

        @fragment
        fn fs_main() -> @location(0) vec4f {
            return vec4f(1.0, 0.0, 0.0, 1.0);
        }
    )";

    ShaderModuleWGSLDescriptor shaderCodeDesc;
    shaderCodeDesc.chain.next  = nullptr;
    shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor;
    shaderCodeDesc.code = shaderSource;

    ShaderModuleDescriptor shaderDesc;
#ifdef WEBGPU_BACKEND_WGPU
    shaderDesc.hintCount = 0;
    shaderDesc.hints = nullptr;
#endif
    shaderDesc.nextInChain = &shaderCodeDesc.chain;
    ShaderModule shaderModule = device.createShaderModule(shaderDesc);

    BlendState blendState;
    blendState.color.srcFactor = BlendFactor::SrcAlpha;
    blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = BlendOperation::Add;

    ColorTargetState colorTarget;
    colorTarget.format    = swapChainFormat;
    colorTarget.blend     = &blendState;
    colorTarget.writeMask = ColorWriteMask::All;

    FragmentState fragmentState;
    fragmentState.module        = shaderModule;
    fragmentState.entryPoint    = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants     = nullptr;
    fragmentState.targetCount   = 1;
    fragmentState.targets       = &colorTarget;

    RenderPipelineDescriptor pipelineDesc;
    pipelineDesc.vertex.bufferCount   = 0;
    pipelineDesc.vertex.buffers       = nullptr;
    pipelineDesc.vertex.module        = shaderModule;
    pipelineDesc.vertex.entryPoint    = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants     = nullptr;
    pipelineDesc.primitive.topology   = PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace  = FrontFace::CCW;
    pipelineDesc.primitive.cullMode   = CullMode::None;
    pipelineDesc.multisample.count    = 1;
    pipelineDesc.multisample.mask     = ~0u; // Default value for the mask, meaning "all bits on"
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    pipelineDesc.layout               = nullptr;
    pipelineDesc.fragment             = &fragmentState;
    pipelineDesc.depthStencil         = nullptr;
    RenderPipeline pipeline = device.createRenderPipeline(pipelineDesc);

    CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label       = "Command Encoder";

    RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.loadOp  = WGPULoadOp_Clear;
    renderPassColorAttachment.storeOp = WGPUStoreOp_Store;

    RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments     = &renderPassColorAttachment;

    CommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.nextInChain = nullptr;
    cmdBufferDescriptor.label       = "Command buffer";

    int i_frame = 0;

    // Main Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        TextureView RT = swapChain.getCurrentTextureView();
        CommandEncoder encoder = device.createCommandEncoder(encoderDesc);

        renderPassColorAttachment.view          = RT;
        renderPassColorAttachment.resolveTarget = nullptr; // For MSAA
        renderPassColorAttachment.clearValue    = WGPUColor{ 0.0, sin(0.0025 * i_frame), 0.0, 1.0 };

        RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
        renderPass.setPipeline(pipeline);
        renderPass.draw(3, 1, 0, 0); // Draw triangle
        renderPass.end();
        renderPass.release();

        CommandBuffer command = encoder.finish(cmdBufferDescriptor);
        queue.submit(1, &command);
        encoder.release();
        command.release();

        RT.release();
        swapChain.present();
        i_frame++;
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
