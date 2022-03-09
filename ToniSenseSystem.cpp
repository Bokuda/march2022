#include "ToniSenseSystem.h"
#include "ToniSenseSettings.h"
#include "ToniSenseStimulus.h"
#include "/Character/CharacterBase.h"
#include "/Core/GameModeBase.h"

// clang-format off
DECLARE_CYCLE_STAT(TEXT("Toni Sense (TriggerStimulus)"), STAT_ToniSense_TriggerStimulus, STATGROUP_TONI);
DECLARE_CYCLE_STAT(TEXT("Toni Sense (SystemTick)"), STAT_ToniSense_SystemTick, STATGROUP_TONI);
// clang-format on

#if !UE_BUILD_SHIPPING
bool UToniSenseSystem::bDrawSmellTrailDebug = false;

static FAutoConsoleCommandWithWorldAndArgs CVarMotionPredictionDebug_Enabled(
    TEXT("ToniSense.DrawSmellTrail"),
    TEXT("Arguments: 0/1\n") TEXT("Controls whether the smell trail visualization is on."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,
                                                             UWorld* World) {
        if (Args.Num() != 0)
        {
            UToniSenseSystem::bDrawSmellTrailDebug = Args[0].ToBool();
        }
    }),
    ECVF_Cheat);
#endif

UToniSenseSystem::UToniSenseSystem()
    : Super()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

const int32 UToniSenseSystem::TONISENSE_STENCIL_VALUE_BASE = 100;
const int32 UToniSenseSystem::TONISENSE_STENCIL_VALUE_RANGE = 10;

void UToniSenseSystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    ActiveBlips.Empty();
    ActiveOutlines.Empty();

#if !UE_BUILD_SHIPPING
    UToniSenseSystem::bDrawSmellTrailDebug = false;
#endif
}

void UToniSenseSystem::RegisterBlip(AActor* InBlipActor, float InLifeTime)
{
    if (IsValid(InBlipActor))
    {
        ActiveBlips.Emplace(InBlipActor, InLifeTime);
    }
}

void UToniSenseSystem::UnregisterBlip(TWeakObjectPtr<AActor> InBlipActorPtr)
{
    if (InBlipActorPtr.IsValid())
    {
        InBlipActorPtr->Destroy();
    }

    ActiveBlips.Remove(InBlipActorPtr);
}

void UToniSenseSystem::RegisterOrUpdateOutline(AActor* InCharacter,
                                                    const FToniSenseActiveStimulus& InParams)
{
    if (IsValid(InCharacter))
    {
        FToniSenseActiveStimulus* ActiveStimulus = ActiveOutlines.Find(InCharacter);
        if (ActiveStimulus == nullptr || !ActiveStimulus->Stimulus.bIsContinuous)
        {
            ActiveOutlines.Emplace(InCharacter, InParams);
        }

        SetCusDepthRenderingEnabled(InCharacter, true);
        UpdateCusDepthsStencilValue(InCharacter, InParams.RemainingLifeTime);
    }
}

void UToniSenseSystem::UnregisterOutline(TWeakObjectPtr<AActor> InCharacterPtr)
{
    if (InCharacterPtr.IsValid())
    {
        SetCusDepthRenderingEnabled(InCharacterPtr.Get(), false);
        UpdateCusDepthsStencilValue(InCharacterPtr.Get(), -1.0f);
    }

    ActiveOutlines.Remove(InCharacterPtr);
}

void UToniSenseSystem::SetCusDepthRenderingEnabled(AActor* InCharacter, bool bEnabled)
{
    USkeletalMeshComponent* Mesh = GetBodyMesh(InCharacter);
    if (IsValid(Mesh))
    {
        Mesh->SetRenderCustomDepth(bEnabled);
    }
}

void UToniSenseSystem::UpdateCustomDepthsStencilValue(AActor* InCharacter,
                                                           float InRemainingLifeTime)
{
    USkeletalMeshComponent* Mesh = GetBodyMesh(InCharacter);
    if (IsValid(Mesh))
    {
        Mesh->SetCustomDepthStencilValue(CalcCustomStencilValue(InRemainingLifeTime));
    }
}

int32 UToniSenseSystem::CalcCustomStencilValue(float InRemainingLifeTime) const
{
    if (InRemainingLifeTime < 0.0)
    {
        return 0;
    }

    float LifeTimeRatio = IsValid(Settings) ? InRemainingLifeTime / Settings->OutlineLifeTime
                                            : 1.0f;
    LifeTimeRatio = FMath::Clamp(LifeTimeRatio, 0.0f, 1.0f);

    return TONISENSE_STENCIL_VALUE_BASE
           + FMath::TruncToInt(LifeTimeRatio * TONISENSE_STENCIL_VALUE_RANGE);
}

