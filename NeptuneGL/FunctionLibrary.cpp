// ===============================================================
// NeptuneGL Utility Function Library - Implementation (Cleaned)
// ===============================================================
//
// Focused on unique, high-value, custom functions.
// Removed redundant thin wrappers and standard BP-accessible functionality.
//
// Copyright (c) 2023-2026 NeptuneGL. All rights reserved.

// ReSharper disable GrammarMistakeInComment - disables Possible typo: you repeated a word.
// ReSharper disable CppUEBlueprintCallableFunctionUnused - disables Function 'name' is never used in Blueprint or C++ code
// ReSharper disable CommentTypo - disables Typo: In word 'word'
// ReSharper disable IdentifierTypo - disables Typo: In identifier 'IdentifierName'
// can consider using IdentifierTypo or StringLiteralTypo if other errata appear

#include "FunctionLibrary.h"  // Header file with class definition and function declarations
#include "Kismet/GameplayStatics.h"  // Utility functions for game logic (player/actor queries, spawning)
#include "Kismet/KismetMathLibrary.h"  // Advanced math helpers (Lerp, Clamp, vector operations beyond basic FMath)
#include "GameFramework/PlayerController.h"  // PlayerController class to access player input and camera data
#include "GameFramework/Pawn.h"  // Pawn base class for retrieving player pawns in the world
#include "Engine/World.h"  // World class for line traces and actor enumeration
#include "Engine/Engine.h"  // Engine core for GEngine access and debug drawing
#include "DrawDebugHelpers.h"  // Debug visualization functions for drawing shapes/vectors
#include "Components/PrimitiveComponent.h"  // PrimitiveComponent base class for bobbing physics queries
#include "EngineUtils.h"  // TActorIterator for iterating through actors in the world
#include "CollisionQueryParams.h"  // Collision query setup for line traces and spatial queries
#include "Engine/OverlapResult.h"  // OverlapResult struct for multi-overlap trace returns

#include "Components/StaticMeshComponent.h"  // StaticMeshComponent for ship mesh velocity and VFX updates
#include "Particles/ParticleSystemComponent.h"  // Cascade particle system for heatblur distortion effects
#include "NiagaraComponent.h"  // Niagara VFX system for exhaust emission and dynamic effects
#include "Engine/GameViewportClient.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/SWindow.h"


#pragma region PLAYER_PAWN_CUSTOM_HELPERS

// Static map to track the last update time for each ship mesh — enables throttling VFX updates to 0.01s intervals
TMap<TObjectKey<UStaticMeshComponent>, double> UFunctionLibrary::LastEngineVFXUpdateTime;


