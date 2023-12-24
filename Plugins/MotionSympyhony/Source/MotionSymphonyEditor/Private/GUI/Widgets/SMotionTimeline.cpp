//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "SMotionTimeline.h"
#include "Widgets/SWidget.h"
#include "SMotionOutliner.h"
#include "SMotionTrackArea.h"
#include "Controls/MotionTimeSliderController.h"
#include "Controls/MotionModel_AnimSequenceBase.h"
#include "Controls/MotionModel_AnimComposite.h"
#include "Controls/MotionModel_BlendSpace.h"
#include "Widgets/Layout/SSplitter.h"
#include "SMotionTimelineOverlay.h"
#include "SMotionTimelineSplitterOverlay.h"
#include "ISequencerWidgetsModule.h"
#include "FrameNumberNumericInterface.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "EditorStyleSet.h"
#include "Fonts/FontMeasure.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/Application/SlateApplication.h"
#include "Modules/ModuleManager.h"
#include "Preferences/PersonaOptions.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "MovieSceneFwd.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SSpinBox.h"
#include "SMotionTimelineTransportControls.h"
#include "Data/MotionAnimAsset.h"
#include "Objects/MotionAnimObject.h"
#include "Toolkits/MotionPreProcessToolkit.h"
#if ENGINE_MINOR_VERSION > 2
#include "TimeSliderArgs.h"
#endif
#define LOCTEXT_NAMESPACE "SMotionTimeline"

// FFrameRate::ComputeGridSpacing doesnt deal well with prime numbers, so we have a custom impl here
static bool ComputeGridSpacing(const FFrameRate& InFrameRate, float PixelsPerSecond, double& OutMajorInterval, int32& OutMinorDivisions, float MinTickPx, float DesiredMajorTickPx)
{
	// First try built-in spacing
	const bool bResult = InFrameRate.ComputeGridSpacing(PixelsPerSecond, OutMajorInterval, OutMinorDivisions, MinTickPx, DesiredMajorTickPx);
	if (!bResult || OutMajorInterval == 1.0)
	{
		if (PixelsPerSecond <= 0.f)
		{
			return false;
		}

		const int32 RoundedFPS = FMath::RoundToInt(InFrameRate.AsDecimal());

		if (RoundedFPS > 0)
		{
			// Showing frames
			TArray<int32, TInlineAllocator<10>> CommonBases;

			// Divide the rounded frame rate by 2s, 3s or 5s recursively
			{
				const int32 Denominators[] = { 2, 3, 5 };

				int32 LowestBase = RoundedFPS;
				for (;;)
				{
					CommonBases.Add(LowestBase);

					if (LowestBase % 2 == 0) { LowestBase = LowestBase / 2; }
					else if (LowestBase % 3 == 0) { LowestBase = LowestBase / 3; }
					else if (LowestBase % 5 == 0) { LowestBase = LowestBase / 5; }
					else
					{
						int32 LowestResult = LowestBase;
						for (int32 Denominator : Denominators)
						{
							const int32 Result = LowestBase / Denominator;
							if (Result > 0 && Result < LowestResult)
							{
								LowestResult = Result;
							}
						}

						if (LowestResult < LowestBase)
						{
							LowestBase = LowestResult;
						}
						else
						{
							break;
						}
					}
				}
			}

			Algo::Reverse(CommonBases);

			const int32 Scale = FMath::CeilToInt(DesiredMajorTickPx / PixelsPerSecond * InFrameRate.AsDecimal());
			const int32 BaseIndex = FMath::Min(Algo::LowerBound(CommonBases, Scale), CommonBases.Num() - 1);
			const int32 Base = CommonBases[BaseIndex];

			const int32 MajorIntervalFrames = FMath::CeilToInt(Scale / static_cast<float>(Base)) * Base;
			OutMajorInterval = MajorIntervalFrames * InFrameRate.AsInterval();

			// Find the lowest number of divisions we can show that's larger than the minimum tick size
			OutMinorDivisions = 0;
			for (int32 DivIndex = 0; DivIndex < BaseIndex; ++DivIndex)
			{
				if (Base % CommonBases[DivIndex] == 0)
				{
					const int32 MinorDivisions = MajorIntervalFrames / CommonBases[DivIndex];
					if (OutMajorInterval / MinorDivisions * PixelsPerSecond >= MinTickPx)
					{
						OutMinorDivisions = MinorDivisions;
						break;
					}
				}
			}
		}
	}

	return OutMajorInterval != 0;
}

