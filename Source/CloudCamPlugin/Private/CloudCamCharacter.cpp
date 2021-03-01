#include "CloudCamCharacter.h"

DEFINE_LOG_CATEGORY(CloudCamCharacterLog);

const float ACloudCamCharacter::BASE_PITCH = 0.0; // Y-axis
const float ACloudCamCharacter::BASE_ROLL = 120.0; // X-axis
const float ACloudCamCharacter::BASE_YAW = 300.0; // Z-axis


/**
 * Just a helper method to set a texture for a user variable because the UNiagaraComponent 
 * has no direct way to edit a texture variable compared to float, vectors, ...
 */

void SetCharacterZeroMqNiagaraVariableTexture(class UNiagaraComponent* niagara, FString variableName, UTexture* texture) {
	if (!niagara || !texture)
		return;

	FNiagaraUserRedirectionParameterStore& overrideParameters = niagara->GetOverrideParameters();
	FNiagaraVariable niagaraVariable = FNiagaraVariable(FNiagaraTypeDefinition(UNiagaraDataInterfaceTexture::StaticClass()), *variableName);

	UNiagaraDataInterfaceTexture* dataInterface = (UNiagaraDataInterfaceTexture*)overrideParameters.GetDataInterface(niagaraVariable);
	dataInterface->SetTexture(texture);
}


ACloudCamCharacter::ACloudCamCharacter() :
	zeromq_thread(nullptr),
	point_buffer(TSharedPtr<RingBuffer>(nullptr))
{
	// Set this actor to call Tick() every frame. 
	// You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	textureWidth = 1280; // NB: Rust program
	textureHeight = 1152; // NB: Rust program
	pointCount = textureWidth * textureHeight;

	region = FUpdateTextureRegion2D(0, 0, 0, 0, textureWidth, textureHeight);

	positions = new float[textureWidth * textureHeight * 4];
	colors = new uint8_t[textureWidth * textureHeight * 4];

	// Establish root component and bind it.
	// This allows us to change the location of the actor in the editor.
	// We can also show this location by parenting an ArrowComponent for in-editor visualization.
	//RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	//RootComponent = RootSceneComponent;
}


ACloudCamCharacter::~ACloudCamCharacter()
{
	delete[] positions;
	delete[] colors;
}