void UFunctionLibrary::UpdateEngineVFX(
    UStaticMeshComponent*                ShipMesh,
    UParticleSystemComponent*            LeftHeatblurParticles,
    UParticleSystemComponent*            RightHeatblurParticles,
    UNiagaraComponent*                   NSMateriaLeft,
    UNiagaraComponent*                   NSMateriaRight,
    const TArray<UMaterialInstanceDynamic*>& LeftExhaustDynamicElements,
    const TArray<UMaterialInstanceDynamic*>& RightExhaustDynamicElements,
    float                                MaxSpeed)
{
    if (!ShipMesh) return;

    // === THROTTLE UPDATES TO 0.01s INTERVALS ===
    // This prevents excessive VFX updates every frame, improving performance
    constexpr double UpdateInterval = 0.01;
    const double CurrentTime = FPlatformTime::Seconds();

    // Create a key for this specific mesh component
    TObjectKey<UStaticMeshComponent> Key(ShipMesh);
    double* LastUpdate = LastEngineVFXUpdateTime.Find(Key);

    if (LastUpdate)
    {
        // If we've seen this mesh before, check if enough time has passed
        if (CurrentTime - *LastUpdate < UpdateInterval)
        {
            return; // Too soon — skip this frame
        }
        // Update the timestamp for next check
        *LastUpdate = CurrentTime;
    }
    else
    {
        // First time seeing this ship — register it with current time
        LastEngineVFXUpdateTime.Add(Key, CurrentTime);

        // Clean up entries for destroyed/invalid mesh components to prevent memory leaks
        for (auto It = LastEngineVFXUpdateTime.CreateIterator(); It; ++It)
        {
            if (!IsValid(It.Key().ResolveObjectPtr()))
                It.RemoveCurrent();
        }
    }
    // === END THROTTLE ===

    // === CALCULATE SPEED-BASED PARAMETERS ===
    // Normalize ship velocity (0.0 at rest, 1.0 at MaxSpeed or higher)
    const float SpeedAlpha = FMath::Clamp(
        ShipMesh->GetComponentVelocity().Size() / FMath::Max(MaxSpeed, 1.0f),
        0.0f, 1.0f
    );

    // PARTICLE SYSTEM UPDATES: Scale heatblur opacity with speed (0.6 to 1.0)
    const float Opacity = FMath::Lerp(0.6f, 1.0f, SpeedAlpha);
    if (LeftHeatblurParticles)  LeftHeatblurParticles->SetFloatParameter(FName("opacity"), Opacity);
    if (RightHeatblurParticles) RightHeatblurParticles->SetFloatParameter(FName("opacity"), Opacity);

    // NIAGARA UPDATES: Scale exhaust emission rate with speed (0.0 to 5.0)
    const float ExhaustRate = FMath::Lerp(0.0f, 5.0f, SpeedAlpha);

    if (NSMateriaLeft)  NSMateriaLeft->SetVariableFloat(FName("ExhaustRate"), ExhaustRate);
    if (NSMateriaRight) NSMateriaRight->SetVariableFloat(FName("ExhaustRate"), ExhaustRate);

    // DYNAMIC MATERIAL UPDATES: Apply exhaust rate to all dynamic material instances
    for (UMaterialInstanceDynamic* MID : LeftExhaustDynamicElements)
    {
        if (IsValid(MID))
            MID->SetScalarParameterValue(FName("multiply"), ExhaustRate);
    }

    for (UMaterialInstanceDynamic* MID : RightExhaustDynamicElements)
    {
        if (IsValid(MID))
            MID->SetScalarParameterValue(FName("multiply"), ExhaustRate);
    }

    const FVector ScaleColor(
        FMath::Lerp(0.0f, 3.0f, SpeedAlpha),
        FMath::Lerp(0.0f, 1.3f, SpeedAlpha),
        FMath::Lerp(0.0f, 1.3f, SpeedAlpha)
    );

    if (NSMateriaLeft)  NSMateriaLeft->SetVariableVec3(FName("ScaleColor"), ScaleColor);
    if (NSMateriaRight) NSMateriaRight->SetVariableVec3(FName("ScaleColor"), ScaleColor);
}

// Static map storage - lives for the duration of the game session.
TMap<TObjectKey<USceneComponent>, UFunctionLibrary::BobState> UFunctionLibrary::BobStateMap;

