// ===============================================================
// AShipPawn - Base C++ ship pawn for NeptuneGL
// ===============================================================
//
// Blueprint children (e.g. BP_ShipPawn) inherit from this class.
// All weapon/electrical variables are declared here as UPROPERTY
// and exposed to Blueprint — no duplicate BP-side declarations needed.
//
// Arrow components (RT_Laser_Arrow, etc.) and events (ReleaseLeftTrigger,
// FireLeft, ChargeUpShake) remain in BP — passed as params or called
// via BlueprintNativeEvent/ImplementableEvent bridges.
//
// Copyright (c) 2023-2026 NeptuneGL. All rights reserved.

// ReSharper disable GrammarMistakeInComment
// ReSharper disable CppUEBlueprintCallableFunctionUnused
// ReSharper disable CommentTypo
// ReSharper disable IdentifierTypo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShipPawn.generated.h"

// Forward declarations — keep header lean
class UStaticMeshComponent;
class UParticleSystemComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;
class UArrowComponent;

/**
 * Base ship pawn with VaryEngine VFX and weapon system.
 *
 * Weapon/electrical variables declared in C++ as UPROPERTY(BlueprintReadWrite)
 * are the single source of truth — BP_ShipPawn sets their defaults in
 * the Details panel, no separate BP variable declarations needed.
 *
 * Arrow components and BP-only events stay in Blueprint and are either
 * passed as function parameters or called via C++→BP event bridges.
 */
UCLASS()
class NEPTUNEGL_API AShipPawn : public APawn
{
    GENERATED_BODY()

public:
    AShipPawn();

    // ================================================================
    // VaryEngine — BP-callable VFX update (all inputs on the node)
    // ================================================================

    /**
     * Updates engine distortion, exhaust, and rocket VFX based on ship
     * mesh velocity.  Call from BP Tick — throttled to 0.01s per actor.
     */
    UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Ship|VFX", meta = (DisplayName = "Update Engine VFX"))
    void BP_UpdateEngineVFX(
        UStaticMeshComponent*                          ShipMesh,
        UParticleSystemComponent*                      LeftHeatblurParticles,
        UParticleSystemComponent*                      RightHeatblurParticles,
        UNiagaraComponent*                             NSMateriaLeft,
        UNiagaraComponent*                             NSMateriaRight,
        UNiagaraComponent*                             NSRocketLeft,
        UNiagaraComponent*                             NSRocketRight,
        const TArray<UMaterialInstanceDynamic*>&       LeftExhaustDynElements,
        const TArray<UMaterialInstanceDynamic*>&       RightExhaustDynElements,
        float                                          MaxSpeed = 2600.0f
    );

private:
    /** Timestamp of the last VFX update, used to throttle BP_UpdateEngineVFX to 0.01s intervals. */
    double LastVFXUpdateTime = 0.0;
};