#include "okrapmlib/platform.h"
#include <algorithm>
#include <cstdlib>
#include <sys/utsname.h>
namespace okrapm {
std::string NormalizeArchitecture(const std::string& Name) {
    std::string Lower = Name;
    std::transform(Lower.begin(), Lower.end(), Lower.begin(),
                   [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
    if (Lower == "amd64" || Lower == "x86-64" || Lower == "x64") return "x86_64";
    if (Lower == "arm64" || Lower == "armv8" || Lower == "armv8l") return "aarch64";
    if (Lower == "i386" || Lower == "i486" || Lower == "i586" || Lower == "i686") return "i686";
    if (Lower == "armv7" || Lower == "armv7l") return "armv7";
    if (Lower == "riscv64") return "riscv64";
    return Lower;
}
std::string HostArchitecture() {
    const char* Override = std::getenv("LUNAR_TARGET_ARCH");
    if (Override && *Override) return NormalizeArchitecture(Override);
    struct utsname Info;
    if (uname(&Info) == 0 && Info.machine[0] != '\0') {
        return NormalizeArchitecture(Info.machine);
    }
    return "unknown";
}
bool ArchitectureMatches(const std::string& Host, const std::string& Target) {
    std::string Left = NormalizeArchitecture(Host);
    std::string Right = NormalizeArchitecture(Target);
    if (Right.empty() || Right == "any" || Right == "noarch") return true;
    if (Left.empty() || Left == "unknown") return true;
    return Left == Right;
}
std::string OaabiName() {
    return "OAABI1";
}
std::string OaabiVersion() {
    return "1";
}
bool OaabiMatches(const std::string& Declared) {
    if (Declared.empty()) return true;
    std::string Lower = Declared;
    std::transform(Lower.begin(), Lower.end(), Lower.begin(),
                   [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
    if (Lower == "any" || Lower == "none") return true;
    return NormalizeArchitecture(Declared) == NormalizeArchitecture(OaabiName()) ||
           Lower == "oaabi1";
}
}