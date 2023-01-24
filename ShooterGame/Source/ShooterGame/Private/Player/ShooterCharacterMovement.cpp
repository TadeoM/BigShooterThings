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

	fTeleportCurrentCooldown -= DeltaSeconds;

	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
}

float UShooterCharacterMovement::GetCurrentTeleportCooldown()
{
	return fTeleportCurrentCooldown;
}

float UShooterCharacterMovement::GetTeleportCooldownDefault()
{
	return fTeleportCooldownDefault;
}

#pragma region Teleportation 
void UShooterCharacterMovement::SetTeleport(bool wantsToTeleport, FVector destination)
{
	if (bWantsToTeleport != wantsToTeleport)
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

	bWantsToTeleport = true;
	SetTeleport(bWantsToTeleport, teleportDestination);
}

bool UShooterCharacterMovement::CanTeleport()
{
	return fTeleportCurrentCooldown <= 0;
}

void UShooterCharacterMovement::ProcessTeleport()
{
	// TODO: create timer from one teleport to the next.
	FHitResult res;
	UE_LOG(LogTemp, Warning, TEXT("processing teleport %s"), *(teleportDestination).ToString());

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
	fTeleportCurrentCooldown = fTeleportCooldownDefault;
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
#pragma endregion

FNetworkPredictionData_Client* UShooterCharacterMovement::GetPredictionData_Client() const
{
	check(PawnOwner != NULL);
	//Bug here I think on listen server, not sure if client or lsiten server yet Commenting out seams to be ok, testing on dedi and listen and issue is fixed when commenting out
	//check(PawnOwner->Role < ROLE_Authority);

	if (!ClientPredictionData)
	{
		UShooterCharacterMovement* MutableThis = const_cast<UShooterCharacterMovement*>(this);

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_ShooterMovement(*this);
		//MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		//MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}
	return ClientPredictionData;
}

FNetworkPredictionData_Client_ShooterMovement::FNetworkPredictionData_Client_ShooterMovement(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{

}

#pragma region SavedMoves

FSavedMovePtr FNetworkPredictionData_Client_ShooterMovement::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_ShooterMovement());
}
void FSavedMove_ShooterMovement::Clear()
{
	Super::Clear();
	savedWantsToTeleport = false;
}

uint8 FSavedMove_ShooterMovement::GetCompressedFlags() const
{
	uint8 result = Super::GetCompressedFlags();

	if (savedWantsToTeleport)
	{
		result |= FLAG_WantsToCrouch;
	}
	return result;
}

bool FSavedMove_ShooterMovement::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	if (savedWantsToTeleport != ((FSavedMove_ShooterMovement*)&NewMove)->savedWantsToTeleport)
		return false;
	if (savedTeleportDestination != ((FSavedMove_ShooterMovement*)&NewMove)->savedTeleportDestination)
		return false;

	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void FSavedMove_ShooterMovement::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	UShooterCharacterMovement* charMove = Cast<UShooterCharacterMovement>(Character->GetCharacterMovement());
	if (charMove)
	{
		savedWantsToTeleport = charMove->bWantsToTeleport;
		savedTeleportDestination = charMove->teleportDestination;
	}
}

void FSavedMove_ShooterMovement::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);
	UShooterCharacterMovement* charMove = Cast<UShooterCharacterMovement>(Character->GetCharacterMovement());
	if (charMove)
	{
		charMove->execSetTeleport(savedWantsToTeleport, savedTeleportDestination);
	}
}
#pragma endregion
