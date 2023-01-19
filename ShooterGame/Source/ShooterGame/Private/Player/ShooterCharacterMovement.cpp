// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterGame.h"
#include "Player/ShooterCharacterMovement.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

//----------------------------------------------------------------------//
// UPawnMovementComponent
//----------------------------------------------------------------------//
UShooterCharacterMovement::UShooterCharacterMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


float UShooterCharacterMovement::GetMaxSpeed() const
{
	float MaxSpeed = Super::GetMaxSpeed();

	const AShooterCharacter* ShooterCharacterOwner = Cast<AShooterCharacter>(PawnOwner);
	if (ShooterCharacterOwner)
	{
		if (ShooterCharacterOwner->IsTargeting())
		{
			MaxSpeed *= ShooterCharacterOwner->GetTargetingSpeedModifier();
		}
		if (ShooterCharacterOwner->IsRunning())
		{
			MaxSpeed *= ShooterCharacterOwner->GetRunningSpeedModifier();
		}
	}

	return MaxSpeed;
}

void UShooterCharacterMovement::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
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

bool UShooterCharacterMovement::CanTeleport()
{
	return IsMovingOnGround();
}

void UShooterCharacterMovement::ProcessTeleport()
{
	// TODO: create timer from one teleport to the next.
	FHitResult res;
	AActor* actor = GetOwner();

	FVector start = GetActorLocation();

	AController* Controller = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	const bool bLimitRotation = (IsMovingOnGround() || IsFalling());
	const FRotator Rotation = bLimitRotation ? actor->GetActorRotation() : Controller->GetControlRotation();
	const FVector Direction = FRotationMatrix(Rotation).GetScaledAxis(EAxis::X);
	UE_LOG(LogTemp, Warning, TEXT("Start: %s"), *FString(start.ToString()));
	UE_LOG(LogTemp, Warning, TEXT("Direction: %s"), *FString(Direction.ToString()));

	start = FVector(start.X + (Direction.X * 200), start.Y + (Direction.Y * 200), start.Z + (Direction.Z * 200));
	FVector end = start + (Direction * 1000);
	UE_LOG(LogTemp, Warning, TEXT("End: %s"), *FString(end.ToString()));
	teleportDestination = end;

	FHitResult hit;

	if (GetWorld())
	{
		bool actorHit = GetWorld()->LineTraceSingleByChannel(hit, start, end, ECC_Pawn, FCollisionQueryParams(), FCollisionResponseParams());
		DrawDebugLine(GetWorld(), start, end, FColor::Blue, false, 2.0f, 0.0f, 10.0f);
		if (actorHit && hit.GetActor())
		{
			UE_LOG(LogTemp, Warning, TEXT("Hit an actor"));
			teleportDestination = hit.GetActor()->GetActorLocation() - (Direction * 100);

		}
	}

	SafeMoveUpdatedComponent(teleportDestination - GetOwner()->GetActorLocation(), GetOwner()->GetActorRotation(), false, res, ETeleportType::TeleportPhysics);

	/*if (GetPawnOwner()->IsLocallyControlled())
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), teleportSound, teleportDestination);
	}*/

	/*if (GetOwner()->HasAuthority())
	{
		MulticastPlayTeleportSound(teleportDestination);
	}*/

	execSetTeleport(false, FVector::ZeroVector);
}

void UShooterCharacterMovement::execSetTeleport(bool wantsToTeleport, FVector destination)
{
	bWantsToTeleport = wantsToTeleport;
	teleportDestination = destination;
}

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
