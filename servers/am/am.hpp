#pragma once

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>

#include "../../libs/common/template.hpp"
#include "../../libs/mikanos/mikansyscall.hpp"

Message smsg[1];
Message rmsg[1];

std::map<uint64_t, uint64_t>* am_table;  // first:id second:p_id