USkeletalMeshComponent* UToniSenseSystem::GetBodyMesh(const AActor* InActor) const
{
    const auto Character = Cast<ACharacterBase>(InActor);
    if (IsValid(Character))
    {
        const auto ParentMesh = Character->GetMesh();
        if (IsValid(ParentMesh))
        {
            // search direct children only
            TArray<USceneComponent*> Components;
            ParentMesh->GetChildrenComponents(false, Components);
            bool bFoundChildMesh = false;
            for (auto Component : Components)
            {
                auto ChildMesh = Cast<USkeletalMeshComponent>(Component);
                if (IsValid(ChildMesh))
                {
                    return ChildMesh;
                }
            }

            return ParentMesh;
        }
    }

    return nullptr;
}

void UToniSenseSystem::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
    SCOPE_CYCLE_COUNTER(STAT_ToniSense_SystemTick);

    // clean up blips
    TArray<TWeakObjectPtr<AActor>> OutdatedBlipActors;

    for (auto& Blip : ActiveBlips)
    {
        auto& BlipActorPtr = Blip.Key;
        if (BlipActorPtr.IsValid())
        {
            Blip.Value -= DeltaTime;
            if (Blip.Value <= 0.f)
            {
                OutdatedBlipActors.Add(BlipActorPtr);
            }
        }
        else
        {
            OutdatedBlipActors.Add(BlipActorPtr);
        }
    }

    for (auto BlipActorPtr : OutdatedBlipActors)
    {
        UnregisterBlip(BlipActorPtr);
    }

    // clean up outlines
    TArray<TWeakObjectPtr<AActor>> OutdatedOutlineActors;

    for (auto& Outline : ActiveOutlines)
    {
        auto& OutlineOwner = Outline.Key;
        if (OutlineOwner.IsValid())
        {
            if (!Outline.Value.Stimulus.bIsContinuous)
            {
                Outline.Value.RemainingLifeTime -= DeltaTime;
                if (Outline.Value.RemainingLifeTime <= 0.f)
                {
                    OutdatedOutlineActors.Add(OutlineOwner);
                    continue;
                }

                UpdateCustomDepthsStencilValue(OutlineOwner.Get(), Outline.Value.RemainingLifeTime);
            }
        }
        else
        {
            OutdatedOutlineActors.Add(OutlineOwner);
        }
    }

    for (auto OutlineOwner : OutdatedOutlineActors)
    {
        UnregisterOutline(OutlineOwner);
    }
}

void UToniSenseSystem::TriggerStimulus(const FToniSenseStimulus& Params)
{
    SCOPE_CYCLE_COUNTER(STAT_ToniSense_TriggerStimulus);
    if (!IsValid(Settings) || !Params.Instigator.IsValid())
    {
        return;
    }

    // generate a blip
    if (Settings->bGenerateBlips && IsValid(Settings->BlipActorClass))
    {
        FTransform Transform;
        Transform.SetLocation(Params.Location);

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        auto BlipActor =
            GetWorld()->SpawnActor<AActor>(Settings->BlipActorClass, Transform, SpawnParams);
        RegisterBlip(BlipActor, Settings->BlipLifeTime);
    }

    // generate outline
    if (Settings->bGenerateOutlines)
    {
        FToniSenseActiveStimulus ActiveStimulus;
        ActiveStimulus.Stimulus = Params;
        ActiveStimulus.RemainingLifeTime = Settings->OutlineLifeTime;
        RegisterOrUpdateOutline(Params.Instigator.Get(), ActiveStimulus);
    }
}

void UToniSenseSystem::StopContinuousStimulus(AActor* Instigator)
{
    if (IsValid(Instigator))
    {
        FToniSenseActiveStimulus* ActiveStimulus = ActiveOutlines.Find(Instigator);
        if (ActiveStimulus != nullptr && ActiveStimulus->Stimulus.bIsContinuous)
        {
            ActiveStimulus->Stimulus.bIsContinuous = false;
        }
    }
}

void UToniSenseSystem::TriggerStimulus(UObject* WorldContextObject,
                                            const FToniSenseStimulus& Params)
{
    auto System = AGameModeBase::GetToniSenseSystem(WorldContextObject);
    if (IsValid(System))
    {
        System->TriggerStimulus(Params);
    }
}

void UToniSenseSystem::StopContinuousStimulus(UObject* WorldContextObject, AActor* Instigator)
{
    auto System = AGameModeBase::GetToniSenseSystem(WorldContextObject);
    if (IsValid(System))
    {
        System->StopContinuousStimulus(Instigator);
    }
}

const UToniSenseSettings* UToniSenseSystem::GetSettings() const
{
    return Settings;
}
