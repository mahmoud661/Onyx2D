// Copyright Onyx2D. All Rights Reserved.

#include "Examples/OnyxLearnExample.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxTileBuilder.h"
#include "Components/BoxComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Examples/OnyxSunnyLandExample.h"
#include "FileHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Input/OnyxInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Blueprint/WidgetTree.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxHealthComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "OnyxHUDWidget.h"
#include "OnyxMainMenu.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "Core/OnyxScoreSubsystem.h"
#include "GameFramework/Pawn.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_FormatText.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_GetSubsystem.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_Self.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/KismetTextLibrary.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "World/OnyxInteractable.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Level/OnyxLevelTools.h"
#include "Misc/PackageName.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "Level/OnyxMapPainter.h"
#include "ObjectTools.h"
#include "OnyxEditorLibrary.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "PaperTileMap.h"
#include "PaperTileSet.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxLearn, Log, All);

const TCHAR* FOnyxLearnExample::ContentFolder = TEXT("/Game/LearnUnreal");
const TCHAR* FOnyxLearnExample::LevelPath = TEXT("/Game/LearnUnreal/LU_Playground");
const TCHAR* FOnyxLearnExample::TalkingLevelPath = TEXT("/Game/LearnUnreal/LU_Talking");
const TCHAR* FOnyxLearnExample::TalkingBlueprintLevelPath = TEXT("/Game/LearnUnreal/LU_TalkingBlueprint");

namespace OnyxLearnBuild
{
	/** The playground's ground, one character per tile ('#' = Ground terrain): flat, with a step on the right. 36 x 14 tiles. */
	const TCHAR* const Layout[] = {
		TEXT("...................................."),
		TEXT("...................................."),
		TEXT("...................................."),
		TEXT("...................................."),
		TEXT("...................................."),
		TEXT("...................................."),
		TEXT("...................................."),
		TEXT("...................................."),
		TEXT("...................................."),
		TEXT(".............................#######"),
		TEXT(".............................#######"),
		TEXT("####################################"),
		TEXT("####################################"),
		TEXT("####################################"),
	};
	/** The last empty row over the ground: things stand on its bottom edge. */
	constexpr int32 FloorRow = 10;
	constexpr int32 BlueprintSpringColumn = 7;
	constexpr int32 CppSpringColumn = 22;
	constexpr int32 HelloColumn = 14;

	/** The "How Classes Talk" level: flat ground, a lever on the left, a door on the right, a lamp watching the door. 26 x 12. */
	const TCHAR* const TalkingLayout[] = {
		TEXT(".........................."),
		TEXT(".........................."),
		TEXT(".........................."),
		TEXT(".........................."),
		TEXT(".........................."),
		TEXT(".........................."),
		TEXT(".........................."),
		TEXT(".........................."),
		TEXT(".........................."),
		TEXT("##########################"),
		TEXT("##########################"),
		TEXT("##########################"),
	};
	constexpr int32 TalkingFloorRow = 8;
	constexpr int32 LeverColumn = 5;
	constexpr int32 DoorColumn = 16;

