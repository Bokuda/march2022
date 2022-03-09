#pragma once

// Copyright 2021 Daedalic Entertainment GmbH. All Rights Reserved.
#pragma once
#include "ToniSenseStimulus.h"
#include <CoreMinimal.h>
#include <Components/ActorComponent.h>
#include "ToniSenseSystem.generated.h"

class UToniSenseSettings;
struct FToniSenseStimulus;

/** Handles life time of Toni Sense Blips.
 *  TODO: If we keep this feature, think about reusing blip actors instead of constantly spawning and destroying them.
 *  Note: spawning blips will most likely beco
 */
UCLASS(BlueprintType)
class TONI_API UToniSenseSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UToniSenseSystem();

    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, float> ActiveBlips;
    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, FToniSenseActiveStimulus> ActiveOutlines;

    static const int32 TONISENSE_STENCIL_VALUE_BASE;
    static const int32 TONISENSE_STENCIL_VALUE_RANGE;

#if !UE_BUILD_SHIPPING
    static bool bDrawSmellTrailDebug;
#endif

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = ToniSense,
              meta = (WorldContext = "WorldContextObject",
                      DisplayName = "TriggerToniSenseStimulus"))
    static void TriggerStimulus(UObject* WorldContextObject, const FToniSenseStimulus& Params);
    UFUNCTION(BlueprintCallable, Category = ToniSense,
              meta = (WorldContext = "WorldContextObject",
                      DisplayName = "StopContinuousToniSenseStimulus"))
    static void StopContinuousStimulus(UObject* WorldContextObject, AActor* Instigator);

    const UToniSenseSettings* GetSettings() const;

protected:
    UPROPERTY(EditDefaultsOnly, Category = ToniSense)
    UToniSenseSettings* Settings;

    void TriggerStimulus(const FToniSenseStimulus& Params);
    void StopContinuousStimulus(AActor* Instigator);
    void RegisterBlip(AActor* InBlipActor, float InLifeTime);
    void UnregisterBlip(TWeakObjectPtr<AActor> InBlipActorPtr);
    void RegisterOrUpdateOutline(AActor* InCharacter,
                                 const FToniSenseActiveStimulus& InParams);
    void UnregisterOutline(TWeakObjectPtr<AActor> InCharacterPtr);

    void SetCustomDepthRenderingEnabled(AActor* InCharacter, bool bEnabled);
    void UpdateCustomDepthsStencilValue(AActor* InCharacter, float InRemainingLifeTime);
    int32 CalcCustomStencilValue(float InRemainingLifeTime) const;

    USkeletalMeshComponent* GetBodyMesh(const AActor* InActor) const;
};
