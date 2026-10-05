// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <tsl/robin_map.h>
#include <shader_compiler/shader_info.h>
#include <gpu/texture/texture.h>
#include "common.h"
#include "tic.h"

namespace skyline::gpu::interconnect {
    class TexturePoolState : dirty::CachedManualDirty {
      public:
        struct EngineRegisters {
            const engine_common::TexHeaderPool &texHeaderPool;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        span<TextureImageControl> textureHeaders;

        TexturePoolState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx);

        void PurgeCaches();
    };

    class Textures {
      private:
        std::shared_ptr<TextureView> nullTextureView{};
        std::shared_ptr<TextureView> nullStorageTextureView{}; //!< Dummy 1x1 storage image used for unsupported storage image bindings
        dirty::ManualDirtyState<TexturePoolState> texturePool;

        tsl::robin_map<TextureImageControl, std::shared_ptr<TextureView>, util::ObjectHash<TextureImageControl>> textureHeaderStore;

        struct CacheEntry {
            TextureImageControl tic;
            TextureView *view;
            u64 sequenceNumber;
        };
        std::vector<CacheEntry> textureHeaderCache;

        /**
         * @brief A private aligned copy of a texel buffer's contents, used for drivers that require strictly aligned VkBufferView offsets and don't support single-texel alignment
         * @note Shared ownership of this object is attached to every fence cycle that binds it so it outlives pending GPU submissions
         */
        struct AlignedTexelShadow {
            memory::Buffer buffer; //!< Host-visible device buffer holding the aligned copy, refreshed from the guest mapping before each use
            vk::raii::BufferView view; //!< Texel view over `buffer` at offset 0

            AlignedTexelShadow(GPU &gpu, vk::DeviceSize size, vk::Format format);
        };

        struct TexelBufferCacheEntry {
            TextureImageControl tic{}; //!< The TIC that the cached view was created from
            u64 sequenceNumber{}; //!< The channel sequence number the cached view was resolved at
            CachedMappedBufferView mappedView{}; //!< Guest-side buffer view for the texel buffer's mapping
            vk::BufferView view{}; //!< Host-side texel view, owned by the underlying Buffer
            bool usesShadow{}; //!< If `view` points at the aligned shadow copy rather than the guest buffer's backing
            std::shared_ptr<AlignedTexelShadow> shadow{}; //!< The aligned shadow copy backing `view` when `usesShadow` is set
            vk::Format shadowFormat{}; //!< The format the shadow view was created with
        };
        std::vector<TexelBufferCacheEntry> texelBufferCache;
        u32 texelBufferWarnCount{}; //!< Throttle counter for texel buffer resolution warnings

        std::optional<memory::Buffer> dummyTexelBuffer; //!< Dummy buffer used for texel buffer views on devices without nullDescriptor
        std::unique_ptr<vk::raii::BufferView> dummyTexelView;

        /**
         * @return A texel buffer view that can be bound for unresolvable texel buffers, a null view on devices supporting nullDescriptor or a dummy view otherwise
         */
        vk::BufferView GetNullTexelView(InterconnectContext &ctx);

      public:
        Textures(DirtyManager &manager, const TexturePoolState::EngineRegisters &engine);

        void MarkAllDirty();

        TextureView *GetTexture(InterconnectContext &ctx, u32 index, Shader::TextureType shaderType);

        Shader::TextureType GetTextureType(InterconnectContext &ctx, u32 index);

        /**
         * @brief Resolves the TIC at `index` in the texture pool into a texel buffer view (VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER/STORAGE_TEXEL_BUFFER)
         * @param storage Whether the view will be bound as a storage texel buffer rather than a uniform texel buffer
         * @param isWritten Whether the shader may write through the view, if so the underlying buffer is marked GPU dirty
         * @return A VkBufferView for the texel buffer, on failure a null/dummy view is returned to ensure the descriptor is always bound
         */
        vk::BufferView GetTexelBuffer(InterconnectContext &ctx, u32 index, bool storage, bool isWritten,
                                      vk::PipelineStageFlagBits dstStage,
                                      vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask);

        /**
         * @brief Resolves the TIC at `index` in the texture pool into a storage image binding (VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
         * @return An image info for the storage image, unresolvable storage images are bound as a null/dummy image to ensure the descriptor is always bound
         */
        vk::DescriptorImageInfo GetStorageImage(InterconnectContext &ctx, u32 index,
                                                vk::PipelineStageFlagBits dstStage,
                                                vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask);
    };
}
