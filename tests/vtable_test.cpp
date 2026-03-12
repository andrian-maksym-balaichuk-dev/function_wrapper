#include <gtest/gtest.h>

#include <fw/detail/vtable.hpp>

#include "test_types.hpp"

namespace
{
using SmallStorage = fw::detail::wrapper_storage<fw::policy::default_policy, int(int, int)>;
using SmallVTable = fw::detail::vtable_instance<int (*)(int, int), fw::policy::default_policy, int(int, int)>;
using HeapVTable = fw::detail::vtable_instance<fw::test_support::LargeAdder, fw::policy::default_policy, int(int, int)>;
using MoveOnlyVTable = fw::detail::vtable_instance<fw::test_support::MoveOnlyAdder, fw::policy::default_policy, int(int, int)>;
using TrivialUnaryVTable = fw::detail::vtable_instance<fw::test_support::TrivialIncrement, fw::policy::default_policy, int(int)>;
using NonTrivialUnaryVTable = fw::detail::vtable_instance<fw::test_support::SmallNonTrivialIncrement, fw::policy::default_policy, int(int)>;
using VoidStorage = fw::detail::wrapper_storage<fw::policy::default_policy, void()>;
using VoidVTable = fw::detail::vtable_instance<fw::test_support::InvocationCounter, fw::policy::default_policy, void()>;
using NoexceptStorage = fw::detail::wrapper_storage<fw::policy::default_policy, int(int, int) noexcept>;
using NoexceptVTable = fw::detail::vtable_instance<int (*)(int, int) noexcept, fw::policy::default_policy, int(int, int) noexcept>;

} // namespace

