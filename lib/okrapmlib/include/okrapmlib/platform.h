#pragma once
#include <string>
namespace okrapm {
std::string HostArchitecture();
std::string NormalizeArchitecture(const std::string& Name);
bool ArchitectureMatches(const std::string& Host, const std::string& Target);
std::string OaabiName();
std::string OaabiVersion();
bool OaabiMatches(const std::string& Declared);
}