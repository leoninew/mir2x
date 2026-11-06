#pragma once
#include <map>
#include <vector>
#include <cstdint>
#include "serdesmsg.hpp"
#include "dropitemconfigtype.hpp"

std::vector<SDItem> getMonsterDropItemList(uint32_t);
const std::map<int, std::vector<DropItemConfig>> &getMonsterDropItemConfigList(uint32_t);
