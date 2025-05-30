module;

#include <boost/describe.hpp>
#include <filesystem>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <vector>

#include "format.hpp"

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

export module config_mod;

namespace config {

namespace fs = std::filesystem;

BOOST_DEFINE_ENUM_CLASS(TargetType, Executable, SharedLibrary, StaticLibrary);

export struct BuildTarget {
  TargetType target_type = TargetType::Executable;
  std::string name;

  std::vector<fs::path> sources;

  std::vector<std::string> compile_args;
  std::vector<std::string> link_args;
};
BOOST_DESCRIBE_STRUCT(BuildTarget, (),
                      (target_type, name, sources, compile_args, link_args));

export struct ProjectConfig {
  fs::path root_dir;
  fs::path build_dir;
  std::vector<BuildTarget> targets;
};

BOOST_DESCRIBE_STRUCT(ProjectConfig, (), (root_dir, build_dir, targets));

auto get_toml_array_string(const toml::array& array) {
  return std::ranges::views::all(array) |
         std::views::transform(
             [](const auto& node) -> std::optional<std::string> {
               return node.template value<std::string>();
             }) |
         std::ranges::views::filter(
             [](const auto& opt) { return opt.has_value(); }) |
         std::views::transform([](const auto& opt) { return opt.value(); }) |
         std::ranges::to<std::vector>();
}

export auto parse_project(const fs::path& dir) {
  ProjectConfig config;

  config.root_dir = dir;
  config.build_dir = dir / "build";

  BuildTarget default_target{};

  const auto result = toml::parse_file((dir / "buildr.toml").string());
  if (!result) {
    std::println(std::cerr, "Failed to parse config file ({}): {}",
                 (dir / "buildr.toml").string(), result.error().description());
  }

  const auto& tbl = result.table();

  default_target.name = dir.stem().string();

  default_target.compile_args =
      get_toml_array_string(*tbl["compile_args"].as_array());

  if (tbl.contains("link_args"))
    default_target.compile_args =
        get_toml_array_string(*tbl["link_args"].as_array());

  if (tbl.contains("srcs")) {
    default_target.sources =
        std::ranges::views::all(*tbl["srcs"].as_array()) |
        std::views::transform([](const auto& node) -> std::optional<fs::path> {
          return node.template value<std::string>();
        }) |
        std::ranges::views::filter(
            [](const auto& opt) { return opt.has_value(); }) |
        std::views::transform([](const auto& opt) { return opt.value(); }) |
        std::ranges::to<std::vector>();
  }

  config.targets.push_back(default_target);

  return config;
}

}  // namespace config
