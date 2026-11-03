#pragma once

namespace Eagle
{
	class Scene;

	// A change that can be reverted and reapplied. It's recorded once the change has been made, see `UndoHistory`.
	// To make something undoable, implement a command that can restore the state from before and after the change,
	// or use an existing one (for example, `SceneTransaction` for changes of entities)
	class UndoCommand
	{
	public:
		virtual ~UndoCommand() = default;

		virtual void Undo() = 0;
		virtual void Redo() = 0;

		// Whether the command can still be applied. It can't if what it changed was changed in a way that wasn't recorded by the history
		// (for example, an entity it changed no longer exists)
		// @bUndo. Whether it's about to be undone or redone
		virtual bool CanApply(bool bUndo) const { return true; }

		// Approximate memory used by the command. The history drops its oldest transactions to stay within its budget
		virtual size_t GetMemoryUsage() const { return 0; }

		// Whether applying the command changes the scene: its entities, or its settings that are saved with it.
		// For example, the editor marks the scene as modified when such a command is run
		virtual bool ModifiesScene(const Scene& scene) const { return false; }
	};

	// What the user undoes or redoes in one step
	struct UndoTransaction
	{
		std::string Name;
		std::vector<Scope<UndoCommand>> Commands; // Redone in order, undone in the reverse order
		size_t MemoryUsage = 0;
	};

	// An edit that's in progress (for example, a value that's being dragged, or typed in). It's recorded into a history once it's complete, see `EditTracker`.
	// While it's in progress, it's registered in the history (see `UndoHistory::AddPendingEdit()`), so that it's recorded on its own
	// before anything else is recorded or applied (see `UndoHistory::FlushPendingEdits()`)
	class PendingUndoEdit
	{
	public:
		virtual ~PendingUndoEdit() = default;

		// Records the edit now if it changed anything. It's unregistered afterwards
		// @historyVersion. The version of the history before the flush began (see `UndoHistory::GetVersion()`).
		// Edits flushed before it might have changed it, but the ones that began at another version are outdated
		virtual void FlushPendingEdit(uint64_t historyVersion) = 0;

		// The history is being destroyed. The edit is dropped, it must not access the history anymore
		virtual void OnHistoryDestroyed() = 0;
	};

	class UndoHistory
	{
	public:
		using AppliedCallbackFn = std::function<void(const UndoTransaction& transaction, bool bUndo)>;

		enum class Result
		{
			Applied,
			NothingToApply,
			Failed, // The transaction couldn't be applied (see `UndoCommand::CanApply`), so it was removed from the history
		};

		static constexpr size_t DefaultMaxTransactions = 200;
		static constexpr size_t DefaultMaxMemoryUsage = 128ull * 1024ull * 1024ull;

		UndoHistory(size_t maxTransactions = DefaultMaxTransactions, size_t maxMemoryUsage = DefaultMaxMemoryUsage)
			: m_MaxTransactions(std::max<size_t>(maxTransactions, 1))
			, m_MaxMemoryUsage(maxMemoryUsage)
		{}

		~UndoHistory();

		UndoHistory(const UndoHistory&) = delete;
		UndoHistory& operator=(const UndoHistory&) = delete;

		// Records a change that has been made. It clears the redo stack.
		// If a transaction is open (see `BeginTransaction()`), the command becomes a part of it.
		// It's ignored while the history is applying a transaction (see `IsApplying()`).
		// Call `FlushPendingEdits()` before making the change
		void Record(std::string name, Scope<UndoCommand> command);

		// The commands recorded until the matching `EndTransaction()` are undone and redone together.
		// Transactions can be nested, the outermost one is recorded, under its name.
		// The edits in progress are recorded first (see `FlushPendingEdits()`), so call it before making the changes
		void BeginTransaction(std::string name);
		void EndTransaction();
		bool IsTransactionOpen() const { return m_OpenTransactionDepth > 0; }

		// @outName. Optional. Receives the name of the transaction (if there was one to apply)
		Result Undo(std::string* outName = nullptr);
		Result Redo(std::string* outName = nullptr);

		bool CanUndo() const { return !m_UndoStack.empty() && !IsTransactionOpen() && !m_bApplying; }
		bool CanRedo() const { return !m_RedoStack.empty() && !IsTransactionOpen() && !m_bApplying; }

		const std::string& GetUndoName() const;
		const std::string& GetRedoName() const;

		void Clear();

		// True while a transaction is being undone or redone
		bool IsApplying() const { return m_bApplying; }

		// Incremented whenever the history changes.
		// It allows to detect whether anything was recorded or applied meanwhile
		uint64_t GetVersion() const { return m_Version; }

		// Incremented whenever a transaction is undone or redone. Unlike the version, it doesn't change when something is recorded.
		// It allows to detect whether what the history tracks was changed by it
		uint64_t GetApplyCount() const { return m_ApplyCount; }

		// Called after a transaction was undone or redone. For example, to mark the edited asset as dirty
		void SetOnAppliedCallback(AppliedCallbackFn callback) { m_OnApplied = std::move(callback); }

		// Registers/unregisters an edit that's in progress, see `PendingUndoEdit`. It's unregistered when the history is destroyed too
		void AddPendingEdit(PendingUndoEdit* edit);
		void RemovePendingEdit(PendingUndoEdit* edit);

		// Records the edits that are in progress now (see `PendingUndoEdit`), each on its own.
		// Something that's recorded drops them (they could be outdated), so call it before making a change that's going to be recorded with `Record()`.
		// Otherwise, for example, a value that's being typed into a field of another panel is never recorded.
		// It does nothing while a transaction is open (the edits were recorded when it began) or applied
		void FlushPendingEdits();

	private:
		void Push(UndoTransaction&& transaction);
		void Trim();

	private:
		std::vector<UndoTransaction> m_UndoStack; // The last element is undone first
		std::vector<UndoTransaction> m_RedoStack; // The last element is redone first
		UndoTransaction m_OpenTransaction;
		AppliedCallbackFn m_OnApplied;
		std::vector<PendingUndoEdit*> m_PendingEdits;
		size_t m_MaxTransactions = 0;
		size_t m_MaxMemoryUsage = 0;
		size_t m_MemoryUsage = 0;
		uint64_t m_Version = 0;
		uint64_t m_ApplyCount = 0;
		uint32_t m_OpenTransactionDepth = 0;
		bool m_bApplying = false;
		bool m_bFlushingPendingEdits = false;
	};

	class ScopedUndoTransaction
	{
	public:
		ScopedUndoTransaction(UndoHistory* history, std::string name)
			: m_History(history)
		{
			if (m_History)
				m_History->BeginTransaction(std::move(name));
		}

		~ScopedUndoTransaction()
		{
			if (m_History)
				m_History->EndTransaction();
		}

		ScopedUndoTransaction(const ScopedUndoTransaction&) = delete;
		ScopedUndoTransaction& operator=(const ScopedUndoTransaction&) = delete;

	private:
		UndoHistory* m_History = nullptr;
	};
}