void UFunctionLibrary::Bob(
    UPrimitiveComponent* Component,   // was UStaticMeshComponent*. capsules, static meshes or any physics based primitive component can be passed
    float DeltaTime,                      // pass in Tick's DeltaTime for frame rate independence
    float BobSpeed,                       // speed of bob (a good value is 5)
    float BobAmount,                      // amount of bob (good value is 50)
    float IdleDampStrength)               // damp stregth (good value is 6)
{
    if (!Component || DeltaTime <= 0.0f || BobSpeed <= 0.0f || BobAmount <= 0.0f)
        return;

    // Cap delta time so a hitch frame (e.g. loading stall) can't kick the
    // spring-damper into an unstable state with a huge time step.
    DeltaTime = FMath::Clamp(DeltaTime, 0.0f, 0.05f);

    // Look up persistent state for this mesh.
    // O(1) hash lookup - effectively free per frame.
    TObjectKey<USceneComponent> Key(Component);
    BobState* ExistingState = BobStateMap.Find(Key);

    if (!ExistingState)
    {
        // This mesh hasn't been seen before (new ship, or first frame).
        // Before adding it, sweep the map for any stale pointers left behind
        // by previously destroyed ships. We only do this on registration,
        // never on the hot tick path, so the cost is paid once per ship lifetime.
        for (auto It = BobStateMap.CreateIterator(); It; ++It)
        {
            if (!IsValid(It.Key().ResolveObjectPtr()))
                It.RemoveCurrent();
        }

        // Register this mesh with a fresh default state.
        ExistingState = &BobStateMap.Add(Key, BobState());
    }

    BobState& State = *ExistingState;

    // Advance the sine wave phase by the elapsed time.
    // Wrapping with Fmod keeps the value in [0, 2PI] forever,
    // preventing float precision drift over long sessions.
    State.BobPhase = FMath::Fmod(State.BobPhase + BobSpeed * DeltaTime, UE_TWO_PI);

    // Derive damping tightness from current physics speed.
    // When idle (speed ~0): SmoothTime = 1/IdleDampStrength (very tight, resists drift).
    // When moving fast (speed >= 150): SmoothTime = 0.22 (looser, floatier feel).
    float CurrentSpeed = Component->GetPhysicsLinearVelocity().Size();
    float SpeedAlpha   = FMath::Clamp(CurrentSpeed / 150.0f, 0.0f, 1.0f);
    double SmoothTime   = FMath::Lerp(1.0f / FMath::Max(IdleDampStrength, 0.001f), 0.22f, SpeedAlpha); // double

    // Compute the target bob velocity for this frame:
    // a sine wave scaled by BobAmount, directed along the mesh's local up axis.
    // Using the mesh's up vector means the bob always feels "vertical" relative
    // to the ship regardless of its orientation in space.
    FVector LocalUp           = Component->GetUpVector();
    FVector TargetBobVelocity = LocalUp * (FMath::Sin(State.BobPhase) * BobAmount);

    // Smoothly interpolate toward the target bob velocity using a spring-damper.
    // SmoothDampTracker is the internal derivative - it must persist between frames
    // so the damper has memory of its own momentum and doesn't snap or overshoot.
    FVector SmoothedVelocity = SmoothDampVector(
        State.BobVelocity,       // where we currently are
        TargetBobVelocity,       // where we want to be
        State.SmoothDampTracker, // spring-damper internal momentum (persists)
        SmoothTime,              // now double for more precision in the math
        static_cast<double>(DeltaTime), // double precision for the SmoothDampVector function
        400.0f                   // max velocity cap - prevents runaway on large deltas
    );

    // KEY: only inject the CHANGE in bob velocity, not the full value.
    // SetPhysicsLinearVelocity with bAddToCurrent=true is additive, so if we
    // injected the full SmoothedVelocity every frame it would stack up and cause
    // the ship to drift continuously. By injecting only the delta, we're effectively
    // saying "adjust by this much" rather than "add this much on top of everything".
    FVector VelocityDelta = SmoothedVelocity - State.BobVelocity;
    Component->SetPhysicsLinearVelocity(VelocityDelta, true);

    // Store this frame's velocity so next frame can compute the correct delta.
    State.BobVelocity = SmoothedVelocity;
    
    //The short version: the map remembers the sine phase and spring-damper state so the 
    //bob is continuous and smooth. The delta injection is what keeps it from drifting.
}

bool UFunctionLibrary::IsPlayerMoving(const UObject* WorldContextObject, float Threshold, int32 PlayerIndex)
{
    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(WorldContextObject, PlayerIndex))
    {
        if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(Pawn->GetRootComponent()))
        {
            return RootComp->GetPhysicsLinearVelocity().SizeSquared() > (Threshold * Threshold);
        }
        return Pawn->GetVelocity().SizeSquared() > (Threshold * Threshold);
    }
    return false;
}

FVector UFunctionLibrary::GetPlayerInputDirection(const UObject* WorldContextObject, int32 PlayerIndex)
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, PlayerIndex);
    if (!PC) return FVector::ZeroVector;

    APawn* Pawn = PC->GetPawn();
    if (!Pawn) return FVector::ZeroVector;

    FRotator ControlRotation = PC->GetControlRotation();
    FVector Forward = FRotationMatrix(ControlRotation).GetScaledAxis(EAxis::X);
    FVector Right = FRotationMatrix(ControlRotation).GetScaledAxis(EAxis::Y);

    float ForwardInput = PC->GetInputAxisValue("MoveForward");
    float RightInput = PC->GetInputAxisValue("MoveRight");

    FVector InputDir = (Forward * ForwardInput) + (Right * RightInput);
    return InputDir.GetSafeNormal();
}

