// Copyright Onyx2D. All Rights Reserved.

#include "Combat/OnyxProjectile.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Combat/OnyxCombatLibrary.h"
#include "Combat/OnyxCombatTypes.h"
#include "Components/SphereComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "World/OnyxFlipbookEffect.h"

AOnyxProjectile::AOnyxProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(16.f);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // sweeps stop at walls and floors
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	Look = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Look"));
	Look->SetupAttachment(Collision);
	Look->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Look->SetCastShadow(false);

	Glow = CreateDefaultSubobject<UOnyxLight2DComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Collision);
	Glow->bCastShadows = false;
}

AOnyxProjectile* AOnyxProjectile::Fire(UObject* WorldContext, const FOnyxProjectileSpec& Spec, FVector From, FVector Direction, AActor* Shooter)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Instigator = Cast<APawn>(Shooter);
	AOnyxProjectile* Projectile = World->SpawnActor<AOnyxProjectile>(From, FRotator::ZeroRotator, Params);
	if (Projectile)
	{
		Projectile->Launch(Spec, Direction, Shooter);
	}
	return Projectile;
}

void AOnyxProjectile::Launch(const FOnyxProjectileSpec& InSpec, const FVector& Direction, AActor* InShooter)
{
	Spec = InSpec;
	Shooter = InShooter;
	bTopDown = UOnyx2DLibrary::IsTopDown(this);
	FVector Flat(Direction.X, 0.f, Direction.Z);
	Velocity = Flat.GetSafeNormal() * Spec.Speed;
	Collision->SetSphereRadius(Spec.Radius);
	Collision->OnComponentBeginOverlap.AddUniqueDynamic(this, &AOnyxProjectile::HandleOverlap);
	Look->SetFlipbook(Spec.Look);
	UOnyx2DLibrary::CenterFlipbook(Look);
	UOnyx2DLibrary::ConfigureSpriteLighting(Look, false);
	Glow->Color = Spec.LightColor;
	Glow->Intensity = Spec.LightIntensity;
	Glow->Radius = Spec.LightRadius;
	Glow->SetVisibility(Spec.LightIntensity > 0.f);
	GlowWhenFlying = Spec.LightIntensity;
	FaceTravel();
	UOnyxAudioSubsystem::PlaySoundAt(this, Spec.LaunchSound, GetActorLocation(), EOnyxSoundCategory::Effects);
	// Already touching someone when fired (point blank).
	TArray<AActor*> Touching;
	Collision->GetOverlappingActors(Touching);
	for (AActor* Other : Touching)
	{
		HandleOverlap(Collision, Other, nullptr, 0, false, FHitResult());
	}
}

void AOnyxProjectile::FaceTravel()
{
	if (Spec.bFaceTravel && !Velocity.IsNearlyZero())
	{
		// Pitch turns the picture in the X / Z plane of the side view.
		Look->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(FMath::Atan2(Velocity.Z, Velocity.X)), 0.f, 0.f));
	}
}

void AOnyxProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bBurst)
	{
		return;
	}
	if (bStuck)
	{
		// Landed: its light goes out at once, the shaft stays, then fades away.
		StuckFor += DeltaSeconds;
		constexpr float LightOut = 0.4f;
		constexpr float FadeTime = 0.6f;
		if (Glow->IsVisible())
		{
			Glow->Intensity = GlowWhenFlying * FMath::Clamp(1.f - StuckFor / LightOut, 0.f, 1.f);
			Glow->SetVisibility(StuckFor < LightOut);
		}
		const float Left = Spec.StickTime - StuckFor;
		if (Left < FadeTime)
		{
			Look->SetCustomPrimitiveDataFloat(Onyx2D::DissolveDataIndex, FMath::Clamp(1.f - Left / FadeTime, 0.f, 1.f));
		}
		if (Left <= 0.f)
		{
			Destroy();
		}
		return;
	}
	Age += DeltaSeconds;
	if (Age >= Spec.Lifetime)
	{
		Burst();
		return;
	}
	if (!bTopDown)
	{
		Velocity.Z += GetWorld()->GetGravityZ() * Spec.Gravity * DeltaSeconds; // from above nothing falls: shots fly straight
	}
	if (Spec.Drag > 0.f)
	{
		Velocity *= FMath::Max(1.f - Spec.Drag * DeltaSeconds, 0.f);
	}
	FHitResult Hit;
	AddActorWorldOffset(Velocity * DeltaSeconds, true, &Hit);
	if (Hit.bBlockingHit)
	{
		if (Spec.StickTime > 0.f && Spec.BurstRadius <= 0.f)
		{
			Stick(Hit);
		}
		else
		{
			Burst();
		}
		return;
	}
	FaceTravel();
}

