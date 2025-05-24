#include <boost/describe.hpp>
#include <boost/program_options.hpp>
#include <print>

// NOLINTNEXTLINE
BOOST_DEFINE_ENUM_CLASS(Subcommand, unknown, help, build, run, test);

void print_help();
void build();
void run();
void test();

auto main(int argc, char **argv) -> int {
  namespace po = boost::program_options;

  po::options_description opts("options");
  opts.add_options()("help", "Show help screen")(
      "command", po::value<std::string>(), "command to execute");

  po::positional_options_description pos;
  pos.add("command", 1);

  const auto &parsed = po::command_line_parser(argc, argv)
                           .options(opts)
                           .positional(pos)
                           .allow_unregistered()
                           .run();

  po::variables_map vm;
  po::store(parsed, vm);
  po::notify(vm);

  Subcommand subcommand{};
  boost::describe::enum_from_string(vm.at("command").as<std::string>(),
                                    subcommand);

  switch (subcommand) {
    case Subcommand::unknown:
    case Subcommand::help:
      print_help();
      std::exit(1);
    case Subcommand::build:
      build();
    case Subcommand::run:
      run();
    case Subcommand::test:
      test();
      break;
  }
}

void print_help() { std::println("HELP"); }
