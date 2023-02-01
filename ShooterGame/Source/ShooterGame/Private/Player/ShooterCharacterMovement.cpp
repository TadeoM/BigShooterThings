// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ShooterCharacterMovement.h"
#include "GameFramework/Character.h"
#include "Engine/Classes/Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Curves/CurveFloat.h"
#include "Engine/Classes/GameFramework/Controller.h"

UShooterCharacterMovement::UShooterCharacterMovement()
{
	fJetpackResource = 1.0;
	bWantsToGlide = false;
	fDesiredThrottle = 0.0;
}
void UShooterCharacterMovement::StartTeleport()
{
	AActor* actor = GetOwner();

	AController* Controller = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	const bool bLimitRotation = (IsMovingOnGround() || IsFalling());
	const FRotator Rotation = bLimitRotation ? actor->GetActorRotation() : Controller->GetControlRotation();
	const FVector Direction = FRotationMatrix(Rotation).GetScaledAxis(EAxis::X);

	FVector start = FVector(GetActorLocation().X + (Direction.X * 100), GetActorLocation().Y + (Direction.Y * 100), GetActorLocation().Z - 50);
	FVector end = FVector(start.X + (Direction.X * 1000), start.Y + (Direction.Y * 1000), start.Z);

	FHitResult hit;

	if (GetWorld())
	{
		bool actorHit = GetWorld()->LineTraceSingleByChannel(hit, start, end, ECC_PhysicsBody, FCollisionQueryParams(), FCollisionResponseParams());
		if (actorHit && hit.GetActor())
		{
			// if we hit an actor, teleport destination is just behind the 
			teleportDestination = FVector(hit.Location.X - (Direction.X * 100), hit.Location.Y - (Direction.Y * 100), GetActorLocation().Z);
			end = hit.Location - (Direction * 50);
		}
		teleportDestination = end;
		DrawDebugLine(GetWorld(), start, end, FColor::Blue, false, 2.0f, 0.0f, 10.0f);
	}

	//bWantsToTeleport = true;
	//bWantsToTeleport ? ActivateCustomMovementFlag(ECustomMovementFlags::CFLAG_Teleport) : ClearMovementFlag(ECustomMovementFlags::CFLAG_Teleport);
	SetTeleport(true, teleportDestination);
}


#pragma region Movement Mode Implementations

void UShooterCharacterMovement::PhysCustom(float deltaTime, int32 Iterations)
{

	if (CustomMovementMode == ECustomMovementMode::CMOVE_JETPACK)
	{
		PhysJetpack(deltaTime, Iterations);
	}
	if (CustomMovementMode == ECustomMovementMode::CMOVE_GLIDE)
	{
		PhysGlide(deltaTime, Iterations);
	}
	if (CustomMovementMode == ECustomMovementMode::CMOVE_SPRINT)
	{
		PhysSprint(deltaTime, Iterations);
	}
	if (CustomMovementMode == ECustomMovementMode::CMOVE_REWIND)
	{
		PhysRewind(deltaTime, Iterations);
	}
	Super::PhysCustom(deltaTime, Iterations);
}

