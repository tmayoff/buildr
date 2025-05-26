module;

#include <boost/algorithm/string/join.hpp>
#include <filesystem>
#include <format>
#include <vector>

export module build_mod;

export namespace builder {

constexpr auto kCompiler = "clang++";

namespace fs = std::filesystem;

auto get_compiler_command(const fs::path& build_root, const fs::path& source,
                          const std::vector<std::string>& extra_args) {
  fs::path out = build_root / fs::path(source.stem().string() + ".o");

  std::vector<std::string> args = extra_args;

  args.emplace_back(
      std::format("-fprebuilt-module-path={}", build_root.string()));

  if (source.extension() == ".cppm") {
    out = build_root / fs::path(source.stem().string() + ".pcm");
    args.emplace_back("--precompile");
  }
  const auto cmd = std::format("{} {} -g -o {} -c {}", kCompiler,
                               boost::algorithm::join(args, " "), out.string(),
                               source.string());
  return std::pair(out, cmd);
}

}  // namespace builder
