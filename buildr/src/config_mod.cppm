module;

#include <boost/describe.hpp>
#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

export module config_mod;

import logging;

namespace config {

namespace fs = std::filesystem;
namespace r = std::ranges;
namespace rv = r::views;

BOOST_DEFINE_ENUM_CLASS(TargetType, Executable, SharedLibrary, StaticLibrary);

export struct Dependency {
  std::string name;
  std::vector<std::string> modules;
};

BOOST_DESCRIBE_STRUCT(Dependency, (), (name, modules));

export struct BuildTarget {
  TargetType target_type = TargetType::Executable;
  std::string name;

  std::vector<fs::path> sources;
  std::vector<fs::path> include_dirs;

  std::vector<Dependency> dependencies;

  std::vector<std::string> compile_args;
  std::vector<std::string> link_args;
};
BOOST_DESCRIBE_STRUCT(BuildTarget, (),
                      (target_type, name, sources, compile_args, link_args));

export struct Workspace {
  std::vector<fs::path> members;
};

export struct ProjectConfig {
  fs::path root_dir;
  fs::path build_dir;
  std::optional<Workspace> workspace;
  std::vector<BuildTarget> targets;
};

BOOST_DESCRIBE_STRUCT(ProjectConfig, (), (root_dir, build_dir, targets));

template <typename T>
auto get_toml_array_as(const toml::array& array) -> std::vector<T> {
  return rv::all(array) |
         rv::transform([](const auto& node) -> std::optional<T> {
           return node.template value<T>();
         }) |
         rv::filter([](const auto& opt) { return opt.has_value(); }) |
         rv::transform([](const auto& opt) { return opt.value(); }) |
         r::to<std::vector>();
}

export auto parse_workspace(const fs::path& workspace) -> BuildTarget;

export auto parse_project(const fs::path& project_root) {
  ProjectConfig config;

  config.root_dir = project_root;
  config.build_dir = project_root / "build";

  BuildTarget default_target{};

  const auto result = toml::parse_file((project_root / "buildr.toml").string());
  if (!result) {
    log::error("Failed to parse config file ({}): {}",
               (project_root / "buildr.toml").string(),
               result.error().description());
  }

  const auto& tbl = result.table();

  if (tbl.contains("workspace")) {
    const auto members =
        get_toml_array_as<std::string>(
            *(tbl["workspace"]["members"].as_array())) |
        rv::transform([](const auto& s) { return fs::path(s); }) |
        r::to<std::vector>();

    for (const auto& member : members) {
      config.targets.push_back(parse_workspace(project_root / member));
    }
  }

  default_target.name = project_root.stem().string();

  if (tbl.contains("compile_args"))
    default_target.compile_args =
        get_toml_array_as<std::string>(*tbl["compile_args"].as_array());

  if (tbl.contains("link_args"))
    default_target.link_args =
        get_toml_array_as<std::string>(*tbl["link_args"].as_array());

  if (tbl.contains("include_dirs")) {
    default_target.include_dirs =
        rv::all(*tbl["include_dirs"].as_array()) |
        rv::transform([](const auto& node) -> std::optional<fs::path> {
          return node.template value<std::string>();
        }) |
        rv::filter([](const auto& opt) { return opt.has_value(); }) |
        rv::transform([](const auto& opt) { return opt.value(); }) |
        r::to<std::vector>();
  }

  if (tbl.contains("srcs")) {
    default_target.sources =
        rv::all(*tbl["srcs"].as_array()) |
        rv::transform([](const auto& node) -> std::optional<fs::path> {
          return node.template value<std::string>();
        }) |
        rv::filter([](const auto& opt) { return opt.has_value(); }) |
        rv::transform([](const auto& opt) { return opt.value(); }) |
        r::to<std::vector>();
  }

  if (tbl.contains("dependencies")) {
    for (const auto& [key, val] : *tbl["dependencies"].as_table()) {
      Dependency dep{.name = std::string(key.str())};

      if (val.as_table() != nullptr) {
        if (val.as_table()->contains("modules")) {
          const auto modules = *(*val.as_table())["modules"].as_array();
          dep.modules = modules | rv::transform([](const auto& m) {
                          return std::string(m.as_string()->value_or(""));
                        }) |
                        r::to<std::vector>();
        }
      }

      default_target.dependencies.push_back(dep);
    }
  }

  config.targets.push_back(default_target);

  return config;
}

export auto parse_workspace(const fs::path& workspace) -> BuildTarget {
  auto p = parse_project(workspace);
  return p.targets.front();
}

}  // namespace config
