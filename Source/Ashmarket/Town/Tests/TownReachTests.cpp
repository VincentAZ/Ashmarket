// Ashmarket: the town outside the cage.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/TownTestHelpers.h"
#include "Town/TownTypes.h"

/**
 *  Ashmarket.Town.Reach   in every place, everybody who can be talked to, every door, everything lying about and every
 *                         way in can be WALKED to from where the player comes in.
 *
 *  The other tests stand the player where they need him. So a thing can stand across the whole of a road and no test
 *  walks into it: on 25 November the gantry over the Gate Yard, sixteen metres of it taken as one solid box, shut the
 *  yard off from the street, and the gate shift could not be stood. This test walks. It lays a grid over the place's
 *  ground, takes as open every square a walker's middle can be in (on a patch of ground, and a walker's half-width clear
 *  of everything that stands on it: the game's own rule, ATownGameMode::GetKeptClear), spreads out from the first way in
 *  over open squares that touch, and asks that somewhere it got to is within reach of each thing.
 *
 *  Being able to get somewhere is not enough: the yard could be got into round the gantry's ends, past the barrels,
 *  where nobody would look for a way. So each place also names the ways anybody would take, as straight lines on the
 *  map, and every step of each has to be open.
 *
 *  It reads the places as they are written down and opens no level, so it is quick. It does not know about people who
 *  stand in the way, or about loose things, which are walked into and go.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownReachTest, "Ashmarket.Town.Reach", GutTests::Flags)

namespace ReachTests
{
	/** A square of the grid is this much across (cm) */
	inline constexpr float Cell = 10.0f;

	/** A walker's half-width: ATownGameMode::WalkerRadius */
	inline constexpr float Walker = 34.0f;

	/** How near is near enough to talk to somebody or use something: a little inside ATownGameMode::Reach, which is 230 */
	inline constexpr float Near = 200.0f;

	/** Where something is on the map, in its own metres, as the lists have it (x east, y north) */
	static FString OnMap(const FVector2D& InLevel)
	{
		return FString::Printf(TEXT("%.1f, %.1f"), (InLevel.Y / 100.0f) + 95.0f, (InLevel.X / 100.0f) + 208.0f);
	}
}

