#pragma once

namespace Eagle
{
	class Fence
	{
	protected:
		Fence() = default;

	public:
		virtual ~Fence() = default;

		virtual void* GetHandle() const = 0;
		virtual bool IsSignaled() const = 0;
		virtual void Reset() = 0;
		virtual void Wait(uint64_t timeout = UINT64_MAX) const = 0;

		static Ref<Fence> Create(bool bSignaled = false);
	};
}