	template <class T>
	T* LoadAsset(const FString& Path)
	{
		return LoadObject<T>(nullptr, *(Path + TEXT(".") + FPaths::GetBaseFilename(Path)), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	void DeleteAsset(const FString& PackageName)
	{
		TArray<FAssetData> Found;
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetsByPackageName(FName(PackageName), Found);
		if (!Found.IsEmpty())
		{
			ObjectTools::DeleteAssets(Found, false);
		}
	}

	/** Deletes an asset and moves what is left of it in memory out of the way, so a new asset can take its name. */
	void ClearAssetPath(const FString& PackageName)
	{
		DeleteAsset(PackageName);
		if (UPackage* Package = FindPackage(nullptr, *PackageName))
		{
			TArray<UObject*> Left;
			GetObjectsWithOuter(Package, Left, false);
			for (UObject* Object : Left)
			{
				Object->Rename(*MakeUniqueObjectName(GetTransientPackage(), Object->GetClass(), Object->GetFName()).ToString(), GetTransientPackage(),
					REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
			}
		}
	}

	// --- Blueprint graphs, node by node ---------------------------------------------------------------------------------

	/** A new actor Blueprint with an empty Event Graph (without the greyed-out starter events a new one gets). */
	UBlueprint* NewActorBlueprint(const FString& Path)
	{
		ClearAssetPath(Path);
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), CreatePackage(*Path), FName(FPackageName::GetShortName(Path)), BPTYPE_Normal);
		if (!Blueprint)
		{
			return nullptr;
		}
		FAssetRegistryModule::AssetCreated(Blueprint);
		if (UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint))
		{
			for (UEdGraphNode* Node : TArray<UEdGraphNode*>(Graph->Nodes))
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			}
		}
		return Blueprint;
	}

	/** Places a node at (X, Y); Setup says what it is before its pins are made. */
	template <class TNode>
	TNode* AddNode(UEdGraph& Graph, int32 X, int32 Y, TFunctionRef<void(TNode&)> Setup)
	{
		FGraphNodeCreator<TNode> Creator(Graph);
		TNode* Node = Creator.CreateNode(false);
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Setup(*Node);
		Creator.Finalize();
		return Node;
	}

	UK2Node_CallFunction* AddCall(UEdGraph& Graph, int32 X, int32 Y, UClass* Class, FName Function)
	{
		return AddNode<UK2Node_CallFunction>(Graph, X, Y, [Class, Function](UK2Node_CallFunction& Node)
		{
			Node.SetFromFunction(Class->FindFunctionByName(Function));
		});
	}

	/** Wires two pins; a missing pin or a refused wire is reported in the log (the Blueprint's compile errors say the rest). */
	void Connect(UEdGraphPin* From, UEdGraphPin* To)
	{
		if (!From || !To)
		{
			UE_LOG(LogOnyxLearn, Warning, TEXT("Connect: missing pin (%s -> %s)"), From ? *From->PinName.ToString() : TEXT("none"), To ? *To->PinName.ToString() : TEXT("none"));
			return;
		}
		if (!GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(From, To))
		{
			UE_LOG(LogOnyxLearn, Warning, TEXT("Connect: could not wire %s.%s to %s.%s"), *From->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString(),
				*From->PinName.ToString(), *To->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString(), *To->PinName.ToString());
		}
	}

	/** Opens the Event Graph at 1:1 with (Left, Top) in its top-left corner, so every node is readable straight away. */
	void SetOpeningView(UBlueprint* Blueprint, UEdGraph* Graph, float Left, float Top)
	{
		Blueprint->LastEditedDocuments.Reset();
		Blueprint->LastEditedDocuments.Add(FEditedDocumentInfo(Graph, FVector2D(Left, Top), 1.f));
	}

	void SetPin(UEdGraphNode* Node, FName Pin, const FString& Value)
	{
		if (UEdGraphPin* Found = Node->FindPin(Pin))
		{
			GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Found, Value);
		}
	}

	/** Event BeginPlay -> Print String: the first Blueprint of the Blueprints page. */
	UBlueprint* MakeHelloWorld(const FString& Folder)
	{
		UBlueprint* Blueprint = NewActorBlueprint(Folder / TEXT("BP_HelloWorld"));
		UEdGraph* Graph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
		if (!Graph)
		{
			return nullptr;
		}
		UK2Node_Event* BeginPlay = AddNode<UK2Node_Event>(*Graph, 0, 0, [](UK2Node_Event& Node)
		{
			Node.EventReference.SetExternalMember(TEXT("ReceiveBeginPlay"), AActor::StaticClass());
			Node.bOverrideFunction = true;
		});
		UK2Node_CallFunction* Print = AddCall(*Graph, 300, 0, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
		Connect(BeginPlay->FindPin(UEdGraphSchema_K2::PN_Then), Print->GetExecPin());
		SetPin(Print, TEXT("InString"), TEXT("Hello from my first Blueprint!"));
		SetPin(Print, TEXT("Duration"), TEXT("10.0"));
		SetOpeningView(Blueprint, Graph, -80.f, -120.f);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return Blueprint;
	}

	/**
	 * The spring pad of the Blueprints page: a Box Collision (Trigger) with the art inside it (Sprite), a Launch Speed
	 * variable, and On Component Begin Overlap (Trigger) -> Cast To Character -> Launch Character (0, 0, Launch Speed).
	 */
	UBlueprint* MakeSpring(const FString& Folder, UPaperSprite* Art)
	{
		UBlueprint* Blueprint = NewActorBlueprint(Folder / TEXT("BP_Spring"));
		if (!Blueprint)
		{
			return nullptr;
		}
		// The box covers the art, so touching the art is touching the box.
		const FBox ArtBox = Art ? Art->GetRenderBounds().GetBox() : FBox(FVector(-32.f, -1.f, 0.f), FVector(32.f, 1.f, 48.f));
		const FVector ArtSize = ArtBox.GetSize();
		USimpleConstructionScript* Construction = Blueprint->SimpleConstructionScript;
		USCS_Node* TriggerNode = Construction->CreateNode(UBoxComponent::StaticClass(), TEXT("Trigger"));
		Construction->AddNode(TriggerNode);
		UBoxComponent* Trigger = CastChecked<UBoxComponent>(TriggerNode->ComponentTemplate);
		Trigger->SetBoxExtent(FVector(ArtSize.X * 0.4f, 50.f, ArtSize.Z * 0.5f));
		Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
		USCS_Node* SpriteNode = Construction->CreateNode(UPaperSpriteComponent::StaticClass(), TEXT("Sprite"));
		TriggerNode->AddChildNode(SpriteNode);
		UPaperSpriteComponent* Sprite = CastChecked<UPaperSpriteComponent>(SpriteNode->ComponentTemplate);
		Sprite->SetSprite(Art);
		Sprite->SetRelativeLocation(FVector(0.f, 0.f, -ArtSize.Z * 0.5f - ArtBox.Min.Z)); // the art's bottom on the box's bottom
		Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UOnyx2DLibrary::ConfigureSpriteLighting(Sprite, false); // M_OnyxLitSprite: 2D lights and the draw order

		FEdGraphPinType Float;
		Float.PinCategory = UEdGraphSchema_K2::PC_Real;
		Float.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		const FName LaunchSpeed(TEXT("LaunchSpeed"));
		FBlueprintEditorUtils::AddMemberVariable(Blueprint, LaunchSpeed, Float, TEXT("1400.0"));
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(Blueprint, LaunchSpeed, false); // Instance Editable: each copy can have its own
		FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, LaunchSpeed, nullptr, INVTEXT("Spring"), true);
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, LaunchSpeed, nullptr, TEXT("tooltip"), TEXT("How fast the spring throws you up, in cm per second."));
		FKismetEditorUtilities::CompileBlueprint(Blueprint); // the Trigger variable exists from here on

		UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
		const FObjectProperty* TriggerProperty = FindFProperty<FObjectProperty>(Blueprint->SkeletonGeneratedClass, TEXT("Trigger"));
		const FMulticastDelegateProperty* BeginOverlap = FindFProperty<FMulticastDelegateProperty>(UPrimitiveComponent::StaticClass(),
			GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, OnComponentBeginOverlap));
		if (!Graph || !TriggerProperty || !BeginOverlap)
		{
			return nullptr;
		}
		UK2Node_ComponentBoundEvent* Touched = AddNode<UK2Node_ComponentBoundEvent>(*Graph, 0, 0, [TriggerProperty, BeginOverlap](UK2Node_ComponentBoundEvent& Node)
		{
			Node.InitializeComponentBoundEventParams(TriggerProperty, BeginOverlap);
		});
		UK2Node_DynamicCast* AsCharacter = AddNode<UK2Node_DynamicCast>(*Graph, 410, 0, [](UK2Node_DynamicCast& Node)
		{
			Node.TargetType = ACharacter::StaticClass();
		});
		UK2Node_VariableGet* Speed = AddNode<UK2Node_VariableGet>(*Graph, 220, 360, [LaunchSpeed](UK2Node_VariableGet& Node)
		{
			Node.VariableReference.SetSelfMember(LaunchSpeed);
		});
		UK2Node_CallFunction* Velocity = AddCall(*Graph, 410, 250, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, MakeVector));
		UK2Node_CallFunction* Launch = AddCall(*Graph, 650, 0, ACharacter::StaticClass(), GET_FUNCTION_NAME_CHECKED(ACharacter, LaunchCharacter));
		Connect(Touched->FindPin(UEdGraphSchema_K2::PN_Then), AsCharacter->GetExecPin());
		Connect(Touched->FindPin(TEXT("OtherActor")), AsCharacter->GetCastSourcePin());
		Connect(AsCharacter->GetValidCastPin(), Launch->GetExecPin());
		Connect(AsCharacter->GetCastResultPin(), Launch->FindPin(UEdGraphSchema_K2::PN_Self));
		Connect(Speed->GetValuePin(), Velocity->FindPin(TEXT("Z")));
		Connect(Velocity->GetReturnValuePin(), Launch->FindPin(TEXT("LaunchVelocity")));
		SetPin(Launch, TEXT("bZOverride"), TEXT("true"));
		SetOpeningView(Blueprint, Graph, -10.f, -60.f);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return Blueprint;
	}

	// --- The talking Blueprints: the lever, door and lamp of "How Classes Talk", built as Blueprints ----------------------

	/** An input of a function, an event dispatcher or a custom event: its name and type. */
	struct FPinSpec
	{
		FName Name;
		FEdGraphPinType Type;
	};

	FEdGraphPinType BoolType()
	{
		FEdGraphPinType Type;
		Type.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		return Type;
	}

	FEdGraphPinType ObjectType(UClass* Class)
	{
		FEdGraphPinType Type;
		Type.PinCategory = UEdGraphSchema_K2::PC_Object;
		Type.PinSubCategoryObject = Class;
		return Type;
	}

	FEdGraphPinType ColorType()
	{
		FEdGraphPinType Type;
		Type.PinCategory = UEdGraphSchema_K2::PC_Struct;
		Type.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
		return Type;
	}

	/** A variable in the My Blueprint panel. Instance Editable ones are set on each placed copy (like EditInstanceOnly in C++). */
	void AddVariable(UBlueprint* Blueprint, FName Name, const FEdGraphPinType& Type, const FString& Default, bool bInstanceEditable,
		const FText& Category, const TCHAR* Tooltip)
	{
		FBlueprintEditorUtils::AddMemberVariable(Blueprint, Name, Type, Default);
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(Blueprint, Name, !bInstanceEditable);
		FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, Name, nullptr, Category, true);
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, Name, nullptr, TEXT("tooltip"), Tooltip);
	}

	/** Compiles, and reports the compiler's errors and warnings in the log (the examples commandlet shows them). */
	bool Compile(UBlueprint* Blueprint, FText& OutError)
	{
		FCompilerResultsLog Results;
		Results.SetSourcePath(Blueprint->GetPathName());
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
		for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
		{
			if (Message->GetSeverity() <= EMessageSeverity::Warning)
			{
				UE_LOG(LogOnyxLearn, Warning, TEXT("%s: %s"), *Blueprint->GetName(), *Message->ToText().ToString());
			}
		}
		if (Blueprint->Status == BS_Error)
		{
			OutError = FText::Format(INVTEXT("{0} has errors (see the log)."), FText::FromString(Blueprint->GetName()));
			return false;
		}
		return true;
	}

	UEdGraphPin* ThenPin(const UEdGraphNode* Node)
	{
		return Node->FindPin(UEdGraphSchema_K2::PN_Then);
	}

	UEdGraphPin* ExecPin(const UEdGraphNode* Node)
	{
		return Node->FindPin(UEdGraphSchema_K2::PN_Execute);
	}

	UEdGraphPin* SelfPin(const UEdGraphNode* Node)
	{
		return Node->FindPin(UEdGraphSchema_K2::PN_Self);
	}

	/** A pin by name and direction, or the first exec pin of that direction when Name is None (macro tunnels name theirs freely). */
	UEdGraphPin* FindPin(const UEdGraphNode* Node, FName Name, EEdGraphPinDirection Direction)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
			if (Pin->Direction == Direction && (Name.IsNone() ? bExec : Pin->PinName == Name))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	UK2Node_VariableGet* AddGet(UEdGraph& Graph, int32 X, int32 Y, FName Variable)
	{
		return AddNode<UK2Node_VariableGet>(Graph, X, Y, [Variable](UK2Node_VariableGet& Node)
		{
			Node.VariableReference.SetSelfMember(Variable);
		});
	}

	/** A variable of another class (Target pin). */
	UK2Node_VariableGet* AddGet(UEdGraph& Graph, int32 X, int32 Y, UClass* Owner, FName Variable)
	{
		return AddNode<UK2Node_VariableGet>(Graph, X, Y, [Owner, Variable](UK2Node_VariableGet& Node)
		{
			Node.VariableReference.SetExternalMember(Variable, Owner);
		});
	}

	UK2Node_VariableSet* AddSet(UEdGraph& Graph, int32 X, int32 Y, FName Variable, UClass* Owner = nullptr)
	{
		return AddNode<UK2Node_VariableSet>(Graph, X, Y, [Owner, Variable](UK2Node_VariableSet& Node)
		{
			if (Owner)
			{
				Node.VariableReference.SetExternalMember(Variable, Owner);
			}
			else
			{
				Node.VariableReference.SetSelfMember(Variable);
			}
		});
	}

	/** A call to a function of this Blueprint (made earlier and compiled). */
	UK2Node_CallFunction* AddSelfCall(UEdGraph& Graph, int32 X, int32 Y, FName Function)
	{
		return AddNode<UK2Node_CallFunction>(Graph, X, Y, [Function](UK2Node_CallFunction& Node)
		{
			Node.FunctionReference.SetSelfMember(Function);
		});
	}

	UK2Node_IfThenElse* AddBranch(UEdGraph& Graph, int32 X, int32 Y)
	{
		return AddNode<UK2Node_IfThenElse>(Graph, X, Y, [](UK2Node_IfThenElse&) {});
	}

	/** The Is Valid macro (the "?" node): goes on only when the object is set. */
	UK2Node_MacroInstance* AddIsValid(UEdGraph& Graph, int32 X, int32 Y)
	{
		UEdGraph* Macro = nullptr;
		if (const UBlueprint* Macros = LoadObject<UBlueprint>(nullptr, TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros")))
		{
			for (UEdGraph* Candidate : Macros->MacroGraphs)
			{
				Macro = Candidate->GetFName() == TEXT("IsValid") ? Candidate : Macro;
			}
		}
		return AddNode<UK2Node_MacroInstance>(Graph, X, Y, [Macro](UK2Node_MacroInstance& Node)
		{
			Node.SetMacroGraph(Macro);
		});
	}

	/** Print String with a fixed line; it also goes to the Output Log (LogBlueprintUserMessages). */
	UK2Node_CallFunction* AddPrint(UEdGraph& Graph, int32 X, int32 Y, const FString& Line)
	{
		UK2Node_CallFunction* Print = AddCall(Graph, X, Y, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
		SetPin(Print, TEXT("InString"), Line);
		SetPin(Print, TEXT("Duration"), TEXT("6.0"));
		return Print;
	}

	/** A Format Text node whose {Arguments} become pins. */
	UK2Node_FormatText* AddFormat(UEdGraph& Graph, int32 X, int32 Y, const TCHAR* Format)
	{
		UK2Node_FormatText* Node = AddNode<UK2Node_FormatText>(Graph, X, Y, [](UK2Node_FormatText&) {});
		GetDefault<UEdGraphSchema_K2>()->TrySetDefaultText(*Node->GetFormatPin(), FText::FromString(Format));
		return Node;
	}

	/** Event BeginPlay, Event End Play and other overridden AActor events. */
	UK2Node_Event* AddActorEvent(UEdGraph& Graph, int32 X, int32 Y, FName Event)
	{
		return AddNode<UK2Node_Event>(Graph, X, Y, [Event](UK2Node_Event& Node)
		{
			Node.EventReference.SetExternalMember(Event, AActor::StaticClass());
			Node.bOverrideFunction = true;
		});
	}

	/** A function graph with the given inputs (My Blueprint > Functions > +). */
	UEdGraph* AddFunction(UBlueprint* Blueprint, FName Name, TConstArrayView<FPinSpec> Inputs, UK2Node_FunctionEntry*& OutEntry)
	{
		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, Name, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, Graph, true, nullptr);
		TArray<UK2Node_FunctionEntry*> Entries;
		Graph->GetNodesOfClass(Entries);
		OutEntry = Entries.IsEmpty() ? nullptr : Entries[0];
		for (const FPinSpec& Input : Inputs)
		{
			if (OutEntry)
			{
				OutEntry->CreateUserDefinedPin(Input.Name, Input.Type, EGPD_Output, false);
			}
		}
		return Graph;
	}

	/** An Event Dispatcher (My Blueprint > Event Dispatchers > +) with the given inputs, the way the Blueprint editor makes one. */
	void AddEventDispatcher(UBlueprint* Blueprint, FName Name, TConstArrayView<FPinSpec> Inputs)
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		FEdGraphPinType Type;
		Type.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
		FBlueprintEditorUtils::AddMemberVariable(Blueprint, Name, Type);
		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, Name, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		Graph->bEditable = false;
		Schema->CreateDefaultNodesForGraph(*Graph);
		Schema->CreateFunctionGraphTerminators(*Graph, static_cast<UClass*>(nullptr));
		Schema->AddExtraFunctionFlags(Graph, FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public);
		Schema->MarkFunctionEntryAsEditable(Graph, true);
		Blueprint->DelegateSignatureGraphs.Add(Graph);
		TArray<UK2Node_FunctionEntry*> Entries;
		Graph->GetNodesOfClass(Entries);
		for (const FPinSpec& Input : Inputs)
		{
			if (!Entries.IsEmpty())
			{
				Entries[0]->CreateUserDefinedPin(Input.Name, Input.Type, EGPD_Output, false);
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	const FMulticastDelegateProperty* FindDispatcher(UClass* Class, FName Name)
	{
		return Class ? FindFProperty<FMulticastDelegateProperty>(Class, Name) : nullptr;
	}

	/** Bind Event to <dispatcher>, Unbind Event from <dispatcher> and Call <dispatcher> nodes. */
	template <class TNode>
	TNode* AddDispatcherNode(UEdGraph& Graph, int32 X, int32 Y, const FMulticastDelegateProperty* Dispatcher, UClass* Owner, bool bSelf)
	{
		return AddNode<TNode>(Graph, X, Y, [Dispatcher, Owner, bSelf](TNode& Node)
		{
			Node.SetFromProperty(Dispatcher, bSelf, Owner);
		});
	}

	/** A component of a new Blueprint: a box (the root or a child) or a sprite, in the Components panel. */
	USCS_Node* AddComponent(UBlueprint* Blueprint, UClass* Class, FName Name, USCS_Node* Parent = nullptr)
	{
		USimpleConstructionScript* Construction = Blueprint->SimpleConstructionScript;
		USCS_Node* Node = Construction->CreateNode(Class, Name);
		if (Parent)
		{
			Parent->AddChildNode(Node);
		}
		else
		{
			Construction->AddNode(Node);
		}
		return Node;
	}

	/** A box sized to the art with the art standing on its bottom, as FitToArt does for placed actors. */
	void FitTemplateToArt(UBoxComponent* Box, UPaperSpriteComponent* Sprite, UPaperSprite* Art)
	{
		const FBox ArtBox = Art->GetRenderBounds().GetBox();
		const FVector Size = ArtBox.GetSize();
		Box->SetBoxExtent(FVector(Size.X * 0.5f, 50.f, Size.Z * 0.5f));
		Sprite->SetSprite(Art);
		Sprite->SetRelativeLocation(FVector(0.f, 0.f, -Size.Z * 0.5f - ArtBox.Min.Z));
		Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UOnyx2DLibrary::ConfigureSpriteLighting(Sprite, false);
	}

	/** Sets a variable's default value (Class Defaults), after the Blueprint has compiled. */
	void SetClassDefault(UBlueprint* Blueprint, FName Name, UObject* Value)
	{
		UObject* Defaults = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
		if (FObjectPropertyBase* Property = Defaults ? FindFProperty<FObjectPropertyBase>(Defaults->GetClass(), Name) : nullptr)
		{
			Property->SetObjectPropertyValue_InContainer(Defaults, Value);
			Blueprint->MarkPackageDirty();
		}
	}

	/**
	 * BP_LearnDoor, the Blueprint ALearnDoor: a Blocker box with the art in it, an Is Open variable, an On Door Changed
	 * event dispatcher (Door, Open), and a Toggle function that flips the door and calls On Door Changed.
	 */
	UBlueprint* MakeBlueprintDoor(const FString& Folder, UPaperSprite* Closed, UPaperSprite* Open, FText& OutError)
	{
		UBlueprint* Blueprint = NewActorBlueprint(Folder / TEXT("BP_LearnDoor"));
		if (!Blueprint)
		{
			return nullptr;
		}
		USCS_Node* BlockerNode = AddComponent(Blueprint, UBoxComponent::StaticClass(), TEXT("Blocker"));
		USCS_Node* SpriteNode = AddComponent(Blueprint, UPaperSpriteComponent::StaticClass(), TEXT("Sprite"), BlockerNode);
		UBoxComponent* Blocker = CastChecked<UBoxComponent>(BlockerNode->ComponentTemplate);
		Blocker->SetCollisionProfileName(TEXT("BlockAll"));
		FitTemplateToArt(Blocker, CastChecked<UPaperSpriteComponent>(SpriteNode->ComponentTemplate), Closed);

		const FText Category = INVTEXT("Door");
		AddVariable(Blueprint, TEXT("bIsOpen"), BoolType(), TEXT("false"), false, Category, TEXT("True while the door is open."));
		AddVariable(Blueprint, TEXT("ClosedSprite"), ObjectType(UPaperSprite::StaticClass()), FString(), false, Category, TEXT("The art while closed."));
		AddVariable(Blueprint, TEXT("OpenSprite"), ObjectType(UPaperSprite::StaticClass()), FString(), false, Category, TEXT("The art while open."));
		if (!Compile(Blueprint, OutError)) // the generated class exists from here on: the dispatcher's Door input is one
		{
			return nullptr;
		}
		AddEventDispatcher(Blueprint, TEXT("OnDoorChanged"), { { TEXT("Door"), ObjectType(Blueprint->GeneratedClass) }, { TEXT("bOpen"), BoolType() } });
		if (!Compile(Blueprint, OutError))
		{
			return nullptr;
		}

		// Toggle: Is Open = NOT Is Open, then the art and the collision for the new state, then On Door Changed.
		UK2Node_FunctionEntry* Entry = nullptr;
		UEdGraph& Toggle = *AddFunction(Blueprint, TEXT("Toggle"), {}, Entry);
		UK2Node_VariableGet* WasOpen = AddGet(Toggle, 0, 150, TEXT("bIsOpen"));
		UK2Node_CallFunction* Flip = AddCall(Toggle, 160, 130, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool));
		UK2Node_VariableSet* SetOpen = AddSet(Toggle, 330, 0, TEXT("bIsOpen"));
		UK2Node_IfThenElse* Branch = AddBranch(Toggle, 560, 0);
		UK2Node_CallFunction* ShowOpen = AddCall(Toggle, 840, -220, UPaperSpriteComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(UPaperSpriteComponent, SetSprite));
		UK2Node_CallFunction* ShowClosed = AddCall(Toggle, 840, 230, UPaperSpriteComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(UPaperSpriteComponent, SetSprite));
		UK2Node_VariableGet* SpriteA = AddGet(Toggle, 660, -160, TEXT("Sprite"));
		UK2Node_VariableGet* OpenArt = AddGet(Toggle, 630, -100, TEXT("OpenSprite"));
		UK2Node_VariableGet* SpriteB = AddGet(Toggle, 660, 290, TEXT("Sprite"));
		UK2Node_VariableGet* ClosedArt = AddGet(Toggle, 620, 350, TEXT("ClosedSprite"));
		UK2Node_CallFunction* PassThrough = AddCall(Toggle, 1170, -220, UPrimitiveComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(UPrimitiveComponent, SetCollisionEnabled));
		UK2Node_CallFunction* Block = AddCall(Toggle, 1170, 230, UPrimitiveComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(UPrimitiveComponent, SetCollisionEnabled));
		UK2Node_VariableGet* BlockerA = AddGet(Toggle, 1010, -60, TEXT("Blocker"));
		UK2Node_VariableGet* BlockerB = AddGet(Toggle, 1010, 390, TEXT("Blocker"));
		UK2Node_CallFunction* SayOpen = AddPrint(Toggle, 1520, -220, TEXT("now open, calling On Door Changed"));
		UK2Node_CallFunction* SayClosed = AddPrint(Toggle, 1520, 230, TEXT("now closed, calling On Door Changed"));
		const FMulticastDelegateProperty* Changed = FindDispatcher(Blueprint->SkeletonGeneratedClass, TEXT("OnDoorChanged"));
		UK2Node_CallDelegate* Call = AddDispatcherNode<UK2Node_CallDelegate>(Toggle, 1960, 0, Changed, Blueprint->SkeletonGeneratedClass, true);
		UK2Node_Self* Self = AddNode<UK2Node_Self>(Toggle, 1790, 100, [](UK2Node_Self&) {});
		UK2Node_VariableGet* IsOpen = AddGet(Toggle, 1770, 160, TEXT("bIsOpen"));

		Connect(ThenPin(Entry), ExecPin(SetOpen));
		Connect(WasOpen->GetValuePin(), Flip->FindPin(TEXT("A")));
		Connect(Flip->GetReturnValuePin(), SetOpen->FindPin(TEXT("bIsOpen")));
		Connect(ThenPin(SetOpen), ExecPin(Branch));
		Connect(SetOpen->FindPin(TEXT("Output_Get")), Branch->GetConditionPin());
		Connect(Branch->GetThenPin(), ExecPin(ShowOpen));
		Connect(Branch->GetElsePin(), ExecPin(ShowClosed));
		Connect(SpriteA->GetValuePin(), SelfPin(ShowOpen));
		Connect(OpenArt->GetValuePin(), ShowOpen->FindPin(TEXT("NewSprite")));
		Connect(SpriteB->GetValuePin(), SelfPin(ShowClosed));
		Connect(ClosedArt->GetValuePin(), ShowClosed->FindPin(TEXT("NewSprite")));
		Connect(ThenPin(ShowOpen), ExecPin(PassThrough));
		Connect(ThenPin(ShowClosed), ExecPin(Block));
		Connect(BlockerA->GetValuePin(), SelfPin(PassThrough));
		Connect(BlockerB->GetValuePin(), SelfPin(Block));
		SetPin(PassThrough, TEXT("NewType"), TEXT("NoCollision"));
		SetPin(Block, TEXT("NewType"), TEXT("QueryAndPhysics"));
		Connect(ThenPin(PassThrough), ExecPin(SayOpen));
		Connect(ThenPin(Block), ExecPin(SayClosed));
		Connect(ThenPin(SayOpen), ExecPin(Call));
		Connect(ThenPin(SayClosed), ExecPin(Call));
		Connect(Self->FindPin(UEdGraphSchema_K2::PN_Self), Call->FindPin(TEXT("Door")));
		Connect(IsOpen->GetValuePin(), Call->FindPin(TEXT("bOpen")));
		SetOpeningView(Blueprint, &Toggle, -40.f, -280.f);
		if (!Compile(Blueprint, OutError))
		{
			return nullptr;
		}
		SetClassDefault(Blueprint, TEXT("ClosedSprite"), Closed);
		SetClassDefault(Blueprint, TEXT("OpenSprite"), Open);
		return Blueprint;
	}

	/**
	 * BP_LearnLever, the Blueprint ALearnLever: implements Onyx Interactable (Event Interact, Get Interaction Prompt =
	 * "Pull"), can be pulled by touching it, and its Pull function flips the art, casts the user to an Onyx character to
	 * print its health, and calls Toggle on its Door (an Instance Editable variable you set in the level).
	 */
	UBlueprint* MakeBlueprintLever(const FString& Folder, UBlueprint* Door, UPaperSprite* Up, UPaperSprite* Down, FText& OutError)
	{
		UBlueprint* Blueprint = NewActorBlueprint(Folder / TEXT("BP_LearnLever"));
		if (!Blueprint)
		{
			return nullptr;
		}
		USCS_Node* TriggerNode = AddComponent(Blueprint, UBoxComponent::StaticClass(), TEXT("Trigger"));
		USCS_Node* SpriteNode = AddComponent(Blueprint, UPaperSpriteComponent::StaticClass(), TEXT("Sprite"), TriggerNode);
		UBoxComponent* Trigger = CastChecked<UBoxComponent>(TriggerNode->ComponentTemplate);
		Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
		FitTemplateToArt(Trigger, CastChecked<UPaperSpriteComponent>(SpriteNode->ComponentTemplate), Up);

		const FText Category = INVTEXT("Lever");
		UClass* DoorClass = Door->GeneratedClass;
		AddVariable(Blueprint, TEXT("Door"), ObjectType(DoorClass), FString(), true, Category,
			TEXT("The door this lever works. Pick it in the level: the eyedropper next to it in Details, then click the door."));
		AddVariable(Blueprint, TEXT("bPullWhenTouched"), BoolType(), TEXT("false"), true, Category, TEXT("Also pull it by stepping on it."));
		AddVariable(Blueprint, TEXT("bDown"), BoolType(), TEXT("false"), false, Category, TEXT("True while the lever is down."));
		AddVariable(Blueprint, TEXT("UpSprite"), ObjectType(UPaperSprite::StaticClass()), FString(), false, Category, TEXT("The art while up."));
		AddVariable(Blueprint, TEXT("DownSprite"), ObjectType(UPaperSprite::StaticClass()), FString(), false, Category, TEXT("The art while down."));
		FBlueprintEditorUtils::ImplementNewInterface(Blueprint, UOnyxInteractable::StaticClass()->GetClassPathName());

		// Pull (User): flip, say who pulled it (and their health if they are an Onyx character), then Toggle the door.
		UK2Node_FunctionEntry* Entry = nullptr;
		UEdGraph& Pull = *AddFunction(Blueprint, TEXT("Pull"), { { TEXT("User"), ObjectType(AActor::StaticClass()) } }, Entry);
		if (!Compile(Blueprint, OutError))
		{
			return nullptr;
		}
		UK2Node_VariableGet* WasDown = AddGet(Pull, 0, 150, TEXT("bDown"));
		UK2Node_CallFunction* Flip = AddCall(Pull, 150, 130, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool));
		UK2Node_VariableSet* SetDown = AddSet(Pull, 330, 0, TEXT("bDown"));
		UK2Node_IfThenElse* Branch = AddBranch(Pull, 560, 0);
		UK2Node_CallFunction* ShowDown = AddCall(Pull, 840, -200, UPaperSpriteComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(UPaperSpriteComponent, SetSprite));
		UK2Node_CallFunction* ShowUp = AddCall(Pull, 840, 190, UPaperSpriteComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(UPaperSpriteComponent, SetSprite));
		UK2Node_VariableGet* SpriteA = AddGet(Pull, 660, -140, TEXT("Sprite"));
		UK2Node_VariableGet* DownArt = AddGet(Pull, 630, -80, TEXT("DownSprite"));
		UK2Node_VariableGet* SpriteB = AddGet(Pull, 660, 250, TEXT("Sprite"));
		UK2Node_VariableGet* UpArt = AddGet(Pull, 640, 310, TEXT("UpSprite"));
		UK2Node_DynamicCast* AsCharacter = AddNode<UK2Node_DynamicCast>(Pull, 330, 560, [](UK2Node_DynamicCast& Node)
		{
			Node.TargetType = AOnyxCharacter::StaticClass();
		});
		UK2Node_VariableGet* Health = AddGet(Pull, 400, 740, AOnyxCharacter::StaticClass(), TEXT("Health"));
		UK2Node_CallFunction* GetHealth = AddCall(Pull, 620, 740, UOnyxHealthComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOnyxHealthComponent, GetHealth));
		UK2Node_CallFunction* Name = AddCall(Pull, 380, 860, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, GetDisplayName));
		UK2Node_CallFunction* NameText = AddCall(Pull, 620, 860, UKismetTextLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_StringToText));
		UK2Node_FormatText* Line = AddFormat(Pull, 880, 760, TEXT("pulled by {Who}, who has {Health} health"));
		UK2Node_CallFunction* Say = AddCall(Pull, 1220, 540, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintText));
		SetPin(Say, TEXT("Duration"), TEXT("6.0"));
		UK2Node_MacroInstance* HasDoor = AddIsValid(Pull, 1620, 560);
		UK2Node_VariableGet* DoorA = AddGet(Pull, 1470, 700, TEXT("Door"));
		UK2Node_CallFunction* Toggle = AddCall(Pull, 1880, 560, DoorClass, TEXT("Toggle"));
		UK2Node_VariableGet* DoorB = AddGet(Pull, 1720, 700, TEXT("Door"));

		Connect(ThenPin(Entry), ExecPin(SetDown));
		Connect(WasDown->GetValuePin(), Flip->FindPin(TEXT("A")));
		Connect(Flip->GetReturnValuePin(), SetDown->FindPin(TEXT("bDown")));
		Connect(ThenPin(SetDown), ExecPin(Branch));
		Connect(SetDown->FindPin(TEXT("Output_Get")), Branch->GetConditionPin());
		Connect(Branch->GetThenPin(), ExecPin(ShowDown));
		Connect(Branch->GetElsePin(), ExecPin(ShowUp));
		Connect(SpriteA->GetValuePin(), SelfPin(ShowDown));
		Connect(DownArt->GetValuePin(), ShowDown->FindPin(TEXT("NewSprite")));
		Connect(SpriteB->GetValuePin(), SelfPin(ShowUp));
		Connect(UpArt->GetValuePin(), ShowUp->FindPin(TEXT("NewSprite")));
		// Two reroute knots carry the flow back to the start of the second row.
		UK2Node_Knot* WrapRight = AddNode<UK2Node_Knot>(Pull, 1140, 470, [](UK2Node_Knot&) {});
		UK2Node_Knot* WrapLeft = AddNode<UK2Node_Knot>(Pull, 250, 470, [](UK2Node_Knot&) {});
		Connect(ThenPin(ShowDown), WrapRight->GetInputPin());
		Connect(ThenPin(ShowUp), WrapRight->GetInputPin());
		Connect(WrapRight->GetOutputPin(), WrapLeft->GetInputPin());
		Connect(WrapLeft->GetOutputPin(), AsCharacter->GetExecPin());
		Connect(Entry->FindPin(TEXT("User")), AsCharacter->GetCastSourcePin());
		Connect(AsCharacter->GetCastResultPin(), SelfPin(Health));
		Connect(Health->GetValuePin(), SelfPin(GetHealth));
		Connect(Entry->FindPin(TEXT("User")), Name->FindPin(TEXT("Object")));
		Connect(Name->GetReturnValuePin(), NameText->FindPin(TEXT("InString")));
		Connect(NameText->GetReturnValuePin(), Line->FindArgumentPin(TEXT("Who")));
		Connect(GetHealth->GetReturnValuePin(), Line->FindArgumentPin(TEXT("Health")));
		Connect(AsCharacter->GetValidCastPin(), ExecPin(Say));
		Connect(Line->FindPin(TEXT("Result")), Say->FindPin(TEXT("InText")));
		Connect(ThenPin(Say), FindPin(HasDoor, NAME_None, EGPD_Input));
		Connect(AsCharacter->GetInvalidCastPin(), FindPin(HasDoor, NAME_None, EGPD_Input));
		Connect(DoorA->GetValuePin(), HasDoor->FindPin(TEXT("InputObject")));
		Connect(HasDoor->FindPin(TEXT("Is Valid")), ExecPin(Toggle));
		Connect(DoorB->GetValuePin(), SelfPin(Toggle));

		// The Event Graph: Interact (the E key, through the interface) and touching it both call Pull.
		UEdGraph& Events = *FBlueprintEditorUtils::FindEventGraph(Blueprint);
		UK2Node_Event* Interact = AddNode<UK2Node_Event>(Events, 0, 0, [](UK2Node_Event& Node)
		{
			Node.EventReference.SetExternalMember(TEXT("Interact"), UOnyxInteractable::StaticClass());
			Node.bOverrideFunction = true;
		});
		UK2Node_CallFunction* PullOnInteract = AddSelfCall(Events, 330, 0, TEXT("Pull"));
		Connect(ThenPin(Interact), ExecPin(PullOnInteract));
		Connect(Interact->FindPin(TEXT("User")), PullOnInteract->FindPin(TEXT("User")));

		const FObjectProperty* TriggerProperty = FindFProperty<FObjectProperty>(Blueprint->SkeletonGeneratedClass, TEXT("Trigger"));
		const FMulticastDelegateProperty* BeginOverlap = FindFProperty<FMulticastDelegateProperty>(UPrimitiveComponent::StaticClass(),
			GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, OnComponentBeginOverlap));
		UK2Node_ComponentBoundEvent* Touched = AddNode<UK2Node_ComponentBoundEvent>(Events, 0, 240, [TriggerProperty, BeginOverlap](UK2Node_ComponentBoundEvent& Node)
		{
			Node.InitializeComponentBoundEventParams(TriggerProperty, BeginOverlap);
		});
		UK2Node_IfThenElse* WhenTouched = AddBranch(Events, 400, 240);
		UK2Node_VariableGet* PullWhenTouched = AddGet(Events, 300, 400, TEXT("bPullWhenTouched"));
		UK2Node_DynamicCast* AsPawn = AddNode<UK2Node_DynamicCast>(Events, 640, 240, [](UK2Node_DynamicCast& Node)
		{
			Node.TargetType = APawn::StaticClass();
		});
		UK2Node_CallFunction* IsPlayer = AddCall(Events, 900, 400, APawn::StaticClass(), GET_FUNCTION_NAME_CHECKED(APawn, IsPlayerControlled));
		UK2Node_IfThenElse* ByPlayer = AddBranch(Events, 1130, 240);
		UK2Node_CallFunction* PullOnTouch = AddSelfCall(Events, 1380, 240, TEXT("Pull"));
		Connect(ThenPin(Touched), ExecPin(WhenTouched));
		Connect(PullWhenTouched->GetValuePin(), WhenTouched->GetConditionPin());
		Connect(WhenTouched->GetThenPin(), AsPawn->GetExecPin());
		Connect(Touched->FindPin(TEXT("OtherActor")), AsPawn->GetCastSourcePin());
		Connect(AsPawn->GetCastResultPin(), SelfPin(IsPlayer));
		Connect(AsPawn->GetValidCastPin(), ExecPin(ByPlayer));
		Connect(IsPlayer->GetReturnValuePin(), ByPlayer->GetConditionPin());
		Connect(ByPlayer->GetThenPin(), ExecPin(PullOnTouch));
		Connect(Touched->FindPin(TEXT("OtherActor")), PullOnTouch->FindPin(TEXT("User")));

		// Get Interaction Prompt (a function of the interface that returns a value): "Pull", shown as "E Pull".
		for (FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
		{
			for (UEdGraph* Graph : Interface.Graphs)
			{
				TArray<UK2Node_FunctionResult*> Results;
				Graph->GetNodesOfClass(Results);
				TArray<UK2Node_FunctionEntry*> Entries;
				Graph->GetNodesOfClass(Entries);
				if (Graph->GetFName() == TEXT("GetInteractionPrompt") && !Results.IsEmpty())
				{
					if (UEdGraphPin* Return = Results[0]->FindPin(TEXT("ReturnValue")))
					{
						GetDefault<UEdGraphSchema_K2>()->TrySetDefaultText(*Return, NSLOCTEXT("OnyxLearn", "LeverPrompt", "Pull"));
					}
				}
				if (!Results.IsEmpty() && !Entries.IsEmpty() && ThenPin(Entries[0]) && ThenPin(Entries[0])->LinkedTo.IsEmpty())
				{
					Connect(ThenPin(Entries[0]), ExecPin(Results[0]));
				}
			}
		}
		SetOpeningView(Blueprint, &Events, -60.f, -80.f);
		if (!Compile(Blueprint, OutError))
		{
			return nullptr;
		}
		SetClassDefault(Blueprint, TEXT("UpSprite"), Up);
		SetClassDefault(Blueprint, TEXT("DownSprite"), Down);
		return Blueprint;
	}

	/**
	 * BP_LearnLamp, the Blueprint ALearnLamp: an Onyx Light 2D that binds to its Watched Door's On Door Changed (red while
	 * closed, green while open), binds to the score subsystem's On Coins Changed, and unbinds at End Play.
	 */
	UBlueprint* MakeBlueprintLamp(const FString& Folder, UBlueprint* Door, FText& OutError)
	{
		UBlueprint* Blueprint = NewActorBlueprint(Folder / TEXT("BP_LearnLamp"));
		if (!Blueprint)
		{
			return nullptr;
		}
		USCS_Node* LightNode = AddComponent(Blueprint, UOnyxLight2DComponent::StaticClass(), TEXT("Light"));
		UOnyxLight2DComponent* Light = CastChecked<UOnyxLight2DComponent>(LightNode->ComponentTemplate);
		Light->Radius = 420.f;
		Light->Intensity = 4.f;
		Light->Color = FLinearColor(1.f, 0.15f, 0.1f);

		const FText Category = INVTEXT("Lamp");
		UClass* DoorClass = Door->GeneratedClass;
		AddVariable(Blueprint, TEXT("WatchedDoor"), ObjectType(DoorClass), FString(), true, Category, TEXT("The door to watch. Pick it in the level."));
		AddVariable(Blueprint, TEXT("ClosedColor"), ColorType(), TEXT("(R=1.000000,G=0.150000,B=0.100000,A=1.000000)"), false, Category, TEXT("The light while the door is closed."));
		AddVariable(Blueprint, TEXT("OpenColor"), ColorType(), TEXT("(R=0.200000,G=1.000000,B=0.350000,A=1.000000)"), false, Category, TEXT("The light while the door is open."));
		if (!Compile(Blueprint, OutError))
		{
			return nullptr;
		}

		// The handlers first: custom events with the dispatchers' inputs (what Add Custom Event... makes from a red pin).
		UEdGraph& Events = *FBlueprintEditorUtils::FindEventGraph(Blueprint);
		const FMulticastDelegateProperty* DoorChanged = FindDispatcher(DoorClass, TEXT("OnDoorChanged"));
		const FMulticastDelegateProperty* CoinsChanged = FindDispatcher(UOnyxScoreSubsystem::StaticClass(), GET_MEMBER_NAME_CHECKED(UOnyxScoreSubsystem, OnCoinsChanged));
		if (!DoorChanged || !CoinsChanged)
		{
			OutError = INVTEXT("BP_LearnLamp: the door or the score subsystem has no event to bind to.");
			return nullptr;
		}
		UK2Node_CustomEvent* OnDoor = UK2Node_CustomEvent::CreateFromFunction(FVector2D(0, 400), &Events, TEXT("HandleDoorChanged"), DoorChanged->SignatureFunction, false);
		UK2Node_CustomEvent* OnCoins = UK2Node_CustomEvent::CreateFromFunction(FVector2D(0, 800), &Events, TEXT("HandleCoinsChanged"), CoinsChanged->SignatureFunction, false);
		if (!Compile(Blueprint, OutError))
		{
			return nullptr;
		}

		// BeginPlay: bind to the door, match its state once, bind to the coins.
		UK2Node_Event* BeginPlay = AddActorEvent(Events, 0, 0, TEXT("ReceiveBeginPlay"));
		UK2Node_MacroInstance* HasDoor = AddIsValid(Events, 260, 0);
		UK2Node_VariableGet* DoorA = AddGet(Events, 60, 140, TEXT("WatchedDoor"));
		UK2Node_AddDelegate* BindDoor = AddDispatcherNode<UK2Node_AddDelegate>(Events, 540, 0, DoorChanged, DoorClass, false);
		UK2Node_VariableGet* DoorB = AddGet(Events, 330, 170, TEXT("WatchedDoor"));
		UK2Node_CallFunction* SyncOnce = AddSelfCall(Events, 930, 0, TEXT("HandleDoorChanged"));
		UK2Node_VariableGet* DoorC = AddGet(Events, 560, 200, TEXT("WatchedDoor"));
		UK2Node_VariableGet* DoorOpen = AddGet(Events, 740, 270, DoorClass, TEXT("bIsOpen"));
		UK2Node_GetSubsystem* Score = AddNode<UK2Node_GetSubsystem>(Events, 1120, 200, [](UK2Node_GetSubsystem& Node)
		{
			Node.Initialize(UOnyxScoreSubsystem::StaticClass());
		});
		UK2Node_AddDelegate* BindCoins = AddDispatcherNode<UK2Node_AddDelegate>(Events, 1340, 0, CoinsChanged, UOnyxScoreSubsystem::StaticClass(), false);
		Connect(ThenPin(BeginPlay), FindPin(HasDoor, NAME_None, EGPD_Input));
		Connect(DoorA->GetValuePin(), HasDoor->FindPin(TEXT("InputObject")));
		Connect(HasDoor->FindPin(TEXT("Is Valid")), ExecPin(BindDoor));
		Connect(DoorB->GetValuePin(), SelfPin(BindDoor));
		Connect(OnDoor->FindPin(UK2Node_Event::DelegateOutputName), BindDoor->GetDelegatePin());
		Connect(ThenPin(BindDoor), ExecPin(SyncOnce));
		Connect(DoorC->GetValuePin(), SyncOnce->FindPin(TEXT("Door")));
		Connect(DoorC->GetValuePin(), SelfPin(DoorOpen));
		Connect(DoorOpen->GetValuePin(), SyncOnce->FindPin(TEXT("bOpen")));
		Connect(ThenPin(SyncOnce), ExecPin(BindCoins));
		Connect(Score->FindPin(UEdGraphSchema_K2::PN_ReturnValue), SelfPin(BindCoins));
		Connect(OnCoins->FindPin(UK2Node_Event::DelegateOutputName), BindCoins->GetDelegatePin());

		// The door handler: green or red, and a line in the log.
		UK2Node_IfThenElse* Branch = AddBranch(Events, 330, 400);
		UK2Node_VariableSet* Green = AddSet(Events, 760, 330, TEXT("Color"), UOnyxLight2DComponent::StaticClass());
		UK2Node_VariableSet* Red = AddSet(Events, 760, 560, TEXT("Color"), UOnyxLight2DComponent::StaticClass());
		UK2Node_VariableGet* LightA = AddGet(Events, 580, 390, TEXT("Light"));
		UK2Node_VariableGet* GreenColor = AddGet(Events, 560, 450, TEXT("OpenColor"));
		UK2Node_VariableGet* LightB = AddGet(Events, 580, 620, TEXT("Light"));
		UK2Node_VariableGet* RedColor = AddGet(Events, 560, 680, TEXT("ClosedColor"));
		UK2Node_CallFunction* SayOpen = AddPrint(Events, 1020, 330, TEXT("heard that the door is open, light is now green"));
		UK2Node_CallFunction* SayClosed = AddPrint(Events, 1020, 560, TEXT("heard that the door is closed, light is now red"));
		Connect(ThenPin(OnDoor), ExecPin(Branch));
		Connect(OnDoor->FindPin(TEXT("bOpen")), Branch->GetConditionPin());
		Connect(Branch->GetThenPin(), ExecPin(Green));
		Connect(Branch->GetElsePin(), ExecPin(Red));
		Connect(LightA->GetValuePin(), SelfPin(Green));
		Connect(GreenColor->GetValuePin(), Green->FindPin(TEXT("Color")));
		Connect(LightB->GetValuePin(), SelfPin(Red));
		Connect(RedColor->GetValuePin(), Red->FindPin(TEXT("Color")));
		Connect(ThenPin(Green), ExecPin(SayOpen));
		Connect(ThenPin(Red), ExecPin(SayClosed));

		// The coins handler.
		UK2Node_FormatText* CoinsLine = AddFormat(Events, 300, 900, TEXT("the player has {Coins} coins now"));
		UK2Node_CallFunction* SayCoins = AddCall(Events, 640, 800, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintText));
		Connect(ThenPin(OnCoins), ExecPin(SayCoins));
		Connect(OnCoins->FindPin(TEXT("Coins")), CoinsLine->FindArgumentPin(TEXT("Coins")));
		Connect(CoinsLine->FindPin(TEXT("Result")), SayCoins->FindPin(TEXT("InText")));

		// End Play: stop watching the door.
		UK2Node_Event* EndPlay = AddActorEvent(Events, 1020, 800, TEXT("ReceiveEndPlay"));
		UK2Node_RemoveDelegate* Unbind = AddDispatcherNode<UK2Node_RemoveDelegate>(Events, 1340, 800, DoorChanged, DoorClass, false);
		UK2Node_VariableGet* DoorD = AddGet(Events, 1150, 950, TEXT("WatchedDoor"));
		Connect(ThenPin(EndPlay), ExecPin(Unbind));
		Connect(DoorD->GetValuePin(), SelfPin(Unbind));
		Connect(OnDoor->FindPin(UK2Node_Event::DelegateOutputName), Unbind->GetDelegatePin());

		SetOpeningView(Blueprint, &Events, -60.f, -80.f);
		return Compile(Blueprint, OutError) ? Blueprint : nullptr;
	}

	// --- Input ---------------------------------------------------------------------------------------------------------

	/** Onyx2D's built-in controls as assets: IA_<Action> per control, IMC_OnyxDefault with the same keys, DA_OnyxControls. */
	bool MakeInputAssets(const FString& Folder, FText& OutError)
	{
		// Held strongly: deleting the old assets below collects garbage, which would free these transient defaults.
		const TStrongObjectPtr<UOnyxInputConfig> Defaults(UOnyxInputConfig::CreateDefault(GetTransientPackage()));
		if (!Defaults || !Defaults->MappingContext)
		{
			OutError = INVTEXT("Could not make Onyx2D's default controls.");
			return false;
		}
		TArray<UPackage*> Packages;
		auto NewPackage = [&Folder, &Packages](const FString& Name)
		{
			ClearAssetPath(Folder / Name);
			UPackage* Package = CreatePackage(*(Folder / Name));
			Packages.Add(Package);
			return Package;
		};
		constexpr EObjectFlags AssetFlags = RF_Public | RF_Standalone | RF_Transactional;
		UOnyxInputConfig* Config = NewObject<UOnyxInputConfig>(NewPackage(TEXT("DA_OnyxControls")), TEXT("DA_OnyxControls"), AssetFlags);

		// Every action keeps its name and value type; the config points at the copies.
		TMap<const UInputAction*, UInputAction*> Copies;
		for (TFieldIterator<FObjectProperty> It(UOnyxInputConfig::StaticClass()); It; ++It)
		{
			const UInputAction* Action = It->PropertyClass->IsChildOf(UInputAction::StaticClass()) ? Cast<UInputAction>(It->GetObjectPropertyValue_InContainer(Defaults.Get())) : nullptr;
			if (Action)
			{
				UInputAction* Copy = DuplicateObject<UInputAction>(Action, NewPackage(Action->GetName()), Action->GetFName());
				Copy->SetFlags(AssetFlags);
				FAssetRegistryModule::AssetCreated(Copy);
				Copies.Add(Action, Copy);
				It->SetObjectPropertyValue_InContainer(Config, Copy);
			}
		}

		// The same keys and modifiers, pointing at the saved actions.
		UInputMappingContext* Context = NewObject<UInputMappingContext>(NewPackage(TEXT("IMC_OnyxDefault")), TEXT("IMC_OnyxDefault"), AssetFlags);
		for (const FEnhancedActionKeyMapping& Mapping : Defaults->MappingContext->GetMappings())
		{
			if (UInputAction* const* Copy = Copies.Find(Mapping.Action.Get()))
			{
				FEnhancedActionKeyMapping& Saved = Context->MapKey(*Copy, Mapping.Key);
				for (const UInputModifier* Modifier : Mapping.Modifiers)
				{
					Saved.Modifiers.Add(DuplicateObject<UInputModifier>(Modifier, Context));
				}
				for (const UInputTrigger* Trigger : Mapping.Triggers)
				{
					Saved.Triggers.Add(DuplicateObject<UInputTrigger>(Trigger, Context));
				}
			}
		}
		FAssetRegistryModule::AssetCreated(Context);
		Config->MappingContext = Context;
		FAssetRegistryModule::AssetCreated(Config);
		for (UPackage* Package : Packages)
		{
			Package->MarkPackageDirty(); // new packages start clean, and only dirty ones are saved
		}
		return FOnyxAssetUtils::SavePackages(Packages);
	}

	// --- The playground ------------------------------------------------------------------------------------------------

	/** Puts a spring (a Box Collision with a Paper Sprite in it) on the floor at a column, its art standing on the ground. */
	void StandOnFloor(AActor* Actor, const FOnyxMapPainter& Painter, int32 Column, int32 Row = FloorRow)
	{
		const FBox Cell = Painter.CellsToWorldBox(FIntRect(FIntPoint(Column, Row), FIntPoint(Column, Row)));
		const UBoxComponent* Box = Actor->FindComponentByClass<UBoxComponent>();
		const float HalfHeight = Box ? Box->GetScaledBoxExtent().Z : 0.f;
		Actor->SetActorLocation(FVector(Cell.GetCenter().X, 0.f, Cell.Min.Z + HalfHeight));
	}

	/** An actor made of one Box Collision and one Paper Sprite: the box covers the art, the art stands on the box bottom. */
	void FitToArt(AActor* Actor, const UPaperSprite* Art)
	{
		const FBox ArtBox = Art->GetRenderBounds().GetBox();
		const FVector Size = ArtBox.GetSize();
		if (UBoxComponent* Box = Actor->FindComponentByClass<UBoxComponent>())
		{
			Box->SetBoxExtent(FVector(Size.X * 0.5f, 50.f, Size.Z * 0.5f));
		}
		if (UPaperSpriteComponent* Sprite = Actor->FindComponentByClass<UPaperSpriteComponent>())
		{
			Sprite->SetRelativeLocation(FVector(0.f, 0.f, -Size.Z * 0.5f - ArtBox.Min.Z));
			UOnyx2DLibrary::ConfigureSpriteLighting(Sprite, false);
		}
	}

	/** Sets a UPROPERTY by name: the demo project classes are only known at run time here (there is no header to include). */
	void SetObjectProperty(UObject* Target, FName Name, UObject* Value)
	{
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Target->GetClass(), Name))
		{
			Property->SetObjectPropertyValue_InContainer(Target, Value);
		}
	}

	void SetBoolProperty(UObject* Target, FName Name, bool bValue)
	{
		if (FBoolProperty* Property = FindFProperty<FBoolProperty>(Target->GetClass(), Name))
		{
			Property->SetPropertyValue_InContainer(Target, bValue);
		}
	}

	/** A fresh level whose tile map is painted from a layout ('#' = the Ground terrain), set up for 2D with a game mode. */
	UWorld* NewPaintedLevel(const FString& LevelPath, const FString& TileMapName, TConstArrayView<const TCHAR*> Rows, UPaperTileSet* TileSet,
		int32 Ground, UClass* GameMode, FOnyxMapPainter& OutPainter, FText& OutError)
	{
		UEditorLoadingAndSavingUtils::NewBlankMap(false);
		DeleteAsset(LevelPath);
		DeleteAsset(FString(FOnyxLearnExample::ContentFolder) / TileMapName);
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World)
		{
			OutError = INVTEXT("No editor world.");
			return nullptr;
		}
		FOnyxTileMapSpec Spec;
		Spec.TileSet = TileSet;
		Spec.Name = TileMapName;
		Spec.Folder = FOnyxLearnExample::ContentFolder;
		Spec.Width = FCString::Strlen(Rows[0]);
		Spec.Height = Rows.Num();
		Spec.CentimetersPerPixel = FOnyxSunnyLandExample::PixelSize;
		Spec.bFillGround = false;
		Spec.bPlaceInLevel = true;
		UPaperTileMap* TileMap = FOnyxTileBuilder::CreateTileMap(Spec, OutError);
		if (!TileMap)
		{
			return nullptr;
		}
		FOnyxLevelTools::SetupCurrentLevel(); // game mode, player start, Onyx Lighting, tile collision
		World->GetWorldSettings()->DefaultGameMode = GameMode;
		OutPainter.SetTileMap(TileMap);
		OutPainter.BeginStroke(INVTEXT("Paint the level"));
		for (int32 Row = 0; Row < Spec.Height; ++Row)
		{
			for (int32 Column = 0; Column < Spec.Width; ++Column)
			{
				if (Rows[Row][Column] == TEXT('#'))
				{
					OutPainter.PaintTerrain(FIntPoint(Column, Row), TileSet, Ground, false);
				}
			}
		}
		OutPainter.EndStroke();
		OutPainter.FlushChanges();
		FOnyxAssetUtils::SavePackages({ TileMap->GetPackage() });
		return World;
	}

	/** Sunny Land sky behind the level, then saved. */
	bool SaveLevel(UWorld* World, const FString& LevelPath, FText& OutError)
	{
		const FString SunnyLand = FOnyxSunnyLandExample::ContentFolder;
		FOnyxSunnyLandExample::AddScenery(World, LoadAsset<UTexture2D>(SunnyLand / TEXT("Textures/T_SunnyLand_Back")),
			LoadAsset<UTexture2D>(SunnyLand / TEXT("Textures/T_SunnyLand_Middle")));
		if (!UEditorLoadingAndSavingUtils::SaveMap(World, LevelPath))
		{
			OutError = FText::Format(INVTEXT("Could not save {0}."), FText::FromString(LevelPath));
			return false;
		}
		return true;
	}

	/** The lever, door and lamp art (Sunny Land props). */
	struct FTalkingArt
	{
		UPaperSprite* DoorClosed = nullptr;
		UPaperSprite* DoorOpen = nullptr;
		UPaperSprite* CrankUp = nullptr;
		UPaperSprite* CrankDown = nullptr;

		bool IsValid() const { return DoorClosed && DoorOpen && CrankUp && CrankDown; }
	};

	/**
	 * A "How Classes Talk to Each Other" level: a lever with a direct reference to a door, and a lamp that binds to the
	 * door's event. Built twice, from the C++ classes of the demo project (Source/Planar2DDemo/Learn) and from the
	 * Blueprints above; both name their variables alike, so the same code sets them. The fox starts over the lever and
	 * lands on it, so playing shows the whole chain in the log.
	 */
	bool BuildTalkingLevel(const TCHAR* LevelPath, const TCHAR* TileMapName, UClass* DoorClass, UClass* LeverClass, UClass* LampClass,
		const FTalkingArt& Art, UPaperTileSet* TileSet, int32 Ground, UClass* GameMode, FText& OutError)
	{
		FOnyxMapPainter Painter;
		UWorld* World = NewPaintedLevel(LevelPath, TileMapName, TalkingLayout, TileSet, Ground, GameMode, Painter, OutError);
		if (!World)
		{
			return false;
		}
		AActor* Door = World->SpawnActor<AActor>(DoorClass, FTransform::Identity);
		AActor* Lever = World->SpawnActor<AActor>(LeverClass, FTransform::Identity);
		AActor* Lamp = World->SpawnActor<AActor>(LampClass, FTransform::Identity);
		if (!Door || !Lever || !Lamp)
		{
			OutError = INVTEXT("Could not place the lever, door and lamp.");
			return false;
		}
		Door->SetActorLabel(TEXT("Door"));
		SetObjectProperty(Door, TEXT("ClosedSprite"), Art.DoorClosed);
		SetObjectProperty(Door, TEXT("OpenSprite"), Art.DoorOpen);
		Door->RerunConstructionScripts();
		FitToArt(Door, Art.DoorClosed);
		StandOnFloor(Door, Painter, DoorColumn, TalkingFloorRow);

		// The references the docs tell you to pick with the eyedropper.
		Lever->SetActorLabel(TEXT("Lever"));
		SetObjectProperty(Lever, TEXT("UpSprite"), Art.CrankUp);
		SetObjectProperty(Lever, TEXT("DownSprite"), Art.CrankDown);
		SetObjectProperty(Lever, TEXT("Door"), Door);
		SetBoolProperty(Lever, TEXT("bPullWhenTouched"), true);
		Lever->RerunConstructionScripts();
		FitToArt(Lever, Art.CrankUp);
		StandOnFloor(Lever, Painter, LeverColumn, TalkingFloorRow);

		Lamp->SetActorLabel(TEXT("Lamp"));
		SetObjectProperty(Lamp, TEXT("WatchedDoor"), Door);
		Lamp->SetActorLocation(Painter.CellToWorld(FIntPoint(DoorColumn - 2, TalkingFloorRow - 1)));

		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			It->SetActorLocation(Painter.CellToWorld(FIntPoint(LeverColumn, 2)));
		}
		if (!SaveLevel(World, LevelPath, OutError))
		{
			return false;
		}
		// Night, so the lamp light shows.
		for (TActorIterator<AOnyxLightingEnvironment> It(World); It; ++It)
		{
			It->Preset = EOnyxMoodPreset::Night;
			It->Mood = FOnyxLightingMood::FromPreset(EOnyxMoodPreset::Night);
		}
		return UEditorLoadingAndSavingUtils::SaveMap(World, LevelPath);
	}

	// --- A HUD with a layout of its own ---------------------------------------------------------------------------------

	/** Adds a widget the way dragging it from the Palette does: named, under its parent panel. */
	template <class TWidget>
	TWidget* AddWidget(UWidgetBlueprint& Blueprint, UPanelWidget* Parent, const TCHAR* Name)
	{
		TWidget* Widget = Blueprint.WidgetTree->ConstructWidget<TWidget>(TWidget::StaticClass(), FName(Name));
#if WITH_EDITORONLY_DATA
		Widget->bExpandedInDesigner = true; // the Hierarchy opens with every name showing
#endif
		FText Error;
		if (!FWidgetBlueprintOperationUtils::AddWidget(&Blueprint, Widget, Parent, INDEX_NONE, Error))
		{
			UE_LOG(LogOnyxLearn, Warning, TEXT("%s: could not add %s (%s)"), *Blueprint.GetName(), Name, *Error.ToString());
		}
		return Widget;
	}

	UTextBlock* AddText(UWidgetBlueprint& Blueprint, UPanelWidget* Parent, const TCHAR* Name, const FText& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Block = AddWidget<UTextBlock>(Blueprint, Parent, Name);
		Block->SetText(Text);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
	}

	/** Anchors a widget on the Canvas Panel at a corner, sized to its content, Position away from that corner. */
	void PlaceOnCanvas(UWidget* Widget, const FVector2D& Corner, const FVector2D& Position)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(Corner.X, Corner.Y));
			CanvasSlot->SetAlignment(Corner);
			CanvasSlot->SetPosition(Position);
			CanvasSlot->SetAutoSize(true);
		}
	}

	/**
	 * WBP_LearnHUD ("Menus and HUD", a HUD with a layout of its own): a child of the Onyx HUD whose Designer puts health
	 * and dash at the bottom left, coins and items at the top right, and the Interact prompt in a dark pill. Onyx2D
	 * finds its parts by their names (PipRow, DashBar, DashFill, CoinPanel, CoinText, ItemsText, Prompt, PromptKey,
	 * PromptVerb) and fills them in; the rest (Status, CoinLabel...) is plain layout.
	 */
	UWidgetBlueprint* MakeLayoutHUD(const FString& Folder, FText& OutError)
	{
		const FString Path = Folder / TEXT("WBP_LearnHUD");
		ClearAssetPath(Path);
		UWidgetBlueprint* Blueprint = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(CreatePackage(*Path), TEXT("WBP_LearnHUD"), BPTYPE_Normal,
			UOnyxHUDWidget::StaticClass(), UCanvasPanel::StaticClass(), NAME_None, false);
		UCanvasPanel* Canvas = Blueprint ? Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget) : nullptr;
		if (!Canvas)
		{
			OutError = INVTEXT("Could not create WBP_LearnHUD.");
			return nullptr;
		}
		FAssetRegistryModule::AssetCreated(Blueprint);
