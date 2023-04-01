#include "egpch.h"
#include "DescriptorSetData.h"

namespace Eagle
{
    void DescriptorSetData::SetArg(uint32_t idx, const Ref<Buffer>& buffer)
    {
        auto& currentBinding = m_Bindings[idx];

        BufferBinding binding(buffer);
        if (currentBinding.BufferBindings[0] != binding)
        {
            currentBinding.BufferBindings[0] = binding;
            m_bDirty = true;
        }
    }

    void DescriptorSetData::SetArg(uint32_t idx, const Ref<Buffer>& buffer, std::size_t offset, std::size_t size)
    {
        auto& currentBinding = m_Bindings[idx];

        BufferBinding binding(buffer, offset, size);
        if (currentBinding.BufferBindings[0] != binding)
        {
            currentBinding.BufferBindings[0] = binding;
            m_bDirty = true;
        }
    }

    void DescriptorSetData::SetArg(uint32_t idx, const Ref<Image>& image)
    {
        SetArg(idx, image, nullptr);
    }

    void DescriptorSetData::SetArg(uint32_t idx, const Ref<Image>& image, const ImageView& imageView)
    {
        SetArg(idx, image, imageView, nullptr);
    }

    void DescriptorSetData::SetArg(uint32_t idx, const Ref<Image>& image, const Ref<Sampler>& sampler)
    {
        auto& currentBinding = m_Bindings[idx];

        ImageBinding binding(image, sampler);
        if (currentBinding.ImageBindings[0] != binding)
        {
            currentBinding.ImageBindings[0] = binding;
            m_bDirty = true;
        }
    }

    void DescriptorSetData::SetArg(uint32_t idx, const Ref<Image>& image, const ImageView& imageView, const Ref<Sampler>& sampler)
    {
        auto& currentBinding = m_Bindings[idx];

        ImageBinding binding(image, imageView, sampler);
        if (currentBinding.ImageBindings[0] != binding)
        {
            currentBinding.ImageBindings[0] = binding;
            m_bDirty = true;
        }
    }

    void DescriptorSetData::SetArgArray(uint32_t idx, const std::vector<Ref<Buffer>>& buffers)
    {
        assert(buffers.size());
        auto& currentBinding = m_Bindings[idx];

        std::vector<BufferBinding> bindings;
        bindings.reserve(buffers.size());

        for (auto& buffer : buffers)
            bindings.emplace_back(buffer);

        if (currentBinding.BufferBindings != bindings)
        {
            currentBinding.BufferBindings = std::move(bindings);
            m_bDirty = true;
        }
    }

    void DescriptorSetData::SetArgArray(uint32_t idx, const std::vector<Ref<Image>>& images)
    {
        assert(images.size());
        auto& currentBinding = m_Bindings[idx];

        std::vector<ImageBinding> bindings;
        bindings.reserve(images.size());

        for (auto& image : images)
            bindings.emplace_back(image);

        if (currentBinding.ImageBindings != bindings)
        {
            currentBinding.ImageBindings = std::move(bindings);
            m_bDirty = true;
        }
    }

    void DescriptorSetData::SetArgArray(uint32_t idx, const std::vector<Ref<Image>>& images, const std::vector<ImageView>& imageViews)
    {
        const size_t imagesCount = images.size();
        assert(imagesCount);
        auto& currentBinding = m_Bindings[idx];

        std::vector<ImageBinding> bindings;
        bindings.reserve(imagesCount);

        for (size_t i = 0; i < imagesCount; ++i)
            bindings.emplace_back(images[i], imageViews[i]);

        if (currentBinding.ImageBindings != bindings)
        {
            currentBinding.ImageBindings = std::move(bindings);
            m_bDirty = true;
        }
    }

    void DescriptorSetData::SetArgArray(uint32_t idx, const Ref<Image>& image, const std::vector<ImageView>& imageViews)
    {
        const size_t imagesCount = imageViews.size();
        auto& currentBinding = m_Bindings[idx];

        std::vector<ImageBinding> bindings;
        bindings.reserve(imagesCount);

        for (size_t i = 0; i < imagesCount; ++i)
            bindings.emplace_back(image, imageViews[i]);

        if (currentBinding.ImageBindings != bindings)
        {
            currentBinding.ImageBindings = std::move(bindings);
            m_bDirty = true;
        }
    }

    void DescriptorSetData::SetArgArray(uint32_t idx, const std::vector<Ref<Image>>& images, const std::vector<Ref<Sampler>>& samplers)
    {
        const size_t imagesCount = images.size();
        assert(imagesCount);
        auto& currentBinding = m_Bindings[idx];

        std::vector<ImageBinding> bindings;
        bindings.reserve(imagesCount);

        for (size_t i = 0; i < imagesCount; ++i)
            bindings.emplace_back(images[i], samplers[i]);

        if (currentBinding.ImageBindings != bindings)
        {
            currentBinding.ImageBindings = std::move(bindings);
            m_bDirty = true;
        }
    }

    void DescriptorSetData::SetArgArray(uint32_t idx, const std::vector<Ref<Image>>& images, const std::vector<ImageView>& imageViews, const std::vector<Ref<Sampler>>& samplers)
    {
        const size_t imagesCount = images.size();
        assert(imagesCount);
        auto& currentBinding = m_Bindings[idx];

        std::vector<ImageBinding> bindings;
        bindings.reserve(imagesCount);

        for (size_t i = 0; i < imagesCount; ++i)
            bindings.emplace_back(images[i], imageViews[i], samplers[i]);

        if (currentBinding.ImageBindings != bindings)
        {
            currentBinding.ImageBindings = std::move(bindings);
            m_bDirty = true;
        }
    }
}
