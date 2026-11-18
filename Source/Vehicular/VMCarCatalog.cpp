#include "VMCarCatalog.h"
#include "VMVehiclePawn.h"

UVMCarCatalog::UVMCarCatalog()
{
	auto Add = [this](FName Id, const TCHAR* Name, const TCHAR* Desc, int32 Price, int32 Wins, float Mass, float Torque, float Grip, float Steer,
		float Health, float Ram, float Nitro, TArray<EVMBodyKit> Kits, FLinearColor Color)
	{
		FVMCarDefinition& Car = Cars.AddDefaulted_GetRef();
		Car.Id = Id;
		Car.DisplayName = Name;
		Car.Description = Desc;
		Car.Price = Price;
		Car.RequiredWins = Wins;
		Car.MassKg = Mass;
		Car.TorqueMultiplier = Torque;
		Car.GripMultiplier = Grip;
		Car.SteerMultiplier = Steer;
		Car.HealthMultiplier = Health;
		Car.RamMultiplier = Ram;
		Car.NitroMultiplier = Nitro;
		Car.BodyKits = MoveTemp(Kits);
		Car.KitColor = Color;
	};

	Add(TEXT("Brawler"), TEXT("STREET BRAWLER"), TEXT("A balanced all-rounder. Nothing special, nothing broken."),
		0, 0, 1500.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, { EVMBodyKit::BullBar }, FLinearColor(0.15f, 0.15f, 0.15f));

	Add(TEXT("Wasp"), TEXT("WASP"), TEXT("Light and twitchy. Fast, grippy, huge nitro - and made of paper."),
		3000, 1, 1100.f, 1.1f, 1.15f, 1.1f, 0.75f, 0.8f, 1.4f, { EVMBodyKit::Spoiler, EVMBodyKit::SideExhausts }, FLinearColor(1.f, 0.8f, 0.f));

	Add(TEXT("Mule"), TEXT("THE MULE"), TEXT("A heavy work truck. Slow to turn, shrugs off hits, hits back hard."),
		4000, 1, 2400.f, 1.25f, 0.95f, 0.85f, 1.5f, 1.3f, 1.f, { EVMBodyKit::BullBar, EVMBodyKit::RoofRack }, FLinearColor(0.35f, 0.25f, 0.1f));

	Add(TEXT("Juggernaut"), TEXT("JUGGERNAUT"), TEXT("Plated, spiked and enormous. A wrecking ball with a steering wheel."),
		8000, 2, 3000.f, 1.4f, 0.9f, 0.8f, 2.f, 1.6f, 0.9f, { EVMBodyKit::SpikeRam, EVMBodyKit::ArmourPlates }, FLinearColor(0.3f, 0.32f, 0.35f));

	Add(TEXT("HornetX"), TEXT("HORNET X"), TEXT("Top-tier race build. Blistering pace with a reinforced cage."),
		12000, 4, 1300.f, 1.35f, 1.25f, 1.15f, 1.1f, 1.f, 1.6f, { EVMBodyKit::Spoiler, EVMBodyKit::SpikeRam, EVMBodyKit::SideExhausts }, FLinearColor(0.9f, 0.05f, 0.05f));
}

const TArray<FVMCarDefinition>& UVMCarCatalog::GetCars()
{
	return GetDefault<UVMCarCatalog>()->Cars;
}

const FVMCarDefinition* UVMCarCatalog::Find(FName Id)
{
	return GetCars().FindByPredicate([Id](const FVMCarDefinition& Car) { return Car.Id == Id; });
}

const FVMCarDefinition& UVMCarCatalog::GetStarter()
{
	static const FVMCarDefinition Fallback;
	const TArray<FVMCarDefinition>& Cars = GetCars();
	return Cars.Num() > 0 ? Cars[0] : Fallback;
}