void SMotionTimeline::Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList, TWeakPtr<FMotionPreProcessToolkit> InMotionPreProcessToolkitPtr)
{
	MotionPreProcessToolkitPtr = InMotionPreProcessToolkitPtr;
	OnReceivedFocus = InArgs._OnReceivedFocus;
	WeakCommandList = TWeakPtr<FUICommandList>(InCommandList);
}

void SMotionTimeline::Rebuild()
{
	if (!Model.IsValid())
	{
		SetVisibility(EVisibility::Hidden);
		return;
	}

	int32 TickResolutionValue = Model->GetTickResolution();
	int32 SequenceFrameRate = Model->GetFrameRate();

	DebugSkelMeshComponent->PreviewInstance->AddKeyCompleteDelegate(FSimpleDelegate::CreateSP(this, &SMotionTimeline::HandleKeyComplete));

	TWeakPtr<FMotionModel> WeakModel = Model;
	ViewRange = MakeAttributeLambda([WeakModel]() { return WeakModel.IsValid() ? WeakModel.Pin()->GetViewRange() : FAnimatedRange(0.0, 30.0); });

	TAttribute<EFrameNumberDisplayFormats> DisplayFormat = MakeAttributeLambda([]()
		{
			return GetDefault<UPersonaOptions>()->TimelineDisplayFormat;
		});

	TAttribute<EFrameNumberDisplayFormats> DisplayFormatSecondary = MakeAttributeLambda([]()
		{
			return GetDefault<UPersonaOptions>()->TimelineDisplayFormat == EFrameNumberDisplayFormats::Frames ? EFrameNumberDisplayFormats::Seconds : EFrameNumberDisplayFormats::Frames;
		});

	TAttribute<FFrameRate> TickResolution = MakeAttributeLambda([TickResolutionValue]()
		{
			return FFrameRate(TickResolutionValue, 1);
		});

	TAttribute<FFrameRate> DisplayRate = MakeAttributeLambda([SequenceFrameRate]()
		{
			return FFrameRate(SequenceFrameRate, 1);
		});

	// Create our numeric type interface so we can pass it to the time slider below.
	NumericTypeInterface = MakeShareable(new FFrameNumberInterface(DisplayFormat, 0, TickResolution, DisplayRate));
	SecondaryNumericTypeInterface = MakeShareable(new FFrameNumberInterface(DisplayFormatSecondary, 0, TickResolution, DisplayRate));

	FTimeSliderArgs TimeSliderArgs;
	{
		TimeSliderArgs.ScrubPosition = MakeAttributeLambda([WeakModel]() { return WeakModel.IsValid() ? WeakModel.Pin()->GetScrubPosition() : FFrameTime(0); });
		TimeSliderArgs.ViewRange = ViewRange;
		TimeSliderArgs.PlaybackRange = MakeAttributeLambda([WeakModel]() { return WeakModel.IsValid() ? WeakModel.Pin()->GetPlaybackRange() : TRange<FFrameNumber>(0, 0); });
		TimeSliderArgs.ClampRange = MakeAttributeLambda([WeakModel]() { return WeakModel.IsValid() ? WeakModel.Pin()->GetWorkingRange() : FAnimatedRange(0.0, 0.0); });
		TimeSliderArgs.DisplayRate = DisplayRate;
		TimeSliderArgs.TickResolution = TickResolution;
		TimeSliderArgs.OnViewRangeChanged = FOnViewRangeChanged::CreateSP(this, &SMotionTimeline::HandleViewRangeChanged);
		TimeSliderArgs.OnClampRangeChanged = FOnTimeRangeChanged::CreateSP(this, &SMotionTimeline::HandleWorkingRangeChanged);
		TimeSliderArgs.IsPlaybackRangeLocked = true;
		TimeSliderArgs.PlaybackStatus = EMovieScenePlayerStatus::Stopped;
		TimeSliderArgs.NumericTypeInterface = NumericTypeInterface;
		TimeSliderArgs.OnScrubPositionChanged = FOnScrubPositionChanged::CreateSP(this, &SMotionTimeline::HandleScrubPositionChanged);
	}

	TimeSliderController = MakeShareable(new FMotionTimeSliderController(TimeSliderArgs, 
		Model.ToSharedRef(), SharedThis(this), SecondaryNumericTypeInterface));

	TSharedRef<FMotionTimeSliderController> TimeSliderControllerRef = TimeSliderController.ToSharedRef();

	// Create the top slider
	const bool bMirrorLabels = false;
	ISequencerWidgetsModule& SequencerWidgets = FModuleManager::Get().LoadModuleChecked<ISequencerWidgetsModule>("SequencerWidgets");
	TopTimeSlider = SequencerWidgets.CreateTimeSlider(TimeSliderControllerRef, bMirrorLabels);

	// Create bottom time range slider
	TSharedRef<ITimeSlider> BottomTimeRange = SequencerWidgets.CreateTimeRange(
		FTimeRangeArgs(
			EShowRange::ViewRange | EShowRange::WorkingRange | EShowRange::PlaybackRange,
			EShowRange::ViewRange | EShowRange::WorkingRange,
			TimeSliderControllerRef,
			EVisibility::Visible,
			NumericTypeInterface.ToSharedRef()
		),
		SequencerWidgets.CreateTimeRangeSlider(TimeSliderControllerRef)
	);

	TSharedRef<SScrollBar> ScrollBar = SNew(SScrollBar)
		.Thickness(FVector2D(5.0f, 5.0f));

	Model->RefreshTracks();

	TrackArea = SNew(SMotionTrackArea, Model.ToSharedRef(), TimeSliderControllerRef);
	Outliner = SNew(SMotionOutliner, Model.ToSharedRef(), TrackArea.ToSharedRef())
		.ExternalScrollbar(ScrollBar)
		.Clipping(EWidgetClipping::ClipToBounds)
		.FilterText_Lambda([this]() { return FilterText; });

	TrackArea->SetOutliner(Outliner);

	ColumnFillCoefficients[0] = 0.2f;
	ColumnFillCoefficients[1] = 0.8f;

	TAttribute<float> FillCoefficient_0, FillCoefficient_1;
	{
		FillCoefficient_0.Bind(TAttribute<float>::FGetter::CreateSP(this, &SMotionTimeline::GetColumnFillCoefficient, 0));
		FillCoefficient_1.Bind(TAttribute<float>::FGetter::CreateSP(this, &SMotionTimeline::GetColumnFillCoefficient, 1));
	}

	const int32 Column0 = 0, Column1 = 1;
	const int32 Row0 = 0, Row1 = 1, Row2 = 2, Row3 = 3, Row4 = 4;

	const float CommonPadding = 3.f;
	const FMargin ResizeBarPadding(4.f, 0, 0, 0);\

	ChildSlot
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SGridPanel)
					.FillRow(1, 1.0f)
					.FillColumn(0, FillCoefficient_0)
					.FillColumn(1, FillCoefficient_1)

					//Outliner Search Box
					+SGridPanel::Slot(Column0, Row0, SGridPanel::Layer(10))
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SAssignNew(SearchBox, SSearchBox)
							.HintText(LOCTEXT("FilterTracksHint", "Filter"))
							.OnTextChanged(this, &SMotionTimeline::OnOutlinerSearchChanged)
						]
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.AutoWidth()
						.Padding(2.0f, 0.0f, 2.0f, 0.0f)
						[
							SNew(SBox)
							.MinDesiredWidth(30.0f)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							[
								//Current Play Time
								SNew(SSpinBox<double>)
								.Style(&FAppStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.PlayTimeSpinBox"))
								.Value_Lambda([this]() -> double
								{
									return Model.IsValid() ? Model->GetScrubPosition().Value : 0.0;
								})
								.OnValueChanged(this, &SMotionTimeline::SetPlayTime)
								.OnValueCommitted_Lambda([this](double InFrame, ETextCommit::Type)
								{
									SetPlayTime(InFrame);
								})
								.MinValue(TOptional<double>())
								.MaxValue(TOptional<double>())
								.TypeInterface(NumericTypeInterface)
								.Delta(this, &SMotionTimeline::GetSpinboxDelta)
								.LinearDeltaSensitivity(25)
							]
						]
					]

					//Main Timeline Area
					+SGridPanel::Slot(Column0, Row1, SGridPanel::Layer(10))
					.ColumnSpan(2)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								//SNew(SBorder)
								SNew(SScrollBorder, Outliner.ToSharedRef())
								[
									SNew(SHorizontalBox)

									//Outliner Tree
									+SHorizontalBox::Slot()
									.FillWidth(FillCoefficient_0)
									[
										SNew(SBox)
										[
											Outliner.ToSharedRef()
										]
									]

									//Track Area
									+SHorizontalBox::Slot()
									.FillWidth(FillCoefficient_1)
									[
										SNew(SBox)
										.Padding(ResizeBarPadding)
										.Clipping(EWidgetClipping::ClipToBounds)
										[
											TrackArea.ToSharedRef()
										]
									]
								]
							]

							+SOverlay::Slot()
							.HAlign(HAlign_Right)
							[
								ScrollBar
							]
						]
					]

					//Transport Control
					+SGridPanel::Slot(Column0, Row3, SGridPanel::Layer(10))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SMotionTimelineTransportControls, DebugSkelMeshComponent, MotionAnim->AnimAsset, MotionAnim->MotionAnimAssetType)
					]

					//Second Column
					+SGridPanel::Slot(Column1, Row0)
					.Padding(ResizeBarPadding)
					.RowSpan(2)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						[
							SNew(SSpacer)
						]
					]

					//Top Time Slider
					+ SGridPanel::Slot(Column1, Row0, SGridPanel::Layer(10))
						.Padding(ResizeBarPadding)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
							.BorderBackgroundColor(FLinearColor(.50f, .50f, .50f, 1.0f))
							.Padding(0)
							.Clipping(EWidgetClipping::ClipToBounds)
							[
								TopTimeSlider.ToSharedRef()
							]
						]

					//Overlay that draws the tick lines
					+SGridPanel::Slot(Column1, Row1, SGridPanel::Layer(10))
					.Padding(ResizeBarPadding)
					[
						SNew(SMotionTimelineOverlay, TimeSliderControllerRef)
						.Visibility(EVisibility::HitTestInvisible)
						.DisplayScrubPosition(false)
						.DisplayTickLines(true)
						.Clipping(EWidgetClipping::ClipToBounds)
						.PaintPlaybackRangeArgs(FPaintPlaybackRangeArgs(FAppStyle::GetBrush(
							"Sequencer.Timeline.PlayRange_L"), FAppStyle::GetBrush(
								"Sequencer.Timeline.PlayRange_R"), 6.f))
					]

					//Overlay that draws the scrub position
					+SGridPanel::Slot(Column1, Row1, SGridPanel::Layer(20))
					.Padding(ResizeBarPadding)
					[
						SNew(SMotionTimelineOverlay, TimeSliderControllerRef)
						.Visibility(EVisibility::HitTestInvisible)
						.DisplayScrubPosition(true)
						.DisplayTickLines(false)
						.Clipping(EWidgetClipping::ClipToBounds)
					]

					//Play Range Slider
					+SGridPanel::Slot(Column1, Row3, SGridPanel::Layer(10))
					.Padding(ResizeBarPadding)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f))
						.Clipping(EWidgetClipping::ClipToBounds)
						.Padding(0)
						[
							BottomTimeRange
						]
					]
				]
				+SOverlay::Slot()
				[
					SNew(SMotionTimelineSplitterOverlay)
					.Style(FAppStyle::Get(), "AnimTimeline.Outliner.Splitter")
					.Visibility(EVisibility::SelfHitTestInvisible)

					+ SSplitter::Slot()
					.Value(FillCoefficient_0)
					.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SMotionTimeline::OnColumnFillCoefficientChanged, 0))
					[
						SNew(SSpacer)
					]

					+SSplitter::Slot()
					.Value(FillCoefficient_1)
					.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SMotionTimeline::OnColumnFillCoefficientChanged, 1))
					[
						SNew(SSpacer)
					]
				]
			]
		]
	];

	SetVisibility(EVisibility::All);
}

