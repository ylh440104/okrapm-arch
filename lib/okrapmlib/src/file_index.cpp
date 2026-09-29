#include "okrapmlib/file_index.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
namespace fs = std::filesystem;
namespace okrapm {
namespace {
std::string Escape(const std::string& Value) {
    std::string Out;
    Out.reserve(Value.size());
    for (char C : Value) {
        if (C == '\t' || C == '\n' || C == '\r') {
            Out += ' ';
        } else {
            Out += C;
        }
    }
    return Out;
}
std::vector<std::string> Split(const std::string& Value, char Sep) {
    std::vector<std::string> Parts;
    std::stringstream Stream(Value);
    std::string Item;
    while (std::getline(Stream, Item, Sep)) {
        if (!Item.empty()) Parts.push_back(Item);
    }
    return Parts;
}
}
bool FileIndex::load(const std::string& Path) {
    Owners_.clear();
    std::ifstream Input(Path);
    if (!Input.is_open()) return false;
    std::string Line;
    while (std::getline(Input, Line)) {
        if (Line.empty() || Line[0] == '#') continue;
        auto Fields = Split(Line, '\t');
        if (Fields.size() < 4) continue;
        FileOwner Owner;
        Owner.ns = Fields[0];
        Owner.name = Fields[1];
        Owner.version = Fields[2];
        Owner.path = Fields[3];
        if (Fields.size() > 4) Owner.target = Fields[4];
        Owners_[Owner.path] = Owner;
    }
    return true;
}
bool FileIndex::save(const std::string& Path) const {
    auto Parent = fs::path(Path).parent_path();
    if (!Parent.empty()) {
        std::error_code Ec;
        fs::create_directories(Parent, Ec);
    }
    std::ofstream Output(Path);
    if (!Output.is_open()) return false;
    Output << "# Lunar file index\n";
    for (const auto& Item : Owners_) {
        const auto& Owner = Item.second;
        Output << Escape(Owner.ns) << '\t'
               << Escape(Owner.name) << '\t'
               << Escape(Owner.version) << '\t'
               << Escape(Owner.path) << '\t'
               << Escape(Owner.target) << '\n';
    }
    return true;
}
void FileIndex::record_entry(const FileOwner& Owner) {
    if (Owner.path.empty()) return;
    Owners_[Owner.path] = Owner;
}
void FileIndex::record(const std::string& Ns, const std::string& Name, const std::string& Version,
                       const std::vector<std::string>& Files) {
    for (const auto& File : Files) {
        if (File.empty()) continue;
        FileOwner Owner;
        Owner.ns = Ns;
        Owner.name = Name;
        Owner.version = Version;
        Owner.path = File;
        Owners_[Owner.path] = Owner;
    }
}
void FileIndex::forget(const std::string& Ns, const std::string& Name) {
    for (auto It = Owners_.begin(); It != Owners_.end();) {
        if (It->second.ns == Ns && It->second.name == Name) {
            It = Owners_.erase(It);
        } else {
            ++It;
        }
    }
}
std::optional<FileOwner> FileIndex::owner_of(const std::string& Path) const {
    std::string Key = Path;
    auto Found = Owners_.find(Key);
    if (Found != Owners_.end()) return Found->second;
    if (!Key.empty() && Key[0] != '/') {
        Key = "/" + Key;
        Found = Owners_.find(Key);
        if (Found != Owners_.end()) return Found->second;
    }
    return std::nullopt;
}
std::vector<FileOwner> FileIndex::owners_under(const std::string& Prefix) const {
    std::vector<FileOwner> Result;
    for (const auto& Item : Owners_) {
        if (Item.first.rfind(Prefix, 0) == 0) Result.push_back(Item.second);
    }
    return Result;
}
std::vector<FileOwner> FileIndex::files_of(const std::string& Ns, const std::string& Name) const {
    std::vector<FileOwner> Result;
    for (const auto& Item : Owners_) {
        if (Item.second.ns == Ns && Item.second.name == Name) Result.push_back(Item.second);
    }
    return Result;
}
std::vector<std::string> FileIndex::conflicts(const std::vector<std::string>& Paths,
                                              const std::string& SkipNs,
                                              const std::string& SkipName) const {
    std::vector<std::string> Result;
    std::set<std::string> Seen;
    for (const auto& Path : Paths) {
        auto Owner = owner_of(Path);
        if (!Owner) continue;
        if (Owner->ns == SkipNs && Owner->name == SkipName) continue;
        std::string Label = Path + " owned by " + Owner->ns + "." + Owner->name;
        if (Seen.insert(Label).second) Result.push_back(Label);
    }
    return Result;
}
std::vector<std::string> FileIndex::package_names() const {
    std::set<std::string> Names;
    for (const auto& Item : Owners_) {
        Names.insert(Item.second.ns + "." + Item.second.name);
    }
    return std::vector<std::string>(Names.begin(), Names.end());
}
}