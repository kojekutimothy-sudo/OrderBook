#pragma once

#include <cassert>
#include <chrono>
#include <climits>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

//Core Allocation
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

enum OpType {Read,Write,Delete};

struct Op {
    OpType type;
    int index;
    int value;
};

// TODO: add poison/canary values on release() (debug builds)
// TODO: add bitset occupancy tracking for double-free detection