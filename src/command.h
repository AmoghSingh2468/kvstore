#pragma once
#include <string>
#include <vector>
#include "store.h"
#include "wal.h"

void execute(Store& store, Wal* wal,
             const std::vector<std::string>& args, std::string& out);