void SMotionTimeline::DestroyTimeline()
{
	SetVisibility(EVisibility::Hidden);

	ChildSlot
	[
		SNew(SOverlay)
	];
}

FReply SMotionTimeline::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	 if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	 {
		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

		const bool bCloseAfterSelection = true;
		FMenuBuilder MenuBuilder(bCloseAfterSelection, Model->GetCommandList());

		MenuBuilder.BeginSection("SnapOptions", LOCTEXT("SnapOptions", "Snapping"));
		{
			//MenuBuilder.AddMenuEntry(FAnimSequenceTimelineCommands::Get().SnapToFrames);
			//MenuBuilder.AddMenuEntry(FAnimSequenceTimelineCommands::Get().SnapToNotifies);
			//MenuBuilder.AddMenuEntry(FAnimSequenceTimelineCommands::Get().SnapToCompositeSegments);
			//MenuBuilder.AddMenuEntry(FAnimSequenceTimelineCommands::Get().SnapToMontageSections);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("TimelineOptions", LOCTEXT("TimelineOptions", "Timeline Options"));
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("TimeFormat", "Time Format"),
				LOCTEXT("TimeFormatTooltip", "Choose the format of times we display in the timeline"),
				FNewMenuDelegate::CreateLambda([](FMenuBuilder& InMenuBuilder)
					{
						InMenuBuilder.BeginSection("TimeFormat", LOCTEXT("TimeFormat", "Time Format"));
						{
							//InMenuBuilder.AddMenuEntry(FAnimSequenceTimelineCommands::Get().DisplaySeconds);
							//InMenuBuilder.AddMenuEntry(FAnimSequenceTimelineCommands::Get().DisplayFrames);
						}
						InMenuBuilder.EndSection();

						InMenuBuilder.BeginSection("TimelineAdditional", LOCTEXT("TimelineAdditional", "Additional Display"));
						{
							//InMenuBuilder.AddMenuEntry(FAnimSequenceTimelineCommands::Get().DisplayPercentage);
							//InMenuBuilder.AddMenuEntry(FAnimSequenceTimelineCommands::Get().DisplaySecondaryFormat);
						}
						InMenuBuilder.EndSection();
					})
			);
		}
		MenuBuilder.EndSection();

		//UAnimSequence* AnimSequence = Cast<UAnimSequence>(Model->GetAnimSequenceBase());
		//if (AnimSequence)
		//{
		//	FFrameTime MouseTime = TimeSliderController->GetFrameTimeFromMouse(MyGeometry, MouseEvent.GetScreenSpacePosition());
		//	float CurrentFrameTime = (float)((double)MouseTime.AsDecimal() / (double)Model->GetTickResolution());
		//	float SequenceLength = AnimSequence->GetPlayLength();
		//	uint32 NumFrames = AnimSequence->GetNumberOfFrames();

		//	MenuBuilder.BeginSection("SequenceEditingContext", LOCTEXT("SequenceEditing", "Sequence Editing"));
		//	{
		//		float CurrentFrameFraction = CurrentFrameTime / SequenceLength;
		//		int32 CurrentFrameNumber = CurrentFrameFraction * NumFrames;

		//		FUIAction Action;
		//		FText Label;

		//		//Menu - "Remove Before"
		//		//Only show this option if the selected frame is greater than frame 1 (first frame)
		//		if (CurrentFrameNumber > 0)
		//		{
		//			CurrentFrameFraction = (float)CurrentFrameNumber / (float)NumFrames;

		//			//Corrected frame time based on selected frame number
		//			float CorrectedFrameTime = CurrentFrameFraction * SequenceLength;

		//			Action = FUIAction(FExecuteAction::CreateSP(this, &SMotionTimeline::OnCropAnimSequence, true, CorrectedFrameTime));
		//			Label = FText::Format(LOCTEXT("RemoveTillFrame", "Remove frame 0 to frame {0}"), FText::AsNumber(CurrentFrameNumber));
		//			MenuBuilder.AddMenuEntry(Label, LOCTEXT("RemoveBefore_ToolTip", "Remove sequence before current position"), FSlateIcon(), Action);
		//		}

		//		uint32 NextFrameNumber = CurrentFrameNumber + 1;

		//		//Menu - "Remove After"
		//		//Only show this option if next frame (CurrentFrameNumber + 1) is valid
		//		if (NextFrameNumber < NumFrames)
		//		{
		//			float NextFrameFraction = (float)NextFrameNumber / (float)NumFrames;
		//			float NextFrameTime = NextFrameFraction * SequenceLength;
		//			Action = FUIAction(FExecuteAction::CreateSP(this, &SMotionTimeline::OnCropAnimSequence, false, NextFrameTime));
		//			Label = FText::Format(LOCTEXT("RemoveFromFrame", "Remove from frame {0} to frame {1}"), FText::AsNumber(NextFrameNumber), FText::AsNumber(NumFrames));
		//			MenuBuilder.AddMenuEntry(Label, LOCTEXT("RemoveAfter_ToolTip", "Remove sequence after current position"), FSlateIcon(), Action);
		//		}

		//		MenuBuilder.AddMenuSeparator();

		//		//Corrected frame time based on selected frame number
		//		float CorrectedFrameTime = CurrentFrameFraction * SequenceLength;

		//		Action = FUIAction(FExecuteAction::CreateSP(this, &SMotionTimeline::OnInsertAnimSequence, true, CurrentFrameNumber));
		//		Label = FText::Format(LOCTEXT("InsertBeforeCurrentFrame", "Insert frame before {0}"), FText::AsNumber(CurrentFrameNumber));
		//		MenuBuilder.AddMenuEntry(Label, LOCTEXT("InsertBefore_ToolTip", "Insert a frame before current position"), FSlateIcon(), Action);

		//		Action = FUIAction(FExecuteAction::CreateSP(this, &SMotionTimeline::OnInsertAnimSequence, false, CurrentFrameNumber));
		//		Label = FText::Format(LOCTEXT("InsertAfterCurrentFrame", "Insert frame after {0}"), FText::AsNumber(CurrentFrameNumber));
		//		MenuBuilder.AddMenuEntry(Label, LOCTEXT("InsertAfter_ToolTip", "Insert a frame after current position"), FSlateIcon(), Action);

		//		MenuBuilder.AddMenuSeparator();

		//		//Corrected frame time based on selected frame number
		//		Action = FUIAction(FExecuteAction::CreateSP(this, &SMotionTimeline::OnShowPopupOfAppendAnimation, WidgetPath, true));
		//		MenuBuilder.AddMenuEntry(LOCTEXT("AppendBegin", "Append in the beginning"), LOCTEXT("AppendBegin_ToolTip", "Append in the beginning"), FSlateIcon(), Action);

		//		Action = FUIAction(FExecuteAction::CreateSP(this, &SMotionTimeline::OnShowPopupOfAppendAnimation, WidgetPath, false));
		//		MenuBuilder.AddMenuEntry(LOCTEXT("AppendEnd", "Append at the end"), LOCTEXT("AppendEnd_ToolTip", "Append at the end"), FSlateIcon(), Action);

		//		MenuBuilder.AddMenuSeparator();
		//		//Menu - "ReZero"
		//		Action = FUIAction(FExecuteAction::CreateSP(this, &SMotionTimeline::OnReZeroAnimSequence, CurrentFrameNumber));
		//		Label = FText::Format(LOCTEXT("ReZeroAtFrame", "Re-zero at frame {0}"), FText::AsNumber(CurrentFrameNumber));
		//		MenuBuilder.AddMenuEntry(Label, FText::Format(LOCTEXT("ReZeroAtFrame_ToolTip", "Resets the root track to (0, 0, 0) at frame {0} and apply the difference to all root transform of the sequence. It moves whole sequence to the amount of current root transform."), FText::AsNumber(CurrentFrameNumber)), FSlateIcon(), Action);

		//		const int32 FrameNumberForCurrentTime = INDEX_NONE;
		//		Action = FUIAction(FExecuteAction::CreateSP(this, &SMotionTimeline::OnReZeroAnimSequence, FrameNumberForCurrentTime));
		//		Label = LOCTEXT("ReZeroAtCurrentTime", "Re-zero at current time");
		//		MenuBuilder.AddMenuEntry(Label, LOCTEXT("ReZeroAtCurrentTime_ToolTip", "Resets the root track to (0, 0, 0) at the animation scrub time and apply the difference to all root transform of the sequence. It moves whole sequence to the amount of current root transform."), FSlateIcon(), Action);
		//	}
		//	MenuBuilder.EndSection();
		//}

		FSlateApplication::Get().PushMenu(SharedThis(this), WidgetPath, MenuBuilder.MakeWidget(), FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

		return FReply::Handled();
	}

	return FReply::Unhandled();
}



