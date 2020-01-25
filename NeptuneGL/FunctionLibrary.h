// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FunctionLibrary.generated.h"

/**
 *
 */
UCLASS()
class NEPTUNEGL_API UFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

		/** Gets a start and ending location for a line trace based on the Player Camera Manager for Player 0 **/

		UFUNCTION(BlueprintPure, Category = "Helpers", meta = (WorldContext = "WorldContextObject", DisplayName = "Line Trace Vectors from Player Camera", Keywords = "Get Line Trace Vectors"))
		static void LineTraceVectorsFromPlayerCamera(const UObject* WorldContextObject, const float distance, FVector &start, FVector &end);

	/** return if in editor or not, useful for functions that should not be called in dev builds **/

	UFUNCTION(BlueprintPure, Category = "Helpers")
		static bool IsWithEditor();

	//UFUNCTION(BlueprintPure, Category = "Helpers")
	//	static bool GIsEditor();
};
