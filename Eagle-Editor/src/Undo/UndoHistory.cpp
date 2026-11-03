#include "UndoHistory.h"

namespace Eagle
{
	static const std::string s_EmptyName;

	UndoHistory::~UndoHistory()
	{
		const std::vector<PendingUndoEdit*> edits = std::move(m_PendingEdits);
		for (PendingUndoEdit* edit : edits)
			edit->OnHistoryDestroyed();
	}

	void UndoHistory::AddPendingEdit(PendingUndoEdit* edit)
	{
		if (edit && !std::ranges::contains(m_PendingEdits, edit))
			m_PendingEdits.push_back(edit);
	}

	void UndoHistory::RemovePendingEdit(PendingUndoEdit* edit)
	{
		std::erase(m_PendingEdits, edit);
	}

	void UndoHistory::FlushPendingEdits()
	{
		// While flushing, the flushed edits record themselves (and they'd flush the others again)
		if (m_bFlushingPendingEdits || m_bApplying || IsTransactionOpen() || m_PendingEdits.empty())
			return;

		m_bFlushingPendingEdits = true;
		const uint64_t version = m_Version;
		const std::vector<PendingUndoEdit*> edits = m_PendingEdits; // They unregister themselves
		for (PendingUndoEdit* edit : edits)
		{
			// One that was flushed earlier could have dropped it
			if (std::ranges::contains(m_PendingEdits, edit))
				edit->FlushPendingEdit(version);
		}
		m_bFlushingPendingEdits = false;
	}

	void UndoHistory::Record(std::string name, Scope<UndoCommand> command)
	{
		if (!command || m_bApplying)
			return;

		if (IsTransactionOpen())
		{
			m_OpenTransaction.MemoryUsage += command->GetMemoryUsage();
			m_OpenTransaction.Commands.push_back(std::move(command));
			return;
		}

		UndoTransaction transaction;
		transaction.Name = std::move(name);
		transaction.MemoryUsage = command->GetMemoryUsage();
		transaction.Commands.push_back(std::move(command));
		Push(std::move(transaction));
	}

	void UndoHistory::BeginTransaction(std::string name)
	{
		if (m_OpenTransactionDepth == 0)
			FlushPendingEdits();

		if (m_OpenTransactionDepth++ == 0)
		{
			m_OpenTransaction = UndoTransaction{};
			m_OpenTransaction.Name = std::move(name);
		}
	}

	void UndoHistory::EndTransaction()
	{
		EG_CORE_ASSERT(m_OpenTransactionDepth > 0, "`EndTransaction()` without `BeginTransaction()`");
		if (m_OpenTransactionDepth == 0 || --m_OpenTransactionDepth > 0)
			return;

		UndoTransaction transaction = std::move(m_OpenTransaction);
		m_OpenTransaction = UndoTransaction{};
		if (!transaction.Commands.empty())
			Push(std::move(transaction));
	}

	UndoHistory::Result UndoHistory::Undo(std::string* outName)
	{
		FlushPendingEdits();

		if (!CanUndo())
			return Result::NothingToApply;

		UndoTransaction transaction = std::move(m_UndoStack.back());
		m_UndoStack.pop_back();
		++m_Version;
		if (outName)
			*outName = transaction.Name;

		// All commands are checked before any of them is applied, so that a transaction is never applied partially
		for (const auto& command : transaction.Commands)
		{
			if (!command->CanApply(true))
			{
				// The transaction is dropped. The ones before it might still be valid, they're checked when they're undone
				EG_CORE_WARN("Failed to undo '{}'. What it changed was modified in a way that the undo history didn't record", transaction.Name);
				m_MemoryUsage -= transaction.MemoryUsage;
				return Result::Failed;
			}
		}

		m_bApplying = true;
		for (auto it = transaction.Commands.rbegin(); it != transaction.Commands.rend(); ++it)
			(*it)->Undo();
		m_bApplying = false;
		++m_ApplyCount;

		if (m_OnApplied)
			m_OnApplied(transaction, true);

		m_RedoStack.push_back(std::move(transaction));
		return Result::Applied;
	}

	UndoHistory::Result UndoHistory::Redo(std::string* outName)
	{
		// A flushed edit is a new change, so it clears the redo stack
		FlushPendingEdits();

		if (!CanRedo())
			return Result::NothingToApply;

		UndoTransaction transaction = std::move(m_RedoStack.back());
		m_RedoStack.pop_back();
		++m_Version;
		if (outName)
			*outName = transaction.Name;

		// All commands are checked before any of them is applied, so that a transaction is never applied partially
		for (const auto& command : transaction.Commands)
		{
			if (!command->CanApply(false))
			{
				// The transactions after it were made on top of it, so they're dropped too
				EG_CORE_WARN("Failed to redo '{}'. What it changed was modified in a way that the undo history didn't record", transaction.Name);
				m_MemoryUsage -= transaction.MemoryUsage;
				for (const UndoTransaction& redoTransaction : m_RedoStack)
					m_MemoryUsage -= redoTransaction.MemoryUsage;
				m_RedoStack.clear();
				return Result::Failed;
			}
		}

		m_bApplying = true;
		for (const auto& command : transaction.Commands)
			command->Redo();
		m_bApplying = false;
		++m_ApplyCount;

		if (m_OnApplied)
			m_OnApplied(transaction, false);

		m_UndoStack.push_back(std::move(transaction));
		return Result::Applied;
	}

	const std::string& UndoHistory::GetUndoName() const
	{
		return m_UndoStack.empty() ? s_EmptyName : m_UndoStack.back().Name;
	}

	const std::string& UndoHistory::GetRedoName() const
	{
		return m_RedoStack.empty() ? s_EmptyName : m_RedoStack.back().Name;
	}

	void UndoHistory::Clear()
	{
		m_UndoStack.clear();
		m_RedoStack.clear();
		m_OpenTransaction.Commands.clear();
		m_OpenTransaction.MemoryUsage = 0;
		m_MemoryUsage = 0;
		++m_Version;
	}

	void UndoHistory::Push(UndoTransaction&& transaction)
	{
		for (const UndoTransaction& redoTransaction : m_RedoStack)
			m_MemoryUsage -= redoTransaction.MemoryUsage;
		m_RedoStack.clear();

		m_MemoryUsage += transaction.MemoryUsage;
		m_UndoStack.push_back(std::move(transaction));
		Trim();
		++m_Version;
	}

	void UndoHistory::Trim()
	{
		// The most recent transaction is always kept, even if it alone exceeds the budget
		size_t dropCount = 0;
		size_t remainingMemory = m_MemoryUsage;
		while (m_UndoStack.size() - dropCount > 1 && (m_UndoStack.size() - dropCount > m_MaxTransactions || remainingMemory > m_MaxMemoryUsage))
		{
			remainingMemory -= m_UndoStack[dropCount].MemoryUsage;
			++dropCount;
		}

		if (dropCount > 0)
		{
			m_UndoStack.erase(m_UndoStack.begin(), m_UndoStack.begin() + dropCount);
			m_MemoryUsage = remainingMemory;
		}
	}
}
