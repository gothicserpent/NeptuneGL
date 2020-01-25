// Fill out your copyright notice in the Description page of Project Settings.

#include "FunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Engine.h"

void UFunctionLibrary::LineTraceVectorsFromPlayerCamera(const UObject * WorldContextObject, const float distance, FVector &start, FVector &end)
{
	APlayerCameraManager * cm = UGameplayStatics::GetPlayerCameraManager(WorldContextObject, 0);
	start = cm->GetCameraLocation();
	end = start + (UKismetMathLibrary::GetForwardVector(cm->GetCameraRotation()) * distance);
}

bool UFunctionLibrary::IsWithEditor()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		return true;
	}
	else
		return false;
#else
	return false;
#endif

	/*
#if WITH_EDITOR
	return true;
#else
	return false;
#endif*/
}

/*
bool UFunctionLibrary::GIsEditor()
{
	//return GIsEditor;
	//return GEngine->IsEditor();

	//	return Engine::();
}
*/