bool FTownReachTest::RunTest(const FString& Parameters)
{
	using namespace ReachTests;

	static const TCHAR* Places[] =
	{
		TEXT("/Game/Town/DA_Town_PoolStreet"),
		TEXT("/Game/Town/DA_Town_BlockSquare"),
		TEXT("/Game/Town/DA_Town_MainStreet"),
		TEXT("/Game/Town/DA_Town_RotgutRow"),
		TEXT("/Game/Town/DA_Town_VelvetTin"),
		TEXT("/Game/Town/DA_Town_ScrappersYard"),
	};

	// the ways anybody would take, in the map's own metres (x east, y north): each a line from one place to the next
	struct FWay
	{
		const TCHAR* Place;
		const TCHAR* Name;
		TArray<FVector2D> Along;
	};

	static const FWay Ways[] =
	{
		{ TEXT("Main Street"), TEXT("down the road from the square, under the gantry, to the head of the foot lane"),
			{ FVector2D(0.5f, 126.0f), FVector2D(0.5f, 20.0f), FVector2D(1.75f, 7.6f) } },
		{ TEXT("Main Street"), TEXT("from the head of the foot lane to the tally desk"), { FVector2D(1.75f, 7.6f), FVector2D(3.6f, 9.3f) } },
		{ TEXT("Main Street"), TEXT("from the tally desk to the Warden on the wagon lane"), { FVector2D(3.6f, 9.3f), FVector2D(-1.9f, 5.4f) } },
		{ TEXT("Rotgut Row"), TEXT("down the cut and up the furrow to the bus"), { FVector2D(-39.0f, 294.0f), FVector2D(-45.0f, 294.0f), FVector2D(-45.0f, 335.5f) } },
		{ TEXT("The Velvet Tin"), TEXT("in at the curtain and across the court to the middle trailer"), { FVector2D(-86.0f, 188.9f), FVector2D(-86.0f, 192.0f), FVector2D(-96.0f, 204.6f) } },
		{ TEXT("The Scrappers' Yard"), TEXT("in at the gate and up the yard to the weighbridge"), { FVector2D(113.2f, 176.2f), FVector2D(118.0f, 180.0f), FVector2D(124.6f, 195.2f) } },
		{ TEXT("Block Square"), TEXT("from Pool Street's lane across the square to the mouth of Tin Lane"), { FVector2D(5.2f, 165.4f), FVector2D(0.0f, 154.6f), FVector2D(-7.2f, 154.6f) } },
	};

	for (const TCHAR* Path : Places)
	{
		const UTownPlaceData* Place = LoadObject<UTownPlaceData>(nullptr, Path);

		if (!Place || Place->Ground.IsEmpty() || Place->Arrivals.IsEmpty())
		{
			AddError(FString::Printf(TEXT("%s is not there, or has no ground or no way in"), Path));
			continue;
		}

		FBox2D All(ForceInit);

		for (const FBox2D& Patch : Place->Ground)
		{
			All += Patch;
		}

		const int32 Across = FMath::CeilToInt32((All.Max.X - All.Min.X) / Cell) + 1;
		const int32 Down = FMath::CeilToInt32((All.Max.Y - All.Min.Y) / Cell) + 1;

		const auto At = [&All](int32 X, int32 Y) { return FVector2D(All.Min.X + (X * Cell), All.Min.Y + (Y * Cell)); };

		// 0 shut, 1 open, 2 got to
		TArray<uint8> Squares;
		Squares.SetNumZeroed(Across * Down);

		for (int32 Y = 0; Y < Down; ++Y)
		{
			for (int32 X = 0; X < Across; ++X)
			{
				const FVector2D Here = At(X, Y);

				bool bOpen = false;

				for (const FBox2D& Patch : Place->Ground)
				{
					if (Patch.IsInside(Here) || Patch.GetClosestPointTo(Here).Equals(Here, 0.01f))
					{
						bOpen = true;
						break;
					}
				}

				for (int32 i = 0; bOpen && i < Place->Blocks.Num(); ++i)
				{
					bOpen = Place->Blocks[i].GetDistance(Here) >= Walker - 0.5f;
				}

				Squares[(Y * Across) + X] = bOpen ? 1 : 0;
			}
		}

		// the open square nearest a place, no further off than Within; and whether any square got to is that near
		const auto Nearest = [&](const FVector2D& Point, float Within, uint8 Kind, FIntPoint& Out)
		{
			const int32 Span = FMath::CeilToInt32(Within / Cell);
			const int32 MidX = FMath::RoundToInt32((Point.X - All.Min.X) / Cell);
			const int32 MidY = FMath::RoundToInt32((Point.Y - All.Min.Y) / Cell);

			float Best = FMath::Square(Within);
			bool bFound = false;

			for (int32 Y = FMath::Max(0, MidY - Span); Y <= FMath::Min(Down - 1, MidY + Span); ++Y)
			{
				for (int32 X = FMath::Max(0, MidX - Span); X <= FMath::Min(Across - 1, MidX + Span); ++X)
				{
					if (Squares[(Y * Across) + X] < Kind)
					{
						continue;
					}

					const float Distance = FVector2D::DistSquared(At(X, Y), Point);

					if (Distance <= Best)
					{
						Best = Distance;
						Out = FIntPoint(X, Y);
						bFound = true;
					}
				}
			}

			return bFound;
		};

		// spread out from the first way in
		const FVector2D Start(Place->Arrivals[0].Place.X, Place->Arrivals[0].Place.Y);
		FIntPoint First;

		if (!Nearest(Start, 80.0f, 1, First))
		{
			AddError(FString::Printf(TEXT("%s: the way in \"%s\" (%s) is not on open ground"), *Place->Name, *Place->Arrivals[0].Name.ToString(), *OnMap(Start)));
			continue;
		}

		TArray<FIntPoint> Edge;
		Edge.Add(First);
		Squares[(First.Y * Across) + First.X] = 2;

		int32 Got = 0;

		while (!Edge.IsEmpty())
		{
			const FIntPoint Here = Edge.Pop(EAllowShrinking::No);

			++Got;

			static const FIntPoint Steps[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

			for (const FIntPoint& Step : Steps)
			{
				const FIntPoint Next = Here + Step;

				if (Next.X >= 0 && Next.Y >= 0 && Next.X < Across && Next.Y < Down && Squares[(Next.Y * Across) + Next.X] == 1)
				{
					Squares[(Next.Y * Across) + Next.X] = 2;
					Edge.Add(Next);
				}
			}
		}

		int32 Asked = 0;
		int32 Missed = 0;
		FIntPoint Unused;

		const auto Ask = [&](const FString& What, const FVector2D& Point, float Within)
		{
			++Asked;

			if (!Nearest(Point, Within, 2, Unused))
			{
				++Missed;

				AddError(FString::Printf(TEXT("%s: %s, at %s on the map, cannot be walked to from the way in \"%s\""), *Place->Name, *What, *OnMap(Point),
					*Place->Arrivals[0].Name.ToString()));
			}
		};

		for (const FTownArrival& Arrival : Place->Arrivals)
		{
			Ask(FString::Printf(TEXT("the way in \"%s\""), *Arrival.Name.ToString()), FVector2D(Arrival.Place.X, Arrival.Place.Y), 80.0f);
		}

		for (const FTownPerson& Person : Place->People)
		{
			// somebody with nothing to say is only seen
			if (!Person.Lines.IsEmpty())
			{
				Ask(Person.Name, FVector2D(Person.Place.X, Person.Place.Y), Near);
			}
		}

		for (const FTownDoor& Door : Place->Doors)
		{
			if (Door.bWalkedThrough && Door.Mouth.bIsValid)
			{
				// walked through: somewhere in its mouth has to be got to
				bool bThrough = false;

				for (int32 Y = 0; Y < Down && !bThrough; ++Y)
				{
					for (int32 X = 0; X < Across && !bThrough; ++X)
					{
						bThrough = Squares[(Y * Across) + X] == 2 && Door.Mouth.IsInside(At(X, Y));
					}
				}

				++Asked;

				if (!bThrough)
				{
					++Missed;

					AddError(FString::Printf(TEXT("%s: the way out \"%s\", at %s on the map, cannot be walked into"), *Place->Name, *Door.Prompt, *OnMap(FVector2D(Door.Place.X, Door.Place.Y))));
				}
			}
			else
			{
				Ask(FString::Printf(TEXT("the door \"%s\""), *Door.Prompt), FVector2D(Door.Place.X, Door.Place.Y), Near);
			}
		}

		for (const FTownFind& Find : Place->Finds)
		{
			Ask(TEXT("something lying about to be found"), FVector2D(Find.Place.X, Find.Place.Y), Near);
		}

		for (const FWay& Way : Ways)
		{
			if (Place->Name != Way.Place)
			{
				continue;
			}

			++Asked;

			bool bOpen = true;

			for (int32 i = 0; bOpen && i + 1 < Way.Along.Num(); ++i)
			{
				// (the map's x is the level's Y, and its y the level's X)
				const FVector2D From((Way.Along[i].Y - 208.0f) * 100.0f, (Way.Along[i].X - 95.0f) * 100.0f);
				const FVector2D To((Way.Along[i + 1].Y - 208.0f) * 100.0f, (Way.Along[i + 1].X - 95.0f) * 100.0f);
				const int32 Paces = FMath::Max(1, FMath::CeilToInt32(FVector2D::Distance(From, To) / Cell));

				for (int32 Pace = 0; bOpen && Pace <= Paces; ++Pace)
				{
					const FVector2D Here = FMath::Lerp(From, To, static_cast<float>(Pace) / Paces);

					// (somebody standing about is stepped round: within a long pace of the line will do. A thing across the
					// whole road is deeper than that)
					if (!Nearest(Here, 120.0f, 2, Unused))
					{
						bOpen = false;
						++Missed;

						// what stands nearest, so that it can be found in the place's list
						FString By = TEXT("it is off the ground that can be walked on");
						float Least = Walker;

						for (const FTownBlock& Block : Place->Blocks)
						{
							if (const float Distance = Block.GetDistance(Here); Distance < Least)
							{
								Least = Distance;
								By = FString::Printf(TEXT("by something %.1f by %.1f m with its middle at %s"), Block.HalfSize.X / 50.0f, Block.HalfSize.Y / 50.0f, *OnMap(Block.Middle));
							}
						}

						AddError(FString::Printf(TEXT("%s: the way %s is shut at %s on the map, %s"), *Place->Name, Way.Name, *OnMap(Here), *By));
					}
				}
			}
		}

		AddInfo(FString::Printf(TEXT("%s: %d of %d things can be walked to, over %.0f square metres of open ground"), *Place->Name, Asked - Missed, Asked,
			Got * Cell * Cell / 10000.0f));
	}

	return true;
}

#endif