TEST(VTable, GivenCallableTypesWhenInspectedThenSupportedSignaturesAreReported)
{
    EXPECT_TRUE((fw::detail::supports_signature<int (*)(int, int), int(int, int)>::value));
    EXPECT_TRUE((fw::detail::supports_signature<int (*)(int, int) noexcept, int(int, int) noexcept>::value));
    EXPECT_FALSE((fw::detail::supports_signature<int (*)(int, int), int(int, int) noexcept>::value));
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

TEST(VTable, GivenNoexceptSignatureWhenEntryIsBuiltThenSlotsUseNoexceptThunks)
{
    auto* noexcept_make = &fw::detail::signature_entry_factory<int (*)(int, int) noexcept, int(int, int) noexcept>::make;
    const auto noexcept_entry = noexcept_make();

    static_assert(std::is_nothrow_invocable_r_v<int, decltype(noexcept_entry.lcall), void*, int, int>);
    static_assert(std::is_nothrow_invocable_r_v<int, decltype(noexcept_entry.clcall), const void*, int, int>);
    static_assert(std::is_nothrow_invocable_r_v<int, decltype(noexcept_entry.rcall), void*, int, int>);

    EXPECT_NE(noexcept_entry.lcall, nullptr);
    EXPECT_NE(noexcept_entry.clcall, nullptr);
    EXPECT_NE(noexcept_entry.rcall, nullptr);
}

TEST(VTable, GivenMemberAdaptersWhenEntriesAreBuiltThenDirectSlotsRemainCallableAcrossMemberKinds)
{
    fw::test_support::MemberAdapterTarget target{ .factor = 4, .offset = 6 };
    const fw::test_support::MemberAdapterTarget const_target{ .factor = 5, .offset = 7 };

    auto mutable_adapter = fw::member_ref(target, &fw::test_support::MemberAdapterTarget::scale);
    auto const_adapter = fw::member_ref(const_target, &fw::test_support::MemberAdapterTarget::scale_const);
    auto object_adapter = fw::member_ref(target, &fw::test_support::MemberAdapterTarget::offset);
    auto noexcept_adapter = fw::member_ref(target, &fw::test_support::MemberAdapterTarget::scale_noexcept);

    const auto mutable_entry = fw::detail::signature_entry_factory<decltype(mutable_adapter), int(int)>::make();
    const auto const_entry = fw::detail::signature_entry_factory<decltype(const_adapter), int(int)>::make();
    const auto object_entry = fw::detail::signature_entry_factory<decltype(object_adapter), int&()>::make();
    const auto noexcept_entry = fw::detail::signature_entry_factory<decltype(noexcept_adapter), int(int) noexcept>::make();

    EXPECT_EQ(mutable_entry.lcall(&mutable_adapter, 3), 12);
    EXPECT_EQ(mutable_entry.clcall(&mutable_adapter, 2), 8);
    EXPECT_EQ(mutable_entry.rcall(&mutable_adapter, 4), 16);
    EXPECT_EQ(const_entry.clcall(&const_adapter, 3), 10);
    EXPECT_EQ(object_entry.lcall(&object_adapter), 6);
    object_entry.rcall(&object_adapter) = 9;
    EXPECT_EQ(target.offset, 9);
    EXPECT_EQ(noexcept_entry.lcall(&noexcept_adapter, 2), 8);
}

TEST(VTable, GivenStoredTypesWhenLifecycleFlagsAreBuiltThenTrivialSmallMetadataMatchesTheirTraits)
{
    // Trivial flags are now encoded in the vt_tagged field of storage, not in the vtable struct.
    using UnaryStorage = fw::detail::wrapper_storage<fw::policy::default_policy, int(int)>;

    UnaryStorage trivial_storage{};
    fw::detail::emplace_small_storage<fw::test_support::TrivialIncrement>(
        trivial_storage, TrivialUnaryVTable::get(), fw::test_support::TrivialIncrement{ 1 });
    EXPECT_TRUE(trivial_storage.has_trivial_small_destroy());
    EXPECT_TRUE(trivial_storage.has_trivial_small_relocate());
    TrivialUnaryVTable::destroy(trivial_storage);

    UnaryStorage nontrivial_storage{};
    fw::detail::emplace_small_storage<fw::test_support::SmallNonTrivialIncrement>(
        nontrivial_storage, NonTrivialUnaryVTable::get(), fw::test_support::SmallNonTrivialIncrement{ 1 });
    EXPECT_TRUE(nontrivial_storage.has_trivial_small_destroy());
    EXPECT_FALSE(nontrivial_storage.has_trivial_small_relocate());
    NonTrivialUnaryVTable::destroy(nontrivial_storage);

    // Heap storage: no trivial flags (heap objects are never trivially stored in SBO)
    SmallStorage heap_storage{};
    fw::detail::emplace_heap_storage<fw::test_support::LargeAdder>(
        heap_storage, HeapVTable::get(), fw::test_support::LargeAdder{});
    EXPECT_FALSE(heap_storage.has_trivial_small_destroy());
    EXPECT_FALSE(heap_storage.has_trivial_small_relocate());
    HeapVTable::destroy(heap_storage);

    SmallStorage move_only_storage{};
    fw::detail::emplace_small_storage<fw::test_support::MoveOnlyAdder>(
        move_only_storage, MoveOnlyVTable::get(), fw::test_support::MoveOnlyAdder{ 4 });
    EXPECT_FALSE(move_only_storage.has_trivial_small_destroy());
    EXPECT_FALSE(move_only_storage.has_trivial_small_relocate());
    MoveOnlyVTable::destroy(move_only_storage);
}

TEST(VTable, GivenMoveOnlyStoredTypeWhenVTableIsBuiltThenCopyThunkIsNullAndMoveStillWorks)
{
    SmallStorage source{};
    const auto* table = MoveOnlyVTable::get();
    const auto& entry = static_cast<const fw::detail::signature_vtable_entry<int(int, int)>&>(*table);

    EXPECT_EQ(table->copy, nullptr);

    fw::detail::fw_construct<fw::test_support::MoveOnlyAdder>(source.payload.sbo, fw::test_support::MoveOnlyAdder{ 4 });
    source.vt_tagged = reinterpret_cast<uintptr_t>(table) | fw::detail::vtable_tag_is_small;
    source.obj = static_cast<void*>(source.payload.sbo);

    SmallStorage moved{};
    MoveOnlyVTable::move(moved, source);

    EXPECT_TRUE(source.is_empty());
    EXPECT_EQ(entry.lcall(fw::detail::storage_ptr(moved), 2, 3), 9);

    MoveOnlyVTable::destroy(moved);
    EXPECT_TRUE(moved.is_empty());
}

TEST(VTable, GivenStorageKindsWhenPointerHelpersAreUsedThenObjectAddressesMatchThePayload)
{
    SmallStorage empty{};
    EXPECT_EQ(fw::detail::storage_ptr(empty), nullptr);
    EXPECT_EQ(fw::detail::storage_ptr(static_cast<const SmallStorage&>(empty)), nullptr);

    SmallStorage small{};
    fw::detail::fw_construct<int (*)(int, int)>(small.payload.sbo, &fw::test_support::add);
    small.vt_tagged = fw::detail::vtable_tag_is_small; // just the flag; no real vtable needed for this check
    small.obj = static_cast<void*>(small.payload.sbo);
    EXPECT_EQ(fw::detail::storage_ptr(small), static_cast<void*>(small.payload.sbo));
    EXPECT_EQ(fw::detail::storage_ptr(static_cast<const SmallStorage&>(small)), static_cast<const void*>(small.payload.sbo));
    SmallVTable::destroy(small);

    SmallStorage heap{};
    heap.payload.heap = new fw::test_support::LargeAdder{};
    heap.vt_tagged = reinterpret_cast<uintptr_t>(HeapVTable::get()); // Heap: no TAG_IS_SMALL
    heap.obj = heap.payload.heap;
    EXPECT_EQ(fw::detail::storage_ptr(heap), heap.payload.heap);
    EXPECT_EQ(fw::detail::storage_ptr(static_cast<const SmallStorage&>(heap)), heap.payload.heap);
    HeapVTable::destroy(heap);
}

TEST(VTable, GivenSmallStorageWhenCopiedMovedAndDestroyedThenLifecycleRemainsValid)
{
    SmallStorage empty_source{};
    SmallStorage empty_copy{};
    SmallStorage empty_move{};

    SmallVTable::copy(empty_copy, empty_source);
    SmallVTable::move(empty_move, empty_source);
    SmallVTable::destroy(empty_source);
    EXPECT_TRUE(empty_copy.is_empty());
    EXPECT_TRUE(empty_move.is_empty());
    EXPECT_TRUE(empty_source.is_empty());
    EXPECT_EQ(fw::detail::storage_ptr(empty_copy), nullptr);
    EXPECT_EQ(fw::detail::storage_ptr(static_cast<const SmallStorage&>(empty_copy)), nullptr);
    EXPECT_EQ(fw::detail::storage_ptr(empty_move), nullptr);
    EXPECT_EQ(fw::detail::storage_ptr(static_cast<const SmallStorage&>(empty_move)), nullptr);

    SmallStorage source{};
    const auto* table = SmallVTable::get();
    const auto& entry = static_cast<const fw::detail::signature_vtable_entry<int(int, int)>&>(*table);

    fw::detail::fw_construct<int (*)(int, int)>(source.payload.sbo, &fw::test_support::add);
    source.vt_tagged = reinterpret_cast<uintptr_t>(table) | fw::detail::vtable_tag_is_small;
    source.obj = static_cast<void*>(source.payload.sbo);

    EXPECT_NE(fw::detail::storage_ptr(source), nullptr);
    EXPECT_NE(fw::detail::storage_ptr(static_cast<const SmallStorage&>(source)), nullptr);
    EXPECT_EQ(entry.lcall(fw::detail::storage_ptr(source), 1, 2), 3);
    EXPECT_EQ(entry.clcall(fw::detail::storage_ptr(static_cast<const SmallStorage&>(source)), 2, 3), 5);
    EXPECT_EQ(entry.rcall(fw::detail::storage_ptr(source), 3, 4), 7);

    SmallStorage copied{};
    SmallVTable::copy(copied, source);
    EXPECT_EQ(entry.clcall(fw::detail::storage_ptr(static_cast<const SmallStorage&>(copied)), 4, 5), 9);

    SmallStorage moved{};
    SmallVTable::move(moved, copied);
    EXPECT_EQ(entry.lcall(fw::detail::storage_ptr(moved), 5, 6), 11);
    EXPECT_TRUE(copied.is_empty());
    EXPECT_EQ(fw::detail::storage_ptr(copied), nullptr);
    EXPECT_EQ(fw::detail::storage_ptr(static_cast<const SmallStorage&>(copied)), nullptr);

    SmallVTable::destroy(source);
    SmallVTable::destroy(moved);

    EXPECT_TRUE(source.is_empty());
    EXPECT_EQ(fw::detail::storage_ptr(source), nullptr);
    EXPECT_EQ(fw::detail::storage_ptr(static_cast<const SmallStorage&>(source)), nullptr);
    EXPECT_TRUE(moved.is_empty());
    EXPECT_EQ(fw::detail::storage_ptr(moved), nullptr);
    EXPECT_EQ(fw::detail::storage_ptr(static_cast<const SmallStorage&>(moved)), nullptr);
}

TEST(VTable, GivenHeapStorageWhenCopiedMovedAndDestroyedThenLifecycleRemainsValid)
{
    SmallStorage source{};
    const auto* table = HeapVTable::get();
    const auto& entry = static_cast<const fw::detail::signature_vtable_entry<int(int, int)>&>(*table);

    source.payload.heap = new fw::test_support::LargeAdder{};
    source.vt_tagged = reinterpret_cast<uintptr_t>(table); // Heap: no TAG_IS_SMALL
    source.obj = source.payload.heap;

    EXPECT_NE(fw::detail::storage_ptr(source), nullptr);
    EXPECT_NE(fw::detail::storage_ptr(static_cast<const SmallStorage&>(source)), nullptr);
    EXPECT_EQ(entry.lcall(fw::detail::storage_ptr(source), 1, 2), 3);
    EXPECT_EQ(entry.clcall(fw::detail::storage_ptr(static_cast<const SmallStorage&>(source)), 2, 3), 5);
    EXPECT_EQ(entry.rcall(fw::detail::storage_ptr(source), 3, 4), 7);

    SmallStorage copied{};
    HeapVTable::copy(copied, source);
    EXPECT_EQ(entry.clcall(fw::detail::storage_ptr(static_cast<const SmallStorage&>(copied)), 4, 5), 9);

    SmallStorage moved{};
    HeapVTable::move(moved, copied);
    EXPECT_EQ(entry.lcall(fw::detail::storage_ptr(moved), 5, 6), 11);
    EXPECT_TRUE(copied.is_empty());
    EXPECT_EQ(copied.payload.heap, nullptr);

    HeapVTable::destroy(source);
    HeapVTable::destroy(moved);

    EXPECT_TRUE(source.is_empty());
    EXPECT_EQ(source.payload.heap, nullptr);
    EXPECT_EQ(fw::detail::storage_ptr(source), nullptr);
    EXPECT_EQ(fw::detail::storage_ptr(static_cast<const SmallStorage&>(source)), nullptr);
    EXPECT_TRUE(moved.is_empty());
    EXPECT_EQ(moved.payload.heap, nullptr);
    EXPECT_EQ(fw::detail::storage_ptr(moved), nullptr);
    EXPECT_EQ(fw::detail::storage_ptr(static_cast<const SmallStorage&>(moved)), nullptr);
}

TEST(VTable, GivenVoidCallableWhenInvokedThenAllSlotsExecuteTheStoredObject)
{
    int calls = 0;
    VoidStorage storage{};
    const auto* table = VoidVTable::get();
    const auto& entry = static_cast<const fw::detail::signature_vtable_entry<void()>&>(*table);

    fw::detail::fw_construct<fw::test_support::InvocationCounter>(storage.payload.sbo, fw::test_support::InvocationCounter{ &calls });
    storage.vt_tagged = reinterpret_cast<uintptr_t>(table) | fw::detail::vtable_tag_is_small;
    storage.obj = static_cast<void*>(storage.payload.sbo);

    entry.lcall(fw::detail::storage_ptr(storage));
    entry.clcall(fw::detail::storage_ptr(static_cast<const VoidStorage&>(storage)));
    entry.rcall(fw::detail::storage_ptr(storage));

    EXPECT_EQ(calls, 3);

    VoidVTable::destroy(storage);
    EXPECT_TRUE(storage.is_empty());
}

TEST(VTable, GivenNoexceptStorageWhenInvokedThenAllSlotsRemainCallable)
{
    NoexceptStorage storage{};
    const auto* table = NoexceptVTable::get();
    const auto& entry = static_cast<const fw::detail::signature_vtable_entry<int(int, int) noexcept>&>(*table);

    fw::detail::fw_construct<int (*)(int, int) noexcept>(storage.payload.sbo, &fw::test_support::add_noexcept);
    storage.vt_tagged = reinterpret_cast<uintptr_t>(table) | fw::detail::vtable_tag_is_small;
    storage.obj = static_cast<void*>(storage.payload.sbo);

    EXPECT_EQ(entry.lcall(fw::detail::storage_ptr(storage), 1, 2), 3);
    EXPECT_EQ(entry.clcall(fw::detail::storage_ptr(static_cast<const NoexceptStorage&>(storage)), 2, 3), 5);
    EXPECT_EQ(entry.rcall(fw::detail::storage_ptr(storage), 3, 4), 7);

    NoexceptVTable::destroy(storage);
    EXPECT_TRUE(storage.is_empty());
}
