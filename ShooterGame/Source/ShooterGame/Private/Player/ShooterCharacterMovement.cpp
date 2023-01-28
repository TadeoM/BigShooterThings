// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterGame.h"
#include "Player/ShooterCharacterMovement.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

//----------------------------------------------------------------------//
// UPawnMovementComponent
//----------------------------------------------------------------------//
UShooterCharacterMovement::UShooterCharacterMovement()
{
	SetNetworkMoveDataContainer(customMoveDataContainer);
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

void UShooterCharacterMovement::ActivateCustomMovementFlag(ECustomMovementFlags flag)
{
	customMovementFlags |= flag;
}

void UShooterCharacterMovement::ClearMovementFlag(ECustomMovementFlags flag)
{
	customMovementFlags &= ~flag;
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

		/*if (!GetOwner()->HasAuthority() && GetPawnOwner()->IsLocallyControlled())
		{
			ServerSetTeleportRPC(wantsToTeleport, destination);
		}
		else if (GetOwner()->HasAuthority() && !GetPawnOwner()->IsLocallyControlled())
		{
			ClientSetTeleportRPC(wantsToTeleport, destination);
		}*/

#pragma endregion

	}
}

void UShooterCharacterMovement::ProcessTeleport()
{
	FHitResult res;

	//UE_LOG(LogTemp, Warning, TEXT("Processing Teleport Function"));

	//SafeMoveUpdatedComponent(teleportDestination - GetOwner()->GetActorLocation(), GetOwner()->GetActorRotation(), false, res, ETeleportType::TeleportPhysics);
	AActor* actor = GetOwner();
	actor->TeleportTo(teleportDestination/* - GetOwner()->GetActorLocation()*/, GetOwner()->GetActorRotation(), false);
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
	bWantsToTeleport ? ActivateCustomMovementFlag(ECustomMovementFlags::CFLAG_Teleport) : ClearMovementFlag(ECustomMovementFlags::CFLAG_Teleport);
	SetTeleport(bWantsToTeleport, teleportDestination);
}

bool UShooterCharacterMovement::CanTeleport()
{
	return fTeleportCurrentCooldown <= 0;
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


void UShooterCharacterMovement::UpdateFromCompressedFlags(uint8 flags)
{
	if (!CharacterOwner)
	{
		return;
	}

	Super::UpdateFromCompressedFlags(flags);

	//FShooterCharacterNetworkMoveData* moveData = static_cast<FShooterCharacterNetworkMoveData*>(GetCurrentNetworkMoveData());
	const bool bWasPressingTeleport = bWantsToTeleport;
	/*CharacterOwner->bPressedJump = ((flags & ECustomMovementFlags::CFLAG_Teleport) != 0);
	bWantsToCrouch = ((flags & FSavedMove_Character::FLAG_Custom_0) != 0);*/

	// Detect change in jump press on the server
	if (CharacterOwner->GetLocalRole() == ROLE_Authority)
	{
		//const bool bIsPressingJump = CharacterOwner->bPressedJump;
		if (bWantsToTeleport)
		{
			ProcessTeleport();
		}
	}
}

FNetworkPredictionData_Client* UShooterCharacterMovement::GetPredictionData_Client() const
{
	check(PawnOwner != NULL);
	//Bug here I think on listen server, not sure if client or lsiten server yet Commenting out seams to be ok, testing on dedi and listen and issue is fixed when commenting out
	//check(PawnOwner->Role < ROLE_Authority);

	if (!ClientPredictionData)
	{
		UShooterCharacterMovement* MutableThis = const_cast<UShooterCharacterMovement*>(this);

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_ShooterMovement(*this);
		//MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 500.f;
		//MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}
	return ClientPredictionData;
}

void UShooterCharacterMovement::MoveAutonomous(float clientTimeStamp, float deltaTime, uint8 compressedFlags, const FVector& newAccel)
{

	FShooterCharacterNetworkMoveData* moveData = static_cast<FShooterCharacterNetworkMoveData*>(GetCurrentNetworkMoveData());
	if (moveData != nullptr)
	{
		customMovementFlags = moveData->MoveData_CustomMovementFlags;
	}

	Super::MoveAutonomous(clientTimeStamp, deltaTime, compressedFlags, newAccel);
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
	Saved_CustomMovementFlags = 0;
}

uint8 FSavedMove_ShooterMovement::GetCompressedFlags() const
{
	uint8 result = Super::GetCompressedFlags();

	if (savedWantsToTeleport)
	{
		result |= FLAG_Custom_0;
	}
	return result;
}

bool FSavedMove_ShooterMovement::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	FSavedMove_ShooterMovement* NewMovePtr = static_cast<FSavedMove_ShooterMovement*>(NewMove.Get());

	if (Saved_CustomMovementFlags != NewMovePtr->Saved_CustomMovementFlags)
	{
		return false;
	}

	if (savedWantsToTeleport != ((FSavedMove_ShooterMovement*)&NewMove)->savedWantsToTeleport)
	{
		return false;
	}

	if (savedTeleportDestination != ((FSavedMove_ShooterMovement*)&NewMove)->savedTeleportDestination)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void FSavedMove_ShooterMovement::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	UShooterCharacterMovement* charMove = Cast<UShooterCharacterMovement>(Character->GetCharacterMovement());
	if (charMove)
	{
		Saved_CustomMovementFlags = charMove->customMovementFlags;

		savedWantsToTeleport = charMove->bWantsToTeleport;
		savedTeleportDestination = charMove->teleportDestination;
		if (savedTeleportDestination.SizeSquared() > 0.0f) 
		{
		}
	}
}

void FSavedMove_ShooterMovement::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);
	UShooterCharacterMovement* charMove = Cast<UShooterCharacterMovement>(Character->GetCharacterMovement());
	if (charMove)
	{
		charMove->customMovementFlags = Saved_CustomMovementFlags;

		charMove->execSetTeleport(savedWantsToTeleport, savedTeleportDestination);
	}
}
#pragma endregion

#pragma region Data Containers
void FShooterCharacterNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	Super::ClientFillNetworkMoveData(ClientMove, MoveType); 

	const FSavedMove_ShooterMovement& savedMove = static_cast<const FSavedMove_ShooterMovement&>(ClientMove);

	MoveData_CustomMovementFlags = savedMove.Saved_CustomMovementFlags;
}

bool FShooterCharacterNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	const bool bIsSaving = Ar.IsSaving();

	SerializeOptionalValue<uint8>(bIsSaving, Ar, MoveData_CustomMovementFlags, 0);
	return !Ar.IsError();
}

FShooterCharacterNetworkMoveDataContainer::FShooterCharacterNetworkMoveDataContainer()
{
	NewMoveData = &CustomDefaultMoveData[0];
	PendingMoveData = &CustomDefaultMoveData[1];
	OldMoveData = &CustomDefaultMoveData[2];
}
#pragma endregion