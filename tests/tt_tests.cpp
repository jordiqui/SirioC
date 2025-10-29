#include <cassert>
#include <cstdint>
#include <vector>

#include "sirio/transposition_table.hpp"

namespace {

std::uint64_t mul_high_u64(std::uint64_t lhs, std::uint64_t rhs) {
    const std::uint64_t lhs_hi = lhs >> 32U;
    const std::uint64_t lhs_lo = lhs & 0xFFFF'FFFFULL;
    const std::uint64_t rhs_hi = rhs >> 32U;
    const std::uint64_t rhs_lo = rhs & 0xFFFF'FFFFULL;

    const std::uint64_t hi_hi = lhs_hi * rhs_hi;
    const std::uint64_t hi_lo = lhs_hi * rhs_lo;
    const std::uint64_t lo_hi = lhs_lo * rhs_hi;
    const std::uint64_t lo_lo = lhs_lo * rhs_lo;

    const std::uint64_t mid = hi_lo + lo_hi;
    const std::uint64_t mid_low = mid << 32U;
    const std::uint64_t mid_high = mid >> 32U;

    const std::uint64_t low = lo_lo + mid_low;
    const std::uint64_t carry = low < lo_lo ? 1ULL : 0ULL;

    return hi_hi + mid_high + carry;
}

std::size_t bucket_index_for(std::size_t bucket_count, std::uint64_t key) {
    if (bucket_count == 0) {
        return 0;
    }
    return static_cast<std::size_t>(mul_high_u64(key, static_cast<std::uint64_t>(bucket_count)));
}

std::vector<std::uint64_t> colliding_keys(std::size_t bucket_count, std::size_t count) {
    std::vector<std::uint64_t> keys;
    keys.reserve(count);
    const std::size_t target_index = 0;
    for (std::uint64_t key = 0; keys.size() < count; ++key) {
        if (bucket_index_for(bucket_count, key) == target_index) {
            keys.push_back(key);
        }
    }
    return keys;
}

void test_collision_eviction() {
    const std::size_t previous_size = sirio::get_transposition_table_size();
    sirio::set_transposition_table_size(1);
    sirio::clear_transposition_tables();

    sirio::GlobalTranspositionTable table;
    std::uint8_t generation = table.prepare_for_search();
    const std::size_t bucket_count = table.bucket_count_for_tests();
    assert(bucket_count > 0);

    const std::size_t key_count = sirio::GlobalTranspositionTable::cluster_capacity() + 1;
    const auto keys = colliding_keys(bucket_count, key_count);
    assert(keys.size() == key_count);

    for (std::size_t i = 0; i < keys.size(); ++i) {
        sirio::TTEntry entry;
        entry.best_move.from = static_cast<int>(i % 64);
        entry.best_move.to = static_cast<int>((i + 1) % 64);
        entry.best_move.piece = sirio::PieceType::Queen;
        entry.depth = static_cast<int>(10 + i);
        entry.score = static_cast<int>(100 + i);
        entry.type = sirio::TTNodeType::Exact;
        entry.static_eval = static_cast<int>(200 + i);
        table.store(keys[i], entry, generation);
    }

    const auto latest = table.probe(keys.back());
    assert(latest.has_value());
    assert(latest->score == static_cast<int>(100 + keys.size() - 1));
    assert(latest->best_move.from == static_cast<int>((keys.size() - 1) % 64));

    std::size_t missing = 0;
    for (std::size_t i = 0; i + 1 < keys.size(); ++i) {
        if (!table.probe(keys[i]).has_value()) {
            ++missing;
        }
    }
    assert(missing >= 1);

    sirio::set_transposition_table_size(previous_size);
    sirio::clear_transposition_tables();
}

void test_collision_exact_replacement() {
    const std::size_t previous_size = sirio::get_transposition_table_size();
    sirio::set_transposition_table_size(1);
    sirio::clear_transposition_tables();

    sirio::GlobalTranspositionTable table;
    std::uint8_t generation = table.prepare_for_search();
    const std::size_t bucket_count = table.bucket_count_for_tests();
    assert(bucket_count > 0);

    const std::size_t key_count = sirio::GlobalTranspositionTable::cluster_capacity() + 1;
    const auto keys = colliding_keys(bucket_count, key_count);
    assert(keys.size() == key_count);

    const std::uint64_t primary_key = keys.front();
    sirio::TTEntry primary;
    primary.best_move.from = 0;
    primary.best_move.to = 1;
    primary.best_move.piece = sirio::PieceType::Knight;
    primary.depth = 12;
    primary.score = 250;
    primary.type = sirio::TTNodeType::LowerBound;
    primary.static_eval = 400;
    table.store(primary_key, primary, generation);

    for (std::size_t i = 1; i < keys.size(); ++i) {
        sirio::TTEntry other;
        other.best_move.from = static_cast<int>(i % 64);
        other.best_move.to = static_cast<int>((i + 2) % 64);
        other.best_move.piece = sirio::PieceType::Bishop;
        other.depth = 8;
        other.score = static_cast<int>(300 + i);
        other.type = sirio::TTNodeType::LowerBound;
        other.static_eval = static_cast<int>(500 + i);
        table.store(keys[i], other, generation);
    }

    sirio::TTEntry exact;
    exact.best_move.from = 5;
    exact.best_move.to = 7;
    exact.best_move.piece = sirio::PieceType::Queen;
    exact.depth = 2;
    exact.score = 999;
    exact.type = sirio::TTNodeType::Exact;
    exact.static_eval = 777;
    table.store(primary_key, exact, generation);

    const auto stored = table.probe(primary_key);
    assert(stored.has_value());
    assert(stored->type == sirio::TTNodeType::Exact);
    assert(stored->score == 999);
    assert(stored->best_move.from == 5);
    assert(stored->best_move.to == 7);

    sirio::set_transposition_table_size(previous_size);
    sirio::clear_transposition_tables();
}

void test_collision_replaces_shallow_entries() {
    const std::size_t previous_size = sirio::get_transposition_table_size();
    sirio::set_transposition_table_size(1);
    sirio::clear_transposition_tables();

    sirio::GlobalTranspositionTable table;
    std::uint8_t generation = table.prepare_for_search();
    const std::size_t bucket_count = table.bucket_count_for_tests();
    assert(bucket_count > 0);

    const std::size_t key_count = sirio::GlobalTranspositionTable::cluster_capacity() + 1;
    const auto keys = colliding_keys(bucket_count, key_count);
    assert(keys.size() == key_count);

    for (std::size_t i = 0; i < sirio::GlobalTranspositionTable::cluster_capacity(); ++i) {
        sirio::TTEntry entry;
        entry.best_move.from = static_cast<int>(i % 64);
        entry.best_move.to = static_cast<int>((i + 3) % 64);
        entry.best_move.piece = sirio::PieceType::Rook;
        entry.depth = static_cast<int>(20 + i);
        entry.score = static_cast<int>(600 + i);
        entry.type = sirio::TTNodeType::UpperBound;
        entry.static_eval = static_cast<int>(700 + i);
        table.store(keys[i], entry, generation);
    }

    sirio::TTEntry shallow;
    shallow.best_move.from = 7;
    shallow.best_move.to = 15;
    shallow.best_move.piece = sirio::PieceType::Queen;
    shallow.depth = 5;
    shallow.score = 800;
    shallow.type = sirio::TTNodeType::Exact;
    shallow.static_eval = 900;
    table.store(keys[sirio::GlobalTranspositionTable::cluster_capacity()], shallow, generation);

    const auto stored_shallow = table.probe(keys[sirio::GlobalTranspositionTable::cluster_capacity()]);
    assert(stored_shallow.has_value());
    assert(stored_shallow->depth == 5);

    const auto replaced = table.probe(keys[0]);
    assert(!replaced.has_value());

    for (std::size_t i = 1; i < sirio::GlobalTranspositionTable::cluster_capacity(); ++i) {
        const auto remaining = table.probe(keys[i]);
        assert(remaining.has_value());
        assert(remaining->depth == static_cast<int>(20 + i));
    }

    sirio::set_transposition_table_size(previous_size);
    sirio::clear_transposition_tables();
}

void test_collision_prefers_older_generations_for_eviction() {
    const std::size_t previous_size = sirio::get_transposition_table_size();
    sirio::set_transposition_table_size(1);
    sirio::clear_transposition_tables();

    sirio::GlobalTranspositionTable table;
    std::uint8_t initial_generation = table.prepare_for_search();
    const std::size_t bucket_count = table.bucket_count_for_tests();
    assert(bucket_count > 0);

    const std::size_t key_count = sirio::GlobalTranspositionTable::cluster_capacity() * 2;
    const auto keys = colliding_keys(bucket_count, key_count);
    assert(keys.size() == key_count);

    for (std::size_t i = 0; i < sirio::GlobalTranspositionTable::cluster_capacity(); ++i) {
        sirio::TTEntry entry;
        entry.best_move.from = static_cast<int>((i + 5) % 64);
        entry.best_move.to = static_cast<int>((i + 6) % 64);
        entry.best_move.piece = sirio::PieceType::Bishop;
        entry.depth = static_cast<int>(10 + i);
        entry.score = static_cast<int>(400 + i);
        entry.type = sirio::TTNodeType::LowerBound;
        entry.static_eval = static_cast<int>(500 + i);
        table.store(keys[i], entry, initial_generation);
    }

    std::uint8_t new_generation = table.prepare_for_search();

    for (std::size_t i = 0; i < sirio::GlobalTranspositionTable::cluster_capacity(); ++i) {
        sirio::TTEntry entry;
        entry.best_move.from = static_cast<int>((i + 11) % 64);
        entry.best_move.to = static_cast<int>((i + 12) % 64);
        entry.best_move.piece = sirio::PieceType::Knight;
        entry.depth = static_cast<int>(100 + i);
        entry.score = static_cast<int>(900 + i);
        entry.type = sirio::TTNodeType::Exact;
        entry.static_eval = static_cast<int>(1000 + i);
        table.store(keys[sirio::GlobalTranspositionTable::cluster_capacity() + i], entry, new_generation);
    }

    for (std::size_t i = 0; i < sirio::GlobalTranspositionTable::cluster_capacity(); ++i) {
        const auto old_entry = table.probe(keys[i]);
        assert(!old_entry.has_value());
    }

    for (std::size_t i = 0; i < sirio::GlobalTranspositionTable::cluster_capacity(); ++i) {
        const auto young_entry =
            table.probe(keys[sirio::GlobalTranspositionTable::cluster_capacity() + i]);
        assert(young_entry.has_value());
        assert(young_entry->depth == static_cast<int>(100 + i));
        assert(young_entry->generation == new_generation);
    }

    sirio::set_transposition_table_size(previous_size);
    sirio::clear_transposition_tables();
}

void test_disabling_transposition_table() {
    const std::size_t previous_size = sirio::get_transposition_table_size();
    sirio::set_transposition_table_size(0);
    sirio::clear_transposition_tables();

    sirio::GlobalTranspositionTable table;
    std::uint8_t generation = table.prepare_for_search();
    (void)generation;
    assert(table.bucket_count_for_tests() == 0);

    sirio::TTEntry entry;
    entry.depth = 4;
    entry.score = 42;
    entry.type = sirio::TTNodeType::Exact;
    table.store(1234ULL, entry, generation);
    assert(!table.probe(1234ULL).has_value());

    sirio::set_transposition_table_size(previous_size);
    sirio::clear_transposition_tables();
}

}  // namespace

void run_tt_tests() {
    test_collision_eviction();
    test_collision_exact_replacement();
    test_collision_replaces_shallow_entries();
    test_collision_prefers_older_generations_for_eviction();
    test_disabling_transposition_table();
}

