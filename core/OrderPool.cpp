#include "OrderPool.h"

//Core Allocation
inline int mrand() {
    static int64_t seed = 123456;
    return seed = (1LL * seed * 233 + 1234567891) % 998244353;
}

std::vector<Op> genOps(int num) {
    std::unordered_set<int> writeIndexes;
    std::vector<Op> ret;

    for (int i = 1;i <= num; i++) {
        OpType op = i <= 1000 ? OpType::Write : static_cast<OpType>(writeIndexes.empty() ? 1 : mrand() % 3);
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
                .type = OpType::Delete,
                .index = *iter,
            });
            writeIndexes.erase(iter);
        }
    }
    return ret;
}

std::vector<Op> genOpsForCompactTest(int num) {
    std::unordered_set<int> writeIndexes;
    std::vector<Op> ret;

    for (int i = 1; i <= num; i++) {
        OpType op = i <= 1000000 ? OpType::Write : static_cast<OpType>(writeIndexes.empty() ? 1 : (mrand() % 2 == 0 ? 0 : 2));
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
                .type = OpType::Delete,
                .index = *iter
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
            auto v = vec[op.index];
            ret.emplace_back(v);
        } else if (op.type == OpType::Write) {
            vec[op.index] = op.value;
        }
    }
    return ret;
}

template <typename T> struct SlabSimple {
    std::vector<T> allocs;
    std::vector<int> freed;

    explicit SlabSimple(int size) { this->allocs.reserve(size); }

    inline int set(T v) {
	if (!freed.empty()) {
        int idx = freed.back();
        freed.pop_back();
        allocs[idx] = v;
        return idx;
	} else {
        int idx = allocs.size();
        allocs.emplace_back(v);
        return idx;
	}
    }

    inline const T& get(int k) { return allocs[k].data.value; }

    inline void remove(int k) { freed.emplace_back(k); }
};

template <typename T> class PagedArray {
    public:
        static constexpr uint16_t BITS = 11;
        static constexpr uint16_t MAX_SIZE = 1 << BITS;
        static constexpr uint16_t SIZE_MASK = MAX_SIZE - 1;

        struct Ptr {
        uint32_t pageIndex = 0;
        uint16_t innerIndex = 0;
        T *ptr = nullptr;

        inline T &value() const { return *ptr; }

        inline uint32_t raw() const { return pageIndex << BITS | innerIndex; }

        inline bool operator!=(const Ptr &rhs) const {
            return pageIndex != rhs.pageIndex || innerIndex != rhs.innerIndex;
        }
        inline bool operator==(const Ptr &rhs) const {
            return pageIndex == rhs.pageIndex && innerIndex == rhs.innerIndex;
        }
        };

        inline void emplace_back(T &&value) {
            auto &back = list_.back();

            if (list_.empty() || list_.back().size == MAX_SIZE) {
                auto innerArray = std::unique_ptr<T[]>(new T[MAX_SIZE]);
                innerArray[0] = std::move(value);
                list_.emplace_back(Inner{
                .array = std::move(innerArray),
                .size = 1,
                .notErasedSize = 1,
                });
            } else {
                auto &back = list_.back();
                back.array[back.size++] = std::move(value);
                ++back.notErasedSize;
            }
            size_++;
        }

        inline void erase(uint32_t k) {
            auto pageIndex = k >> BITS;
            auto &paged = list_[pageIndex];
            --paged.notErasedSize;
            if (paged.notErasedSize < 0) {
                throw std::runtime_error("page " + std::to_string(k) + " erase fail");
            }
        }

        inline void compact(const std::function<bool(const T &)> hasValue) {
        for (auto &inner : list_) {
            if (inner.notErasedSize == 0) {
            inner.array = nullptr;
            inner.size = 0;
            }
        }

        while (!list_.empty()) {
            auto &back = list_.back();
            if (back.array == nullptr) {
            list_.pop_back();
            size_ -= MAX_SIZE;
            continue;
            }

            while (back.size > 0 && !hasValue(back.array[back.size - 1])) {
            --back.size;
            --back.notErasedSize;
            --size_;
            }
            if (back.size == 0) {
            list_.pop_back();
            continue;
            }
            break;
        }
        }

        inline uint32_t size() const { return size_; }

        inline T &operator[](uint32_t idx) { return list_[idx >> BITS].array[idx & SIZE_MASK]; }

        inline constexpr Ptr begin() {
        return {
            .pageIndex = 0,
            .innerIndex = 0,
            .ptr = list_.empty() ? nullptr : &list_.front().array[0],
        };
        }

        inline Ptr end() {
        return {
            .pageIndex = static_cast<uint32_t>(list_.empty() ? 0 : list_.size() - 1),
            .innerIndex = static_cast<uint16_t>(list_.empty() ? 0 : list_.back().size),
            .ptr = nullptr,
        };
        }

        inline void nextPtr(Ptr &it) {
        uint32_t listSize = this->list_.size();
        if (listSize == 0) {
            it = end();
            return;
        }

        auto innerMax = it.pageIndex + 1 < listSize ? MAX_SIZE : list_[it.pageIndex].size;

        if (it.innerIndex < innerMax) {
            ++it.innerIndex;
            ++it.ptr;
        }
        if (it.innerIndex == innerMax) {
            if (it.pageIndex + 1 == listSize) {
            it.ptr = nullptr;
            return;
            }

            ++it.pageIndex;
            it.innerIndex = 0;
            Inner *inner = &list_[it.pageIndex];
            while (it.pageIndex + 1 < listSize && inner->array == nullptr) {
            ++it.pageIndex;
            ++inner;
            }
            if (it.pageIndex + 1 == listSize && inner->array == nullptr) {
            it = end();
            return;
            }
            it.ptr = &inner->array[0];
        }
        }

private:
    struct Inner {
	std::unique_ptr<T[]> array = nullptr;
	uint16_t size = 0;
	int16_t notErasedSize = 0;
    };
    uint32_t size_ = 0;
    std::vector<Inner> list_;
};
template <typename T> struct Slab {

    union Data {
	T value;
	uint32_t next = UINT_MAX;
    };
    struct WT {
	bool hasValue = false;
	Data data;
    };
    PagedArray<WT> allocs;
    uint32_t next = UINT_MAX;

    explicit Slab() {}

    inline int set(const T &v) {
	if (this->next == UINT_MAX) {
            uint32_t idx = allocs.size();
            allocs.emplace_back(WT{
            .hasValue = true,
            .data =
                Data{
                .value = v,
                },
            });
            return idx;
        } else {
            uint32_t idx = this->next;
            auto &block = allocs[idx];
            this->next = block.data.next;
            block = WT{
            .hasValue = true,
            .data =
                Data{
                    .value = v,
                },
            };
            return idx;
	    }
    }

    inline const T& get(int k) { return allocs[k].data.value;}

    inline void remove(int k) {
        auto &block = this->allocs[k];
        if (!block.hasValue) {
            throw std::runtime_error("no value");
        }
        block.data.next = this->next;
        block.hasValue = false;
        this->next = k;
    }

    inline void compact() {
	allocs.compact([](const auto &v) { return v.hasValue; });

	auto itEnd = allocs.end();

	auto it = allocs.begin();
	while (it != itEnd && it.value().hasValue) {
            allocs.nextPtr(it);
	}
	this->next = it == itEnd ? UINT_MAX : it.raw();

	while (it != itEnd) {
            auto itNext = it;
            allocs.nextPtr(itNext);
            while (itNext != itEnd && itNext.value().hasValue) {
            allocs.nextPtr(itNext);
            }
            it.value().data.next = itNext == itEnd ? UINT_MAX : itNext.raw();

            it = itNext;
	}
    }
};

