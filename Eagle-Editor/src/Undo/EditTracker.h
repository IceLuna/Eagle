#pragma once

#include "UndoHistory.h"

namespace Eagle
{
	// Turns the edits made through the widgets of a panel (dragging a value, typing, picking a value from a combo, etc) into undo transactions.
	// It doesn't need to know which widgets the panel has, and they don't need to report their changes:
	// - Before the panel's widgets are drawn, on frames where the panel received input, the edited target is captured.
	// - Once no widget is active anymore (immediately for clicks, or when a drag or typing ends), the target is captured again.
	//   If it differs, the edit is recorded as one transaction.
	// While an edit is in progress, it's registered in the history (see `PendingUndoEdit`): before something else is recorded or applied,
	// the edit is recorded on its own (if the user is still in the middle of it, the rest is recorded as another edit).
	// If anything is recorded or applied by the history without that (see `UndoHistory::FlushPendingEdits()`), the edit is dropped, since it could be outdated.
	// @TargetType. What's edited. Needs `operator==`, so that an edit is finished when the panel starts editing another target.
	// @StateType. The state of the target. Needs `operator==`
	template<typename TargetType, typename StateType>
	class EditTracker : public PendingUndoEdit
	{
	public:
		// The state of the target. Null if it can't be captured (for example, it no longer exists)
		using CaptureFn = std::function<std::optional<StateType>(const TargetType& target)>;

		// Records an edit of the target into the history
		using RecordFn = std::function<void(UndoHistory& history, const TargetType& target, StateType&& before, StateType&& after)>;

		// @bWaitUntilSettled. For targets that apply some changes a few frames after the input that caused them
		// (for example, the node editor of graphs creates a link on the frame after the mouse is released).
		// Once the interaction ends, the edit is only completed after the target stayed the same for a whole frame.
		// The target is captured every frame until then
		EditTracker(CaptureFn capture, RecordFn record, bool bWaitUntilSettled = false)
			: m_Capture(std::move(capture)), m_Record(std::move(record)), m_bWaitUntilSettled(bWaitUntilSettled) {}

		~EditTracker() override { Reset(); }

		// It's registered in a history by its address while an edit is in progress
		EditTracker(const EditTracker&) = delete;
		EditTracker& operator=(const EditTracker&) = delete;

		// Call within the panel's window, right before its widgets are drawn
		// @history. Can be null, nothing is recorded then
		void BeginFrame(UndoHistory* history, const TargetType& target)
		{
			if (m_Before && (history != m_History || !(target == *m_Target)))
				Finish(); // The panel switched to another target, the edit of the previous one is complete

			m_FrameHistory = history;

			// A transaction was undone or redone since the end of the previous frame (for example, by a shortcut).
			// The target hasn't been drawn since, so it might not be up to date yet (for example, a rebuilt graph measures its nodes when it's drawn),
			// and capturing it now could record that as an edit. The user can't edit what wasn't shown yet anyway
			const bool bAppliedSinceLastFrame = history && (history != m_PrevHistory || history->GetApplyCount() != m_ObservedApplyCount);

			if (m_Before || !history || history->IsApplying())
				return;

			// The edit was interrupted by something else that was recorded (see `FlushPendingEdit()`), and the user continues it without new input
			const bool bContinue = m_bContinueEdit;
			m_bContinueEdit = false;
			if (bAppliedSinceLastFrame || (!bContinue && !UI::HasWidgetInput()))
				return;

			m_Before = m_Capture(target);
			if (m_Before)
			{
				m_Target = target;
				m_History = history;
				m_HistoryVersion = history->GetVersion();
				history->AddPendingEdit(this);
			}
		}

		// Call every frame after the panel, whether it was drawn or not
		void EndFrame()
		{
			if (m_Before)
			{
				// Something was recorded or applied meanwhile. Either the change was already recorded, or the captured state is outdated
				if (m_History->GetVersion() != m_HistoryVersion)
					Reset();
				else if (UI::IsInteracting())
					m_Settling.reset(); // The user continued the edit
				else if (!m_bWaitUntilSettled)
					Finish();
				else
					UpdateSettling();
			}

			// What the panel has shown by the end of this frame
			if (m_FrameHistory)
			{
				m_PrevHistory = m_FrameHistory;
				m_ObservedApplyCount = m_FrameHistory->GetApplyCount();
				m_FrameHistory = nullptr;
			}
		}

		void Reset()
		{
			if (m_History)
				m_History->RemovePendingEdit(this);

			m_Before.reset();
			m_Settling.reset();
			m_Target.reset();
			m_History = nullptr;
		}

		void FlushPendingEdit(uint64_t historyVersion) override
		{
			if (!m_Before)
				return;

			const bool bInteracting = UI::IsInteracting();
			const bool bValid = m_HistoryVersion == historyVersion;
			EndEdit(m_Capture(*m_Target), bValid, false);

			// The rest of the edit (for example, the rest of a drag) is recorded as another one. It's captured on the next frame, after what's being recorded now
			m_bContinueEdit = bInteracting;
		}

		void OnHistoryDestroyed() override
		{
			m_History = nullptr; // It's not unregistered from it
			Reset();
			m_FrameHistory = nullptr;
			m_PrevHistory = nullptr;
			m_bContinueEdit = false;
		}

	private:
		void Finish()
		{
			Finish(m_Capture(*m_Target));
		}

		void Finish(std::optional<StateType>&& after)
		{
			const bool bValid = m_History->GetVersion() == m_HistoryVersion;
			EndEdit(std::move(after), bValid, true);
		}

		// Ends the edit, and records it if it changed the target
		// @bValid. Whether the captured state is up to date
		// @bFlushOthers. Whether the other edits in progress are recorded first, so that they aren't dropped when this one is recorded.
		// Not while they're being flushed (they're recorded one after another anyway)
		void EndEdit(std::optional<StateType>&& after, bool bValid, bool bFlushOthers)
		{
			UndoHistory* history = m_History;
			std::optional<StateType> before = std::move(m_Before);
			std::optional<TargetType> target = std::move(m_Target);
			Reset(); // It isn't pending anymore

			if (!bValid || !after || *after == *before)
				return;

			if (bFlushOthers)
				history->FlushPendingEdits();
			m_Record(*history, *target, std::move(*before), std::move(*after));
		}

		void UpdateSettling()
		{
			std::optional<StateType> current = m_Capture(*m_Target);
			if (!current)
			{
				Reset(); // The target no longer exists
				return;
			}

			if (m_Settling && *current == *m_Settling)
				Finish(std::move(current));
			else
				m_Settling = std::move(current);
		}

	private:
		CaptureFn m_Capture;
		RecordFn m_Record;
		std::optional<TargetType> m_Target;
		std::optional<StateType> m_Before;
		std::optional<StateType> m_Settling; // The target at the end of the previous frame, while waiting for it to settle
		UndoHistory* m_History = nullptr;
		uint64_t m_HistoryVersion = 0;

		UndoHistory* m_FrameHistory = nullptr;
		const UndoHistory* m_PrevHistory = nullptr; // The history at the end of the previous frame. Only compared, never accessed
		uint64_t m_ObservedApplyCount = 0;
		bool m_bWaitUntilSettled = false;
		bool m_bContinueEdit = false;
	};
}
