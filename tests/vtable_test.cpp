#include <gtest/gtest.h>

#include <fw/detail/vtable.hpp>

#include "test_types.hpp"

namespace
{

using SmallStorage = fw::detail::wrapper_storage<int(int, int)>;
using SmallVTable = fw::detail::vtable_instance<int (*)(int, int), int(int, int)>;
using HeapVTable = fw::detail::vtable_instance<fw::test_support::LargeAdder, int(int, int)>;
using MoveOnlyVTable = fw::detail::vtable_instance<fw::test_support::MoveOnlyAdder, int(int, int)>;
using VoidStorage = fw::detail::wrapper_storage<void()>;
using VoidVTable = fw::detail::vtable_instance<fw::test_support::InvocationCounter, void()>;

} // namespace

TEST(VTable, GivenCallableTypesWhenInspectedThenSupportedSignaturesAreReported)
{
    EXPECT_TRUE((fw::detail::supports_signature<int (*)(int, int), int(int, int)>::value));
    EXPECT_FALSE((fw::detail::supports_signature<fw::test_support::LvalueOnlyFunction, int(int)>::value));
    EXPECT_TRUE((fw::detail::supports_any_signature_v<fw::test_support::LvalueOnlyFunction, int(), void()>));
}

TEST(VTable, GivenSupportedCallablesWhenEntriesAreBuiltThenExpectedSlotsExist)
{
    auto* small_make = &fw::detail::signature_entry_factory<int (*)(int, int), int(int, int)>::make;
    auto* heap_make = &fw::detail::signature_entry_factory<fw::test_support::LargeAdder, int(int, int)>::make;
    auto* move_only_make = &fw::detail::signature_entry_factory<fw::test_support::MoveOnlyAdder, int(int, int)>::make;
    auto* void_make = &fw::detail::signature_entry_factory<fw::test_support::InvocationCounter, void()>::make;

    const auto small_entry = small_make();
    const auto heap_entry = heap_make();
    const auto move_only_entry = move_only_make();
    const auto void_entry = void_make();

    EXPECT_NE(small_entry.lcall, nullptr);
    EXPECT_NE(small_entry.clcall, nullptr);
    EXPECT_NE(small_entry.rcall, nullptr);
    EXPECT_NE(heap_entry.lcall, nullptr);
    EXPECT_NE(heap_entry.clcall, nullptr);
    EXPECT_NE(heap_entry.rcall, nullptr);
    EXPECT_NE(move_only_entry.lcall, nullptr);
    EXPECT_NE(move_only_entry.clcall, nullptr);
    EXPECT_NE(move_only_entry.rcall, nullptr);
    EXPECT_NE(void_entry.lcall, nullptr);
    EXPECT_NE(void_entry.clcall, nullptr);
    EXPECT_NE(void_entry.rcall, nullptr);
}

TEST(VTable, GivenMoveOnlyStoredTypeWhenVTableIsBuiltThenCopyThunkIsNullAndMoveStillWorks)
{
    SmallStorage source{};
    const auto* table = MoveOnlyVTable::get();
    const auto& entry = static_cast<const fw::detail::signature_vtable_entry<int(int, int)>&>(*table);

    EXPECT_EQ(table->copy, nullptr);

    fw::detail::fw_construct<fw::test_support::MoveOnlyAdder>(source.payload.sbo, fw::test_support::MoveOnlyAdder{ 4 });
    source.kind = fw::detail::storage_kind::Small;
    source.vt = table;

    SmallStorage moved{};
    MoveOnlyVTable::move(moved, source);

    EXPECT_FALSE(source.vt);
    EXPECT_EQ(source.kind, fw::detail::storage_kind::Empty);
    EXPECT_EQ(entry.lcall(MoveOnlyVTable::get_ptr(moved), 2, 3), 9);

    MoveOnlyVTable::destroy(moved);
    EXPECT_EQ(moved.kind, fw::detail::storage_kind::Empty);
    EXPECT_EQ(moved.vt, nullptr);
}

