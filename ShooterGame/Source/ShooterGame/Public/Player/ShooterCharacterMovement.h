// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Movement component meant for use with Pawns.
 */

#pragma once
#include "ShooterCharacterMovement.generated.h"

UCLASS()
class UShooterCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_UCLASS_BODY()

	virtual float GetMaxSpeed() const override;

	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
protected:
	bool bWantsToTeleport : 1;


	UPROPERTY(BlueprintReadOnly, Category = "Custom|State")
		FVector teleportDestination;
	void ProcessTeleport();
	void execSetTeleport(bool wantsToTeleport, FVector destination);

	UFUNCTION(BlueprintCallable)
		void SetTeleport(bool wantsToTeleport, FVector destination);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable)
		void ServerSetTeleportRPC(bool wantsToTeleport, FVector destination);
	UFUNCTION(Client, Reliable, BlueprintCallable)
		void ClientSetTeleportRPC(bool wantsToTeleport, FVector destination);

	bool CanTeleport();

private:
	UPROPERTY(EditAnywhere, Category = "Camera")
		UCameraComponent* cam;
};

/** FSavedMove_Character represents a saved move on the client that has been sent to the server and might need to be played back. */
class FSavedMove_JPGMovement : public FSavedMove_Character
{

	friend class UJPGMovementComponent;


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
class FNetworkPredictionData_Client_JPGMovement : public FNetworkPredictionData_Client_Character
{
public:
	FNetworkPredictionData_Client_JPGMovement(const UCharacterMovementComponent& ClientMovement);
	typedef FNetworkPredictionData_Client_Character Super;
	virtual FSavedMovePtr AllocateNewMove() override;
};

