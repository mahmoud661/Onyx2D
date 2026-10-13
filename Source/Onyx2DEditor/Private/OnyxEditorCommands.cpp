// Copyright Onyx2D. All Rights Reserved.

// Editor console commands: open assets, folders, settings pages and tabs, play the level, resize tile maps, run a
// command later and save a picture of a window. They work from the Cmd box, from -ExecCmds and from exec scripts, so
// a demo, a test or a set of docs screenshots can be driven without the mouse.

#include "Level/OnyxLevelTools.h"
#include "Level/OnyxMapPainter.h"
#include "ContentBrowserModule.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "BlueprintEditor.h"
#include "BlueprintModes/WidgetBlueprintApplicationModes.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "GraphEditor.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "GameProjectGenerationModule.h"
#include "HAL/IConsoleManager.h"
#include "IContentBrowserSingleton.h"
#include "ISettingsModule.h"
#include "ImageUtils.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "Selection.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PaperTileMap.h"
#include "PaperTileMapActor.h"
#include "PaperTileMapComponent.h"
#include "PlayInEditorDataTypes.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxEditorCommands, Log, All);

namespace OnyxEditorCommands
{
	/** An asset from its path, with or without the object name ("/Game/Art/T_Hero" or "/Game/Art/T_Hero.T_Hero"). */
	UObject* LoadAsset(const FString& Path)
	{
		const FString Full = Path.Contains(TEXT(".")) ? Path : Path + TEXT(".") + FPackageName::GetShortName(Path);
		return FSoftObjectPath(Full).TryLoad();
	}

	/** The world commands run in: the game while playing in the editor, else the level being edited. */
	UWorld* GetCommandWorld()
	{
		return GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr;
	}

	/** A tile map by the name it has in the open level, or by asset path. */
	UPaperTileMap* FindTileMap(const FString& NameOrPath)
	{
		if (UWorld* World = FOnyxLevelTools::GetEditorWorld())
		{
			for (TActorIterator<APaperTileMapActor> It(World); It; ++It)
			{
				UPaperTileMap* Map = It->GetRenderComponent() ? It->GetRenderComponent()->TileMap.Get() : nullptr;
				if (Map && Map->GetName() == NameOrPath)
				{
					return Map;
				}
			}
		}
		return Cast<UPaperTileMap>(LoadAsset(NameOrPath));
	}

	/** The window to capture: the main editor window, the front-most one ("Top"), or the first whose title contains Which. */
	TSharedPtr<SWindow> FindWindow(const FString& Which)
	{
		if (Which.IsEmpty() || Which.Equals(TEXT("Main"), ESearchCase::IgnoreCase))
		{
			return FGlobalTabmanager::Get()->GetRootWindow();
		}
		TArray<TSharedRef<SWindow>> Windows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
		if (Which.Equals(TEXT("Top"), ESearchCase::IgnoreCase))
		{
			return Windows.IsEmpty() ? nullptr : TSharedPtr<SWindow>(Windows.Last());
		}
		for (const TSharedRef<SWindow>& Window : Windows)
		{
			if (Window->GetTitle().ToString().Contains(Which))
			{
				return Window;
			}
		}
		return nullptr;
	}

