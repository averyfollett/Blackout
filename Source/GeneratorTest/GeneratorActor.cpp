// Fill out your copyright notice in the Description page of Project Settings.


#include "GeneratorActor.h"
#include "CoreUObject.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"

AGeneratorActor::AGeneratorActor()
{

}

// Sets default values
AGeneratorActor::AGeneratorActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//Enable replication
	bReplicates = true; //this allows actor to be replicated at start
	bAlwaysRelevant = true; //this forces actor to always be replicated
	bNetLoadOnClient = true; //this makes the object replicate if it is placed in the level (not spawned)
	SetReplicatingMovement(true); //this allows an object to be replicated as it is moved around

 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>("BaseMeshComponent");

	auto MeshAsset = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("Static Mesh'/Engine/BasicShapes/Cube.Cube'"));
	if (MeshAsset.Object != nullptr)
	{
		Mesh->SetStaticMesh(MeshAsset.Object);
	}

	Mesh->bEditableWhenInherited = true;
}

// Called when the game starts or when spawned
void AGeneratorActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AGeneratorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	//SetActorLocation(GetActorLocation() + FVector(1.0, 0.0, 0.0)); //just using this to test movement replication
}

void AGeneratorActor::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGeneratorActor, Mesh);
	DOREPLIFETIME(AGeneratorActor, MovementComponent);
	DOREPLIFETIME(AGeneratorActor, BoxComponent);
	DOREPLIFETIME(AGeneratorActor, StaticMesh);
}