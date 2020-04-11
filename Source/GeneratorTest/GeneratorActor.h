// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeneratorActor.generated.h"

UCLASS()
class GENERATORTEST_API AGeneratorActor : public AActor
{
	GENERATED_BODY()
	
public:
	// Default constructor
	AGeneratorActor();

	// Initialize for replication
	AGeneratorActor(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(replicated, EditAnywhere, BlueprintReadWrite, Category = "Mesh")
		UStaticMeshComponent* Mesh;

	// Movement component for handling projectile movement.
	UPROPERTY(replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Components")
		class UMovementComponent* MovementComponent;

	// Box component used to test collision.
	UPROPERTY(replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Components")
		class UBoxComponent* BoxComponent;

	// Static Mesh used to provide a visual representation of the object.
	UPROPERTY(replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Components")
		class UStaticMeshComponent* StaticMesh;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

};