void UShooterCharacterMovement::PhysSprint(float deltaTime, int32 Iterations)
{

	if (!IsCustomMovementMode(ECustomMovementMode::CMOVE_SPRINT))
	{
		SetSprinting(false);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	if (!bWantsToSprint)
	{
		SetSprinting(false);
		SetMovementMode(EMovementMode::MOVE_Walking);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	PhysWalking(deltaTime, Iterations);
}

void UShooterCharacterMovement::PhysJetpack(float deltaTime, int32 Iterations)
{
	if (!IsCustomMovementMode(ECustomMovementMode::CMOVE_JETPACK))
	{
		SetJetpacking(0.0);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	if (!bWantsToJetpack ||
		fDistanceFromGround <= 0 ||
		fJetpackResource <= (deltaTime / JetpackMaxTime))
	{
		SetJetpacking(0.0);
		SetMovementMode(EMovementMode::MOVE_Falling);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

#pragma region Print Data

	if (GetPawnOwner()->IsLocallyControlled())
	{
		UKismetSystemLibrary::PrintString(GetWorld(), FString("Speed: ") + FString::SanitizeFloat(Velocity.Size()), true, false, FLinearColor::Red, 0.0);
		UKismetSystemLibrary::PrintString(GetWorld(), FString("Distance from Ground: ") + FString::SanitizeFloat(fDistanceFromGround), true, false, FLinearColor::Red, 0.0);
		UKismetSystemLibrary::PrintString(GetWorld(), FString("Resource: ") + FString::SanitizeFloat(fJetpackResource), true, false, FLinearColor::Red, 0.0);
	}

#pragma endregion

	float jetpackAcceleration = JetpackBaseForce / Mass;
	float curveMultiplier = 1.0;
	if (JetpackHeightToForceMultiplier)
		curveMultiplier = JetpackHeightToForceMultiplier->GetFloatValue(fDistanceFromGround);
	if (JetpackVelocityToForceMultiplier)
		curveMultiplier = FMath::Max(curveMultiplier, JetpackVelocityToForceMultiplier->GetFloatValue(Velocity.Z));

	jetpackAcceleration *= curveMultiplier;

	float jetpackSurplusAccel = FMath::Max<float>(0, jetpackAcceleration + GetGravityZ());
	float desiredSurplusJetpackAccel = jetpackSurplusAccel * fDesiredThrottle;
	float desiredTotalJetpackAccel = (GetGravityZ() * -1) + desiredSurplusJetpackAccel;

	float totalDesiredVelocity = Velocity.Z + (desiredTotalJetpackAccel * deltaTime);
	float velLimitWGravCounteract = JetpackMaxVelocity + (GetGravityZ() * deltaTime * -1);
	float resultingAccel = 0.0;

	float deltaFromGround = JetpackMaxDistanceFromGround - fDistanceFromGround;

	if (Velocity.Z > velLimitWGravCounteract)
	{
		resultingAccel = 0.0f;
	}
	else
	{
		if (totalDesiredVelocity > velLimitWGravCounteract)
		{
			float velLimitClampAmount = totalDesiredVelocity - velLimitWGravCounteract;
			resultingAccel = FMath::Clamp<float>(desiredTotalJetpackAccel - (velLimitClampAmount / deltaTime), 0, desiredTotalJetpackAccel);
		}
		else
		{
			resultingAccel = desiredTotalJetpackAccel;
		}
	}

	float intermediaryVelocity = Velocity.Z + resultingAccel * deltaTime;
	float distanceWithIntermediareVelocity = intermediaryVelocity * deltaTime;

	if (deltaFromGround > 0)
	{
		if (intermediaryVelocity < 0)
		{

		}
		else
		{
			float stopTime = intermediaryVelocity / (GetGravityZ() * -1);
			float timeToCrossDelta = deltaFromGround / intermediaryVelocity;
			if (stopTime >= timeToCrossDelta)
			{
				float timeClampAmount = stopTime - timeToCrossDelta;
				resultingAccel -= FMath::Clamp<float>(deltaFromGround / (timeClampAmount * timeClampAmount), 0, resultingAccel);
			}
			else
			{

			}
		}
	}
	else
	{

		if (Velocity.Z > 0)
		{
			resultingAccel = 0.0f;
		}
		else
		{

			float stopTime = ((Velocity.Z + (GetGravityZ() * deltaTime)) * -1) / desiredSurplusJetpackAccel;
			float timeToCrossDelta = deltaFromGround / (Velocity.Z + GetGravityZ() * deltaTime);
			if (stopTime > timeToCrossDelta)
			{
				float timeClampAmount = stopTime - timeToCrossDelta;
				resultingAccel += FMath::Clamp<float>(deltaFromGround / (timeClampAmount * timeClampAmount), 0, desiredSurplusJetpackAccel - resultingAccel);
			}
			else
			{
				resultingAccel = 0.0f;
			}
		}
	}

	fEffectiveThrottle = resultingAccel / (JetpackBaseForce / Mass);
	Velocity.Z += resultingAccel * deltaTime;

	if (DisableLateralFrictionForJetpack)
	{
		float oldFallingLateralFriction = FallingLateralFriction;
		FallingLateralFriction = 0;
		PhysFalling(deltaTime, Iterations);
		FallingLateralFriction = oldFallingLateralFriction;
	}
	else
	{
		PhysFalling(deltaTime, Iterations);
	}

	fJetpackResource = FMath::Clamp<float>(fJetpackResource - (deltaTime / JetpackMaxTime) * fEffectiveThrottle, 0, 1);
}

void UShooterCharacterMovement::PhysGlide(float deltaTime, int32 Iterations)
{
	if (!IsCustomMovementMode(ECustomMovementMode::CMOVE_GLIDE))
	{
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	//airspeed
	float velSize = Velocity.Size();
	FRotator controlDirection = GetPawnOwner()->GetControlRotation();

	//if we do not want to glide anymore, or we are too low, cancel gliding mode
	if (!bWantsToGlide || fDistanceFromGround < GliderCancelDistanceFromGround)
	{
		SetGliding(false);
		SetMovementMode(EMovementMode::MOVE_Falling);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	FRotator controlDirectionDelta = controlDirection - previousControlDirection;

	//normalize shifts angles to -180 to 180 range, pitch of control direction cannot exceed this range, it is locked in that range and cannot rollover
	controlDirectionDelta.Normalize();

	//at 1 we have full control, at 0 we have no control
	float velocityControlMultiplier = FMath::Clamp<float>((velSize - GliderControlLossVelocity) / GliderControlLossVelocityRange, 0, 1);
	//the slower we are, the more gravity forces us to turn downwards
	float pitchLoss = (deltaTime * 180 * (1 - FMath::InterpEaseOut<float>(0, 1, velocityControlMultiplier, 2)));

	controlDirectionDelta = FRotator(
		FMath::Clamp<float>(controlDirectionDelta.Pitch + pitchLoss, deltaTime * GliderMaxPitchRate * -1, deltaTime * GliderMaxPitchRate),
		FMath::Clamp<float>(controlDirectionDelta.Yaw, deltaTime * GliderMaxYawRate * -1 * velocityControlMultiplier, deltaTime * GliderMaxYawRate * velocityControlMultiplier),
		controlDirectionDelta.Roll
	);

	controlDirection = previousControlDirection + controlDirectionDelta;

	//clamp shifts angle to 0-360 range, for some reason SetControlRotation prefers it this way
	GetPawnOwner()->Controller->SetControlRotation(controlDirection.Clamp());

	//pitch is flipped to invert vertical mouselook, and is rotated by further 90 degrees, because we are parallel to the ground when we glide
	FRotator pitchFlippedControlDirection = FRotator((controlDirection.Pitch * -1) - 90, controlDirection.Yaw, controlDirection.Roll).Clamp();

	// 1 is down, 0 is parallel, -1 is up
	angleOfAttack = (FMath::Abs(pitchFlippedControlDirection.GetNormalized().Pitch) / 90) - 1;

	float dragMultiplier = 1 - (GliderDragFactor * deltaTime);
	float gravityVelocity = angleOfAttack * GetGravityZ() * deltaTime;

	//shift velocity heading to upward vector of actor, ie directon of flight defined by control direction
	Velocity = FMath::Max<float>((velSize - gravityVelocity) * dragMultiplier, 0) * FRotator(pitchFlippedControlDirection).Add(90, 0, 0).Vector().GetSafeNormal();

	FHitResult out;
	SafeMoveUpdatedComponent(Velocity * deltaTime, pitchFlippedControlDirection.Quaternion(), true, out);

	previousControlDirection = controlDirection;

	if (out.bBlockingHit)
	{
		SetMovementMode(EMovementMode::MOVE_Falling);
		SetGliding(false);
	}
}

void UShooterCharacterMovement::PhysRewind(float deltaTime, int32 Iterations)
{

	if (!IsCustomMovementMode(ECustomMovementMode::CMOVE_REWIND))
	{
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	if (!bWantsToRewind || fDistanceFromGround < GliderCancelDistanceFromGround)
	{
		SetGliding(false);
		SetMovementMode(EMovementMode::MOVE_Falling);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}
	if (!bWantsToRewind || fDistanceFromGround < GliderCancelDistanceFromGround)
	{
		SetGliding(false);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

}

#pragma endregion

#pragma region Overrides

void UShooterCharacterMovement::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//recharge jetpack resource
	if (MovementMode != EMovementMode::MOVE_Custom || CustomMovementMode != ECustomMovementMode::CMOVE_JETPACK)
	{
		fJetpackResource = FMath::Clamp<float>(fJetpackResource + (DeltaTime / JetpackFullRechargeSeconds), 0, 1);
	}
#pragma region Debug Prints
	//FVector normVel = FVector(Velocity);
	//normVel.Normalize();
	//UKismetSystemLibrary::PrintString(GetWorld(), FString("Distance from ground: ") + FString::SanitizeFloat(fDistanceFromGround), true, false, FLinearColor::Red, 0.0);
	//UKismetSystemLibrary::PrintString(GetWorld(), FString("Acceleration: ") + Velocity.ToString(), true, false, FLinearColor::Red, 0.0);
	//UKismetSystemLibrary::PrintString(GetWorld(), FString("Up: ") + GetOwner()->GetActorUpVector().ToString(), true, false, FLinearColor::Red, 0.0);
	//UKismetSystemLibrary::PrintString(GetWorld(), FString("Fwd: ") + GetOwner()->GetActorForwardVector().ToString(), true, false, FLinearColor::Red, 0.0);
	//UKismetSystemLibrary::PrintString(GetWorld(), FString("NormVel: ") + normVel.ToString(), true, false, FLinearColor::Red, 0.0);
	//UKismetSystemLibrary::PrintString(GetWorld(), FString("VelLen: ") + FString::SanitizeFloat(Velocity.Size()), true, false, FLinearColor::Red, 0.0);
	//UKismetSystemLibrary::PrintString(GetWorld(), FString("Effective Throttle: ") + FString::SanitizeFloat(fEffectiveThrottle), true, false, FLinearColor::Red, 0.0);
	//UKismetSystemLibrary::PrintString(GetWorld(), FString("Desired Throttle ") + FString::SanitizeFloat(fDesiredThrottle), true, false, FLinearColor::Red, 0.0);
	//UKismetSystemLibrary::PrintString(GetWorld(), FString("Jetpack Resource: ") + FString::SanitizeFloat(fJetpackResource), true, false, FLinearColor::Red, 0.0);
	//UKismetSystemLibrary::PrintString(GetWorld(), FString("Control Direction: ") + GetPawnOwner()->GetControlRotation().ToString(), true, false, FLinearColor::Green, 0.0);
	//UKismetSystemLibrary::PrintString(GetWorld(), FString("RotatedUpVecotr: ") + GetOwner()->GetActorRotation().UnrotateVector(GetOwner()->GetActorUpVector()).ToString(), true, false, FLinearColor::Green, 0.0);
	//UKismetSystemLibrary::PrintString(GetWorld(), FString("AR: ") + FRotator(GetOwner()->GetActorRotation()).Clamp().ToString(), true, false, FLinearColor::Green, 0.0);

#pragma endregion
}

void UShooterCharacterMovement::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	MeasureDistanceFromGround();

	if (bWantsToSprint)
	{
		if (CanSprint())
		{
			SetMovementMode(EMovementMode::MOVE_Custom, ECustomMovementMode::CMOVE_SPRINT);
		}
		/*else if (!IsCustomMovementMode(ECustomMovementMode::CMOVE_SPRINT))
		{
			SetSprinting(false);
		}*/
	}

	if (bWantsToJetpack)
	{
		if (CanJetpack())
		{
			SetMovementMode(EMovementMode::MOVE_Custom, ECustomMovementMode::CMOVE_JETPACK);
		}
		/*else if (!IsCustomMovementMode(ECustomMovementMode::CMOVE_JETPACK))
		{
			SetJetpacking(0);
		}*/
	}

	if (bWantsToGlide)
	{
		if (CanGlide())
		{
			SetMovementMode(EMovementMode::MOVE_Custom, ECustomMovementMode::CMOVE_GLIDE);
		}
		else if (!IsCustomMovementMode(ECustomMovementMode::CMOVE_GLIDE))
		{
			SetGliding(false);
		}
	}

	if (bWantsToRewind)
	{
		if (CanRewind())
		{

		}
		else 
		{

		}
	}

	if (bWantsToTeleport)
	{
		if (CanTeleport())
		{
			ProcessTeleport();
		}
		else
		{
			SetTeleport(false, FVector::ZeroVector);
		}
	}

	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
}

void UShooterCharacterMovement::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	bool suppressSuperNotification = false;

	if (PreviousMovementMode == MovementMode && PreviousCustomMode == CustomMovementMode)
	{
		Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
		return;
	}

#pragma region Leaving State Handlers

	if (PreviousMovementMode == EMovementMode::MOVE_Custom && PreviousCustomMode == ECustomMovementMode::CMOVE_SPRINT)
	{
		SetSprinting(false);
		MaxWalkSpeed /= SprintSpeedMultiplier;
	}
	if (PreviousMovementMode == EMovementMode::MOVE_Custom && PreviousCustomMode == ECustomMovementMode::CMOVE_JETPACK)
	{
		SetJetpacking(0);
		fEffectiveThrottle = 0;
	}
	if (PreviousMovementMode == EMovementMode::MOVE_Custom && PreviousCustomMode == ECustomMovementMode::CMOVE_GLIDE)
	{
		SetGliding(false);

		if (GetPawnOwner() && GetPawnOwner()->Controller)
		{
			FRotator controlDirection = GetPawnOwner()->GetControlRotation();
			GetPawnOwner()->Controller->SetControlRotation(FRotator(controlDirection.GetNormalized().Pitch * -1, controlDirection.Yaw, controlDirection.Roll).Clamp());
			GetOwner()->SetActorRotation(FRotator(0, GetOwner()->GetActorRotation().Yaw, 0));
		}
	}

#pragma endregion

#pragma region Entering State Handlers

	if (IsCustomMovementMode(ECustomMovementMode::CMOVE_SPRINT))
	{
		MaxWalkSpeed *= SprintSpeedMultiplier;
		suppressSuperNotification = true;
	}
	if (IsCustomMovementMode(ECustomMovementMode::CMOVE_JETPACK))
	{

	}
	if (IsCustomMovementMode(ECustomMovementMode::CMOVE_GLIDE))
	{
		if (GetOwner())
		{
			Velocity += GetOwner()->GetActorForwardVector() * GliderInitialImpulse;
		}

		if (GetPawnOwner() && GetPawnOwner()->Controller)
		{
			FRotator currentControlRotation = GetPawnOwner()->GetControlRotation();
			currentControlRotation.GetNormalized();
			GetPawnOwner()->Controller->SetControlRotation(FRotator(currentControlRotation.Pitch * -1, currentControlRotation.Yaw, currentControlRotation.Roll).Clamp());
			previousControlDirection = GetPawnOwner()->GetControlRotation();
		}
	}

#pragma endregion
	if (!suppressSuperNotification)
		Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	else
		CharacterOwner->OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

float UShooterCharacterMovement::GetMaxSpeed() const
{
	if (MovementMode == EMovementMode::MOVE_Custom && CustomMovementMode == ECustomMovementMode::CMOVE_GLIDE)
	{
		return GliderMaxSpeed;
	}

	if (IsCustomMovementMode(ECustomMovementMode::CMOVE_SPRINT))
	{
		return Super::GetMaxSpeed() * SprintSpeedMultiplier;
	}

	return Super::GetMaxSpeed();
}

float UShooterCharacterMovement::GetMaxAcceleration() const
{
	return Super::GetMaxAcceleration();
}

bool UShooterCharacterMovement::IsFalling() const
{
	return Super::IsFalling() || IsCustomMovementMode(ECustomMovementMode::CMOVE_JETPACK);
}

bool UShooterCharacterMovement::IsMovingOnGround() const
{
	return Super::IsMovingOnGround() || (IsCustomMovementMode((uint8)ECustomMovementMode::CMOVE_SPRINT) && UpdatedComponent);
}

#pragma endregion

#pragma region Helpers

bool UShooterCharacterMovement::IsCustomMovementMode(uint8 cm) const
{
	if (MovementMode == EMovementMode::MOVE_Custom && CustomMovementMode == cm)
		return true;
	return false;
}

void UShooterCharacterMovement::MeasureDistanceFromGround()
{
	//Don't measure distance for actors, that are not controlled by you if you are a client
	/*if (!GetOwner()->HasAuthority() && !GetPawnOwner()->IsLocallyControlled())
		return;*/

	FHitResult res;
	FVector start = GetOwner()->GetActorLocation() + distanceCheckOrigin;

	if (UKismetSystemLibrary::LineTraceSingle(GetWorld(),
		start,
		(FVector(0, 0, -1) * DistanceCheckRange) + start,
		ETraceTypeQuery::TraceTypeQuery1,
		false,
		TArray<AActor*>(),
		EDrawDebugTrace::ForOneFrame,
		res,
		true

	) && res.bBlockingHit)
	{
		fDistanceFromGround = res.Distance;
	}
	else
	{
		fDistanceFromGround = DistanceCheckRange;
	}
}

void UShooterCharacterMovement::ProcessTeleport()
{
	FHitResult res;

	SafeMoveUpdatedComponent(teleportDestination - GetOwner()->GetActorLocation(), GetOwner()->GetActorRotation(), false, res, ETeleportType::TeleportPhysics);

	if (GetPawnOwner()->IsLocallyControlled())
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), teleportSound, teleportDestination);
	}

	if (GetOwner()->HasAuthority())
	{
		MulticastPlayTeleportSound(teleportDestination);
	}

	execSetTeleport(false, FVector::ZeroVector);
}

void UShooterCharacterMovement::MulticastPlayTeleportSound_Implementation(FVector location)
{
	//this should only execute on proxies
	if (!GetPawnOwner()->IsLocallyControlled())
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), teleportSound, location);
	}
}
#pragma endregion

