#pragma once

// Unreal Core
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

// Unreal Niagara
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceTexture.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

// CloudCam
#include "ZeroMqPollingThread.h"
#include "CloudCamActor.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(CloudCamActorLog, Log, All);

UCLASS(Blueprintable)
class CLOUDCAMPLUGIN_API ACloudCamActor : public AActor
{
	GENERATED_BODY()

public:

	ACloudCamActor();
	~ACloudCamActor();

	// The niagara system which should be used for rendering:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CloudCam")
	UNiagaraSystem* pointCloudRenderer;
	
	// Niagara spawning location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CloudCam")
	FVector spawnLocation;

	// Niagara spawning rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CloudCam")
	FRotator spawnRotation;

	// Niagara scale
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CloudCam")
	FVector scale;

	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void BeginDestroy() override;

private:
	FZeroMqPollingThread* zeromq_thread;
	TSharedPtr<RingBuffer> point_buffer;

	// The instance of the niagara system.
	UNiagaraComponent* rendererInstance;

	// Points from Rust/ZeroMQ are loaded into these textures for Niagara to consume.
	UTexture2D* positionTexture;
	UTexture2D* colorTexture;

	// Definition of the region which should be updated.
	FUpdateTextureRegion2D region;

	// Texture sizes
	uint32_t textureWidth;
	uint32_t textureHeight;
	uint32_t pointCount;

	// Data payloads from Rust/ZeroMQ to load into the textures.
	float* positions;
	uint8_t* colors;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};