	FAutoConsoleCommand EditCommand(TEXT("Onyx2D.Edit"),
		TEXT("Opens an asset in its editor: Onyx2D.Edit <asset path> [Full] [Tab=<tab id>]. Full opens a Blueprint with only settings in the full ")
		TEXT("Blueprint Editor; Tab brings one of the editor's tabs to the front, e.g. Tab=SCSViewport for a Blueprint's Viewport."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UObject* Asset = Args.IsEmpty() || !GEditor ? nullptr : LoadAsset(Args[0]);
			if (!Asset)
			{
				UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.Edit <asset path> [Full]: no asset '%s'."), Args.IsEmpty() ? TEXT("") : *Args[0]);
				return;
			}
			UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
			if (Blueprint && Args.ContainsByPredicate([](const FString& Arg) { return Arg.Equals(TEXT("Full"), ESearchCase::IgnoreCase); }))
			{
				Blueprint->bForceFullEditor = true;
			}
			UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			Editors->OpenEditorForAsset(Asset);
			for (const FString& Arg : Args)
			{
				FString Tab;
				IAssetEditorInstance* Editor = Arg.Split(TEXT("="), nullptr, &Tab) && Arg.StartsWith(TEXT("Tab=")) ? Editors->FindEditorForAsset(Asset, true) : nullptr;
				const TSharedPtr<FTabManager> Tabs = Editor ? Editor->GetAssociatedTabManager() : nullptr;
				if (Tabs.IsValid() && !Tabs->TryInvokeTab(FTabId(*Tab)).IsValid())
				{
					UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.Edit: the editor has no tab '%s'."), *Tab);
				}
			}
		}));

	FAutoConsoleCommand ShowGraphCommand(TEXT("Onyx2D.ShowGraph"),
		TEXT("Opens a Blueprint at one of its graphs: Onyx2D.ShowGraph <Blueprint path> <graph name> [zoom]. Without a zoom the graph ")
		TEXT("is fitted to the window; with one (1 = 100%) its top-left node is shown at that zoom. E.g. /Game/LearnUnreal/BP_LearnLever Pull 0.8"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UBlueprint* Blueprint = Args.Num() < 2 || !GEditor ? nullptr : Cast<UBlueprint>(LoadAsset(Args[0]));
			if (!Blueprint)
			{
				UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.ShowGraph <Blueprint path> <graph name> [zoom]: no Blueprint '%s'."), Args.IsEmpty() ? TEXT("") : *Args[0]);
				return;
			}
			TArray<UEdGraph*> Graphs;
			Blueprint->GetAllGraphs(Graphs);
			UEdGraph* const* Found = Graphs.FindByPredicate([&Args](const UEdGraph* Graph) { return Graph->GetName() == Args[1]; });
			if (!Found)
			{
				UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.ShowGraph: %s has no graph '%s'."), *Blueprint->GetName(), *Args[1]);
				return;
			}
			Blueprint->bForceFullEditor = true;
			UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			Editors->OpenEditorForAsset(Blueprint);
			IAssetEditorInstance* Instance = Editors->FindEditorForAsset(Blueprint, true);
			const bool bWidgetEditor = Instance && Instance->GetEditorName() == TEXT("WidgetBlueprintEditor");
			if (!Instance || (!bWidgetEditor && Instance->GetEditorName() != TEXT("BlueprintEditor")))
			{
				return;
			}
			FBlueprintEditor* Editor = static_cast<FBlueprintEditor*>(Instance);
			if (bWidgetEditor)
			{
				Editor->SetCurrentMode(FWidgetBlueprintApplicationModes::GraphMode); // a Widget Blueprint opens on its Designer
			}
			const TSharedPtr<SGraphEditor> GraphEditor = Editor->OpenGraphAndBringToFront(*Found);
			if (!GraphEditor.IsValid())
			{
				return;
			}
			if (Args.Num() < 3)
			{
				GraphEditor->ZoomToFit(false);
				return;
			}
			FVector2f TopLeft(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
			for (const UEdGraphNode* Node : (*Found)->Nodes)
			{
				TopLeft.X = FMath::Min(TopLeft.X, float(Node->NodePosX));
				TopLeft.Y = FMath::Min(TopLeft.Y, float(Node->NodePosY));
			}
			// Room above the first row: the graph panel draws its breadcrumb bar over the top edge.
			GraphEditor->SetViewLocation(TopLeft - FVector2f(50.f, 150.f), FCString::Atof(*Args[2]));
		}));

	FAutoConsoleCommand NewCppClassCommand(TEXT("Onyx2D.NewCppClass"), TEXT("Opens the Add C++ Class dialog (Tools > New C++ Class)."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			FGameProjectGenerationModule::Get().OpenAddCodeToProjectDialog();
		}));

	FAutoConsoleCommand SelectCommand(TEXT("Onyx2D.Select"),
		TEXT("Selects actors of the level by their label (their name in the Outliner): Onyx2D.Select <label> [label...]. No label clears the selection."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
			if (!World)
			{
				return;
			}
			GEditor->SelectNone(false, true);
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (Args.Contains(It->GetActorLabel()))
				{
					GEditor->SelectActor(*It, true, false);
				}
			}
			GEditor->NoteSelectionChange();
		}));

	FAutoConsoleCommand BrowseCommand(TEXT("Onyx2D.Browse"),
		TEXT("Shows a folder or an asset in the Content Browser: Onyx2D.Browse /Game/StarterGame"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.IsEmpty())
			{
				UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.Browse <folder or asset path>"));
				return;
			}
			IContentBrowserSingleton& Browser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
			if (UObject* Asset = FPackageName::DoesPackageExist(Args[0]) ? LoadAsset(Args[0]) : nullptr)
			{
				Browser.SyncBrowserToAssets({ FAssetData(Asset) });
			}
			else
			{
				Browser.SyncBrowserToFolders({ Args[0] });
			}
		}));

	FAutoConsoleCommand ShowSettingsCommand(TEXT("Onyx2D.ShowSettings"),
		TEXT("Opens a page of the Project Settings: Onyx2D.ShowSettings <category> <section>, e.g. Project Maps, Engine Input, Plugins Onyx2DSettings. ")
		TEXT("Three words open another container: Onyx2D.ShowSettings Editor General Appearance."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			ISettingsModule* Settings = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
			if (!Settings || Args.Num() < 2)
			{
				UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.ShowSettings [container] <category> <section>"));
				return;
			}
			const bool bContainer = Args.Num() > 2;
			Settings->ShowViewer(bContainer ? FName(*Args[0]) : FName(TEXT("Project")), FName(*Args[bContainer ? 1 : 0]), FName(*Args[bContainer ? 2 : 1]));
		}));

	FAutoConsoleCommand ShowTabCommand(TEXT("Onyx2D.ShowTab"),
		TEXT("Opens an editor tab by its id: Onyx2D.ShowTab <id>, e.g. OutputLog, PluginsEditor, WorldSettingsTab, PlacementBrowser, LevelEditorSceneOutliner."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.IsEmpty())
			{
				UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.ShowTab <tab id>"));
				return;
			}
			const FTabId Tab(*Args[0]);
			// Level editor panels live in its own tab manager; windows such as the Output Log in the global one.
			const TSharedPtr<FTabManager> LevelTabs = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor").GetLevelEditorTabManager();
			if (!(LevelTabs.IsValid() && LevelTabs->HasTabSpawner(Tab.TabType) && LevelTabs->TryInvokeTab(Tab).IsValid()) && !FGlobalTabmanager::Get()->TryInvokeTab(Tab).IsValid())
			{
				UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.ShowTab: no tab '%s'."), *Args[0]);
			}
		}));

	FAutoConsoleCommand FrameLevelCommand(TEXT("Onyx2D.FrameLevel"), TEXT("Shows the whole 2D level in the side view of the level viewport."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UWorld* World = FOnyxLevelTools::GetEditorWorld())
			{
				FOnyxLevelTools::ShowSideView(FOnyxLevelTools::GetLevelBounds2D(World));
			}
		}));

	FAutoConsoleCommand ViewportCommand(TEXT("Onyx2D.Viewport"),
		TEXT("Sets up the level viewports: Onyx2D.Viewport Side | Perspective | Mode <Lit|Unlit|Wireframe> | Frame (the selection) ")
		TEXT("| Show <flag> <On|Off> (e.g. Show Collision On) | GameView <On|Off>."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UWorld* World = FOnyxLevelTools::GetEditorWorld();
			if (Args.IsEmpty() || !GEditor || !World)
			{
				UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.Viewport Side | Perspective | Mode <name> | Frame | Show <flag> <On|Off> | GameView <On|Off>"));
				return;
			}
			const FString What = Args[0];
			const bool bOn = Args.Num() < 2 || !Args.Last().Equals(TEXT("Off"), ESearchCase::IgnoreCase);
			if (What.Equals(TEXT("Side"), ESearchCase::IgnoreCase))
			{
				FOnyxLevelTools::ShowSideView(FOnyxLevelTools::GetLevelBounds2D(World));
				return;
			}
			if (What.Equals(TEXT("Frame"), ESearchCase::IgnoreCase))
			{
				TArray<AActor*> Selected;
				GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(Selected);
				if (!Selected.IsEmpty())
				{
					GEditor->MoveViewportCamerasToActor(Selected, false);
				}
				return;
			}
			const FBox Level = FOnyxLevelTools::GetLevelBounds2D(World);
			for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
			{
				if (What.Equals(TEXT("Perspective"), ESearchCase::IgnoreCase) && Level.IsValid)
				{
					// From the front, up and to the side, so the depth of the level (background layers behind) shows.
					const FVector Center = Level.GetCenter();
					const double Size = FMath::Max(Level.GetExtent().X, Level.GetExtent().Z);
					const FVector Eye = Center + FVector(-0.55 * Size, 1.9 * Size, 0.7 * Size);
					Client->SetViewportType(LVT_Perspective);
					Client->SetViewLocation(Eye);
					Client->SetViewRotation((Center - Eye).Rotation());
				}
				else if (What.Equals(TEXT("Mode"), ESearchCase::IgnoreCase) && Args.Num() > 1)
				{
					const EViewModeIndex Mode = Args[1].Equals(TEXT("Unlit"), ESearchCase::IgnoreCase) ? VMI_Unlit
						: Args[1].Equals(TEXT("Wireframe"), ESearchCase::IgnoreCase) ? VMI_BrushWireframe : VMI_Lit;
					Client->SetViewModes(Mode, Mode);
				}
				else if (What.Equals(TEXT("Show"), ESearchCase::IgnoreCase) && Args.Num() > 1)
				{
					const int32 Flag = FEngineShowFlags::FindIndexByName(*Args[1]);
					if (Flag == INDEX_NONE)
					{
						UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.Viewport Show: no show flag '%s'."), *Args[1]);
						return;
					}
					Client->EngineShowFlags.SetSingleFlag(Flag, bOn);
				}
				else if (What.Equals(TEXT("GameView"), ESearchCase::IgnoreCase))
				{
					Client->SetGameView(bOn);
				}
				Client->Invalidate();
			}
		}));

	FAutoConsoleCommand PlayCommand(TEXT("Onyx2D.Play"),
		TEXT("Plays the open level in the editor's viewport, like the Play button. Onyx2D.Stop ends it."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (GEditor && !GEditor->PlayWorld)
			{
				FRequestPlaySessionParams Params;
				FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
				Params.DestinationSlateViewport = LevelEditor.GetFirstActiveViewport();
				GEditor->RequestPlaySession(Params);
			}
		}));

	FAutoConsoleCommand StopCommand(TEXT("Onyx2D.Stop"), TEXT("Stops playing in the editor."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (GEditor && GEditor->PlayWorld)
			{
				GEditor->RequestEndPlayMap();
			}
		}));

	FAutoConsoleCommand LaterCommand(TEXT("Onyx2D.Later"),
		TEXT("Runs a console command after a delay: Onyx2D.Later <seconds> <command>. Chain several to script a demo or a set of screenshots."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 2)
			{
				UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.Later <seconds> <command>"));
				return;
			}
			const float Delay = FMath::Max(FCString::Atof(*Args[0]), 0.f);
			const FString Command = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Command](float)
			{
				if (GEngine)
				{
					GEngine->Exec(GetCommandWorld(), *Command);
				}
				return false; // once
			}), Delay);
		}));

	FAutoConsoleCommand CaptureCommand(TEXT("Onyx2D.Capture"),
		TEXT("Saves a PNG of an editor window: Onyx2D.Capture <file> [Main | Top | part of the window title]. ")
		TEXT("A relative file goes in Saved/Onyx2DScreens."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.IsEmpty() || !FSlateApplication::IsInitialized())
			{
				UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.Capture <file> [Main | Top | window title]"));
				return;
			}
			const TSharedPtr<SWindow> Window = FindWindow(Args.Num() > 1 ? FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" ")) : FString());
			TArray<FColor> Pixels;
			FIntVector Size = FIntVector::ZeroValue;
			if (!Window.IsValid() || !FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(), Pixels, Size) || Size.X <= 0)
			{
				UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.Capture: no window to capture."));
				return;
			}
			for (FColor& Pixel : Pixels)
			{
				Pixel.A = 255;
			}
			FString File = FPaths::IsRelative(Args[0]) ? FPaths::ProjectSavedDir() / TEXT("Onyx2DScreens") / Args[0] : Args[0];
			if (FPaths::GetExtension(File).IsEmpty())
			{
				File += TEXT(".png");
			}
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
			const bool bSaved = FFileHelper::SaveArrayToFile(Png, *File);
			UE_LOG(LogOnyxEditorCommands, Display, TEXT("Onyx2D.Capture: %s %s (%d x %d)"), bSaved ? TEXT("saved") : TEXT("could not save"), *File, Size.X, Size.Y);
		}));

	FAutoConsoleCommand ResizeMapCommand(TEXT("Onyx2D.ResizeMap"),
		TEXT("Grows a tile map from its edges (negative numbers shrink it), keeping everything where it is in the level: ")
		TEXT("Onyx2D.ResizeMap <tile map name or path> <left> <top> <right> <bottom>. One undo step."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UPaperTileMap* Map = Args.Num() == 5 ? FindTileMap(Args[0]) : nullptr;
			if (!Map)
			{
				UE_LOG(LogOnyxEditorCommands, Warning, TEXT("Onyx2D.ResizeMap <tile map name or path> <left> <top> <right> <bottom>: no tile map '%s'."),
					Args.IsEmpty() ? TEXT("") : *Args[0]);
				return;
			}
			FOnyxMapPainter Painter;
			Painter.SetTileMap(Map);
			FOnyxMapEdges Edges;
			Edges.Left = FCString::Atoi(*Args[1]);
			Edges.Top = FCString::Atoi(*Args[2]);
			Edges.Right = FCString::Atoi(*Args[3]);
			Edges.Bottom = FCString::Atoi(*Args[4]);
			const FIntPoint Before = Painter.GetMapSize();
			const APaperTileMapActor* Actor = Painter.FindMapActor();
			const FVector From = Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
			const bool bResized = Painter.ResizeEdges(Edges);
			UE_LOG(LogOnyxEditorCommands, Display, TEXT("Onyx2D.ResizeMap %s: %d x %d -> %d x %d%s, map actor %s -> %s"), *Map->GetName(), Before.X, Before.Y,
				Painter.GetMapSize().X, Painter.GetMapSize().Y,
				bResized ? TEXT("") : (Edges.IsZero() ? TEXT(" (nothing to change)") : TEXT(" (unchanged: the size must stay 1..1024)")),
				*From.ToCompactString(), Actor ? *Actor->GetActorLocation().ToCompactString() : TEXT("(not in the level)"));
		}));
}