#pragma region State Setters

void UShooterCharacterMovement::SetJetpacking(float throttle)
{
	if (throttle != fDesiredThrottle)
	{
		execSetJetpacking(throttle);

#pragma region Networking

		if (!GetOwner() || !GetPawnOwner())
			return;

		if (!GetOwner()->HasAuthority() && GetPawnOwner()->IsLocallyControlled())
		{
			ServerSetJetpackingRPC(throttle);
		}
		else if (GetOwner()->HasAuthority() && !GetPawnOwner()->IsLocallyControlled())
		{
			ClientSetJetpackingRPC(throttle);
		}

#pragma endregion

	}
}

void UShooterCharacterMovement::SetGliding(bool wantsToGlide)
{
	if (bWantsToGlide != wantsToGlide)
	{
		execSetGliding(wantsToGlide);

#pragma region Networking

		if (!GetOwner() || !GetPawnOwner())
			return;

		if (!GetOwner()->HasAuthority() && GetPawnOwner()->IsLocallyControlled())
		{
			ServerSetGlidingRPC(wantsToGlide);
		}
		else if (GetOwner()->HasAuthority() && !GetPawnOwner()->IsLocallyControlled())
		{
			ClientSetGlidingRPC(wantsToGlide);
		}

#pragma endregion

	}
}

