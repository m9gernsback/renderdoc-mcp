#include "mcp/tools/tools.h"
#include "mcp/tool_registry.h"
#include "mcp/serialization.h"
#include "core/session.h"

namespace renderdoc::mcp::tools {

void registerSessionTools(ToolRegistry& registry) {
    registry.registerTool({
        "open_capture",
        "Open a RenderDoc capture file (.rdc) for analysis. Returns the graphics API type "
        "and total event/draw counts. Closes any previously opened capture.\n\n"
        "LOCAL REPLAY: Just pass 'path'. Works when the capture's GPU matches the local PC.\n\n"
        "REMOTE REPLAY (Android): Pass 'host' to replay on the original device's GPU. Required when "
        "the capture uses device-specific Vulkan extensions (e.g. VK_EXT_fragment_density_map on Adreno). "
        "Setup steps:\n"
        "1. Ensure renderdoccmd is running on the device (start via RenderDoc GUI or: "
        "adb shell am start -n org.renderdoc.renderdoccmd.arm64/.Loader)\n"
        "2. Set up adb port forwarding: adb forward tcp:39920 localabstract:renderdoc_39920\n"
        "3. Close the RenderDoc GUI (it holds an exclusive connection)\n"
        "4. Call open_capture with host='localhost:39920'\n\n"
        "If local replay fails with 'hardware unsupported' or 'extension not supported', retry with remote replay.\n"
        "If remote replay fails with 'busy', the RenderDoc GUI needs to be closed first.\n"
        "If remote replay fails with 'Network I/O', renderdoccmd may not be running or there's a DLL version mismatch.",
        {{"type", "object"},
         {"properties", {
             {"path", {{"type", "string"},
                       {"description", "Absolute path to the .rdc capture file (local path on this PC)"}}},
             {"host", {{"type", "string"},
                       {"description", "Remote replay server host (e.g. 'localhost:39920' for Android with adb forwarding). If omitted, replays locally. Only needed when local replay fails due to GPU incompatibility."}}}
         }},
         {"required", {"path"}}},
        [](mcp::ToolContext& ctx, const nlohmann::json& args) -> nlohmann::json {
            auto& session = ctx.session;
            std::string path = args["path"].get<std::string>();

            if (args.contains("host") && !args["host"].get<std::string>().empty()) {
                std::string host = args["host"].get<std::string>();
                auto info = session.openRemote(host, path);
                return to_json(info);
            } else {
                auto info = session.open(path);
                return to_json(info);
            }
        }
    });

    registry.registerTool({
        "close_capture",
        "Close the currently opened capture and release resources. "
        "For remote replay, this closes the replay on the device but keeps the remote connection alive. "
        "Use disconnect_remote to fully disconnect from the device.",
        {{"type", "object"}, {"properties", nlohmann::json::object()}},
        [](mcp::ToolContext& ctx, const nlohmann::json&) -> nlohmann::json {
            ctx.session.close();
            return {{"status", "closed"}};
        }
    });

    registry.registerTool({
        "disconnect_remote",
        "Disconnect from the remote replay server and close any open capture. "
        "Use this when done with remote replay to free resources on the device. "
        "After disconnecting, the renderdoccmd on the device becomes available for "
        "other connections (e.g. RenderDoc GUI).",
        {{"type", "object"}, {"properties", nlohmann::json::object()}},
        [](mcp::ToolContext& ctx, const nlohmann::json&) -> nlohmann::json {
            ctx.session.close();
            ctx.session.disconnectRemote();
            return {{"status", "disconnected"}};
        }
    });

    registry.registerTool({
        "session_status",
        "Query whether a capture is currently open. Returns session state including "
        "capture path, API type, current event ID, total events, and remote replay status. "
        "When isRemote is true, the replay is running on a remote device (e.g. Android).",
        {{"type", "object"}, {"properties", nlohmann::json::object()}},
        [](mcp::ToolContext& ctx, const nlohmann::json&) -> nlohmann::json {
            return to_json(ctx.session.status());
        }
    });
}

} // namespace renderdoc::mcp::tools