template <typename S> std::vector<int> work_slab(S &slab, const std::vector<Op> &ops) {
    std::vector<int> vec;
    vec.reserve(ops.size() + 5);

    std::vector<int> ret;
    ret.reserve(ops.size() + 5);

    for (int i = 0; i < ops.size(); i++) {
	const auto &op = ops[i];
	if (op.type == OpType::Read) {
            auto k = vec[op.index];
            ret.emplace_back(slab.get(k));
        } else if (op.type == OpType::Write) {
            vec[op.index] = slab.set(op.value);
        } else {
            slab.remove(vec[op.index]);
        }

        if constexpr (std::is_same<S, Slab<int>>()) {
            static constexpr int mask = (1 << 22) - 1;
            if ((i & mask) == mask) {
            slab.compact();
            }
        }
    }
    return ret;
}

std::vector<int> work_slab_simple(const std::vector<Op> &ops) {
    SlabSimple<int> slab(1024);
    return work_slab(slab, ops);
}

std::vector<int> work_slab(const std::vector<Op> &ops) {
    Slab<int> slab;
    return work_slab(slab, ops);
}

template <typename T> T doPerf(const char *scope, const std::function<T()> &fn) {
    auto t1 = high_resolution_clock::now();
    const T &ret = fn();
    auto t2 = high_resolution_clock::now();
    auto ms_int = duration_cast<milliseconds>(t2 - t1);
    std::cout << "[" << scope << "] run " << ms_int.count() << " ms" << std::endl;
    return ret;
}

int main() {
    const auto ops = genOps(100000000);

    auto ans = doPerf<std::vector<int>>("New/Delete", [&]() { return work_new_delete(ops); });
    auto ans_static = doPerf<std::vector<int>>("Static", [&]() { return work_static(ops); });
    auto s2 = doPerf<std::vector<int>>("Slab", [&]() { return work_slab(ops); });
    auto s1 = doPerf<std::vector<int>>("Slab Simple", [&]() { return work_slab_simple(ops); });

    std::cout << ans.size() << std::endl;
    std::cout << (ans == s1) << std::endl;
    std::cout << (ans == s2) << std::endl;
    std::cout << (ans == ans_static) << std::endl;

    return 0;
}