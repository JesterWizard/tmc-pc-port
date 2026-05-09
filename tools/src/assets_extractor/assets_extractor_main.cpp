#include "asset_extractor_runner.h"

#include <cstdint>
#include <filesystem>
#include <iostream>

#include <cstddef>
#include <fstream>

#include <cstddef>
#include <fstream>

extern "C" {
std::uint8_t* gRomData = nullptr;
std::uint32_t gRomSize = 0;

int main(int argc, char* argv[])
{
    bool verbose = false;
    bool runtime_only = false;
    bool force = false;
    bool pack_runtime = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--runtime-only") {
            runtime_only = true;
        } else if (arg == "--force" || arg == "-f") {
            force = true;
        } else if (arg == "--pak") {
            pack_runtime = true;
        } else if (arg == "--help" || arg == "-h") {
            fmt::print("Usage: asset_extractor [--verbose] [--runtime-only] [--force] [--pak]\n"
                       "  --verbose       Print per-asset notes/warnings.\n"
                       "  --runtime-only  Skip writing the editable assets_src/ tree.\n"
                       "  --force         Re-extract even if assets/ are already up to date.\n"
                       "  --pak           Pack runtime assets into per-category .pak archives\n"
                       "                  instead of writing thousands of loose files.\n");
            return 0;
        }
    }

    std::filesystem::path executable_dir;
    if (argc > 0) {
        executable_dir = AssetExtractorApi::FindExecutableDirectory(argv[0]);
    }
    if (executable_dir.empty()) {
        executable_dir = std::filesystem::current_path();
    }

    std::string error;
    if (!RunEmbeddedAssetExtractor(executable_dir, &error)) {
        std::cerr << "Failed to extract assets: " << error << std::endl;
        return 1;
    }

    /* Always emit sounds.json next to the binary (= where tmc_pc launches
     * from). Same directory as assets_src/ and assets/. */
    write_sounds_json(executable_dir / "sounds.json");

    /* Always emit sounds.json next to the binary (= where tmc_pc launches
     * from). Same directory as assets_src/ and assets/. */
    write_sounds_json(executable_dir / "sounds.json");

    return 0;
}
