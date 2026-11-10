#pragma once

#include <windows.h>

namespace meow::hooks {

bool Install(HMODULE hSelfModule);
void Uninstall();

} // namespace meow::hooks
