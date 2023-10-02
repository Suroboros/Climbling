// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ClimbingCharacterAnimInstance.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, HideCategories = AnimInstance, BlueprintType)
class CLIMBLING_API UClimbingCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Climb, meta = (AllowPrivateAccess = "true"))
		bool bIsOnWall;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Climb, meta = (AllowPrivateAccess = "true"))
		FVector2D SpeedOnWall;

public:
	void SetOnWall(bool isOnWall);

	void SetSpeedOnWall(FVector2D speed);
};
