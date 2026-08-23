// The slot manager maps the transient ids (session ids, tracker ids, names) a
// protocol reports onto a fixed set of tree slots, so that a score can refer to
// "the first touch" or "tracker 3" while ids come and go.

#include <Common/TrackingSlotManager.hpp>
#include <Common/TrackingTypes.hpp>

#include <catch2/catch_all.hpp>

using Tracking::SlotManager;
using Tracking::SlotStrategy;
using Tracking::TrackedPoint;

TEST_CASE("Slots are handed out in order and stay stable", "[tracking][slots]")
{
  SlotManager<TrackedPoint> slots{4};
  REQUIRE(slots.size() == 4);

  CHECK(slots.find_or_allocate(100) == 0);
  CHECK(slots.find_or_allocate(200) == 1);
  CHECK(slots.find_or_allocate(100) == 0);
  CHECK(slots.find_by_id(200) == 1);
  CHECK(slots.find_by_id(300) == -1);
}

TEST_CASE("Alive lists free the slots of vanished ids", "[tracking][slots]")
{
  // A TUIO bundle: alive 1 2 3, then alive 1 3 -> id 2 is gone
  SlotManager<TrackedPoint> slots{4};
  slots.find_or_allocate(1);
  slots.find_or_allocate(2);
  slots.find_or_allocate(3);

  slots.mark_all_inactive();
  slots.mark_active_by_id(1);
  slots.mark_active_by_id(3);
  auto freed = slots.free_inactive_slots();
  REQUIRE(freed == std::vector<int>{1});
  CHECK(slots.find_by_id(2) == -1);
  CHECK(slots.find_by_id(1) == 0);
  CHECK(slots.find_by_id(3) == 2);

  // The freed slot is reused by the next newcomer: the tree stays compact
  CHECK(slots.find_or_allocate(4) == 1);
}

TEST_CASE("Name-based slots (RTTrP trackables)", "[tracking][slots]")
{
  SlotManager<TrackedPoint> slots{3, SlotStrategy::NameBased};
  CHECK(slots.find_or_allocate(std::string{"Dancer A"}) == 0);
  CHECK(slots.find_or_allocate(std::string{"Dancer B"}) == 1);
  CHECK(slots.find_or_allocate(std::string{"Dancer A"}) == 0);
  CHECK(slots.find_by_name("Dancer B") == 1);
  CHECK(slots.find_by_name("nobody") == -1);
}

TEST_CASE("Once full, an inactive slot is recycled before anything is clobbered", "[tracking][slots]")
{
  SlotManager<TrackedPoint> slots{2};
  slots.find_or_allocate(10);
  slots.find_or_allocate(20);
  // A new frame: only 10 is confirmed alive
  slots.mark_all_inactive();
  slots.mark_active(0);
  // Slot 1 not confirmed alive: it is the one to recycle
  CHECK(slots.find_or_allocate(30) == 1);
  CHECK(slots.find_by_id(20) == -1);
  CHECK(slots.find_by_id(10) == 0);
}

TEST_CASE("Overflow with everything active falls back to slot 0, and only warns once", "[tracking][slots]")
{
  SlotManager<TrackedPoint> slots{2};
  slots.find_or_allocate(1);
  slots.find_or_allocate(2);
  slots.mark_active(0);
  slots.mark_active(1);
  const int s = slots.find_or_allocate(3);
  CHECK(s >= 0);
  CHECK(s < 2);
  // Still a valid slot the second time around
  const int s2 = slots.find_or_allocate(4);
  CHECK(s2 >= 0);
  CHECK(s2 < 2);
}

TEST_CASE("Entities ride along with their slot", "[tracking][slots]")
{
  SlotManager<TrackedPoint> slots{2};
  const int s = slots.find_or_allocate(7);
  slots.entity(s).position.x = 0.25f;
  CHECK(slots.entity(slots.find_by_id(7)).position.x == 0.25f);
  slots.free_slot(7);
  CHECK(slots.find_by_id(7) == -1);
}