void UShooterCharacterMovement::SetTeleport(bool wantsToTeleport, FVector destination)
{

	if (bWantsToTeleport != wantsToTeleport || teleportDestination != destination)
	{
		execSetTeleport(wantsToTeleport, destination);

#pragma region Networking

		if (!GetOwner() || !GetPawnOwner())
			return;

		if (!GetOwner()->HasAuthority() && GetPawnOwner()->IsLocallyControlled())
		{
			ServerSetTeleportRPC(wantsToTeleport, destination);
		}
		else if (GetOwner()->HasAuthority() && !GetPawnOwner()->IsLocallyControlled())
		{
			ClientSetTeleportRPC(wantsToTeleport, destination);
		}

#pragma endregion

	}
}

void UShooterCharacterMovement::SetSprinting(bool wantsToSprint)
{

	if (bWantsToSprint != wantsToSprint)
	{
		execSetSprinting(wantsToSprint);

#pragma region Networking

		if (!GetOwner() || !GetPawnOwner())
			return;

		if (!GetOwner()->HasAuthority() && GetPawnOwner()->IsLocallyControlled())
		{
			ServerSetSprintingRPC(wantsToSprint);
		}
		else if (GetOwner()->HasAuthority() && !GetPawnOwner()->IsLocallyControlled())
		{
			ClientSetSprintingRPC(wantsToSprint);
		}

#pragma endregion

	}
}