bool SMotionTimeline::GetGridMetrics(float PhysicalWidth, double& OutMajorInterval, int32& OutMinorDivisions) const
{
	const FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);
	const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	const FFrameRate DisplayRate(FMath::RoundToInt(Model->GetFrameRate()), 1);
	const double BiggestTime = ViewRange.Get().GetUpperBoundValue();
	const FString TickString = NumericTypeInterface->ToString((BiggestTime * DisplayRate).FrameNumber.Value);
	const FVector2D MaxTextSize = FontMeasureService->Measure(TickString, SmallLayoutFont);

	static float MajorTickMultiplier = 2.f;

	const float MinTickPx = MaxTextSize.X + 5.f;
	const float DesiredMajorTickPx = MaxTextSize.X * MajorTickMultiplier;

	// if (PhysicalWidth > 0)
	// {
	// 	return ComputeGridSpacing(
	// 		DisplayRate,
	// 		PhysicalWidth / ViewRange.Get().Size<double>(),
	// 		OutMajorInterval,
	// 		OutMinorDivisions,
	// 		MinTickPx,
	// 		DesiredMajorTickPx);
	// }

	return false;
}



TSharedPtr<ITimeSliderController> SMotionTimeline::GetTimeSliderController() const
{
	return TimeSliderController;
}

