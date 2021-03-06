#ifndef INGLUED_GENERATE_CMAKELISTS_HPP
#define INGLUED_GENERATE_CMAKELISTS_HPP

#include <string>
#include <fstream>
#include <regex>
#include <mstch/mstch.hpp>
#include <boost/filesystem.hpp>

#include <inglued/dep.hpp>
#include <inglued/tpl.hpp>
#include <inglued/cmakelist_tpl.hpp>
#include <inglued/adapter/boostorg.hpp>

namespace inglued {

  namespace fs = boost::filesystem;

  constexpr auto CMAKELISTS_TPL_PATH = "CMakeLists.txt.tpl";
  constexpr auto CMAKE_PACKAGE_CONFIG_TPL_PATH = "cmake/modules/Config.cmake.in.tpl";
  
  constexpr auto CMAKELISTS_PATH = "CMakeLists.txt";
  constexpr auto CMAKE_PACKAGE_CONFIG_PATH = "cmake/modules/Config.cmake.in";

  struct cmake_info {
    std::string package;
    std::string target;
  };

  //TODO: Make this cmake mapping loadable from a file to allow adapting to sysroot installed packages
  const std::map<std::string, cmake_info> cmake_package_map {
    {"boostorg/.*", {"Boost", "Boost::boost"}},
    {"nlohmann/json", {"nlohmann_json", "nlohmann_json"} }
  };

  //! Generates CMakeLists.txt to make the project usable by cmake projects
  inline void generate_cmakelists(const std::string& org, const std::string& project, const std::string& project_srcs, map_deps_t& deps) {
    using boost::algorithm::ends_with;

    std::string cmakelist_view{cmakelist_tpl};
    std::string package_config_view{cmakelist_package_config_tpl};

    use_template_if_exists(CMAKELISTS_TPL_PATH, cmakelist_view);
    use_template_if_exists(CMAKE_PACKAGE_CONFIG_TPL_PATH, package_config_view);

    auto generate_cmakelists_txt = [&](auto path) {
      mstch::config::escape = [](const std::string& str) { return str; };

      mstch::array mstch_deps;
      for (auto d : deps) {

        auto cmake_package_name = d.second.get_gh_organization();
        auto cmake_target_name = d.second.get_gh_organization() + "::" + d.second.get_gh_name();

        // Check if library has a cmake package mapping where general rule doesn't fit anymore
        for (const auto& mapping : cmake_package_map) {
          std::regex rx{mapping.first};
          if (std::regex_match(d.second.git_uri, rx)) {
            cmake_package_name = mapping.second.package;
            cmake_target_name = mapping.second.target;
            break;
          }
        }

        mstch_deps.push_back(
          mstch::map{
            {"cmake_package_name", cmake_package_name},
            {"cmake_target_name", cmake_target_name},
            {"org", d.second.get_gh_organization()},
            {"name", d.second.get_gh_name()},
            {"ref", d.second.ref},
            {"include_path", d.second.include_path},
            {"include_path_end_backslash", ((d.second.include_path.empty()) ? 
                "" 
              : ((ends_with(d.second.include_path, "/")) ? d.second.include_path : (d.second.include_path + "/")) 
              )
            }
          }
        );
      }

      mstch::map context{
        {"org", org},
        {"project", project},
        {"project_srcs", project_srcs},
        {"deps", mstch_deps}
      };
      
      if (!fs::path(path).parent_path().empty()) { fs::create_directories(fs::path(path).parent_path()); }

      std::fstream cmakelists{path, std::ios::trunc | std::ios::in | std::ios::out };
      cmakelists.exceptions(std::ios::badbit);

auto constexpr cmakelist_header= R"(
# This is an #inglued <> generated CMakeLists.txt (https://github.com/header-only/inglued)
# To modify it edit CMakeLists.txt.tpl ( i.e. Generate it with `inglued cmaketpl` )

)";

      cmakelists << cmakelist_header;
      cmakelists << mstch::render(cmakelist_view, context);

      std::cout << "Generated " << path << std::endl;
    };

    auto generate_package_config = [&](auto path) {
      if (!fs::path(path).parent_path().empty()) { fs::create_directories(fs::path(path).parent_path()); }

      std::fstream pkgconfig{path, std::ios::trunc | std::ios::in | std::ios::out };
      pkgconfig.exceptions(std::ios::badbit);
      pkgconfig << package_config_view;

      std::cout << "Generated " << path << std::endl;
    };

    generate_cmakelists_txt(CMAKELISTS_PATH);
    generate_package_config(CMAKE_PACKAGE_CONFIG_PATH);
  }

  //! Generate tpl file for the cmake freaks !
  void generate_cmakelists_tpl() {

    std::cout << "😁 CMake expert : generating templates that you can edit in "
      << CMAKELISTS_TPL_PATH << " and " << CMAKE_PACKAGE_CONFIG_PATH
      << std::endl;

    std::fstream cmakelists{CMAKELISTS_TPL_PATH, std::ios::trunc | std::ios::in | std::ios::out };
auto constexpr inline_help = R"(
# This is an #inglued <> template !
#
# The template is processed by http://mustache.github.io/, more info about
# syntax here : http://mustache.github.io/mustache.5.html 
#
# You can access the following variables : 
# * {{org}} : github organization name
# * {{project}} : current project name
# * {{project_srcs}} : current project srcs folder.
#
# * {{#deps}} {{/deps}} : all deps direct and transitive 
#   - {{cmake_package_name}} : The cmake package name from the cmake_package_map otherwise: {{org}}
#   - {{cmake_target_name}} : The cmake target name from cmake_package_map otherwise: {{cmake_package_name}}::{{name}}
#   - {{org}} : the github organization name
#   - {{name}} : the dependency repository name
#   - {{ref}} : tag or branch wished for the dep
#   - {{include_path}} : the path you specified in deps/inglued -I
#   - {{include_path_end_backslash}} : same as above but with a guaranteed end slash.
#
)";

    cmakelists << inline_help << cmakelist_tpl;

    std::fstream pkgconfig{CMAKE_PACKAGE_CONFIG_TPL_PATH, std::ios::trunc | std::ios::in | std::ios::out };
    pkgconfig << cmakelist_package_config_tpl;
  }
  
  /*TODO: Handle the mapping of inglued libraries and their system find_package counterparts.
            * Meaning not all libraries are inglued today (which is sad)
            * So how can we include them for the moment ? Anything in boostorg/ can be #inglued
            * with the official Boost find_package. But do we want this ?
  */

    
  //TODO: Generation of simple test folder for the lib.

}

#endif
