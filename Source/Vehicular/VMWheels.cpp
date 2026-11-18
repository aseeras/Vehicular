#include "VMWheels.h"

UVMWheelFront::UVMWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
	bAffectedByHandbrake = false;
	bAffectedByEngine = true;
	WheelRadius = 39.f;
	WheelWidth = 35.f;
	FrictionForceMultiplier = 3.f;
	CorneringStiffness = 1000.f;
	SuspensionMaxRaise = 10.f;
	SuspensionMaxDrop = 10.f;
	MaxBrakeTorque = 4500.f;
}

UVMWheelRear::UVMWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedBySteering = false;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
	WheelRadius = 39.f;
	WheelWidth = 35.f;
	FrictionForceMultiplier = 3.f;
	CorneringStiffness = 1000.f;
	SuspensionMaxRaise = 10.f;
	SuspensionMaxDrop = 10.f;
	MaxBrakeTorque = 4500.f;
	MaxHandBrakeTorque = 6000.f;
}