TSharedRef<INumericTypeInterface<double>> SMotionTimeline::GetNumericTypeInterface() const
{
	return NumericTypeInterface.ToSharedRef();
}

void SMotionTimeline::OnOutlinerSearchChanged(const FText& Filter)
{
	FilterText = Filter;
	Outliner->RefreshFilter();
}

void SMotionTimeline::OnColumnFillCoefficientChanged(float FillCoefficient, int32 ColumnIndex)
{
	ColumnFillCoefficients[ColumnIndex] = FillCoefficient;
}

void SMotionTimeline::HandleKeyComplete()
{
	if(!MotionAnim)
	{
		return;
	}

	Model->RefreshTracks();
}

UAnimSingleNodeInstance* SMotionTimeline::GetPreviewInstance() const
{
	if(!Model)
	{
		return nullptr;
	}
	
	UDebugSkelMeshComponent* PreviewMeshComponent = Model->DebugMesh;
	return PreviewMeshComponent && PreviewMeshComponent->IsPreviewOn() ? PreviewMeshComponent->PreviewInstance : nullptr;
}

void SMotionTimeline::HandleScrubPositionChanged(FFrameTime NewScrubPosition, bool bIsScrubbing, bool bParam)
{
	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		if (PreviewInstance->IsPlaying())
		{
			PreviewInstance->SetPlaying(false);
		}
	}

	Model->SetScrubPosition(NewScrubPosition);
}


