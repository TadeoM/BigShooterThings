// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterCharacterMovement.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/Classes/Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Curves/CurveFloat.h"
#include "Engine/Classes/GameFramework/Controller.h"

UShooterCharacterMovement::UShooterCharacterMovement()
{
	static ConstructorHelpers::FObjectFinder<UMaterial> CharacterBaseMaterialOb(TEXT("/Characters/Materials/HeroTPP"));
	static ConstructorHelpers::FObjectFinder<UMaterial> CharacterRewindingMaterialOb(TEXT("/Characters/Materials/HeroTPPRewinding"));
	charBaseMaterial = CharacterBaseMaterialOb.Object;
	charReindMaterial = CharacterRewindingMaterialOb.Object;

	rewindLerpInterval = 0.1f;
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

	SetTeleport(true, teleportDestination);
}

void UShooterCharacterMovement::StartRewind()
{
	SetRewind(true);
}


#pragma region Movement Mode Implementations

void UShooterCharacterMovement::PhysCustom(float deltaTime, int32 Iterations)
{
	if (CustomMovementMode == ECustomMovementMode::CMOVE_REWIND)
	{
		PhysRewind(deltaTime, Iterations);
	}
	Super::PhysCustom(deltaTime, Iterations);
}

void UShooterCharacterMovement::PhysRewind(float deltaTime, int32 Iterations)
{
	if (!IsCustomMovementMode(ECustomMovementMode::CMOVE_REWIND))
	{
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	if (!bWantsToRewind)
	{
		SetMovementMode(EMovementMode::MOVE_Walking);
		SetRewind(false);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	// rewind arrays are not updated while teleporting, so if the array is empty, stop rewinding
	if (rewindTimeStampStack.Num() == 0 || rewindLocationsStack.Num() == 0)
	{
		SetMovementMode(EMovementMode::MOVE_Walking);
		SetRewind(false);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}
	FHitResult res;
	FVector finalPos = rewindLocationsStack.Last();

	rewindLerpCurrentTime += deltaTime;
	
	float rewindCurrentAlpha = rewindLerpCurrentTime / rewindLerpInterval;
	FVector lerpedNewPos = FMath::Lerp(lerpStartPos, finalPos, rewindCurrentAlpha);
	UE_LOG(LogTemp, Warning, TEXT("%s: %s"), *GetOwner()->GetName(), *lerpedNewPos.ToString());

	// rewind to the next previous position. This should be lerped, in case of teleporting ability uses 
	SafeMoveUpdatedComponent(lerpedNewPos - GetOwner()->GetActorLocation(), GetOwner()->GetActorRotation(), false, res, ETeleportType::TeleportPhysics);

	if (rewindCurrentAlpha >= 1.0f)
	{
		// one last teleport to the last location to make sure it's 100% accurate
		//SafeMoveUpdatedComponent(rewindLocationsStack.Last() - GetOwner()->GetActorLocation(), GetOwner()->GetActorRotation(), false, res, ETeleportType::TeleportPhysics);
		
		lerpStartPos = GetOwner()->GetActorLocation();
		rewindLerpCurrentTime = 0.0f;
		rewindTimeStampStack.RemoveAt(rewindTimeStampStack.Num() - 1);
		rewindLocationsStack.RemoveAt(rewindLocationsStack.Num() - 1);
	}
}

#pragma endregion

#pragma region Overrides

void UShooterCharacterMovement::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UShooterCharacterMovement::CooldownTick(float DeltaSeconds)
{
	if (teleportCooldown > 0)
	{
		teleportCooldown -= DeltaSeconds;
	}
	if (!IsRewinding() && rewindCooldown > 0)
	{
		rewindCooldown -= DeltaSeconds;
	}
}

void UShooterCharacterMovement::RewindDataTick(float DeltaSeconds)
{
	// I'd like this to be a Deque, but TDeques are a UE5 Data Container :(
	if (!IsCustomMovementMode(ECustomMovementMode::CMOVE_REWIND))
	{
		float currentTime = GetWorld()->GetTimeSeconds();
		if(rewindTimeStampStack.Num() == 0 
			|| (rewindTimeStampStack.Last() <= currentTime - .1f && FVector::Distance(rewindLocationsStack.Last(), GetActorLocation()) > 100.0f))
		{
				rewindTimeStampStack.Add(currentTime);
				rewindLocationsStack.Add(GetActorLocation());
		}

		if (rewindTimeStampStack[0] <= currentTime - 2.0f)
		{
			rewindTimeStampStack.RemoveAt(0);
			rewindLocationsStack.RemoveAt(0);
		}
	}
}

void UShooterCharacterMovement::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	CooldownTick(DeltaSeconds);
	RewindDataTick(DeltaSeconds);

	if (bWantsToRewind)
	{
		if (CanRewind())
		{
			SetMovementMode(EMovementMode::MOVE_Custom, ECustomMovementMode::CMOVE_REWIND);

			rewindCooldown = rewindCooldownDefault;
			lerpStartPos = GetOwner()->GetActorLocation();
			rewindLerpCurrentTime = 0.f;
		}
		// might be unecessary, move once confirmed
		else if (!IsCustomMovementMode(ECustomMovementMode::CMOVE_REWIND))
		{
			SetRewind(false);
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

	if (PreviousMovementMode == EMovementMode::MOVE_Custom && PreviousCustomMode == ECustomMovementMode::CMOVE_REWIND)
	{
		SetRewind(false);
	}

#pragma endregion

#pragma region Entering State Handlers

	if (IsCustomMovementMode(ECustomMovementMode::CMOVE_REWIND))
	{
		TArray<UStaticMeshComponent*> Components;
		GetOwner()->GetComponents<UStaticMeshComponent>(Components);
		for (int32 i = 0; i < Components.Num(); i++)
		{
			UStaticMeshComponent* StaticMeshComponent = Components[i];
			UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
			//StaticMesh->SetMaterial();
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

float UShooterCharacterMovement::GetMaxAcceleration() const
{
	return Super::GetMaxAcceleration();
}

bool UShooterCharacterMovement::IsFalling() const
{
	return Super::IsFalling();
}

bool UShooterCharacterMovement::IsMovingOnGround() const
{
	return Super::IsMovingOnGround();
}

#pragma endregion

#pragma region Helpers

bool UShooterCharacterMovement::IsCustomMovementMode(uint8 cm) const
{
	if (MovementMode == EMovementMode::MOVE_Custom && CustomMovementMode == cm)
		return true;
	return false;
}

void UShooterCharacterMovement::ProcessTeleport()
{
	FHitResult res;

	SafeMoveUpdatedComponent(teleportDestination - GetOwner()->GetActorLocation(), GetOwner()->GetActorRotation(), false, res, ETeleportType::TeleportPhysics);
	teleportCooldown = teleportCooldownDefault;

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

void UShooterCharacterMovement::SetRewind(bool wantsToRewind)
{
	if (bWantsToRewind != wantsToRewind)
	{
		execSetRewind(wantsToRewind);

#pragma region Networking

		if (!GetOwner() || !GetPawnOwner())
			return;

		if (!GetOwner()->HasAuthority() && GetPawnOwner()->IsLocallyControlled())
		{
			ServerSetRewindRPC(wantsToRewind);
		}
		else if (GetOwner()->HasAuthority() && !GetPawnOwner()->IsLocallyControlled())
		{
			ClientSetRewindRPC(wantsToRewind);
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

#pragma region Implementations

void UShooterCharacterMovement::execSetRewind(bool wantsToRewind)
{
	bWantsToRewind = wantsToRewind;
}

void UShooterCharacterMovement::execSetTeleport(bool wantsToTeleport, FVector destination)
{
	bWantsToTeleport = wantsToTeleport;
	teleportDestination = destination;
}

#pragma endregion


#pragma region Rewind Replication

void UShooterCharacterMovement::ClientSetRewindRPC_Implementation(bool wantsToRewind)
{
	execSetRewind(wantsToRewind);
}

bool UShooterCharacterMovement::ServerSetRewindRPC_Validate(bool wantsToTeleport)
{
	return true;
}

void UShooterCharacterMovement::ServerSetRewindRPC_Implementation(bool wantsToRewind)
{
	execSetRewind(wantsToRewind);
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
	return teleportCooldown <= 0.f;
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
	savedWantsToRewind = false;
}

uint8 FSavedMove_ShooterCharacterMovement::GetCompressedFlags() const
{
	return Super::GetCompressedFlags();
}

bool FSavedMove_ShooterCharacterMovement::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	if (savedWantsToRewind != ((FSavedMove_ShooterCharacterMovement*)&NewMove)->savedWantsToRewind)
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
		savedWantsToRewind = CharMov->bWantsToRewind;
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
		CharMov->bWantsToTeleport = savedWantsToTeleport;
		CharMov->execSetTeleport(savedWantsToTeleport, savedTeleportDestination);
		CharMov->bWantsToRewind = savedWantsToRewind;
		CharMov->execSetRewind(savedWantsToRewind);
	}
}

#pragma endregion