bool UFunctionLibrary::IsPositionInFieldOfView(const UObject* WorldContextObject, const FVector& WorldPosition, float FOVAngle, int32 PlayerIndex)
{
    FVector PlayerLoc = UGameplayStatics::GetPlayerPawn(WorldContextObject, PlayerIndex) ? 
                        UGameplayStatics::GetPlayerPawn(WorldContextObject, PlayerIndex)->GetActorLocation() : FVector::ZeroVector;
    if (FVector::DistSquared(PlayerLoc, WorldPosition) < KINDA_SMALL_NUMBER) return true;

    FVector ViewDir = GetPlayerViewDirection(WorldContextObject, PlayerIndex);
    FVector ToTarget = (WorldPosition - PlayerLoc).GetSafeNormal();

    float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(ViewDir, ToTarget), -1.0f, 1.0f)) * (180.0f / UE_PI);
    return Angle <= FOVAngle;
}

#pragma endregion


#pragma region CAMERA_LINE_TRACE

void UFunctionLibrary::LineTraceVectorsFromPlayerCamera(const UObject* WorldContextObject, float Distance, FVector& OutStart, FVector& OutEnd, int32 PlayerIndex)
{
    if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(WorldContextObject, PlayerIndex))
    {
        OutStart = CamMgr->GetCameraLocation();
        OutEnd = OutStart + CamMgr->GetCameraRotation().Vector() * Distance;
    }
    else
    {
        OutStart = FVector::ZeroVector;
        OutEnd = FVector::ZeroVector;
    }
}

bool UFunctionLibrary::LineTraceFromCamera(const UObject* WorldContextObject, float Distance, FHitResult& OutHit, ECollisionChannel Channel, int32 PlayerIndex)
{
    FVector Start, End;
    LineTraceVectorsFromPlayerCamera(WorldContextObject, Distance, Start, End, PlayerIndex);

    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World) return false;

    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;
    return World->LineTraceSingleByChannel(OutHit, Start, End, Channel, QueryParams);
}

bool UFunctionLibrary::LineTraceFromPlayerEyes(const UObject* WorldContextObject, float Distance, FHitResult& HitResult, ECollisionChannel CollisionChannel, int32 PlayerIndex)
{
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(WorldContextObject, PlayerIndex);
    if (!Pawn) return false;

    FVector EyeLocation;
    FRotator EyeRotation;
    Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);

    FVector End = EyeLocation + EyeRotation.Vector() * Distance;

    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World) return false;

    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;
    return World->LineTraceSingleByChannel(HitResult, EyeLocation, End, CollisionChannel, QueryParams);
}

bool UFunctionLibrary::MultiLineTraceFromCamera(const UObject* WorldContextObject, float Distance, TArray<FHitResult>& HitResults, ECollisionChannel CollisionChannel, int32 PlayerIndex)
{
    FVector Start, End;
    LineTraceVectorsFromPlayerCamera(WorldContextObject, Distance, Start, End, PlayerIndex);

    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World) return false;

    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;
    return World->LineTraceMultiByChannel(HitResults, Start, End, CollisionChannel, QueryParams);
}

FVector UFunctionLibrary::GetPlayerViewDirection(const UObject* WorldContextObject, int32 PlayerIndex)
{
    if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(WorldContextObject, PlayerIndex))
    {
        return CamMgr->GetCameraRotation().Vector();
    }
    return FVector::ForwardVector;
}

FVector UFunctionLibrary::GetPlayerCameraLocation(const UObject* WorldContextObject, int32 PlayerIndex)
{
    if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(WorldContextObject, PlayerIndex))
    {
        return CamMgr->GetCameraLocation();
    }
    return FVector::ZeroVector;
}

float UFunctionLibrary::GetPlayerCameraFOV(const UObject* WorldContextObject, int32 PlayerIndex)
{
    if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(WorldContextObject, PlayerIndex))
    {
        return CamMgr->GetFOVAngle();
    }
    return 90.0f;
}

float UFunctionLibrary::GetAspectRatio(const UObject* WorldContextObject, int32 PlayerIndex)
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, PlayerIndex);
    if (!PC) return 1.777778f;

    int32 X, Y;
    PC->GetViewportSize(X, Y);
    return (Y > 0) ? static_cast<float>(X) / static_cast<float>(Y) : 1.777778f;
}

