#include "fixture.hpp"

#include <filesystem>

std::string getFixturePath(const std::string& name) {
    std::filesystem::path directory = FIXTURE_DIRECTORY;

    return (directory / ("fixture_" + name)).string();
}