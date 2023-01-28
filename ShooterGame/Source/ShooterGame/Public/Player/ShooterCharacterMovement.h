// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Movement component meant for use with Pawns.
 */

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ObjectMacros.h"
#include "Sound/SoundCue.h"
#include "ShooterCharacterMovement.generated.h"

UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_JETPACK = 0,
	CMOVE_GLIDE = 1,
	CMOVE_SPRINT = 2
};

class FShooterCharacterNetworkMoveData : public FCharacterNetworkMoveData
{
public:
	typedef FCharacterNetworkMoveData Super;

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;

	uint8 MoveData_CustomMovementFlags = 0;
};

class FShooterCharacterNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
public:
	FShooterCharacterNetworkMoveDataContainer();
	FShooterCharacterNetworkMoveData CustomDefaultMoveData[3];
};

class FSavedMove_ShooterMovement : public FSavedMove_Character
{
	friend class UShooterCharacterMovement;
public:
	typedef FSavedMove_Character Super;
	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* Character) override;

	uint8 Saved_CustomMovementFlags;

	float savedDistanceFromGround;
	float savedDesiredThrottle;

	bool savedWantsToTeleport : 1;
	FVector savedTeleportDestination;
};

/** Get prediction data for a client game. Should not be used if not running as a client. Allocates the data on demand and can be overridden to allocate a custom override if desired. Result must be a FNetworkPredictionData_Client_Character. */
class FNetworkPredictionData_Client_ShooterMovement : public FNetworkPredictionData_Client_Character
{
public:
	FNetworkPredictionData_Client_ShooterMovement(const UCharacterMovementComponent& ClientMovement);
	typedef FNetworkPredictionData_Client_Character Super;
	virtual FSavedMovePtr AllocateNewMove() override;
};

/** FSavedMove_Character represents a saved move on the client that has been sent to the server and might need to be played back. */

UCLASS()
class UShooterCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

	friend class FSavedMove_ShooterMovement;

#pragma region Overrides
	/*virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;*/
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual float GetMaxSpeed() const override;
	/*virtual float GetMaxAcceleration() const override;
	virtual bool IsFalling() const override;
	virtual bool IsMovingOnGround() const override;*/
#pragma endregion

	void ProcessTeleport();

#pragma region Networking
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
#pragma endregion

public:

	UShooterCharacterMovement();
	FShooterCharacterNetworkMoveDataContainer customMoveDataContainer;

	enum ECustomMovementFlags
	{
		CFLAG_Teleport = 1 << 0
	};

	uint8 customMovementFlags = 0;

	virtual void ActivateCustomMovementFlag(ECustomMovementFlags flag);
	virtual void ClearMovementFlag(ECustomMovementFlags flag);
	bool IsCustomFlagSet(ECustomMovementFlags flag) const { return (customMovementFlags & flag) != 0; }

	virtual void UpdateFromCompressedFlags(uint8 flags) override;
	virtual void MoveAutonomous(float clientTimeStamp, float deltaTime, uint8 compressedFlags, const FVector& newAccel) override;

	UPROPERTY(EditAnywhere, Category = "Camera")
		UCameraComponent* cam;

	float GetCurrentTeleportCooldown();
	float GetTeleportCooldownDefault();

	float fTeleportCurrentCooldown;
	float fTeleportCooldownDefault = 1.0f;
	UFUNCTION(BlueprintCallable)
		void StartTeleport();
	bool bWantsToTeleport : 1;

	UPROPERTY(BlueprintReadOnly, Category = "Custom|State")
		FVector teleportDestination;

	bool CanTeleport();

	UFUNCTION(BlueprintCallable)
		void SetTeleport(bool wantsToTeleport, FVector destination);

protected:
	void execSetTeleport(bool wantsToTeleport, FVector destination);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable)
		void ServerSetTeleportRPC(bool wantsToTeleport, FVector destination);
	UFUNCTION(Client, Reliable, BlueprintCallable)
		void ClientSetTeleportRPC(bool wantsToTeleport, FVector destination);
};




