#pragma once

namespace Eagle
{
	class Semaphore
	{
	protected:
		Semaphore() = default;

	public:
		virtual ~Semaphore() = default;

		virtual void* GetHandle() const = 0;

		static Ref<Semaphore> Create();
	};
}