#pragma region Implementations

void UShooterCharacterMovement::execSetSprinting(bool wantsToSprint)
{
	bWantsToSprint = wantsToSprint;
}

void UShooterCharacterMovement::execSetJetpacking(float throttle)
{
	throttle = FMath::Clamp<float>(throttle, 0.0, 1.0);
	bWantsToJetpack = throttle > 0;
	fDesiredThrottle = throttle;
}

void UShooterCharacterMovement::execSetGliding(bool wantsToGlide)
{
	bWantsToGlide = wantsToGlide;
}

void UShooterCharacterMovement::execSetTeleport(bool wantsToRewind, FVector destination)
{
	bWantsToRewind = wantsToRewind;
}

void UShooterCharacterMovement::execSetTeleport(bool wantsToTeleport, FVector destination)
{
	bWantsToTeleport = wantsToTeleport;
	teleportDestination = destination;
	if (teleportCooldown <= 0)
	{
		teleportCooldown = teleportTimerDefault;
	}
}

#pragma endregion

#pragma region Jetpacking Replication

void UShooterCharacterMovement::ClientSetJetpackingRPC_Implementation(float throttle)
{
	execSetJetpacking(throttle);
}

bool UShooterCharacterMovement::ServerSetJetpackingRPC_Validate(float throttle)
{
	return true;
}

