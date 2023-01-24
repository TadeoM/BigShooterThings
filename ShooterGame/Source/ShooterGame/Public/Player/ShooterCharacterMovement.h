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

UCLASS()
class UShooterCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_UCLASS_BODY()

		UShooterCharacterMovement();

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
	UPROPERTY(EditAnywhere, Category = "Camera")
		UCameraComponent* cam;

	float GetCurrentTeleportCooldown();
	float GetTeleportCooldownDefault();

	float fTeleportCurrentCooldown;
	float fTeleportCooldownDefault = 1.0f;

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

/** FSavedMove_Character represents a saved move on the client that has been sent to the server and might need to be played back. */
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

