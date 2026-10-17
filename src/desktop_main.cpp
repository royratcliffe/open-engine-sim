#include "../include/desktop_platform_sdl.h"
#include "../include/engine_sim_application.h"
#include "../include/runtime_paths.h"
#include "../include/sdl_gpu_renderer.h"
#include "../include/sdl_audio_output.h"
#include "../include/remote_control.h"

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>

int main(int, char **) {
    DesktopPlatformSdl platform;
    if (!platform.initialize("Open Engine Simulator", 1920, 1080)) {
        std::fprintf(stderr, "SDL initialization failed: %s\n", platform.lastError().c_str());
        return 1;
    }

    const RuntimePaths paths = RuntimePaths::discover(
        platform.applicationDirectory(), ENGINE_SIM_SOURCE_ASSET_DIRECTORY);
    const std::filesystem::path packagedShaderDirectory = paths.assetDirectory / "shaders";
    const std::string shaderDirectory = std::filesystem::exists(
            packagedShaderDirectory / "engine_sim.vertex.msl")
        ? packagedShaderDirectory.string()
        : ENGINE_SIM_SHADER_DIRECTORY;

    SdlGpuRenderer renderer;
    if (!renderer.initialize(platform.nativeWindowHandle(), shaderDirectory)) {
        std::fprintf(stderr, "SDL GPU initialization failed: %s\n", renderer.lastError());
        return 1;
    }
    std::fprintf(stderr, "Renderer: %s\n", renderer.backendDescription().c_str());

    EngineSimApplication application;
    SdlAudioOutput audioOutput;
    application.initialize(&platform, &renderer, &audioOutput, paths);

    // Opt-in: set ENGINE_SIM_REMOTE_PORT to enable the loopback UDP interface.
    RemoteControl remote;
    if (const char *portText = std::getenv("ENGINE_SIM_REMOTE_PORT")) {
        const long port = std::strtol(portText, nullptr, 10);
        if (port > 0 && port <= 65535 && remote.open(static_cast<std::uint16_t>(port))) {
            std::fprintf(stderr, "Remote control listening on 127.0.0.1:%ld\n", port);
        } else {
            std::fprintf(stderr, "Remote control disabled: invalid or unavailable port '%s'\n", portText);
        }
    }
    while (application.tick()) {
        remote.poll(application);
        application.waitForNextFrame();
    }
    remote.close();
    application.destroy();

    renderer.shutdown();
    platform.shutdown();
    return 0;
}
