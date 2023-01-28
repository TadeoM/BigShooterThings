// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "MyShooterPickup_DeathAmmo.h"

AMyShooterPickup_DeathAmmo::AMyShooterPickup_DeathAmmo(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AmmoClips = 2;
	bCanRespawn = false;
	RespawnTime = 5.0f;
}