void UShooterCharacterMovement::ServerSetJetpackingRPC_Implementation(float throttle)
{
	execSetJetpacking(throttle);
}

#pragma endregion

#pragma region Gliding Replication

void UShooterCharacterMovement::ClientSetGlidingRPC_Implementation(bool wantsToGlide)
{
	execSetGliding(wantsToGlide);
}

bool UShooterCharacterMovement::ServerSetGlidingRPC_Validate(bool wantsToGlide)
{
	return true;
}

void UShooterCharacterMovement::ServerSetGlidingRPC_Implementation(bool wantsToGlide)
{

	execSetGliding(wantsToGlide);
}

#pragma endregion

#pragma region Sprinting Replication

void UShooterCharacterMovement::ClientSetSprintingRPC_Implementation(bool wantsToSprint)
{
	execSetSprinting(wantsToSprint);
}

bool UShooterCharacterMovement::ServerSetSprintingRPC_Validate(bool wantsToSprint)
{
	return true;
}

void UShooterCharacterMovement::ServerSetSprintingRPC_Implementation(bool wantsToSprint)
{
	execSetSprinting(wantsToSprint);

}

#pragma endregion

#pragma region Rewind Replication

void UShooterCharacterMovement::ClientSetRewindRPC_Implementation(bool wantsToTeleport, FVector destination)
{
	execSetRewind(wantsToTeleport, destination);
}

