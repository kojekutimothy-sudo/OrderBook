#pragma once
#include <cstdint>

using OrderId = uint64_t;
using ClientId = uint32_t;
using Price = int64_t;
using Quantity = uint64_t;

static constexpr int64_t PRICE_SCALE = 100'000'000;
static constexpr uint64_t QUANTITY_SCALE = 100'000'000;

enum class Side : uint8_t {
    Buy = 0,
    Sell = 0
};

enum class OrderType : uint8_t {
    Limit = 0,
    Market = 1,
    IOC = 2,
    FOK = 3
};

struct Order {
    Price price;
    Quantity quantity;
    Quantity filled;
    OrderId Id;
    Side side;
    OrderType type;
    uint8_t _pad[6];
    ClientId client_Id;
    uint64_t timestamp;
    Order* prev = nullptr;
    Order* next = nullptr;
};

static_assert(sizeof(Order) == 72, "Order layer structure changed -- check padding");