#if WITH_EDITORONLY_DATA
		Canvas->bExpandedInDesigner = true;
#endif
		const FLinearColor Gold(1.f, 0.78f, 0.25f);
		const FLinearColor Shade(0.02f, 0.03f, 0.05f, 0.7f);

		// Health pips and the dash meter, bottom left.
		UVerticalBox* Status = AddWidget<UVerticalBox>(*Blueprint, Canvas, TEXT("Status"));
		PlaceOnCanvas(Status, FVector2D(0.f, 1.f), FVector2D(48.f, -40.f));
		AddWidget<UHorizontalBox>(*Blueprint, Status, TEXT("PipRow"));
		USizeBox* DashBar = AddWidget<USizeBox>(*Blueprint, Status, TEXT("DashBar"));
		DashBar->SetWidthOverride(160.f);
		DashBar->SetHeightOverride(6.f);
		if (UVerticalBoxSlot* BarSlot = Cast<UVerticalBoxSlot>(DashBar->Slot))
		{
			BarSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}
		UOverlay* DashLayers = AddWidget<UOverlay>(*Blueprint, DashBar, TEXT("DashLayers"));
		UImage* DashBack = AddWidget<UImage>(*Blueprint, DashLayers, TEXT("DashBack"));
		DashBack->SetColorAndOpacity(Shade);
		USizeBox* DashFill = AddWidget<USizeBox>(*Blueprint, DashLayers, TEXT("DashFill"));
		for (UWidget* Layer : { static_cast<UWidget*>(DashBack), static_cast<UWidget*>(DashFill) })
		{
			if (UOverlaySlot* LayerSlot = Cast<UOverlaySlot>(Layer->Slot))
			{
				LayerSlot->SetHorizontalAlignment(Layer == DashFill ? HAlign_Left : HAlign_Fill);
				LayerSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
		AddWidget<UImage>(*Blueprint, DashFill, TEXT("DashColor"))->SetColorAndOpacity(Gold);

		// Coins in a dark panel, top right, and the held items under it.
		UBorder* CoinPanel = AddWidget<UBorder>(*Blueprint, Canvas, TEXT("CoinPanel"));
		CoinPanel->SetBrushColor(Shade);
		CoinPanel->SetPadding(FMargin(14.f, 6.f));
		PlaceOnCanvas(CoinPanel, FVector2D(1.f, 0.f), FVector2D(-48.f, 40.f));
		UHorizontalBox* CoinRow = AddWidget<UHorizontalBox>(*Blueprint, CoinPanel, TEXT("CoinRow"));
		UTextBlock* CoinLabel = AddText(*Blueprint, CoinRow, TEXT("CoinLabel"), INVTEXT("COINS"), 14, Gold);
		if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(CoinLabel->Slot))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			LabelSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}
		AddText(*Blueprint, CoinRow, TEXT("CoinText"), INVTEXT("0"), 22, FLinearColor::White);
		UTextBlock* Items = AddText(*Blueprint, Canvas, TEXT("ItemsText"), FText::GetEmpty(), 14, FLinearColor(0.45f, 0.88f, 1.f));
		Items->SetJustification(ETextJustify::Right);
		PlaceOnCanvas(Items, FVector2D(1.f, 0.f), FVector2D(-48.f, 96.f));

		// The Interact prompt: a dark pill, its bottom centre over the player's head.
		UBorder* Prompt = AddWidget<UBorder>(*Blueprint, Canvas, TEXT("Prompt"));
		Prompt->SetBrushColor(Shade);
		Prompt->SetPadding(FMargin(10.f, 4.f));
		PlaceOnCanvas(Prompt, FVector2D(0.f, 0.f), FVector2D(200.f, 200.f));
		if (UCanvasPanelSlot* PromptSlot = Cast<UCanvasPanelSlot>(Prompt->Slot))
		{
			PromptSlot->SetAlignment(FVector2D(0.5f, 1.f));
		}
		UHorizontalBox* PromptRow = AddWidget<UHorizontalBox>(*Blueprint, Prompt, TEXT("PromptRow"));
		UTextBlock* PromptKey = AddText(*Blueprint, PromptRow, TEXT("PromptKey"), INVTEXT("E"), 14, Gold);
		if (UHorizontalBoxSlot* KeySlot = Cast<UHorizontalBoxSlot>(PromptKey->Slot))
		{
			KeySlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		AddText(*Blueprint, PromptRow, TEXT("PromptVerb"), INVTEXT("Use"), 14, FLinearColor::White);

		return Compile(Blueprint, OutError) ? Blueprint : nullptr;
	}

	/** Stretches a widget over the whole Canvas Panel. */
	void FillCanvas(UWidget* Widget)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			CanvasSlot->SetOffsets(FMargin(0.f));
		}
	}

	/** A Button with a label, dark so its white label reads. */
	UButton* AddMenuButton(UWidgetBlueprint& Blueprint, UPanelWidget* Parent, const TCHAR* Name, const TCHAR* LabelName, const FText& Label)
	{
		UButton* Button = AddWidget<UButton>(Blueprint, Parent, Name);
		Button->bIsVariable = true; // its On Clicked event needs it as a variable
		Button->SetBackgroundColor(FLinearColor(0.08f, 0.09f, 0.12f, 0.9f));
		if (UVerticalBoxSlot* ButtonSlot = Cast<UVerticalBoxSlot>(Button->Slot))
		{
			ButtonSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Left);
		}
		const UTextBlock* LabelText = AddText(Blueprint, Button, LabelName, Label, 20, FLinearColor::White);
		if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(LabelText->Slot))
		{
			LabelSlot->SetPadding(FMargin(28.f, 6.f));
		}
		return Button;
	}

	/**
	 * WBP_LearnTitle ("Menus and HUD", a title screen with a layout of its own): a child of the Onyx title screen whose
	 * Designer has a name, a line and two Buttons. Onyx2D fades in the parts named Title, Subtitle, Divider, Footer and
	 * FadeOverlay; the graph focuses Play when the screen opens and calls Start Game and Request Quit from the Buttons.
	 */
	UWidgetBlueprint* MakeLayoutTitle(const FString& Folder, FText& OutError)
	{
		const FString Path = Folder / TEXT("WBP_LearnTitle");
		ClearAssetPath(Path);
		UWidgetBlueprint* Blueprint = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(CreatePackage(*Path), TEXT("WBP_LearnTitle"), BPTYPE_Normal,
			UOnyxMainMenu::StaticClass(), UCanvasPanel::StaticClass(), NAME_None, false);
		UCanvasPanel* Canvas = Blueprint ? Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget) : nullptr;
		if (!Canvas)
		{
			OutError = INVTEXT("Could not create WBP_LearnTitle.");
			return nullptr;
		}
		FAssetRegistryModule::AssetCreated(Blueprint);
