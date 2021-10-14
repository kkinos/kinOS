#pragma once

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>

#include "../../libs/common/template.hpp"
#include "../../libs/gui/guisyscall.hpp"

Message sent_message[1];
Message received_message[1];

std::map<uint64_t, uint64_t>* am_table;  // first:id second:p_id

void ProcessAccordingToMessage(Message* msg);

void CreateNewTask(uint64_t p_id, uint64_t fs_id, char* arg);

void ExecuteAppTask(uint64_t p_id, uint64_t id, char* arg, uint64_t fs_id);