float UFunctionLibrary::GetLookAtAngle2D(const UObject* WorldContextObject, const FVector& WorldPosition, int32 PlayerIndex)
{
    FVector PlayerLoc = UGameplayStatics::GetPlayerPawn(WorldContextObject, PlayerIndex) ? 
                        UGameplayStatics::GetPlayerPawn(WorldContextObject, PlayerIndex)->GetActorLocation() : FVector::ZeroVector;
    FVector ViewDir = GetPlayerViewDirection(WorldContextObject, PlayerIndex);

    FVector ToTarget = WorldPosition - PlayerLoc;
    ToTarget.Z = 0.0f;
    ViewDir.Z = 0.0f;

    if (ToTarget.IsNearlyZero() || ViewDir.IsNearlyZero()) return 0.0f;

    ToTarget.Normalize();
    ViewDir.Normalize();

    float Dot = FVector::DotProduct(ViewDir, ToTarget);
    float Cross = FVector::CrossProduct(ViewDir, ToTarget).Z;
    float Angle = FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)) * (180.0f / UE_PI);

    return (Cross >= 0.0f) ? Angle : -Angle;
}

#pragma endregion


#pragma region SCREEN_SPACE

bool UFunctionLibrary::WorldToScreenPosition(const UObject* WorldContextObject, const FVector& WorldPosition, FVector2D& ScreenPosition, int32 PlayerIndex)
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, PlayerIndex);
    if (!PC) return false;
    return PC->ProjectWorldLocationToScreen(WorldPosition, ScreenPosition);
}

bool UFunctionLibrary::IsPositionOnScreen(const UObject* WorldContextObject, const FVector& WorldPosition, int32 PlayerIndex)
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, PlayerIndex);
    if (!PC) return false;

    FVector2D ScreenPos;
    if (!PC->ProjectWorldLocationToScreen(WorldPosition, ScreenPos)) return false;

    int32 ViewportX, ViewportY;
    PC->GetViewportSize(ViewportX, ViewportY);

    return ScreenPos.X >= 0 && ScreenPos.X <= ViewportX && ScreenPos.Y >= 0 && ScreenPos.Y <= ViewportY;
}

#pragma endregion


#pragma region MATH_GEOMETRY

double UFunctionLibrary::SmoothDamp(double Current, double Target, double& CurrentVelocity, double SmoothTime, double DeltaTime, double MaxSpeed)
{
    if (SmoothTime <= 0.0) return Target;

    SmoothTime = FMath::Max(0.0001, SmoothTime);
    double Omega = 2.0 / SmoothTime;

    double x = Omega * DeltaTime;
    double exp = 1.0 / (1.0 + x + 0.48 * x * x + 0.235 * x * x * x);

    double change = Current - Target;
    double originalTo = Target;

    double maxChange = MaxSpeed * SmoothTime;
    change = FMath::Clamp(change, -maxChange, maxChange);

    Target = Current - change;

    double temp = (CurrentVelocity + Omega * change) * DeltaTime;
    CurrentVelocity = (CurrentVelocity - Omega * temp) * exp;
    double output = Target + (change + temp) * exp;

    if ((originalTo - Current > 0.0) == (output > originalTo))
    {
        output = originalTo;
        CurrentVelocity = (output - originalTo) / DeltaTime;
    }

    return output;
}

FVector UFunctionLibrary::SmoothDampVector(const FVector& Current, const FVector& Target, FVector& CurrentVelocity, double SmoothTime, double DeltaTime, double MaxSpeed)
{
    FVector Result;
    Result.X = SmoothDamp(Current.X, Target.X, CurrentVelocity.X, SmoothTime, DeltaTime, MaxSpeed);
    Result.Y = SmoothDamp(Current.Y, Target.Y, CurrentVelocity.Y, SmoothTime, DeltaTime, MaxSpeed);
    Result.Z = SmoothDamp(Current.Z, Target.Z, CurrentVelocity.Z, SmoothTime, DeltaTime, MaxSpeed);
    return Result;
}

