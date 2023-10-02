// Fill out your copyright notice in the Description page of Project Settings.


#include "ClimbingCharacterAnimInstance.h"

void UClimbingCharacterAnimInstance::SetOnWall(bool isOnWall)
{
	bIsOnWall = isOnWall;
}

void UClimbingCharacterAnimInstance::SetSpeedOnWall(FVector2D speed)
{
	SpeedOnWall = speed;
}