void ACloudCamCharacter::BeginPlay()
{
	Super::BeginPlay();

	FTransform rootTransform = GetTransform();
	FQuat rotation = rootTransform.GetRotation();
	FRotator rotator = rotation.Rotator();

	FRotator updatedSpawnRotation = FRotator(
		ACloudCamCharacter::BASE_PITCH + rotator.Pitch, // y-axis rotation
		ACloudCamCharacter::BASE_YAW + rotator.Yaw, // z-axis rotation
		ACloudCamCharacter::BASE_ROLL + rotator.Roll // x-axis rotation
	);


	// Initialize the Niagara System:
	/*rendererInstance = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		pointCloudRenderer,
		rootTransform.GetTranslation(),
		updatedSpawnRotation,
		rootTransform.GetScale3D(),
		true, 
		true,
		ENCPoolMethod::AutoRelease,
		true
	);*/

	rendererInstance = UNiagaraFunctionLibrary::SpawnSystemAttached(
		pointCloudRenderer,
		RootComponent,
		TEXT("PointCloudSystem"),
		FVector(500.0, 0.0, 500.0), // location
		updatedSpawnRotation, // rotation
		FVector(0.5), // scale
		EAttachLocation::SnapToTarget,
		true, // auto destroy
		ENCPoolMethod::AutoRelease,
		true, // auto activate
		true // pre cull check
	);

	rendererInstance->SetCastShadow(true);

	positionTexture = UTexture2D::CreateTransient(textureWidth, textureHeight, PF_A32B32G32R32F, "PositionData");
	positionTexture->Filter = TF_Nearest;
	positionTexture->UpdateResource();

	colorTexture = UTexture2D::CreateTransient(textureWidth, textureHeight, PF_B8G8R8A8, "ColorTexture");
	colorTexture->Filter = TF_Nearest;
	colorTexture->UpdateResource();

	SetCharacterZeroMqNiagaraVariableTexture(rendererInstance, "User.PositionTexture", positionTexture);
	SetCharacterZeroMqNiagaraVariableTexture(rendererInstance, "User.ColorTexture", colorTexture);

	rendererInstance->SetVariableInt("User.TextureWidth", textureWidth);
	rendererInstance->SetVariableInt("User.TextureHeight", textureHeight);
	rendererInstance->SetVariableInt("User.PointCount", pointCount);

	// Begin polling for ZeroMQ clients
	zeromq_thread = FZeroMqPollingThread::StaticInitThread();

	point_buffer = zeromq_thread->GetPointBuffer();

	// Render a flat plane until we have points...
	for (uint32_t x = 0; x < textureWidth; ++x) {
		for (uint32_t y = 0; y < textureHeight; ++y) {
			int id = x + y * textureWidth;

			positions[id * 4 + 0] = float(x) / textureWidth * 1000 - 500;  // < x
			positions[id * 4 + 1] = float(y) / textureHeight * 1000 - 500; // < y
			positions[id * 4 + 2] = 0;
			positions[id * 4 + 3] = 1; // < could be removed, currently not used.

			colors[id * 4 + 0] = 0;  // < b
			colors[id * 4 + 1] = uint8_t(float(y) / textureHeight * 255); // < g
			colors[id * 4 + 2] = uint8_t(float(x) / textureWidth * 255);  // < r
			colors[id * 4 + 3] = 10;									  // < a
		}
	}

	positionTexture->UpdateTextureRegions(0, 1, &region, textureWidth * 16, 16, (uint8*)positions);
	colorTexture->UpdateTextureRegions(0, 1, &region, textureWidth * 4, 4, (uint8*)colors);
}

void ACloudCamCharacter::BeginDestroy()
{
	Super::BeginDestroy();
}

void ACloudCamCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//rendererInstance->
	
	if (!point_buffer->CurrentlyHasPoints()) {
		UE_LOG(CloudCamCharacterLog, Log, TEXT("Tick(): Buffer currently does not have points; early exit."));
		return;
	}

	auto points = point_buffer->GetPoints();

	//UE_LOG(CloudCamCharacterLog, Log, TEXT("Tick(): Copy points"));

	uint32_t count_good = 0;
	uint32_t count_bad = 0;

	for (uint32_t x = 0; x < textureWidth; ++x) {
		for (uint32_t y = 0; y < textureHeight; ++y) {
			int id = x + y * textureWidth;

			if (id < points.Num()) {
				CloudCamPoint point = points[id];

				positions[id * 4 + 0] = point.x;
				positions[id * 4 + 1] = point.y;
				positions[id * 4 + 2] = point.z;

				colors[id * 4 + 0] = point.b; // b
				colors[id * 4 + 1] = point.g; // g
				colors[id * 4 + 2] = point.r; // r
				colors[id * 4 + 3] = point.a; // a

				//count_good += 1;

			} else {
				colors[id * 4 + 0] = 0; // b
				colors[id * 4 + 1] = 0; // g
				colors[id * 4 + 2] = 0; // r
				colors[id * 4 + 3] = 0; // a

				// Probably not necessary to move the point, but why not...
				positions[id * 4 + 0] = -300000.0;
				positions[id * 4 + 1] = -300000.0;
				positions[id * 4 + 2] = -300000.0;

				//count_bad += 1;
			}
		}
	}

	positionTexture->UpdateTextureRegions(0, 1, &region, textureWidth * 16, 16, (uint8*)positions);
	colorTexture->UpdateTextureRegions(0, 1, &region, textureWidth * 4, 4, (uint8*)colors);

	//UE_LOG(CloudCamCharacterLog, Log, TEXT("Tick(): Count good: %d, bad: %d"), count_good, count_bad);
}