FVector UFunctionLibrary::RotateVectorTowards(const FVector& Current, const FVector& Target, float MaxDegreesPerSecond, float DeltaTime)
{
    if (Current.IsNearlyZero() || Target.IsNearlyZero()) return Current;

    FVector CurrentDir = Current.GetSafeNormal();
    FVector TargetDir = Target.GetSafeNormal();

    float AngleDiff = FMath::Acos(FMath::Clamp(FVector::DotProduct(CurrentDir, TargetDir), -1.0f, 1.0f));
    float MaxAngleThisFrame = FMath::DegreesToRadians(MaxDegreesPerSecond) * DeltaTime;

    if (AngleDiff <= MaxAngleThisFrame)
    {
        return TargetDir * Current.Size();
    }

    FVector Axis = FVector::CrossProduct(CurrentDir, TargetDir).GetSafeNormal();
    FQuat Rotation = FQuat(Axis, MaxAngleThisFrame);
    return Rotation.RotateVector(CurrentDir) * Current.Size();
}

FVector UFunctionLibrary::GetDirectionTo(const FVector& From, const FVector& To)
{
    return (To - From).GetSafeNormal();
}

float UFunctionLibrary::GetAngleBetweenVectors(const FVector& A, const FVector& B)
{
    FVector ANorm = A.GetSafeNormal();
    FVector BNorm = B.GetSafeNormal();
    float Dot = FVector::DotProduct(ANorm, BNorm);
    return FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)) * (180.0f / UE_PI);
}

float UFunctionLibrary::GetSignedAngleBetweenVectors(const FVector& A, const FVector& B, const FVector& Axis)
{
    float UnsignedAngle = GetAngleBetweenVectors(A, B);
    FVector Cross = FVector::CrossProduct(A, B);
    float Sign = FVector::DotProduct(Cross, Axis);
    return (Sign >= 0.0f) ? UnsignedAngle : -UnsignedAngle;
}

FVector UFunctionLibrary::GetClosestPointOnLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point)
{
    FVector LineDir = LineEnd - LineStart;
    float LineLength = LineDir.Size();
    if (LineLength < KINDA_SMALL_NUMBER) return LineStart;

    LineDir /= LineLength;
    float Projection = FVector::DotProduct(Point - LineStart, LineDir);
    Projection = FMath::Clamp(Projection, 0.0f, LineLength);
    return LineStart + LineDir * Projection;
}

float UFunctionLibrary::GetDistanceToLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point)
{
    FVector Closest = GetClosestPointOnLine(LineStart, LineEnd, Point);
    return FVector::Dist(Point, Closest);
}

FVector UFunctionLibrary::GetRandomPointInSphere(const FVector& Origin, float Radius)
{
    return Origin + FMath::VRand() * FMath::FRandRange(0.0f, Radius);
}

FVector UFunctionLibrary::GetRandomPointOnSphere(const FVector& Origin, float Radius)
{
    return Origin + FMath::VRand() * Radius;
}

FVector UFunctionLibrary::GetRandomPointInBox(const FVector& Center, const FVector& HalfExtents)
{
    return Center + FVector(
        FMath::FRandRange(-HalfExtents.X, HalfExtents.X),
        FMath::FRandRange(-HalfExtents.Y, HalfExtents.Y),
        FMath::FRandRange(-HalfExtents.Z, HalfExtents.Z)
    );
}

#pragma endregion 


#pragma region PROJECTILE_COMBAT_MATH

FVector UFunctionLibrary::GetLeadTargetPosition(const FVector& ShooterLocation, const FVector& TargetLocation, const FVector& TargetVelocity, float ProjectileSpeed, float& OutTimeToImpact)
{
    FVector ToTarget = TargetLocation - ShooterLocation;
    float DistSq = ToTarget.SizeSquared();

    if (DistSq < KINDA_SMALL_NUMBER || ProjectileSpeed <= 0.0f)
    {
        OutTimeToImpact = 0.0f;
        return TargetLocation;
    }

    float a = FVector::DotProduct(TargetVelocity, TargetVelocity) - FMath::Square(ProjectileSpeed);
    float b = 2.0f * FVector::DotProduct(ToTarget, TargetVelocity);
    float c = DistSq;

    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f || FMath::IsNearlyZero(a))
    {
        // No real solution or linear case - fall back to simple time-to-impact
        OutTimeToImpact = FMath::Sqrt(DistSq) / ProjectileSpeed;
        return TargetLocation + TargetVelocity * OutTimeToImpact;
    }

    float sqrtDisc = FMath::Sqrt(discriminant);
    float t1 = (-b - sqrtDisc) / (2.0f * a);
    float t2 = (-b + sqrtDisc) / (2.0f * a);

    float t = (t1 > 0.0f) ? t1 : t2;
    if (t < 0.0f) t = FMath::Max(t1, t2); // both negative? use the least negative
    if (t < 0.0f) t = 0.0f;

    OutTimeToImpact = t;
    return TargetLocation + TargetVelocity * t;
}

