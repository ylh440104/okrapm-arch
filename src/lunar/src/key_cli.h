#pragma once
#include <string>
#include <vector>

namespace okrapm {
int RunKeyCommand(const std::vector<std::string>& Args);
int RunKeyringCommand(const std::vector<std::string>& Args);
}