module;

#include <boost/describe.hpp>
#include <ranges>
// #include <boost/json.hpp>
// #include <boost/json/src.hpp>
#include <filesystem>
#include <fstream>
#include <string>

export module compile_db;

export namespace compiledb {

// template <class T,
//           class D1 = boost::describe::describe_members<
//               T, boost::describe::mod_public |
//               boost::describe::mod_protected>,
//           class D2 = boost::describe::describe_members<
//               T, boost::describe::mod_private>,
//           class En = std::enable_if_t<boost::mp11::mp_empty<D2>::value &&
//                                       !std::is_union<T>::value>>
// void to_json(nlohmann::json &json, const T &obj) {
//   boost::mp11::mp_for_each<D1>(
//       [&](auto D) { json[D.name] = nlohmann::json(obj.*D.pointer); });
// }

struct CompileDatabase {
  std::filesystem::path directory;
  std::string command;
  std::filesystem::path file;
  std::filesystem::path output;
};

BOOST_DESCRIBE_STRUCT(CompileDatabase, (), (directory, command, file, output))

void generate() {
  CompileDatabase b{};

  // auto const json_str = boost::json::value_from(b);
  // std::ofstream f("build/compile_commands.json");
  // f << json_str;
  // f.close();
}

} // namespace compiledb
