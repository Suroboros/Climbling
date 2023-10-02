// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimblingCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ClimbingCharacterAnimInstance.h"
#include "GenericPlatform/GenericPlatformMath.h"


//////////////////////////////////////////////////////////////////////////
// AClimblingCharacter

AClimblingCharacter::AClimblingCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)

	bIsOnWall = false;
}

void AClimblingCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	MeshComponent = Cast<USkeletalMeshComponent>(GetComponentByClass(USkeletalMeshComponent::StaticClass()));
	check(MeshComponent);
	AnimInst = Cast<UClimbingCharacterAnimInstance>(MeshComponent->GetAnimInstance());
}

void AClimblingCharacter::DetectWall(FVector offset = FVector::ZeroVector, float moveScale = 0)
{
	check(MeshComponent);

	FHitResult hit;
	bool bHit = false;
	FVector lineTraceStart = MeshComponent->GetSocketLocation(FName("pelvis")) + offset;
	FVector lineTraceEnd = lineTraceStart + GetActorForwardVector() * 200.f;
	DrawDebugLine(GetWorld(), lineTraceStart, lineTraceEnd, FColor::Red, false, 2.f);
	bHit = GetWorld()->LineTraceSingleByChannel(hit, lineTraceStart, lineTraceEnd, ECollisionChannel::ECC_Visibility);
	if (!bHit)
	{
		return;
	}
	DrawDebugSphere(GetWorld(), hit.ImpactPoint, 5, 3, FColor::Blue, false, 2.f);

	lineTraceStart = MeshComponent->GetSocketLocation(FName("head")) + offset;
	lineTraceEnd = lineTraceStart + GetActorForwardVector() * 200.f;
	DrawDebugLine(GetWorld(), lineTraceStart, lineTraceEnd, FColor::Orange, false, 2.f);
	bHit = GetWorld()->LineTraceSingleByChannel(hit, lineTraceStart, lineTraceEnd, ECollisionChannel::ECC_Visibility);
	if (!bHit)
	{
		return;
	}
	DrawDebugSphere(GetWorld(), hit.ImpactPoint, 5, 3, FColor::Cyan, false, 2.f);

	if (!bIsOnWall)
	{
		FTimerHandle TimerHandle;
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindUFunction(this, FName("GrabWall"), hit);
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 0.2f, false);
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, FString::Printf(TEXT("%f %f %f"), offset.X, offset.Y, offset.Z));
		//GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, FString::Printf(TEXT("%f %f %f"), offset.GetSafeNormal().X, offset.GetSafeNormal().Y, offset.GetSafeNormal().Z));
		MoveOnWall(offset.GetSafeNormal().GetAbs(), moveScale);
	}
}

void AClimblingCharacter::GrabWall(FHitResult hit)
{
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
	check(AnimInst);
	bIsOnWall = true;
	AnimInst->SetOnWall(bIsOnWall);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->bOrientRotationToMovement = false;
	FLatentActionInfo latentActionInfo = FLatentActionInfo::FLatentActionInfo();
	latentActionInfo.UUID = __LINE__;
	latentActionInfo.CallbackTarget = this;
	UKismetSystemLibrary::MoveComponentTo(RootComponent, hit.Location + hit.Normal * 50.f, FRotator(GetActorRotation().Pitch,  hit.Normal.Rotation().Yaw - 180.f, GetActorRotation().Roll), false, false, 0.2, false, EMoveComponentAction::Move, latentActionInfo);
}

void AClimblingCharacter::PreMoveOnWall(FVector inputDirect, float moveScale)
{
	FHitResult hit;
	FVector moveDestination;
	FVector moveDirect;
	if (FGenericPlatformMath::Abs( inputDirect.X) > 0.9)
	{
		moveDirect = GetActorRightVector();
	}
	else if (FGenericPlatformMath::Abs(inputDirect.Y) > 0.9)
	{
		moveDirect = GetActorUpVector();
	}
	FVector capsuleTraceStart = GetActorLocation();
	FVector capsuleTraceEnd = capsuleTraceStart + moveDirect * moveScale * 50.f;
	GetCapsuleComponent()->GetCollisionShape();
	bool isHit = GetWorld()->SweepSingleByChannel(hit, capsuleTraceStart, capsuleTraceEnd, FQuat::Identity, ECollisionChannel::ECC_Visibility, GetCapsuleComponent()->GetCollisionShape());
	//DrawDebugCapsule(GetWorld(), capsuleTraceEnd, GetCapsuleComponent()->GetCollisionShape().GetCapsuleHalfHeight(), GetCapsuleComponent()->GetCollisionShape().GetCapsuleRadius(), FQuat::Identity, FColor::Green, false, 2.f);
	if (!isHit)
	{
		moveDestination = capsuleTraceEnd;
	}
	else
	{
		moveDestination = hit.Location;
	}
	
	DetectWall(moveDestination - GetActorLocation(), moveScale);
}

void AClimblingCharacter::MoveOnWall(FVector direction, float moveSpeed)
{
	AddMovementInput(direction, moveSpeed);
	//AnimInst->SetHorizonSpeedOnWall(GetMovementComponent()->Velocity.Dot(direction));
}

void AClimblingCharacter::Jump()
{
	Super::Jump();

	if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Walking)
	{
		DetectWall();
	}
}

void AClimblingCharacter::StopJumping()
{
	Super::StopJumping();

	if (bIsOnWall)
	{
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);
		GetCharacterMovement()->bOrientRotationToMovement = true;
		check(AnimInst);
		bIsOnWall = false;
		AnimInst->SetOnWall(bIsOnWall);
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AClimblingCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AClimblingCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AClimblingCharacter::Look);

	}

}

void AClimblingCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		//GEngine->AddOnScreenDebugMessage(-1, 3, FColor::Red, FString::Printf(TEXT("%f %f %f"), ForwardDirection.X, ForwardDirection.Y, ForwardDirection.Z));
		//GEngine->AddOnScreenDebugMessage(-1, 3, FColor::Green, FString::Printf(TEXT("%f %f %f"), RightDirection.X, RightDirection.Y, RightDirection.Z));
		if (bIsOnWall)
		{
			PreMoveOnWall(ForwardDirection, MovementVector.Y);
			PreMoveOnWall(RightDirection, MovementVector.X);
		}
		else
		{
			// add movement 
			AddMovementInput(ForwardDirection, MovementVector.Y);
			AddMovementInput(RightDirection, MovementVector.X);
		}
	}
}

void AClimblingCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}