bool UShooterCharacterMovement::ServerSetRewindRPC_Validate(bool wantsToTeleport, FVector destination)
{
	return true;
}

void UShooterCharacterMovement::ServerSetRewindRPC_Implementation(bool wantsToTeleport, FVector destination)
{
	execSetRewind(wantsToTeleport, destination);
}

#pragma endregion

#pragma region Teleport Replication

void UShooterCharacterMovement::ClientSetTeleportRPC_Implementation(bool wantsToTeleport, FVector destination)
{
	execSetTeleport(wantsToTeleport, destination);
}

bool UShooterCharacterMovement::ServerSetTeleportRPC_Validate(bool wantsToTeleport, FVector destination)
{
	return true;
}

void UShooterCharacterMovement::ServerSetTeleportRPC_Implementation(bool wantsToTeleport, FVector destination)
{
	execSetTeleport(wantsToTeleport, destination);
}

#pragma endregion

#pragma endregion

#pragma region State Queries

bool UShooterCharacterMovement::IsSprinting()
{
	return IsCustomMovementMode(ECustomMovementMode::CMOVE_SPRINT);
}

bool UShooterCharacterMovement::IsJetpacking()
{
	return IsCustomMovementMode(ECustomMovementMode::CMOVE_JETPACK);
}

bool UShooterCharacterMovement::IsGliding()
{
	return IsCustomMovementMode(ECustomMovementMode::CMOVE_GLIDE);
}
bool UShooterCharacterMovement::IsRewinding()
{
	return IsCustomMovementMode(ECustomMovementMode::CMOVE_REWIND);
}

bool UShooterCharacterMovement::CanRewind()
{
	return rewindCooldown <= 0.f;
}

bool UShooterCharacterMovement::CanTeleport()
{
	return teleportCooldown;
}

#pragma endregion

#pragma region State Conditions

bool UShooterCharacterMovement::CanJetpack()
{
	if (fDistanceFromGround < JetpackMinDistanceFromGround)
	{
		return false;
	}

	if (MovementMode != EMovementMode::MOVE_Falling)
	{
		return false;
	}

	if (fJetpackResource <= 0)
	{
		return false;
	}

	return true;
}

