#include "mcp/tools/tools.h"
#include "mcp/tool_registry.h"
#include "mcp/serialization.h"
#include "core/session.h"
#include "core/shaders.h"

namespace renderdoc::mcp::tools {

void registerShaderTools(ToolRegistry& registry) {

    // ── list_disassembly_targets ─────────────────────────────────────────────
    registry.registerTool({
        "list_disassembly_targets",
        "List available shader disassembly targets from the replay API. Returns built-in targets only "
        "(e.g. 'SPIR-V (RenderDoc)', 'KHR_pipeline_executable_properties', 'AMD GCN ISA'). "
        "Use the returned target names with get_shader's target parameter.\n\n"
        "NOTE: External tools like AdrenoOfflineCompiler, spirv-dis, or Mali Offline Compiler are "
        "NOT listed here — they are a GUI-only feature in RenderDoc. Use run_shader_tool instead "
        "to invoke external tools on the shader's raw bytes.",
        {{"type", "object"}, {"properties", nlohmann::json::object()}},
        [](mcp::ToolContext& ctx, const nlohmann::json& /*args*/) -> nlohmann::json {
            auto& session = ctx.session;
            auto targets = core::listDisassemblyTargets(session);
            return {{"targets", targets}, {"count", targets.size()}};
        }
    });

    // ── get_shader ────────────────────────────────────────────────────────────
    registry.registerTool({
        "get_shader",
        "Get shader disassembly or reflection data at an event for a given stage. "
        "Use the target parameter to select a specific disassembly format (from list_disassembly_targets).",
        {{"type", "object"},
         {"properties", {
             {"eventId", {{"type", "integer"}, {"description", "Event ID (uses current if omitted)"}}},
             {"stage",   {{"type", "string"},  {"enum", nlohmann::json::array({"vs","hs","ds","gs","ps","cs"})}}},
             {"mode",    {{"type", "string"},  {"enum", nlohmann::json::array({"disasm","reflect"})}, {"default", "disasm"}}},
             {"target",  {{"type", "string"},  {"description", "Disassembly target (from list_disassembly_targets). Uses default native format if omitted. Only used with mode=disasm."}}}
         }},
         {"required", nlohmann::json::array({"stage"})}},
        [](mcp::ToolContext& ctx, const nlohmann::json& args) -> nlohmann::json {
            auto& session = ctx.session;
            const std::string stageStr = args["stage"].get<std::string>();
            const std::string mode     = args.value("mode", std::string("disasm"));

            core::ShaderStage stage = parseShaderStage(stageStr);

            std::optional<uint32_t> eventId;
            if (args.contains("eventId"))
                eventId = args["eventId"].get<uint32_t>();

            if (mode == "reflect") {
                auto refl = core::getShaderReflection(session, stage, eventId);
                return to_json(refl);
            } else {
                // Default: disasm
                std::optional<std::string> target;
                if (args.contains("target"))
                    target = args["target"].get<std::string>();
                auto disasm = core::getShaderDisassembly(session, stage, eventId, target);
                return to_json(disasm);
            }
        }
    });

    // ── list_shaders ──────────────────────────────────────────────────────────
    registry.registerTool({
        "list_shaders",
        "List all unique shaders used in the capture with their stages and usage count",
        {{"type", "object"},
         {"properties", nlohmann::json::object()}},
        [](mcp::ToolContext& ctx, const nlohmann::json& /*args*/) -> nlohmann::json {
            auto& session = ctx.session;
            auto shaders = core::listShaders(session);
            nlohmann::json result;
            result["shaders"] = to_json_array(shaders);
            result["count"]   = shaders.size();
            return result;
        }
    });

    // ── search_shaders ────────────────────────────────────────────────────────
    registry.registerTool({
        "search_shaders",
        "Search shader disassembly text across all shaders for a pattern",
        {{"type", "object"},
         {"properties", {
             {"pattern", {{"type", "string"},  {"description", "Text pattern to search for (case-insensitive substring)"}}},
             {"stage",   {{"type", "string"},  {"enum", nlohmann::json::array({"vs","hs","ds","gs","ps","cs"})}, {"description", "Limit to specific stage"}}},
             {"limit",   {{"type", "integer"}, {"description", "Max results, default 50"}}}
         }},
         {"required", nlohmann::json::array({"pattern"})}},
        [](mcp::ToolContext& ctx, const nlohmann::json& args) -> nlohmann::json {
            auto& session = ctx.session;
            const std::string pattern = args["pattern"].get<std::string>();
            uint32_t limit = (uint32_t)args.value("limit", 50);

            std::optional<core::ShaderStage> stageFilter;
            if (args.contains("stage")) {
                const std::string stageStr = args["stage"].get<std::string>();
                if (!stageStr.empty())
                    stageFilter = parseShaderStage(stageStr);
            }

            auto matches = core::searchShaders(session, pattern, stageFilter, limit);

            nlohmann::json result;
            result["matches"] = to_json_array(matches);
            result["count"]   = matches.size();
            result["pattern"] = pattern;
            return result;
        }
    });

    // ── run_shader_tool ──────────────────────────────────────────────────────
    registry.registerTool({
        "run_shader_tool",
        "Run an external shader processing tool on the bound shader's raw bytes (e.g. SPIR-V). "
        "Extracts the shader binary, writes it to a temp file, executes the tool, and returns the output. "
        "Supports tools like AdrenoOfflineCompiler, spirv-dis, spirv-cross, Mali Offline Compiler, etc. "
        "Use placeholders in args: {input_file}, {output_file}, {entry_point}, {stage}.",
        {{"type", "object"},
         {"properties", {
             {"stage",      {{"type", "string"}, {"enum", nlohmann::json::array({"vs","hs","ds","gs","ps","cs"})},
                             {"description", "Shader stage to extract"}}},
             {"executable", {{"type", "string"}, {"description", "Path to the external tool executable"}}},
             {"args",       {{"type", "string"}, {"description", "Command-line arguments with placeholders: {input_file}, {output_file}, {entry_point}, {stage}. Defaults to just passing {input_file} if omitted."}}},
             {"eventId",    {{"type", "integer"}, {"description", "Event ID (uses current if omitted)"}}}
         }},
         {"required", nlohmann::json::array({"stage", "executable"})}},
        [](mcp::ToolContext& ctx, const nlohmann::json& args) -> nlohmann::json {
            auto& session = ctx.session;
            const std::string stageStr    = args["stage"].get<std::string>();
            const std::string executable  = args["executable"].get<std::string>();
            const std::string toolArgs    = args.value("args", std::string(""));

            core::ShaderStage stage = parseShaderStage(stageStr);

            std::optional<uint32_t> eventId;
            if (args.contains("eventId"))
                eventId = args["eventId"].get<uint32_t>();

            auto result = core::runShaderTool(session, stage, executable, toolArgs, eventId);
            return to_json(result);
        }
    });

}

} // namespace renderdoc::mcp::tools
