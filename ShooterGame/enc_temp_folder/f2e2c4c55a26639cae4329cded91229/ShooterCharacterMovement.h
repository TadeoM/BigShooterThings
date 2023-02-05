// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ObjectMacros.h"
#include "Sound/SoundCue.h"
#include "ShooterCharacterMovement.generated.h"

/**
 *
 */
UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_DEFAULT = 0,
	CMOVE_REWIND = 1
};

UCLASS()
class UShooterCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

	FRotator previousControlDirection;
	USoundCue* teleportSound;

	UShooterCharacterMovement();

	friend class FSavedMove_ShooterCharacterMovement;

#pragma region Overrides

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual bool IsFalling() const override;
	virtual bool IsMovingOnGround() const override;

#pragma region Networking
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
#pragma endregion

#pragma endregion

#pragma region Movement Mode Implementations
	void PhysRewind(float deltaTime, int32 Iterations);
#pragma endregion	

#pragma region Helpers
	bool IsCustomMovementMode(uint8 cm) const;
	void ProcessTeleport();

	UFUNCTION(NetMulticast, unreliable)
		void MulticastPlayTeleportSound(FVector location);

#pragma endregion

protected:

#pragma region Local State Setters

	void execSetRewind(bool wantsToTeleport);
	void execSetTeleport(bool wantsToTeleport, FVector destination);

#pragma endregion

	FVector lerpStartPos;
	float rewindLerpCurrentTime;
	float rewindLerpInterval;

	float rewindCooldown;
	float rewindCooldownDefault = 5.0f;

	float teleportCooldown;
	float teleportCooldownDefault = 5.0f;

	void CooldownTick(float DeltaSeconds);
	void RewindDataTick(float DeltaSeconds);

public:

	void StartTeleport();
	void StartRewind();

	float GetRewindCooldown() { return rewindCooldown; }
	float GetTeleportCooldown() { return teleportCooldown; }

	float GetRewindCooldownMax() { return rewindCooldownDefault; }
	float GetTeleportCooldownMax() { return teleportCooldownDefault; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Status)
		TEnumAsByte<ECustomMovementMode> ECustomMovementMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Custom")
		float DistanceCheckRange = 10000;

#pragma region State Setters

	UFUNCTION(BlueprintCallable)
		void SetRewind(bool wantsToRewind);
	UFUNCTION(BlueprintCallable)
		void SetTeleport(bool wantsToTeleport, FVector destination);

#pragma region RPCs

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable)
		void ServerSetRewindRPC(bool wantsToRewind);
	UFUNCTION(Client, Reliable, BlueprintCallable)
		void ClientSetRewindRPC(bool wantsToRewind);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable)
		void ServerSetTeleportRPC(bool wantsToTeleport, FVector destination);
	UFUNCTION(Client, Reliable, BlueprintCallable)
		void ClientSetTeleportRPC(bool wantsToTeleport, FVector destination);

#pragma endregion

#pragma endregion

#pragma region State Queries

	UFUNCTION(BlueprintCallable)
		bool IsRewinding();

#pragma endregion

#pragma region State Conditions

	bool CanRewind();
	bool CanTeleport();

#pragma endregion

#pragma region State Variables

	bool bWantsToRewind : 1;
	bool bWantsToTeleport : 1;

	TArray<float> rewindTimeStampStack;
	TArray<FVector> rewindLocationsStack;

	
	UPROPERTY(BlueprintReadOnly, Category = "Custom|State")
		FVector teleportDestination;

	UPROPERTY(BlueprintReadOnly, Category = "Custom|State")
		float angleOfAttack;

#pragma endregion

	FVector distanceCheckOrigin;
};

#pragma region Networking

/** FSavedMove_Character represents a saved move on the client that has been sent to the server and might need to be played back. */
class FSavedMove_ShooterCharacterMovement : public FSavedMove_Character
{

	friend class UShooterCharacterMovement;

public:
	typedef FSavedMove_Character Super;
	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* Character) override;

	bool savedWantsToRewind : 1;
	FVector savedRewindDestination;

	bool savedWantsToTeleport : 1;
	FVector savedTeleportDestination;
};

/** Get prediction data for a client game. Should not be used if not running as a client. Allocates the data on demand and can be overridden to allocate a custom override if desired. Result must be a FNetworkPredictionData_Client_Character. */
class FNetworkPredictionData_Client_ShooterCharacterMovement : public FNetworkPredictionData_Client_Character
{
public:
	FNetworkPredictionData_Client_ShooterCharacterMovement(const UCharacterMovementComponent& ClientMovement);
	typedef FNetworkPredictionData_Client_Character Super;
	virtual FSavedMovePtr AllocateNewMove() override;
};

#pragma endregion