// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnyxScreen.h"
#include "OnyxShopScreen.generated.h"

class AOnyxShop;
class UTextBlock;

/**
 * An Onyx Shop's list: each thing it sells with its price, bought one at a time with Select (if the coins are there);
 * Back leaves. The game waits while it is open.
 */
UCLASS()
class ONYX2DUI_API UOnyxShopScreen : public UOnyxScreen
{
	GENERATED_BODY()

public:
	void SetShop(AOnyxShop* InShop, AActor* InBuyer);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void Close();

protected:
	virtual void BuildUI() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnBackPressed() override { Close(); }

private:
	void BuyEntry(int32 Index);
	/** The title and the list, from the shop (again once the shop is set). */
	void FillMenu();

	TWeakObjectPtr<AOnyxShop> Shop;
	TWeakObjectPtr<AActor> Buyer;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<class UVerticalBox> MenuBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CoinsText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NoteText;

	float NoteAge = 10.f;
};