void AOnyxProjectile::Stick(const FHitResult& Hit)
{
	const FVector Along = Velocity.GetSafeNormal();
	FaceTravel(); // pointing the way it flew in
	bStuck = true;
	StuckFor = 0.f;
	Velocity = FVector::ZeroVector;
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision); // a stuck arrow hurts nobody
	// The picture is centred on it: put its point a little way into the surface.
	const UPaperFlipbook* Picture = Look->GetFlipbook();
	const float HalfLength = Picture ? Picture->GetRenderBounds().BoxExtent.X * Look->GetComponentScale().X : Spec.Radius;
	FVector At = Hit.ImpactPoint - Along * FMath::Max(HalfLength - Spec.StickDepth, 0.f);
	At.Y = GetActorLocation().Y;
	SetActorLocation(At);
	AOnyxFlipbookEffect::Spawn(this, Spec.ImpactEffect, Hit.ImpactPoint);
	UOnyxAudioSubsystem::PlaySoundAt(this, Spec.ImpactSound, Hit.ImpactPoint, EOnyxSoundCategory::Effects);
}

void AOnyxProjectile::HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep,
	const FHitResult& Sweep)
{
	AActor* From = Shooter.Get();
	if (bBurst || !Other || Other == From || !UOnyxCombatLibrary::AreHostile(From ? From : this, Other))
	{
		return;
	}
	if (Spec.BurstRadius > 0.f)
	{
		Burst(); // a bomb: the burst hurts it, and everything else close by
		return;
	}
	FOnyxDamageInfo Info;
	Info.Amount = Spec.Damage;
	Info.Instigator = From;
	Info.Knockback = FVector((Other->GetActorLocation().X >= GetActorLocation().X ? 1.f : -1.f) * Spec.Knockback.X, 0.f, Spec.Knockback.Y);
	Info.DamageType = TEXT("Projectile");
	UOnyxCombatLibrary::ApplyDamage(Other, Info);
	Burst();
}

void AOnyxProjectile::Burst()
{
	if (bBurst)
	{
		return;
	}
	bBurst = true;
	if (Spec.BurstRadius > 0.f)
	{
		// Everything hostile within reach of the burst, once each, thrown back from its middle.
		TArray<FOverlapResult> Overlaps;
		GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeSphere(Spec.BurstRadius));
		AActor* From = Shooter.Get();
		TSet<AActor*> Struck;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Other = Overlap.GetActor();
			if (!Other || Other == From || Struck.Contains(Other) || !UOnyxCombatLibrary::AreHostile(From ? From : this, Other))
			{
				continue;
			}
			Struck.Add(Other);
			FOnyxDamageInfo Info;
			Info.Amount = Spec.Damage;
			Info.Instigator = From;
			Info.Knockback = FVector((Other->GetActorLocation().X >= GetActorLocation().X ? 1.f : -1.f) * Spec.Knockback.X, 0.f, Spec.Knockback.Y);
			Info.DamageType = TEXT("Burst");
			UOnyxCombatLibrary::ApplyDamage(Other, Info);
		}
	}
	AOnyxFlipbookEffect::Spawn(this, Spec.ImpactEffect, GetActorLocation());
	UOnyxAudioSubsystem::PlaySoundAt(this, Spec.ImpactSound, GetActorLocation(), EOnyxSoundCategory::Effects);
	Destroy();
}
