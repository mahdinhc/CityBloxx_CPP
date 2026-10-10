// assets.h - read-only game data under the assets root (see assets/manifest.txt).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace cbx {
struct Assets {
    static void setRoot(const std::string& dir);
    static const std::string& root();
    // True if 'dir' is a valid assets root (contains manifest.txt).
    static bool isValidRoot(const std::string& dir);
    // Reads an asset by path relative to the root, e.g. "images/ui/arrow.png". Like the original, '\\' is
    // accepted as a separator, leading '/' is ignored and paths containing ".." are rejected.
    // Returns false if the file does not exist.
    static bool read(const std::string& name, std::vector<uint8_t>& out);
    // Directory part of an asset path ("models/a.obj" -> "models/"), "" for top level.
    static std::string dirOf(const std::string& path);
};
// Writable save location (RMS replacement).
struct SaveStore {
    static void setRoot(const std::string& dir);
    static bool read(const std::string& name, std::vector<uint8_t>& out);
    static bool write(const std::string& name, const std::vector<uint8_t>& data);
};
}