#if WITH_EDITORONLY_DATA
		Canvas->bExpandedInDesigner = true;
#endif
		const FLinearColor Gold(1.f, 0.78f, 0.25f);

		// Darken the level behind the menu.
		UImage* Shade = AddWidget<UImage>(*Blueprint, Canvas, TEXT("Shade"));
		Shade->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		FillCanvas(Shade);

		// The name, a line and the buttons, left of centre.
		UVerticalBox* Column = AddWidget<UVerticalBox>(*Blueprint, Canvas, TEXT("Column"));
		PlaceOnCanvas(Column, FVector2D(0.f, 0.5f), FVector2D(150.f, 0.f));
		AddText(*Blueprint, Column, TEXT("Subtitle"), INVTEXT("A LEARN UNREAL EXAMPLE"), 16, Gold);
		AddText(*Blueprint, Column, TEXT("Title"), INVTEXT("MY GAME"), 72, FLinearColor::White);
		UImage* Divider = AddWidget<UImage>(*Blueprint, Column, TEXT("Divider"));
		FSlateBrush Line = Divider->GetBrush();
		Line.ImageSize = FVector2D(320.f, 2.f);
		Divider->SetBrush(Line);
		Divider->SetColorAndOpacity(Gold);
		Divider->SetRenderTransformPivot(FVector2D(0.f, 0.5f)); // grows from its left edge
		if (UVerticalBoxSlot* LineSlot = Cast<UVerticalBoxSlot>(Divider->Slot))
		{
			LineSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 28.f));
			LineSlot->SetHorizontalAlignment(HAlign_Left);
		}
		AddMenuButton(*Blueprint, Column, TEXT("PlayButton"), TEXT("PlayLabel"), INVTEXT("PLAY"));
		AddMenuButton(*Blueprint, Column, TEXT("QuitButton"), TEXT("QuitLabel"), INVTEXT("QUIT"));

		UTextBlock* Footer = AddText(*Blueprint, Canvas, TEXT("Footer"), INVTEXT("Made with Onyx2D"), 14, FLinearColor(1.f, 1.f, 1.f, 0.6f));
		PlaceOnCanvas(Footer, FVector2D(0.f, 1.f), FVector2D(150.f, -56.f));

		// Black over everything, last so it is on top: fades out as the screen opens, back in before the game level.
		UImage* Fade = AddWidget<UImage>(*Blueprint, Canvas, TEXT("FadeOverlay"));
		Fade->SetColorAndOpacity(FLinearColor::Black);
		FillCanvas(Fade);
