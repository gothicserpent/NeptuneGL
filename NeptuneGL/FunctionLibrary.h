// ===============================================================
// NeptuneGL Utility Function Library
// ===============================================================
//
// This is the central C++ utility library for NeptuneGL.
// It provides high-performance, Blueprint-callable functions focused on:
// - Custom player input & FOV helpers (multiplayer-ready with PlayerIndex)
// - Camera-centric line traces & view utilities (high-value conveniences)
// - Screen-space projection helpers for HUD/radar
// - Advanced math (critically-damped smoothing, signed angles, line projection)
// - Unique spatial queries (cone + visibility)
// - Gameplay spawning & targeting helpers
// - Ship engine VFX (distortion, exhaust, automatic throttling)
// - Stable bobbing animations for actors
// - Multiplayer controller enumeration
// - Debug visualization & safe naming
//
// Removed: Thin wrappers around UGameplayStatics, basic FVector/FMath ops,
//          standard BP nodes (overlaps, map range, print string, basic spawns,
//          time getters, etc.). Kept only unique/high-ROI functions.
//
// Best practices followed:
// - All functions are static
// - Heavy use of WorldContext + PlayerIndex support
// - Robust null checking
// - Clean organization
//
// Copyright (c) 2023-2026 NeptuneGL. All rights reserved.

// ReSharper disable GrammarMistakeInComment - disables Possible typo: you repeated a word.
// ReSharper disable CppUEBlueprintCallableFunctionUnused - disables Function 'name' is never used in Blueprint or C++ code
// ReSharper disable CommentTypo - disables Typo: In word 'word'
// can consider using IdentifierTypo or StringLiteralTypo if other errata appear


#pragma once

#include "CoreMinimal.h"  // Essential UE4 headers (FVector, FName, UPROPERTY, etc.)
#include "Kismet/BlueprintFunctionLibrary.h"  // Base class for Blueprint-callable static function libraries
#include "Engine/World.h"  // World class for line traces and actor enumeration queries
#include "GameFramework/Actor.h"  // Actor base class for spawning and gameplay logic
#include "Components/PrimitiveComponent.h"  // PrimitiveComponent for physics and bobbing animations
#include "UObject/ObjectKey.h"  // TObjectKey container for mapping mesh components to update times (VFX throttling)
#include "FunctionLibrary.generated.h"  // UE reflection system — auto-generated from UCLASS/UFUNCTION macros

/**
 * NeptuneGL Core Utility Functions
 *
 * Focused library of custom helpers that are either:
 * - Not easily exposed or tedious in Blueprint
 * - Performance or convenience wrappers for common patterns in this project
 * - Advanced math/geometry not covered by KismetMathLibrary in one node
 */


// Define a custom enum that the Unreal Header Tool (UHT) can understand
UENUM(BlueprintType)
enum class ECustomNetMode : uint8
{
	StandaloneWindow   UMETA(DisplayName = "Standalone Window (Game/Packaged)"),
	PIEViewport        UMETA(DisplayName = "PIE - Selected Viewport"),
	PIENewWindow       UMETA(DisplayName = "PIE - New Editor Window"),
	DedicatedServer    UMETA(DisplayName = "Dedicated Server"),
	ListenServer       UMETA(DisplayName = "Listen Server"),
	Client             UMETA(DisplayName = "Client Window")
};

