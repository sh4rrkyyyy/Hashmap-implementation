#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include "hashmap.h"
#include <algorithm>
#include <vector>
#include <list>
#include <utility>
#include <iostream>
#include <unordered_map>
#include <chrono>

using namespace std::chrono;

TEST_CASE("const") {
    std::vector<std::pair<int, int>> orig_data{{1, 5}, {3, 4}, {2, 1}};
    const HashMap<int, int> map{{1, 5}, {3, 4}, {2, 1}};
    REQUIRE(!map.empty());
    REQUIRE(map.size() == 3);
    auto it = map.find(3);
    REQUIRE(it->second == 4);
    it = map.find(7);
    REQUIRE(it == map.end());
    std::sort(orig_data.begin(), orig_data.end());
    std::vector<std::pair<int, int>> new_data;
    for (const auto& elem : map) {
        new_data.push_back(elem);
    }
    std::sort(new_data.begin(), new_data.end());
    REQUIRE(std::equal(orig_data.begin(), orig_data.end(), new_data.begin(), new_data.end()));
}

TEST_CASE("references") {
    std::list<std::pair<int, int>> l{{3, 4}, {8, 5}, {4, 7}, {-1, -3}};
    HashMap<int, int> map(l.begin(), l.end());
    map[3] = 7;
    REQUIRE(map[3] == 7);
    REQUIRE(map.size() == 4);
    REQUIRE(map[0] == 0);
    REQUIRE(map.size() == 5);
}

size_t smart_hash(int) {
    return 0;
}

struct Int {
    int x;
    bool operator==(Int rhs) const {
        return x == rhs.x;
    }
};

struct Hasher {
    size_t operator()(Int x) const {
        return x.x % 17239;
    }
};

TEST_CASE("hash") {
    {
        HashMap<Int, std::string, Hasher> m;
        REQUIRE(m.empty());
        m.insert(Int{0}, "a");
        REQUIRE(m[Int{0}] == "a");
        m.insert(Int{17239}, "b");
        REQUIRE(m[Int{17239}] == "b");
        REQUIRE(m.size() == 2);
        REQUIRE(m.hash_function()(Int{17239}) == 0);
    }
    {
        HashMap<int, int, std::function<size_t(int)>> smart_map(smart_hash);
        auto hash_func = smart_map.hash_function();
        for (int i = 0; i < 1000; ++i) {
            smart_map[i] = i + 1;
            REQUIRE(hash_func(i) == 0);
        }
        REQUIRE(smart_map.size() == 1000);
    }
}

TEST_CASE("iterators") {
    HashMap<int, int> first(10000);
    first.insert(1, 2);
    HashMap<int, int>::iterator it = first.begin();
    REQUIRE(it++ == first.begin());
    REQUIRE(!(it != first.end()));
    REQUIRE(++first.begin() == first.end());
    REQUIRE(*first.begin() == std::make_pair(1, 2));
    REQUIRE(first.begin()->second == 2);
    first.erase(1);
    REQUIRE(first.begin() == first.end());
}

TEST_CASE("backshift") {
    auto h = [](int v) {
        return v % 1000;
    };
    HashMap<int, int, decltype(h)> m(1000, h);
    for (int i = 1; i <= 100; ++i) {
        m.insert(i, 100 - i);
        REQUIRE(m.bucket(i) == i);
    }
    REQUIRE(m.size() == 100);
    for (int i = 90; i <= 100; ++i) {
        m[i + 1000] = 1;
    }
    REQUIRE(m.size() == 111);
    REQUIRE(m.bucket(1100) == 111);
    m.erase(50);
    REQUIRE(m.bucket(55) == 55);
    m.erase(1091);
    REQUIRE(m.bucket(1100) == 110);
}

template<class Map>
std::pair<nanoseconds, nanoseconds> RunStress(Map& map, int seed, size_t elems_count, size_t iterations, std::vector<int>& responses) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> dist(0, elems_count);
    for (size_t i = 0; i < elems_count; ++i) {
        map.insert({dist(gen), {}});
    }
    auto start = steady_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        auto val = dist(gen);
        auto resp = map.erase(val);
        responses.push_back(resp);
        if (!resp) {
            map.insert({val, {}});
        }
    }
    auto stop = steady_clock::now();
    auto duration = stop - start;

    nanoseconds max = {};
    for (size_t i = 0; i < iterations; ++i) {
        auto val = dist(gen);
        auto start = steady_clock::now();
        if (!map.erase(val)) {
            map.insert({val, {}});
        }
        auto stop = steady_clock::now();
        max = std::max(max, duration_cast<nanoseconds>(stop - start));
    }
    return std::make_pair(duration, max);
}

TEST_CASE("stress") {
    const size_t elems_count = 1e5;
    const size_t iterations = 1e6;
    const int seed = 12345;
    struct Data {
        double d[3];
    };
    std::vector<int> ok_responses;
    ok_responses.reserve(iterations);
    std::vector<int> check_responses;
    check_responses.reserve(iterations);
    HashMap<int, Data> h(elems_count);
    std::unordered_map<int, Data> m(elems_count);

    auto [h_duration, h_max] = RunStress(h, seed, elems_count, iterations, check_responses);
    auto [m_duration, m_max] = RunStress(m, seed, elems_count, iterations, ok_responses);
    REQUIRE(check_responses == ok_responses);

    std::cerr << "unordered_map: mean " << duration_cast<nanoseconds>(m_duration).count() / iterations << ", max "
              << m_max.count() << " ns/iter" << "\n";
    std::cerr << "hashmap      : mean " << duration_cast<nanoseconds>(h_duration).count() / iterations << ", max "
              << h_max.count() << " ns/iter" << "\n";
}
