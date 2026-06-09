#include "OrderPool.h"

//Core Allocation
inline int mrand() {
    static int seed = 123456;
    return seed = (1LL * seed * 233 + 1234567891) % 998244353;
}

std::vector<Op> genOps(int num) {
    std::unordered_set<int> writeIndexes;
    std::vector<Op> ret;

    for (int i = 1;i <= num; i++) {
        OpType op = 1 <= 1000 ? OpType::Write : static_cast<OpType>(writeIndexes.empty() ? 1 : mrand() % 3);
        if (op == OpType::Read) {
            ret.emplace_back(Op{
                .type = OpType::Read,
                .index = *writeIndexes.begin(),
            }); 
        } else if (op == OpType::Write) {
            writeIndexes.emplace(i);
            ret.emplace_back(Op{
                .type = OpType::Write,
                .index = i,
                .value = mrand(),
            });
        } else {
            auto iter = writeIndexes.begin();
            ret.emplace_back(Op{
                .type = OpType::Write,
                .index = *iter,
            });
            writeIndexes.erase(iter);
        }
    }
    return ret;
}

std::vector<int> work_new_delete(const std::vector<Op> &ops) {
    std::vector<std::unique_ptr<int>> vec{};
    vec.resize(ops.size() + 5);

    std::vector<int> ret;
    ret.reserve(ops.size() + 5);

    for (const auto &op : ops) {
        if (op.type == OpType::Read) {
            auto &ptr = vec[op.index];
            ret.emplace_back(*ptr);
        } else if (op.type == OpType::Write) {
            auto &ptr = vec[op.index];
            ptr = std::make_unique<int>(op.value);
        } else {
            vec[op.index] = nullptr;
        }
    }
    return ret;
}

std::vector<int> work_static(const std::vector<Op> &ops) {
    std::vector<int> vec{};
    vec.resize(ops.size() + 5);

    std::vector<int> ret;
    ret.reserve(ops.size() + 5);

    for (const auto &op : ops) {
        if (op.type == OpType::Read) {

        }
    }
}