#pragma endregion


#pragma region GAMEPLAY_UTIL

void UFunctionLibrary::SpawnProjectileAtPlayer(const UObject* WorldContextObject, TSubclassOf<AActor> ProjectileClass, int32 PlayerIndex)
{
    if (!WorldContextObject || !ProjectileClass) return;

    APawn* Pawn = UGameplayStatics::GetPlayerPawn(WorldContextObject, PlayerIndex);
    if (!Pawn) return;

    FVector SpawnLocation = Pawn->GetActorLocation();
    APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(WorldContextObject, PlayerIndex);
    FRotator SpawnRotation = CamMgr ? CamMgr->GetCameraRotation() : Pawn->GetActorRotation();

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    SpawnParams.Owner = Pawn;
    SpawnParams.Instigator = Pawn;

    World->SpawnActor<AActor>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
}

AActor* UFunctionLibrary::SpawnActorAtLocation(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FVector& Location, const FRotator& Rotation, AActor* Owner, APawn* Instigator)
{
    if (!WorldContextObject || !ActorClass) return nullptr;

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    SpawnParams.Owner = Owner;
    SpawnParams.Instigator = Instigator;

    return World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
}

AActor* UFunctionLibrary::GetClosestActorOfClass(const UObject* WorldContextObject, const FVector& Position, TSubclassOf<AActor> ActorClass)
{
    if (!WorldContextObject || !ActorClass) return nullptr;

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;

    AActor* ClosestActor = nullptr;
    float ClosestDistSq = MAX_FLT;

    for (TActorIterator<AActor> It(World, ActorClass); It; ++It)
    {
        float DistSq = FVector::DistSquared(Position, It->GetActorLocation());
        if (DistSq < ClosestDistSq)
        {
            ClosestDistSq = DistSq;
            ClosestActor = *It;
        }
    }
    return ClosestActor;
}

void UFunctionLibrary::GetActorsInCone(const UObject* WorldContextObject, const FVector& Origin, const FVector& Direction, float ConeAngle, float Range, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors)
{
    OutActors.Empty();
    TArray<AActor*> Candidates;
    
    // Use overlap for candidates (more efficient than full actor iteration)
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World) return;

    TArray<FOverlapResult> Overlaps;
    FCollisionShape Shape = FCollisionShape::MakeSphere(Range);
    World->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_WorldDynamic, Shape);

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Actor = Overlap.GetActor();
        if (Actor && Actor->IsA(ActorClass))
        {
            Candidates.AddUnique(Actor);
        }
    }

    FVector NormalizedDir = Direction.GetSafeNormal();
    float ConeAngleRad = ConeAngle * (UE_PI / 180.0f);
    float CosHalfAngle = FMath::Cos(ConeAngleRad * 0.5f);

    for (AActor* Actor : Candidates)
    {
        FVector ToActor = (Actor->GetActorLocation() - Origin).GetSafeNormal();
        if (FVector::DotProduct(NormalizedDir, ToActor) >= CosHalfAngle)
        {
            OutActors.Add(Actor);
        }
    }
}

bool UFunctionLibrary::IsActorVisible(const UObject* WorldContextObject, AActor* TargetActor, ECollisionChannel CollisionChannel, int32 PlayerIndex)
{
    if (!WorldContextObject || !TargetActor) return false;

    APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(WorldContextObject, PlayerIndex);
    if (!CamMgr) return false;

    FVector CameraLocation = CamMgr->GetCameraLocation();
    FVector TargetLocation = TargetActor->GetActorLocation();

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return false;

    FHitResult Hit;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(TargetActor);

    bool bBlocked = World->LineTraceSingleByChannel(Hit, CameraLocation, TargetLocation, CollisionChannel, QueryParams);
    return !bBlocked;
}