#if WITH_EDITORONLY_DATA
		Fade->bHiddenInDesigner = true; // the eye in the Hierarchy: hidden while designing, there in the game
#endif

		// The Buttons are variables from the first compile on; their On Clicked events need that.
		if (!Compile(Blueprint, OutError))
		{
			return nullptr;
		}
		UEdGraph* Events = FBlueprintEditorUtils::FindEventGraph(Blueprint);
		if (!Events)
		{
			OutError = INVTEXT("WBP_LearnTitle has no Event Graph.");
			return nullptr;
		}
		for (UEdGraphNode* Node : TArray<UEdGraphNode*>(Events->Nodes))
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}

		// Event Construct: keyboard and gamepad start on Play.
		UK2Node_Event* Construct = AddNode<UK2Node_Event>(*Events, 0, 0, [](UK2Node_Event& Node)
		{
			Node.EventReference.SetExternalMember(TEXT("Construct"), UUserWidget::StaticClass());
			Node.bOverrideFunction = true;
		});
		UK2Node_VariableGet* PlayForFocus = AddGet(*Events, 60, 130, TEXT("PlayButton"));
		UK2Node_CallFunction* Focus = AddCall(*Events, 300, 0, UWidget::StaticClass(), GET_FUNCTION_NAME_CHECKED(UWidget, SetKeyboardFocus));
		Connect(ThenPin(Construct), ExecPin(Focus));
		Connect(PlayForFocus->GetValuePin(), SelfPin(Focus));

		// On Clicked (PlayButton) -> Start Game, On Clicked (QuitButton) -> Request Quit.
		const FMulticastDelegateProperty* Clicked = FindFProperty<FMulticastDelegateProperty>(UButton::StaticClass(), GET_MEMBER_NAME_CHECKED(UButton, OnClicked));
		const TPair<const TCHAR*, FName> Wires[] = {
			{ TEXT("PlayButton"), GET_FUNCTION_NAME_CHECKED(UOnyxMainMenu, StartGame) },
			{ TEXT("QuitButton"), GET_FUNCTION_NAME_CHECKED(UOnyxMainMenu, RequestQuit) },
		};
		int32 Row = 240;
		for (const TPair<const TCHAR*, FName>& Wire : Wires)
		{
			FObjectProperty* ButtonProperty = FindFProperty<FObjectProperty>(Blueprint->SkeletonGeneratedClass, Wire.Key);
			if (!ButtonProperty || !Clicked)
			{
				OutError = FText::Format(INVTEXT("WBP_LearnTitle: {0} is not a variable."), FText::FromString(Wire.Key));
				return nullptr;
			}
			UK2Node_ComponentBoundEvent* OnClicked = AddNode<UK2Node_ComponentBoundEvent>(*Events, 0, Row, [ButtonProperty, Clicked](UK2Node_ComponentBoundEvent& Node)
			{
				Node.InitializeComponentBoundEventParams(ButtonProperty, Clicked);
			});
			UK2Node_CallFunction* Call = AddSelfCall(*Events, 360, Row, Wire.Value);
			Connect(ThenPin(OnClicked), ExecPin(Call));
			Row += 120;
		}
		return Compile(Blueprint, OutError) ? Blueprint : nullptr;
	}

	/** Both talking levels: the C++ one when the demo project classes are compiled in, and the Blueprint one always. */
	bool BuildTalkingLevels(UPaperTileSet* TileSet, int32 Ground, UClass* GameMode, FText& OutError)
	{
		FTalkingArt Art;
		Art.DoorClosed = FOnyxSunnyLandExample::ImportProp(TEXT("door"));
		Art.DoorOpen = FOnyxSunnyLandExample::ImportProp(TEXT("door-opened"));
		Art.CrankUp = FOnyxSunnyLandExample::ImportProp(TEXT("crank-up"));
		Art.CrankDown = FOnyxSunnyLandExample::ImportProp(TEXT("crank-down"));
		if (!Art.IsValid())
		{
			OutError = INVTEXT("Could not import the door and lever art.");
			return false;
		}

		UClass* DoorClass = LoadClass<AActor>(nullptr, TEXT("/Script/Planar2DDemo.LearnDoor"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		UClass* LeverClass = LoadClass<AActor>(nullptr, TEXT("/Script/Planar2DDemo.LearnLever"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		UClass* LampClass = LoadClass<AActor>(nullptr, TEXT("/Script/Planar2DDemo.LearnLamp"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (DoorClass && LeverClass && LampClass
			&& !BuildTalkingLevel(FOnyxLearnExample::TalkingLevelPath, TEXT("TM_Talking"), DoorClass, LeverClass, LampClass, Art, TileSet, Ground, GameMode, OutError))
		{
			return false;
		}

		const FString Folder = FOnyxLearnExample::ContentFolder;
		UBlueprint* Door = MakeBlueprintDoor(Folder, Art.DoorClosed, Art.DoorOpen, OutError);
		UBlueprint* Lever = Door ? MakeBlueprintLever(Folder, Door, Art.CrankUp, Art.CrankDown, OutError) : nullptr;
		UBlueprint* Lamp = Lever ? MakeBlueprintLamp(Folder, Door, OutError) : nullptr;
		if (!Lamp)
		{
			return false;
		}
		FOnyxAssetUtils::SavePackages({ Door->GetPackage(), Lever->GetPackage(), Lamp->GetPackage() });
		return BuildTalkingLevel(FOnyxLearnExample::TalkingBlueprintLevelPath, TEXT("TM_TalkingBlueprint"), Door->GeneratedClass, Lever->GeneratedClass,
			Lamp->GeneratedClass, Art, TileSet, Ground, GameMode, OutError);
	}

	/** Everything, in the order the pages make it. */
	bool BuildAll(FText& OutError)
	{
		const FString SunnyLand = FOnyxSunnyLandExample::ContentFolder;
		UPaperTileSet* TileSet = LoadAsset<UPaperTileSet>(SunnyLand / TEXT("TS_SunnyLand"));
		const UBlueprint* GameMode = LoadAsset<UBlueprint>(SunnyLand / TEXT("BP_SunnyLandGameMode"));
		const int32 Ground = TileSet ? UOnyxEditorLibrary::GetTerrainNames(TileSet).IndexOfByKey(TEXT("Ground")) : INDEX_NONE;
		UPaperSprite* Mushrooms = FOnyxSunnyLandExample::ImportProp(TEXT("shrooms"));
		if (!TileSet || !GameMode || Ground == INDEX_NONE || !Mushrooms)
		{
			OutError = INVTEXT("Build the Sunny Land example first: the Learn Unreal assets use its tiles, fox and art.");
			return false;
		}
		const FString Folder = FOnyxLearnExample::ContentFolder;

		// The assets the pages make.
		UBlueprint* Hello = MakeHelloWorld(Folder);
		UBlueprint* Spring = MakeSpring(Folder, Mushrooms);
		if (!Hello || !Spring)
		{
			OutError = INVTEXT("Could not build the Learn Unreal Blueprints.");
			return false;
		}
		FOnyxAssetUtils::SavePackages({ Hello->GetPackage(), Spring->GetPackage() });
		if (!MakeInputAssets(Folder / TEXT("Input"), OutError))
		{
			return false;
		}
		UWidgetBlueprint* LayoutHUD = MakeLayoutHUD(Folder, OutError);
		UWidgetBlueprint* LayoutTitle = LayoutHUD ? MakeLayoutTitle(Folder, OutError) : nullptr;
		if (!LayoutTitle)
		{
			return false;
		}
		FOnyxAssetUtils::SavePackages({ LayoutHUD->GetPackage(), LayoutTitle->GetPackage() });

		// The playground: a fresh level and tile map, painted like the Level Painter does.
		FOnyxMapPainter Painter;
		UWorld* World = NewPaintedLevel(FOnyxLearnExample::LevelPath, TEXT("TM_Playground"), Layout, TileSet, Ground, GameMode->GeneratedClass, Painter, OutError);
		if (!World)
		{
			return false;
		}

		// Both springs, the hello actor, and the fox starting over the Blueprint spring.
		if (AActor* BlueprintSpring = World->SpawnActor<AActor>(Spring->GeneratedClass, FTransform::Identity))
		{
			BlueprintSpring->SetActorLabel(TEXT("BP_Spring"));
			StandOnFloor(BlueprintSpring, Painter, BlueprintSpringColumn);
		}
		// The C++ version, when the demo project's module (Source/Planar2DDemo/SpringPad.h) is compiled in.
		if (UClass* SpringPad = LoadClass<AActor>(nullptr, TEXT("/Script/Planar2DDemo.SpringPad"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			if (AActor* CppSpring = World->SpawnActor<AActor>(SpringPad, FTransform::Identity))
			{
				CppSpring->SetActorLabel(TEXT("SpringPad"));
				const FBox ArtBox = Mushrooms->GetRenderBounds().GetBox();
				if (UBoxComponent* Box = CppSpring->FindComponentByClass<UBoxComponent>())
				{
					Box->SetBoxExtent(FVector(ArtBox.GetSize().X * 0.4f, 50.f, ArtBox.GetSize().Z * 0.5f));
				}
				if (UPaperSpriteComponent* Sprite = CppSpring->FindComponentByClass<UPaperSpriteComponent>())
				{
					Sprite->SetSprite(Mushrooms);
					Sprite->SetRelativeLocation(FVector(0.f, 0.f, -ArtBox.GetSize().Z * 0.5f - ArtBox.Min.Z));
					UOnyx2DLibrary::ConfigureSpriteLighting(Sprite, false);
				}
				StandOnFloor(CppSpring, Painter, CppSpringColumn);
			}
		}
		// The demo project's lifecycle logger (Source/Planar2DDemo/Learn), when compiled in: it logs every step of an
		// actor's life and the order of one frame (the "How Your Blueprints and Code Run" page).
		if (UClass* Logger = LoadClass<AActor>(nullptr, TEXT("/Script/Planar2DDemo.LifecycleLogger"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			if (AActor* LoggerActor = World->SpawnActor<AActor>(Logger, FTransform::Identity))
			{
				LoggerActor->SetActorLabel(TEXT("LifecycleLogger"));
				LoggerActor->SetActorLocation(Painter.CellToWorld(FIntPoint(HelloColumn + 2, FloorRow - 1)));
			}
		}
		if (AActor* HelloActor = World->SpawnActor<AActor>(Hello->GeneratedClass, FTransform::Identity))
		{
			HelloActor->SetActorLabel(TEXT("BP_HelloWorld"));
			HelloActor->SetActorLocation(Painter.CellToWorld(FIntPoint(HelloColumn, FloorRow - 1)));
		}
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			It->SetActorLocation(Painter.CellToWorld(FIntPoint(BlueprintSpringColumn, 4)));
		}
		if (!SaveLevel(World, FOnyxLearnExample::LevelPath, OutError))
		{
			return false;
		}
		const FBox PlaygroundArea = FOnyxLevelTools::GetLevelBounds2D(World);

		// The talking levels, then the playground opens again (the editor shows it when built from there).
		if (!BuildTalkingLevels(TileSet, Ground, GameMode->GeneratedClass, OutError))
		{
			return false;
		}
		FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(FOnyxLearnExample::LevelPath, FPackageName::GetMapPackageExtension()));
		FOnyxLevelTools::ShowSideView(PlaygroundArea);
		return true;
	}
}

bool FOnyxLearnExample::Build(FText& OutError)
{
	return OnyxLearnBuild::BuildAll(OutError);
}

bool FOnyxLearnExample::BuildWithSunnyLand(FText& OutError)
{
	const FString SunnyLandTiles = FString(FOnyxSunnyLandExample::ContentFolder) / TEXT("TS_SunnyLand");
	if (!FPackageName::DoesPackageExist(SunnyLandTiles) && !FOnyxSunnyLandExample::Build(OutError))
	{
		return false;
	}
	return Build(OutError);
}
