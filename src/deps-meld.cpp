#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <regex>
#include <boost/format.hpp>
#include <nlohmann/json.hpp>
#include <boost/process.hpp>
#include <boost/filesystem.hpp>

#include <boost/process/system.hpp>


namespace inclusive {

  using fmt = boost::format;
  namespace bp = boost::process;
  namespace fs = boost::filesystem;

  constexpr auto GLUE_PATH = "deps/glue";

  struct dep {
    //! The github path or a git clone uri.
    std::string git_uri;

    //! Git tag or commit ref.
    std::string ref;

    //! The subrepository path to add to include path.
    std::string include_path;

    //! True when dependency is needed because of another dependency project.
    bool transitive{};

    //! \return The gihub clone URI if simple-single-slash path otherwise git_uri as-is.
    std::string get_uri() const {
      std::regex github_path("[^/]+/[^/]+");
 
      if (std::regex_match(git_uri, github_path)) {
        return std::string("https://github.com/") + git_uri + ".git";
      } else {
        return git_uri;
      }
    }

    std::string get_name() const {
      std::regex only_name("[^/]+/([^/]+)(\\.git)?");
      std::smatch matched;
      std::regex_match(git_uri, matched, only_name);
      
      if (matched.size() < 2) {
        throw std::runtime_error(str(fmt("Error \"%1%\" is an invalid repository URI or github-path !") % git_uri));
      }

      return matched[1];
    }

  };

  //! Reads the list of deps in the from the given  file, and if it exits it's accompanying .transitive.
  inline std::map<std::string, dep> read_deps(const std::string& path) {
    std::map<std::string, dep> deps;
    
    auto to_dep = [](nlohmann::json::iterator& it) {
      dep d;
      d.git_uri = it.key();
      d.ref = it.value()["@"].get<std::string>();
      
      if (!it.value()["?"].is_null()) {
        d.include_path = it.value()["?"].get<std::string>();
      }

      return d;
    };   

    { // Direct dependencies
      std::ifstream ifs(path.data(), std::ios::in );

      nlohmann::json j;
      ifs >> j;
      for (nlohmann::json::iterator it = j.begin(); it != j.end(); ++it) {
        auto d = to_dep(it);
        deps[d.git_uri] = d;
      }
    }

    auto transitive_path = path + ".transitive";
    if (fs::exists(transitive_path)) { // Transitive dependencies
      std::ifstream ifs(transitive_path.data(), std::ios::in );

      nlohmann::json j;
      ifs >> j;
      for (nlohmann::json::iterator it = j.begin(); it != j.end(); ++it) {
        auto d = to_dep(it);
        d.transitive = true;
        deps[d.git_uri] = d;
      }
    }

    return deps;
  }

  //! Reads the list of deps in the deps.inclusive json file.
  inline void write_deps(const std::string& path, const std::map<std::string, dep>& deps, bool write_direct = false) {
    auto to_json = [](auto d) {
      nlohmann::json dep_j;
      dep_j["@"] = d.second.ref;

      if (d.second.include_path != "") {
        dep_j["-I"] = d.second.include_path;
      }

      return dep_j;
    };

    if (write_direct) { // Direct dependencies
      std::fstream ofs(path.data(), std::ios::in | std::ios::out | std::ios::trunc );
      nlohmann::json j;

      for (auto& dep_entry : deps ) {
        if (!dep_entry.second.transitive) {
          j[dep_entry.first] = to_json(dep_entry);
        }
      }

      ofs << j.dump(2);
    }

    { // Transitive dependencies

      nlohmann::json j;
      for (auto& dep_entry : deps ) {
        if (dep_entry.second.transitive) {
          j[dep_entry.first] = to_json(dep_entry);
        }
      }

      if (j.size() > 0) {
        auto transitive_path = (path + ".transitive");
        std::fstream ofs(transitive_path.data(), std::ios::in | std::ios::out | std::ios::trunc );
        ofs << j.dump(2);
      }
    }
  }


  //! Run git to install the dep if not already present
  inline void check_and_clone(const dep& d) {

    auto returncode = bp::system(bp::shell, "git", "subtree", 
      (fs::exists(fs::path("deps") / d.get_name()) ? "pull" : "add"),
      "--prefix", std::string("deps/") + d.get_name(),
      d.get_uri(), d.ref,
      "--squash");

    if (returncode != 0) {
      throw std::runtime_error("Cannot add the dependency, your working tree must be all commited first!");
    }
  }

  //! Delves in each dependencies at 1 depth level and lookup for further includesive deps. If some pull them up here.
  inline void hikeup_deep_deps(std::map<std::string, dep>& deps) {

    for (auto& d : deps) {
      auto tape = fs::path("deps") / d.second.get_name() / GLUE_PATH;
      if (fs::exists(tape)) {
        auto deep_deps = read_deps(tape.native());

        // For us they are all transitive
        std::for_each(deep_deps.begin(), deep_deps.end(),
          [](auto& p) { p.second.transitive  = true;});

        deps.insert(deep_deps.begin(), deep_deps.end());
      }
    }

    // Write down the transitive deps
    write_deps(GLUE_PATH, deps);   

    // Now that we have hiked up the deps of our deps, clone and add them again at our level.
    // We have to duplicate the files because windows-git-clone symlinks impossible. Sad.
    for (auto& d : deps) { check_and_clone(d.second); }
  }


  //TODO: Generate CMakeLists.txt to include the stuffs
  //TODO: Generate CONSUME.md with all compilers include path generated to explain how to use.

}

  

int main() {
  auto deps = inclusive::read_deps(inclusive::GLUE_PATH); 

  for (auto& d : deps) { inclusive::check_and_clone(d.second); }
  hikeup_deep_deps(deps);

  return 0;
}