int32 UFunctionLibrary::DestroyAllActorsOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass)
{
    if (!WorldContextObject || !ActorClass) return 0;

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return 0;

    int32 Count = 0;
    for (TActorIterator<AActor> It(World, ActorClass); It; ++It)
    {
        It->Destroy();
        Count++;
    }
    return Count;
}

#pragma endregion


#pragma region MULTIPLAYER_HELPERS

TArray<APlayerController*> UFunctionLibrary::GetAllPlayerControllers(const UObject* WorldContextObject)
{
    TArray<APlayerController*> Controllers;
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World) return Controllers;

    for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        if (APlayerController* PC = Iterator->Get())
        {
            Controllers.Add(PC);
        }
    }
    return Controllers;
}

int32 UFunctionLibrary::GetPlayerCount(const UObject* WorldContextObject)
{
    return GetAllPlayerControllers(WorldContextObject).Num();
}

#pragma endregion


#pragma region DEVELOPMENT_DEBUG

bool UFunctionLibrary::IsWithEditor()
{
#if WITH_EDITOR
    return GIsEditor;
#else
    return false;
#endif
}

FString UFunctionLibrary::GetObjectNameSafe(const AActor* Actor)
{
    if (!Actor) return TEXT("None");
    return Actor->GetName();
}

FString UFunctionLibrary::ConcatTransformToString(const FTransform& Transform)
{
    FVector Loc = Transform.GetLocation();
    FRotator Rot = Transform.GetRotation().Rotator();
    FVector Scale = Transform.GetScale3D();

    return FString::Printf(TEXT("Loc(%.1f, %.1f, %.1f) Rot(%.1f, %.1f, %.1f) Scale(%.1f, %.1f, %.1f)"),
        Loc.X, Loc.Y, Loc.Z,
        Rot.Pitch, Rot.Yaw, Rot.Roll,
        Scale.X, Scale.Y, Scale.Z);
}

void UFunctionLibrary::DrawDebugArrowPersistent(const UObject* WorldContextObject, const FVector& Start, const FVector& End, float Thickness, FLinearColor Color, float Duration)
{
    if (UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
    {
        DrawDebugDirectionalArrow(World, Start, End, 20.0f, Color.ToFColor(true), false, Duration, 0, Thickness);
    }
}

void UFunctionLibrary::FlushPersistentDebugLines(const UObject* WorldContextObject)
{
    if (UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
    {
        FlushPersistentDebugLines(World);
    }
}

ECustomNetMode UFunctionLibrary::GetCurrentNetMode(const UObject* WorldContextObject)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World) return ECustomNetMode::StandaloneWindow;

    ENetMode EngineNetMode = GEngine->GetNetMode(World);
    if (EngineNetMode == ENetMode::NM_DedicatedServer) return ECustomNetMode::DedicatedServer;
    if (EngineNetMode == ENetMode::NM_ListenServer)    return ECustomNetMode::ListenServer;

    if (!GIsEditor)
    {
        if (EngineNetMode == ENetMode::NM_Client) return ECustomNetMode::Client;
        return ECustomNetMode::StandaloneWindow;
    }

    if (World->WorldType == EWorldType::PIE)
    {
        if (GEngine->GameViewport)
        {
            TSharedPtr<SWindow> Win = GEngine->GameViewport->GetWindow();
            if (Win.IsValid())
            {
                const FString Title = Win->GetTitle().ToString();
                // Embedded PIE reuses the main editor window — title contains "Unreal Editor"
                // New window PIE gets its own window — title contains "Preview"
                if (Title.Contains(TEXT("Preview")))
                {
                    return ECustomNetMode::PIENewWindow;
                }
            }
        }
        return ECustomNetMode::PIEViewport;
    }

    if (EngineNetMode == ENetMode::NM_Client) return ECustomNetMode::Client;
    return ECustomNetMode::StandaloneWindow;
}

#pragma endregion

