module;

#include <libpkgconf/libpkgconf.h>

#include <functional>
#include <memory>
#include <print>
#include <ranges>
#include <string>
#include <vector>

#include "format.hpp"

export module dependencies_mod;

import config_mod;

namespace deps {

static auto error_handler(const char* message, const pkgconf_client_t*, void*)
    -> bool {
  std::println("pkgconf error: {}", message);
  return true;
}

struct FragmentsDeleter {
  void operator()(pkgconf_list_t* f) const { pkgconf_fragment_free(f); }
};

constexpr auto kPkgConfFlags =
    PKGCONF_PKG_PKGF_SIMPLIFY_ERRORS | PKGCONF_PKG_PKGF_SKIP_PROVIDES;

static auto get_pkgconf_client() {
  std::unique_ptr<pkgconf_client_t, void (*)(pkgconf_client_t*)> client(
      pkgconf_client_new(error_handler, nullptr,
                         pkgconf_cross_personality_default()),
      [](pkgconf_client_t* c) {
        if (c != nullptr) pkgconf_client_free(c);
      });

  pkgconf_client_set_flags(client.get(), kPkgConfFlags);

  pkgconf_client_dir_list_build(client.get(),
                                pkgconf_cross_personality_default());
  return client;
}

export auto check_deps(const std::vector<config::Dependency>& deps) -> bool {
  auto client = get_pkgconf_client();

  if (client == nullptr) {
    std::println("failed to init pkgconf client");
  }

  for (const auto& dep : deps) {
    std::println("Searching for dependency: {}", dep.name);

    auto pkg = pkgconf_pkg_find(client.get(), dep.name.c_str());
    std::println("Found: {}", pkg != nullptr);
  }

  return false;
}

export auto get_compile_args(const config::Dependency& dep)
    -> std::vector<std::string> {
  auto client = get_pkgconf_client();

  pkgconf_client_set_flags(client.get(),
                           kPkgConfFlags | PKGCONF_PKG_PKGF_SEARCH_PRIVATE |
                               PKGCONF_PKG_PKGF_MERGE_PRIVATE_FRAGMENTS);

  auto pkg = pkgconf_pkg_find(client.get(), dep.name.c_str());
  if (pkg == nullptr) {
    std::println("Failed to find package: {}", dep.name);
    return {};
  }

  pkgconf_list_t list = pkg->cflags;

  std::vector<std::string> cflags;

  std::unique_ptr<pkgconf_list_t, FragmentsDeleter> fd(&list);
  pkgconf_node_t* node = nullptr;
  PKGCONF_FOREACH_LIST_ENTRY(list.head, node) {
    auto frag = static_cast<const pkgconf_fragment_t*>(node->data);
    cflags.push_back(std::format("-{}{}", frag->type, frag->data));
  }

  return cflags;
}

export auto get_compile_args(const std::vector<config::Dependency>& deps) {
  return deps | std::views::transform([](const auto& dep) {
           return get_compile_args(dep);
         }) |
         std::views::join | std::ranges::to<std::vector>();
}

auto get_link_args(const std::string& dep_name) -> std::vector<std::string> {
  auto client = get_pkgconf_client();

  pkgconf_client_set_flags(client.get(),
                           kPkgConfFlags | PKGCONF_PKG_PKGF_SEARCH_PRIVATE |
                               PKGCONF_PKG_PKGF_MERGE_PRIVATE_FRAGMENTS);

  auto pkg = pkgconf_pkg_find(client.get(), dep_name.c_str());
  if (pkg == nullptr) {
    std::println("Failed to find package: {}", dep_name);
    return {};
  }

  pkgconf_list_t list = pkg->libs;

  std::vector<std::string> libs;

  std::unique_ptr<pkgconf_list_t, FragmentsDeleter> fd(&list);
  pkgconf_node_t* node = nullptr;
  PKGCONF_FOREACH_LIST_ENTRY(list.head, node) {
    auto frag = static_cast<const pkgconf_fragment_t*>(node->data);
    libs.push_back(std::format("-{}{}", frag->type, frag->data));
  }

  return libs;
}

auto get_var(const std::string& dep_name, const std::string& var_name)
    -> std::string {
  auto client = get_pkgconf_client();
  auto pkg = pkgconf_pkg_find(client.get(), dep_name.c_str());
  return pkgconf_tuple_find(client.get(), &pkg->vars, var_name.c_str());
}

auto get_link_args(const config::Dependency& dep) -> std::vector<std::string> {
  if (dep.name == "boost") {
    std::vector<std::string> mods = {"-L" + get_var(dep.name, "libdir")};
    for (const auto& mod : dep.modules) mods.push_back("-lboost_" + mod);
    return mods;
  }

  return get_link_args(dep.name);
}

export auto get_link_args(const std::vector<config::Dependency>& deps) {
  return deps | std::views::transform([](const auto& dep) {
           return get_link_args(dep);
         }) |
         std::views::join | std::ranges::to<std::vector>();
}

}  // namespace deps
