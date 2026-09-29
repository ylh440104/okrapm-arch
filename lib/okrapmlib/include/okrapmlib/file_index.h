#pragma once
#include <string>
#include <vector>
#include <optional>
#include <map>
namespace okrapm {
struct FileOwner {
    std::string ns;
    std::string name;
    std::string version;
    std::string path;
    bool symlink{false};
    std::string target;
};
class FileIndex {
public:
    bool load(const std::string& Path);
    bool save(const std::string& Path) const;
    void record(const std::string& Ns, const std::string& Name, const std::string& Version,
                const std::vector<std::string>& Files);
    void record_entry(const FileOwner& Owner);
    void forget(const std::string& Ns, const std::string& Name);
    std::optional<FileOwner> owner_of(const std::string& Path) const;
    std::vector<FileOwner> owners_under(const std::string& Prefix) const;
    std::vector<FileOwner> files_of(const std::string& Ns, const std::string& Name) const;
    std::vector<std::string> conflicts(const std::vector<std::string>& Paths,
                                       const std::string& SkipNs,
                                       const std::string& SkipName) const;
    bool empty() const { return Owners_.empty(); }
    size_t size() const { return Owners_.size(); }
    std::vector<std::string> package_names() const;
private:
    std::map<std::string, FileOwner> Owners_;
};
}