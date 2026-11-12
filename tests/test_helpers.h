#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

// Gives every test its own empty folder in the system temp directory
// and deletes it afterwards, so tests never see each other's files.
class TempDirTest : public testing::Test {
protected:
    void SetUp() override {
        const testing::TestInfo* info = testing::UnitTest::GetInstance()->current_test_info();
        dir = std::filesystem::temp_directory_path() / "password_manager_tests" /
              (std::string(info->test_suite_name()) + "_" + info->name());
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
    }

    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(dir, ignored);
    }

    [[nodiscard]] std::string path(const std::string& file_name) const {
        return (dir / file_name).string();
    }

    std::filesystem::path dir;
};

inline std::string read_file(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

inline void write_file(const std::string& path, const std::string& content) {
    std::ofstream stream(path, std::ios::binary);
    stream << content;
}