bool UShooterCharacterMovement::CanGlide()
{
	if (IsFalling() && fDistanceFromGround > GliderMinDistanceFromGround)
		return true;
	return false;
}

bool UShooterCharacterMovement::CanSprint()
{
	return Super::IsMovingOnGround();
}

#pragma endregion

#pragma region Network

FNetworkPredictionData_Client*
UShooterCharacterMovement::GetPredictionData_Client() const
{
	check(PawnOwner != NULL);
	//Bug here I think on listen server, not sure if client or lsiten server yet Commenting out seams to be ok, testing on dedi and listen and issue is fixed when commenting out
	//check(PawnOwner->Role < ROLE_Authority);

	if (!ClientPredictionData)
	{
		UShooterCharacterMovement* MutableThis = const_cast<UShooterCharacterMovement*>(this);

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_ShooterCharacterMovement(*this);
		//MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		//MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}
	return ClientPredictionData;
}

FNetworkPredictionData_Client_ShooterCharacterMovement::FNetworkPredictionData_Client_ShooterCharacterMovement(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{

}

FSavedMovePtr FNetworkPredictionData_Client_ShooterCharacterMovement::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_ShooterCharacterMovement());
}

void FSavedMove_ShooterCharacterMovement::Clear()
{
	Super::Clear();
	savedJetpackResource = 1.0;
	savedDistanceFromGround = 0;
	savedDesiredThrottle = 0;
	savedWantsToGlide = false;
	savedWantsToSprint = false;
}

uint8 FSavedMove_ShooterCharacterMovement::GetCompressedFlags() const
{
	return Super::GetCompressedFlags();
}

bool FSavedMove_ShooterCharacterMovement::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	if (savedJetpackResource != ((FSavedMove_ShooterCharacterMovement*)&NewMove)->savedJetpackResource)
		return false;
	if (savedDistanceFromGround != ((FSavedMove_ShooterCharacterMovement*)&NewMove)->savedDistanceFromGround)
		return false;
	if (savedDesiredThrottle != ((FSavedMove_ShooterCharacterMovement*)&NewMove)->savedDesiredThrottle)
		return false;
	if (savedWantsToSprint != ((FSavedMove_ShooterCharacterMovement*)&NewMove)->savedWantsToSprint)
		return false;
	if (savedWantsToGlide != ((FSavedMove_ShooterCharacterMovement*)&NewMove)->savedWantsToGlide)
		return false;
	if (savedWantsToTeleport != ((FSavedMove_ShooterCharacterMovement*)&NewMove)->savedWantsToTeleport)
		return false;
	if (savedTeleportDestination != ((FSavedMove_ShooterCharacterMovement*)&NewMove)->savedTeleportDestination)
		return false;

	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void FSavedMove_ShooterCharacterMovement::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	UShooterCharacterMovement* CharMov = Cast<UShooterCharacterMovement>(Character->GetCharacterMovement());
	if (CharMov)
	{
		savedJetpackResource = CharMov->fJetpackResource;
		savedDistanceFromGround = CharMov->fDistanceFromGround;
		savedDesiredThrottle = CharMov->fDesiredThrottle;
		savedWantsToSprint = CharMov->bWantsToSprint;
		savedWantsToGlide = CharMov->bWantsToGlide;
		savedWantsToTeleport = CharMov->bWantsToTeleport;
		savedTeleportDestination = CharMov->teleportDestination;
	}
}

void FSavedMove_ShooterCharacterMovement::PrepMoveFor(ACharacter* Character)
{

	Super::PrepMoveFor(Character);
	UShooterCharacterMovement* CharMov = Cast<UShooterCharacterMovement>(Character->GetCharacterMovement());
	if (CharMov)
	{
		CharMov->fJetpackResource = savedJetpackResource;
		CharMov->fDistanceFromGround = savedDistanceFromGround;
		CharMov->execSetJetpacking(savedDesiredThrottle);
		CharMov->bWantsToSprint = savedWantsToSprint;
		CharMov->bWantsToGlide = savedWantsToGlide;
		CharMov->execSetTeleport(savedWantsToGlide, savedTeleportDestination);
	}
}

#pragma endregion