UCLASS()  // Reflection-enabled class for Unreal's Blueprint system
class NEPTUNEGL_API UFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	#pragma region PLAYER_PAWN_CUSTOM_HELPERS
	
	// ============================================================
	// Player & Pawn Custom Helpers (Multiplayer Ready)
	// ============================================================

	
	// ============================================================
	// Ship Engine VFX (Engine Distortion & Exhaust)
	// ============================================================

	/**
	 * Updates engine distortion and exhaust VFX based on ship mesh velocity.
	 * Replaces the ENGINE DISTORTION AND EXHAUST Blueprint composite node.
	 * 
	 * Automatically throttles updates to 0.01s intervals for performance optimization.
	 * Tracks per-mesh update times to prevent excessive VFX recalculations each frame.
	 *
	 * Speed alpha = Clamp(VSize(ShipMeshVelocity) / MaxSpeed, 0, 1)
	 *
	 * Updates applied based on SpeedAlpha:
	 * - Cascade heatblur opacity:   Lerp(0.6, 1.0, SpeedAlpha) → "opacity" float param
	 * - Niagara ExhaustRate:        Lerp(0.0, 5.0, SpeedAlpha) → "ExhaustRate" float var
	 * - Dynamic material multiply:  Lerp(0.0, 5.0, SpeedAlpha) → "multiply" scalar param
	 *
	 * @param ShipMesh                           The ship's static mesh component (must have velocity)
	 * @param LeftHeatblurParticles              Left cascade heatblur distortion particle system component
	 * @param RightHeatblurParticles             Right cascade heatblur distortion particle system component
	 * @param NSMateriaLeft                      Left Niagara exhaust component
	 * @param NSMateriaRight                     Right Niagara exhaust component
	 * @param LeftExhaustDynamicElements         Dynamic material instances for left exhaust
	 * @param RightExhaustDynamicElements        Dynamic material instances for right exhaust
	 * @param MaxSpeed                           Speed at which alpha reaches 1.0 (default 2600.0)
	 */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Ship|VFX")
	static void UpdateEngineVFX(
		UStaticMeshComponent*                ShipMesh,
		UParticleSystemComponent*            LeftHeatblurParticles,
		UParticleSystemComponent*            RightHeatblurParticles,
		UNiagaraComponent*                   NSMateriaLeft,
		UNiagaraComponent*                   NSMateriaRight,
		const TArray<UMaterialInstanceDynamic*>& LeftExhaustDynamicElements,
		const TArray<UMaterialInstanceDynamic*>& RightExhaustDynamicElements,
		float                                MaxSpeed = 2600.0f
	);
	
	
	/**
	 * Applies a smooth bobbing animation to an actor using critically-damped spring physics.
	 * Currently used for possessed player pawns, enemy ships, and powerups.
	 * Call this every frame with DeltaTime to produce continuous bobbing motion.
	 */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Ship|Visual")
	static void Bob(
		UPrimitiveComponent* Component,   // Any component with physics — applies bobbing offset
		float DeltaTime,                  // Frame delta time to calculate smooth motion
		float BobSpeed = 5.0f,            // Oscillation frequency in cycles per second
		float BobAmount = 50.0f,          // Maximum vertical displacement in Unreal units
		float IdleDampStrength = 6.0f     // Spring damping factor for smooth settling
	);
	
	/** Returns true if the player's speed exceeds the given threshold */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Player", meta = (WorldContext = "WorldContextObject"))
	static bool IsPlayerMoving(const UObject* WorldContextObject, float Threshold = 1.0f, int32 PlayerIndex = 0);

	/** Returns the current movement input direction based on control rotation (more reliable than raw InputComponent for 3D flight) */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Player|Input")
	static FVector GetPlayerInputDirection(const UObject* WorldContextObject, int32 PlayerIndex = 0);

	/** Checks if a world position is inside the player's field of view cone */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Player|Visibility", meta = (WorldContext = "WorldContextObject"))
	static bool IsPositionInFieldOfView(const UObject* WorldContextObject, const FVector& WorldPosition, float FOVAngle = 90.0f, int32 PlayerIndex = 0);

	#pragma endregion

	#pragma region CAMERA_LINE_TRACE

	/** Gets start and end vectors for a line trace from the player camera */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Camera|Line Trace", meta = (WorldContext = "WorldContextObject", DisplayName = "Line Trace Vectors from Player Camera"))
	static void LineTraceVectorsFromPlayerCamera(const UObject* WorldContextObject, float Distance, FVector& OutStart, FVector& OutEnd, int32 PlayerIndex = 0);

	/** Single line trace from player camera. Returns true if hit. */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Camera|Line Trace", meta = (WorldContext = "WorldContextObject"))
	static bool LineTraceFromCamera(const UObject* WorldContextObject, float Distance, FHitResult& OutHit, ECollisionChannel Channel = ECC_Visibility, int32 PlayerIndex = 0);

	/** Single line trace from the pawn's eye view point (first person / cockpit accurate) */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Camera|Line Trace", meta = (WorldContext = "WorldContextObject"))
	static bool LineTraceFromPlayerEyes(const UObject* WorldContextObject, float Distance, FHitResult& HitResult, ECollisionChannel CollisionChannel = ECC_Visibility, int32 PlayerIndex = 0);

	/** Multi line trace from player camera - returns all hits */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Camera|Line Trace", meta = (WorldContext = "WorldContextObject"))
	static bool MultiLineTraceFromCamera(const UObject* WorldContextObject, float Distance, TArray<FHitResult>& HitResults, ECollisionChannel CollisionChannel = ECC_Visibility, int32 PlayerIndex = 0);

	/** Returns the normalized forward direction of the player camera */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Camera|Orientation", meta = (WorldContext = "WorldContextObject"))
	static FVector GetPlayerViewDirection(const UObject* WorldContextObject, int32 PlayerIndex = 0);

	/** Returns the current world location of the player camera */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Camera", meta = (WorldContext = "WorldContextObject"))
	static FVector GetPlayerCameraLocation(const UObject* WorldContextObject, int32 PlayerIndex = 0);

	/** Returns the current FOV angle of the player camera */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Camera", meta = (WorldContext = "WorldContextObject"))
	static float GetPlayerCameraFOV(const UObject* WorldContextObject, int32 PlayerIndex = 0);

	/** Returns the current viewport aspect ratio (width/height) */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Camera", meta = (WorldContext = "WorldContextObject"))
	static float GetAspectRatio(const UObject* WorldContextObject, int32 PlayerIndex = 0);

	/** Returns signed 2D yaw angle from player to target (perfect for radar/compass UI) */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Camera|Orientation", meta = (WorldContext = "WorldContextObject"))
	static float GetLookAtAngle2D(const UObject* WorldContextObject, const FVector& WorldPosition, int32 PlayerIndex = 0);

	#pragma endregion

	#pragma region SCREEN_SPACE

	/** Projects world position to screen coordinates. Returns true if successful. */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Camera|Screen Space", meta = (WorldContext = "WorldContextObject"))
	static bool WorldToScreenPosition(const UObject* WorldContextObject, const FVector& WorldPosition, FVector2D& ScreenPosition, int32 PlayerIndex = 0);

	/** Returns true if the world position projects inside the current viewport */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Camera|Screen Space", meta = (WorldContext = "WorldContextObject"))
	static bool IsPositionOnScreen(const UObject* WorldContextObject, const FVector& WorldPosition, int32 PlayerIndex = 0);

	#pragma endregion

	#pragma region MATH_GEOMETRY

	/** Exponential smoothing (critical damping) - excellent for camera follow, bob, thrusters, UI motion.
	 *  Uses double precision internally for compatibility with UE5 FVector (which is double-based).
	 */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Math")
	static double SmoothDamp(double Current, double Target, double& CurrentVelocity, double SmoothTime, double DeltaTime, double MaxSpeed);

	/** Vector version of critically damped smooth - perfect replacement for many Timeline lerps.
	 *  Works directly with FVector (double precision).
	 */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Math")
	static FVector SmoothDampVector(const FVector& Current, const FVector& Target, FVector& CurrentVelocity, double SmoothTime, double DeltaTime, double MaxSpeed);

	/** Rotates Current vector towards Target at a maximum angular speed (great for nose tracking, turrets) */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Math|Geometry")
	static FVector RotateVectorTowards(const FVector& Current, const FVector& Target, float MaxDegreesPerSecond, float DeltaTime);

	/** Returns normalized direction from From to To */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Math")
	static FVector GetDirectionTo(const FVector& From, const FVector& To);

	/** Angle in degrees between two vectors (0-180) */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Math|Geometry")
	static float GetAngleBetweenVectors(const FVector& A, const FVector& B);

	/** Signed angle between two vectors around an axis (useful for 2D radar, turning decisions) */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Math|Geometry")
	static float GetSignedAngleBetweenVectors(const FVector& A, const FVector& B, const FVector& Axis);

	/** Closest point on a finite line segment */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Math|Geometry")
	static FVector GetClosestPointOnLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point);

	/** Shortest distance from point to line segment */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Math|Geometry")
	static float GetDistanceToLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point);

	/** Random point inside sphere volume */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Math|Random")
	static FVector GetRandomPointInSphere(const FVector& Origin, float Radius);

	/** Random point on sphere surface (great for shotgun spread, particle offsets, flak) */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Math|Random")
	static FVector GetRandomPointOnSphere(const FVector& Origin, float Radius);

	/** Random point inside box */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Math|Random")
	static FVector GetRandomPointInBox(const FVector& Center, const FVector& HalfExtents);

	#pragma endregion

	#pragma region PROJECTILE_COMBAT_MATH

	/**
	 * Calculates the lead/intercept position for a moving target.
	 * Solves the quadratic for time-to-impact. Returns current target location if no valid solution.
	 * Perfect for railgun/shotgun reticles, missile guidance, or player aim assist.
	 */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Combat|Targeting")
	static FVector GetLeadTargetPosition(const FVector& ShooterLocation, const FVector& TargetLocation, const FVector& TargetVelocity, float ProjectileSpeed, float& OutTimeToImpact);

	#pragma endregion

	#pragma region GAMEPLAY_UTIL

	/** Spawns a projectile at the player's current location and camera rotation (game-specific convenience) */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Gameplay|Projectiles", meta = (WorldContext = "WorldContextObject"))
	static void SpawnProjectileAtPlayer(const UObject* WorldContextObject, TSubclassOf<AActor> ProjectileClass, int32 PlayerIndex = 0);

	/** General purpose actor spawn with sensible defaults for collision handling and ownership */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Gameplay", meta = (WorldContext = "WorldContextObject"))
	static AActor* SpawnActorAtLocation(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FVector& Location, const FRotator& Rotation, AActor* Owner = nullptr, APawn* Instigator = nullptr);

	/** Find the closest actor of a class to a position (single node vs GetAll + loop) */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Gameplay", meta = (WorldContext = "WorldContextObject"))
	static AActor* GetClosestActorOfClass(const UObject* WorldContextObject, const FVector& Position, TSubclassOf<AActor> ActorClass);

	/** Get actors inside a cone from origin+direction (unique - overlap + angle filter in one call) */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Gameplay", meta = (WorldContext = "WorldContextObject"))
	static void GetActorsInCone(const UObject* WorldContextObject, const FVector& Origin, const FVector& Direction, float ConeAngle, float Range, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors);

	/** Line of sight check from player camera to target actor (encapsulates ignore + trace) */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Gameplay|Visibility", meta = (WorldContext = "WorldContextObject"))
	static bool IsActorVisible(const UObject* WorldContextObject, AActor* TargetActor, ECollisionChannel CollisionChannel = ECC_Visibility, int32 PlayerIndex = 0);

	/** Destroy all actors of a given class in the world and return count (great for round reset / cleanup) */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Gameplay", meta = (WorldContext = "WorldContextObject"))
	static int32 DestroyAllActorsOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass);

	#pragma endregion

	#pragma region MULTIPLAYER_HELPERS

	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Multiplayer", meta = (WorldContext = "WorldContextObject"))
	static TArray<APlayerController*> GetAllPlayerControllers(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Multiplayer", meta = (WorldContext = "WorldContextObject"))
	static int32 GetPlayerCount(const UObject* WorldContextObject);

	#pragma endregion

	#pragma region DEVELOPMENT_DEBUG


	/** Returns true when running inside the Unreal Editor */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Development|Editor")
	static bool IsWithEditor();

	/** Safe way to get an actor's name (handles null) */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Development|Debug")
	static FString GetObjectNameSafe(const AActor* Actor);

	/** Converts a transform to a readable string for logging */
	UFUNCTION(BlueprintPure, Category = "NeptuneGL|Development|Debug")
	static FString ConcatTransformToString(const FTransform& Transform);

	/** Draws a persistent debug arrow (Duration = -1 for persistent) */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Development|Debug", meta = (WorldContext = "WorldContextObject"))
	static void DrawDebugArrowPersistent(const UObject* WorldContextObject, const FVector& Start, const FVector& End, float Thickness = 5.0f, FLinearColor Color = FLinearColor::Red, float Duration = -1.0f);

	/** Clears all persistent debug lines and arrows */
	UFUNCTION(BlueprintCallable, Category = "NeptuneGL|Development|Debug", meta = (WorldContext = "WorldContextObject"))
	static void FlushPersistentDebugLines(const UObject* WorldContextObject);
	
	UFUNCTION(BlueprintPure, Category = "Networking", meta = (WorldContext = "WorldContextObject"))
	static ECustomNetMode GetCurrentNetMode(const UObject* WorldContextObject);
	
	#pragma endregion 
	
	// ============================================================
	// Private: Ship Bob State (keyed per mesh, no actor changes needed)
	// ============================================================
private:
	struct BobState
	{
		// The smoothed bob velocity from last frame.
		// Stored so SmoothDampVector can interpolate continuously rather than snapping.
		FVector BobVelocity       = FVector::ZeroVector;

		// Internal derivative tracker used by SmoothDampVector.
		// Represents the current rate of change - must persist between frames
		// so the spring-damper has memory of its own momentum.
		FVector SmoothDampTracker = FVector::ZeroVector;
		
		// Current position in the sine wave cycle (0 to 2PI).
		// Advances each frame by BobSpeed * DeltaTime and wraps to prevent
		// floating point precision loss over long play sessions.
		float BobPhase            = 0.0f;
	};

	// TObjectKey is the Epic-recommended key type for UObject-derived pointers in TMap.
	// More stable than raw pointers across GC cycles. Does not prevent garbage collection.
	// Stale entries are still swept on new ship registration (see Bob).
	static TMap<TObjectKey<USceneComponent>, BobState> BobStateMap;
	
	// NEW: Throttling map for UpdateEngineVFX
	static TMap<TObjectKey<UStaticMeshComponent>, double> LastEngineVFXUpdateTime;

};