double SMotionTimeline::GetSpinboxDelta() const
{
	return FFrameRate(Model->GetTickResolution(), 1).AsDecimal()
		* FFrameRate(FMath::RoundToInt(Model->GetFrameRate()), 1).AsInterval();
}

void SMotionTimeline::SetPlayTime(double InFrameTime)
{
	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->SetPosition(InFrameTime / (double)Model->GetTickResolution());
	}
}

void SMotionTimeline::HandleViewRangeChanged(TRange<double> InRange, EViewRangeInterpolation InInterpolation)
{
	if (!Model)
	{
		return;
	}

	Model->HandleViewRangeChanged(InRange, InInterpolation);
}

void SMotionTimeline::HandleWorkingRangeChanged(TRange<double> InRange)
{
	if(!Model)
	{
		return;
	}

	Model->HandleWorkingRangeChanged(InRange);
}

void SMotionTimeline::SetAnimation(::TObjectPtr<UMotionAnimObject> InMotionAnim, UDebugSkelMeshComponent* InDebugMeshComponent)
{
	MotionAnim = InMotionAnim;
	DebugSkelMeshComponent = InDebugMeshComponent;
	
	if(MotionAnim && DebugSkelMeshComponent)
	{
		switch (MotionAnim->MotionAnimAssetType)
		{
			case EMotionAnimAssetType::Sequence:
			{
				Model = MakeShared<FMotionModel_AnimSequenceBase>(Cast<UMotionSequenceObject>(MotionAnim),
				                                                  InDebugMeshComponent);
			} break;
			case EMotionAnimAssetType::BlendSpace:
			{
				Model = MakeShared<FMotionModel_BlendSpace>(Cast<UMotionBlendSpaceObject>(MotionAnim),
				                                            InDebugMeshComponent);
			} break;
			case EMotionAnimAssetType::Composite:
			{
				Model = MakeShared<FMotionModel_AnimComposite>(Cast<UMotionCompositeObject>(MotionAnim),
				                                               InDebugMeshComponent);
			} break;
		default: break;
		}

		Model->WeakCommandList = WeakCommandList;
		Model->Initialize();
		Model->OnHandleObjectsSelected().AddSP(MotionPreProcessToolkitPtr.Pin().Get(), &FMotionPreProcessToolkit::HandleTagsSelected);

		

		Rebuild();
	}
	else
	{
		Model.Reset(); 
		DestroyTimeline();
	}
}

#undef LOCTEXT_NAMESPACE