TEST(VTable, GivenSmallStorageWhenCopiedMovedAndDestroyedThenLifecycleRemainsValid)
{
    SmallStorage empty_source{};
    SmallStorage empty_copy{};
    SmallStorage empty_move{};

    SmallVTable::copy(empty_copy, empty_source);
    SmallVTable::move(empty_move, empty_source);
    SmallVTable::destroy(empty_source);
    EXPECT_EQ(empty_copy.kind, fw::detail::storage_kind::Empty);
    EXPECT_EQ(empty_copy.vt, nullptr);
    EXPECT_EQ(empty_move.kind, fw::detail::storage_kind::Empty);
    EXPECT_EQ(empty_move.vt, nullptr);
    EXPECT_EQ(empty_source.kind, fw::detail::storage_kind::Empty);
    EXPECT_EQ(empty_source.vt, nullptr);
    EXPECT_EQ(SmallVTable::get_ptr(empty_copy), nullptr);
    EXPECT_EQ(SmallVTable::get_cptr(empty_copy), nullptr);
    EXPECT_EQ(SmallVTable::get_ptr(empty_move), nullptr);
    EXPECT_EQ(SmallVTable::get_cptr(empty_move), nullptr);

    SmallStorage source{};
    const auto* table = SmallVTable::get();
    const auto& entry = static_cast<const fw::detail::signature_vtable_entry<int(int, int)>&>(*table);

    fw::detail::fw_construct<int (*)(int, int)>(source.payload.sbo, &fw::test_support::add);
    source.kind = fw::detail::storage_kind::Small;
    source.vt = table;

    EXPECT_NE(SmallVTable::get_ptr(source), nullptr);
    EXPECT_NE(SmallVTable::get_cptr(source), nullptr);
    EXPECT_EQ(entry.lcall(SmallVTable::get_ptr(source), 1, 2), 3);
    EXPECT_EQ(entry.clcall(SmallVTable::get_cptr(source), 2, 3), 5);
    EXPECT_EQ(entry.rcall(SmallVTable::get_ptr(source), 3, 4), 7);

    SmallStorage copied{};
    SmallVTable::copy(copied, source);
    EXPECT_EQ(entry.clcall(SmallVTable::get_cptr(copied), 4, 5), 9);

    SmallStorage moved{};
    SmallVTable::move(moved, copied);
    EXPECT_EQ(entry.lcall(SmallVTable::get_ptr(moved), 5, 6), 11);
    EXPECT_EQ(copied.kind, fw::detail::storage_kind::Empty);
    EXPECT_EQ(copied.vt, nullptr);
    EXPECT_EQ(SmallVTable::get_ptr(copied), nullptr);
    EXPECT_EQ(SmallVTable::get_cptr(copied), nullptr);

    SmallVTable::destroy(source);
    SmallVTable::destroy(moved);

    EXPECT_EQ(source.kind, fw::detail::storage_kind::Empty);
    EXPECT_EQ(source.vt, nullptr);
    EXPECT_EQ(SmallVTable::get_ptr(source), nullptr);
    EXPECT_EQ(SmallVTable::get_cptr(source), nullptr);
    EXPECT_EQ(moved.kind, fw::detail::storage_kind::Empty);
    EXPECT_EQ(moved.vt, nullptr);
    EXPECT_EQ(SmallVTable::get_ptr(moved), nullptr);
    EXPECT_EQ(SmallVTable::get_cptr(moved), nullptr);
}

TEST(VTable, GivenHeapStorageWhenCopiedMovedAndDestroyedThenLifecycleRemainsValid)
{
    SmallStorage source{};
    const auto* table = HeapVTable::get();
    const auto& entry = static_cast<const fw::detail::signature_vtable_entry<int(int, int)>&>(*table);

    source.payload.heap = new fw::test_support::LargeAdder{};
    source.kind = fw::detail::storage_kind::Heap;
    source.vt = table;

    EXPECT_NE(HeapVTable::get_ptr(source), nullptr);
    EXPECT_NE(HeapVTable::get_cptr(source), nullptr);
    EXPECT_EQ(entry.lcall(HeapVTable::get_ptr(source), 1, 2), 3);
    EXPECT_EQ(entry.clcall(HeapVTable::get_cptr(source), 2, 3), 5);
    EXPECT_EQ(entry.rcall(HeapVTable::get_ptr(source), 3, 4), 7);

    SmallStorage copied{};
    HeapVTable::copy(copied, source);
    EXPECT_EQ(entry.clcall(HeapVTable::get_cptr(copied), 4, 5), 9);

    SmallStorage moved{};
    HeapVTable::move(moved, copied);
    EXPECT_EQ(entry.lcall(HeapVTable::get_ptr(moved), 5, 6), 11);
    EXPECT_EQ(copied.kind, fw::detail::storage_kind::Empty);
    EXPECT_EQ(copied.vt, nullptr);
    EXPECT_EQ(copied.payload.heap, nullptr);

    HeapVTable::destroy(source);
    HeapVTable::destroy(moved);

    EXPECT_EQ(source.kind, fw::detail::storage_kind::Empty);
    EXPECT_EQ(source.vt, nullptr);
    EXPECT_EQ(source.payload.heap, nullptr);
    EXPECT_EQ(HeapVTable::get_ptr(source), nullptr);
    EXPECT_EQ(HeapVTable::get_cptr(source), nullptr);
    EXPECT_EQ(moved.kind, fw::detail::storage_kind::Empty);
    EXPECT_EQ(moved.vt, nullptr);
    EXPECT_EQ(moved.payload.heap, nullptr);
    EXPECT_EQ(HeapVTable::get_ptr(moved), nullptr);
    EXPECT_EQ(HeapVTable::get_cptr(moved), nullptr);
}

TEST(VTable, GivenVoidCallableWhenInvokedThenAllSlotsExecuteTheStoredObject)
{
    int calls = 0;
    VoidStorage storage{};
    const auto* table = VoidVTable::get();
    const auto& entry = static_cast<const fw::detail::signature_vtable_entry<void()>&>(*table);

    fw::detail::fw_construct<fw::test_support::InvocationCounter>(storage.payload.sbo, fw::test_support::InvocationCounter{ &calls });
    storage.kind = fw::detail::storage_kind::Small;
    storage.vt = table;

    entry.lcall(VoidVTable::get_ptr(storage));
    entry.clcall(VoidVTable::get_cptr(storage));
    entry.rcall(VoidVTable::get_ptr(storage));

    EXPECT_EQ(calls, 3);

    VoidVTable::destroy(storage);
    EXPECT_EQ(storage.kind, fw::detail::storage_kind::Empty);
    EXPECT_EQ(storage.vt, nullptr);
}
