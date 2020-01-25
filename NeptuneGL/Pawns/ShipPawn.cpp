// ===============================================================
// AShipPawn - Implementation
// ===============================================================
//
// VaryEngine VFX logic (BP_UpdateEngineVFX) and weapon system
// (FireLaserAll, DepleteEnergyLaser). All component refs and
// tuning are passed as node inputs. Throttle is per-actor.
//
// Copyright (c) 2023-2026 NeptuneGL. All rights reserved.

#include "ShipPawn.h"
#include "GenericPlatform/GenericPlatformTime.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/UnrealMathUtility.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Sound/SoundBase.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

// ================================================================
// Constructor
// ================================================================

AShipPawn::AShipPawn()
{
    PrimaryActorTick.bCanEverTick = false;
}

// ================================================================
// VaryEngine — BP_UpdateEngineVFX
// ================================================================

void AShipPawn::BP_UpdateEngineVFX(
    UStaticMeshComponent*                          ShipMesh,
    UParticleSystemComponent*                      LeftHeatblurParticles,
    UParticleSystemComponent*                      RightHeatblurParticles,
    UNiagaraComponent*                             NSMateriaLeft,
    UNiagaraComponent*                             NSMateriaRight,
    UNiagaraComponent*                             NSRocketLeft,
    UNiagaraComponent*                             NSRocketRight,
    const TArray<UMaterialInstanceDynamic*>&       LeftExhaustDynElements,
    const TArray<UMaterialInstanceDynamic*>&       RightExhaustDynElements,
    float                                          MaxSpeed)
{
    if (!ShipMesh)
    {
        return;
    }

    // ---- throttle to 0.01s intervals (per-actor, no static map) ----
    constexpr double UpdateInterval = 0.01;
    const double CurrentTime = FPlatformTime::Seconds();

    if (CurrentTime - LastVFXUpdateTime < UpdateInterval)
    {
        return;
    }
    LastVFXUpdateTime = CurrentTime;

    // ---- calculate speed-based parameters ----
    const float SpeedAlpha = FMath::Clamp(
        ShipMesh->GetComponentVelocity().Size() / FMath::Max(MaxSpeed, 1.0f),
        0.0f, 1.0f
    );

    // Heatblur opacity: Lerp(0.6, 1.0, alpha)
    const float Opacity = FMath::Lerp(0.6f, 1.0f, SpeedAlpha);
    if (LeftHeatblurParticles)  LeftHeatblurParticles->SetFloatParameter(FName("opacity"), Opacity);
    if (RightHeatblurParticles) RightHeatblurParticles->SetFloatParameter(FName("opacity"), Opacity);

    // Niagara exhaust rate: Lerp(0.0, 5.0, alpha)
    const float ExhaustRate = FMath::Lerp(0.0f, 5.0f, SpeedAlpha);
    if (NSMateriaLeft)  NSMateriaLeft->SetVariableFloat(FName("ExhaustRate"), ExhaustRate);
    if (NSMateriaRight) NSMateriaRight->SetVariableFloat(FName("ExhaustRate"), ExhaustRate);

    // Dynamic material multiply
    for (UMaterialInstanceDynamic* MID : LeftExhaustDynElements)
    {
        if (IsValid(MID))
        {
            MID->SetScalarParameterValue(FName("multiply"), ExhaustRate);
        }
    }
    for (UMaterialInstanceDynamic* MID : RightExhaustDynElements)
    {
        if (IsValid(MID))
        {
            MID->SetScalarParameterValue(FName("multiply"), ExhaustRate);
        }
    }

    // Niagara ScaleColor
    const FVector ScaleColor(
        FMath::Lerp(0.0f, 3.0f,  SpeedAlpha),
        FMath::Lerp(0.0f, 1.3f, SpeedAlpha),
        FMath::Lerp(0.0f, 1.3f, SpeedAlpha)
    );
    if (NSMateriaLeft)  NSMateriaLeft->SetVariableVec3(FName("ScaleColor"), ScaleColor);
    if (NSMateriaRight) NSMateriaRight->SetVariableVec3(FName("ScaleColor"), ScaleColor);

    // ---- Rocket Niagara spawn rate scaling (2%-100% by SpeedAlpha) ----
    // At rest (alpha=0) each emitter runs at 2% of its max rate.
    // At max speed (alpha=1) each emitter runs at 100% of its max rate.
    if (NSRocketLeft)
    {
        NSRocketLeft->SetVariableFloat(FName("EnergyCore_SpawnRate"),   FMath::Lerp(200.0f  * 0.02f, 200.0f,   SpeedAlpha));
        NSRocketLeft->SetVariableFloat(FName("HeatHaze_SpawnRate"),    FMath::Lerp(100.0f  * 0.02f, 100.0f,   SpeedAlpha));
        NSRocketLeft->SetVariableFloat(FName("Particulate_SpawnRate"), FMath::Lerp(2000.0f * 0.02f, 2000.0f,  SpeedAlpha));
        NSRocketLeft->SetVariableFloat(FName("Smoke_SpawnRate"),       FMath::Lerp(0.0f    * 0.02f, 0.0f,     SpeedAlpha));
        NSRocketLeft->SetVariableFloat(FName("Thrusters_SpawnRate"),   FMath::Lerp(800.0f  * 0.02f, 800.0f,   SpeedAlpha));
    }
    if (NSRocketRight)
    {
        NSRocketRight->SetVariableFloat(FName("EnergyCore_SpawnRate"),   FMath::Lerp(200.0f  * 0.02f, 200.0f,   SpeedAlpha));
        NSRocketRight->SetVariableFloat(FName("HeatHaze_SpawnRate"),    FMath::Lerp(100.0f  * 0.02f, 100.0f,   SpeedAlpha));
        NSRocketRight->SetVariableFloat(FName("Particulate_SpawnRate"), FMath::Lerp(2000.0f * 0.02f, 2000.0f,  SpeedAlpha));
        NSRocketRight->SetVariableFloat(FName("Smoke_SpawnRate"),       FMath::Lerp(0.0f    * 0.02f, 0.0f,     SpeedAlpha));
        NSRocketRight->SetVariableFloat(FName("Thrusters_SpawnRate"),   FMath::Lerp(800.0f  * 0.02f, 800.0f,   SpeedAlpha));
    }
}

