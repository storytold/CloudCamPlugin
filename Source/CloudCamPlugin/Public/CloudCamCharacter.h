#pragma once

// Unreal Core
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

// Actor system
#include "GameFramework/Character.h"

// Unreal Niagara
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceTexture.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

// CloudCam
#include "ZeroMqPollingThread.h"
#include "CloudCamCharacter.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(CloudCamCharacterLog, Log, All);

UCLASS(config = Game, BlueprintType, meta = (ShortTooltip = "CloudCam actor that can move around as a character"))
class CLOUDCAMPLUGIN_API ACloudCamCharacter : public ACharacter
{

	GENERATED_BODY()

public:

	ACloudCamCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	//ACloudCamCharacter();
	~ACloudCamCharacter();

	// The niagara system which should be used for rendering:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CloudCam")
	UNiagaraSystem* pointCloudRenderer;

	// Where to spawn the niagara system relative to actor origin (0, 0, 0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CloudCam")
	FVector NiagaraSpawnPoint;
	
	// Root component (for editable actor placement)
	//UPROPERTY()
	//class USceneComponent* RootSceneComponent;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void BeginDestroy() override;

private:
	/** 
	 * These rotations are applied to essentially "zero" the actor. 
	 * Any custom rotations are applied after these are made.
	 */
	static const float BASE_PITCH; // Y-axis rotation 
	static const float BASE_ROLL; // X-axis rotation
	static const float BASE_YAW; // Z-axis rotation

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

