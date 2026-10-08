// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 yuzu Emulator Project (https://github.com/yuzu-emu/yuzu/)

#include <algorithm>
#include <atomic>
#include <gpu/interconnect/command_executor.h>
#include <gpu/texture/format.h>
#include <gpu/texture/layout.h>
#include <soc.h>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include "maxwell_dma.h"

namespace skyline::soc::gm20b::engine {
    namespace {
        /**
         * @brief Sanity limits for DMA copies; legitimate copies never approach these, while garbage
         * registers previously produced wild reads/writes (SIGSEGV) or multi-terabyte translation loops
         */
        constexpr size_t MaxDmaSurfaceDimension{32768}; //!< The maximum texture dimension supported by the Tegra X1, larger values are garbage
        constexpr size_t MaxDmaPitch{1 * 1024 * 1024}; //!< The maximum plausible pitch of a pixel surface (1 MiB)
        constexpr size_t MaxDmaLayerCount{65536}; //!< The maximum plausible layer count of a surface
        constexpr size_t MaxDmaCopyExtent{size_t{1} << 33}; //!< The maximum extent of a validated copy (8 GiB), anything larger is garbage
        constexpr u64 GmmuAddressSpaceEnd{1ull << GmmuAddressSpaceBits}; //!< The end of the GMMU address space

        /**
         * @brief Running totals of pitch->block-linear DMA copies, included in abort
         * diagnostics to expose how much legitimate traffic shares the aborted address region
         */
        std::atomic<u64> pitchToBlockLinearOk{}, pitchToBlockLinearAborted{};

        /**
         * @brief Checks that a translated range is fully mapped; TranslateRange returns spans with a
         * null data pointer for unmapped regions which would crash if they were dereferenced
         */
        bool IsFullyMapped(const TranslatedAddressRange &mappings) {
            for (auto &mapping : mappings)
                if (mapping.data() == nullptr)
                    return false;
            return true;
        }
    }

    MaxwellDma::MaxwellDma(const DeviceState &state, ChannelContext &channelCtx)
        : channelCtx{channelCtx},
          syncpoints{state.soc->host1x.syncpoints},
          interconnect{*state.gpu, channelCtx},
          copyCache() {}

    __attribute__((always_inline)) void MaxwellDma::CallMethod(u32 method, u32 argument) {
        Logger::Verbose("Called method in Maxwell DMA: 0x{:X} args: 0x{:X}", method, argument);

        HandleMethod(method, argument);
    }

    void MaxwellDma::HandleMethod(u32 method, u32 argument) {
        registers.raw[method] = argument;

        if (method == ENGINE_OFFSET(launchDma))
            LaunchDma();
    }

    void MaxwellDma::LaunchDma() {
        DmaCopy();

        ReleaseSemaphore();
    }

    bool MaxwellDma::TryTranslateRange(TranslatedAddressRange &out, u64 address, size_t size) {
        // Translating a region extending past the end of the address space is undefined behavior
        if (address >= GmmuAddressSpaceEnd || size > (GmmuAddressSpaceEnd - address))
            return false;

        out = channelCtx.asCtx->gmmu.TranslateRange(address, size);
        return true;
    }

    void MaxwellDma::DmaCopy() {
        if (registers.launchDma->multiLineEnable) {
            if (registers.launchDma->remapEnable) [[unlikely]] {
                Logger::Warn("Remapped DMA copies are unimplemented!");
                return;
            }

            channelCtx.executor.Submit();

            if (registers.launchDma->srcMemoryLayout == registers.launchDma->dstMemoryLayout) [[unlikely]] {
                // Pitch to Pitch copy
                if (registers.launchDma->srcMemoryLayout == Registers::LaunchDma::MemoryLayout::Pitch) [[likely]] {
                    CopyPitchToPitch();
                } else {
                    Logger::Warn("BlockLinear to BlockLinear DMA copies are unimplemented!");
                }
            } else if (registers.launchDma->srcMemoryLayout == Registers::LaunchDma::MemoryLayout::BlockLinear) {
                CopyBlockLinearToPitch();
            } else [[likely]] {
                CopyPitchToBlockLinear();
            }
        } else {
            // 1D copy
            // TODO: implement swizzled 1D copies based on VMM 'kind'
            if (*registers.lineLengthIn == 0) [[unlikely]] {
                Logger::Debug("Skipping zero-sized 1D DMA copy");
                return;
            }

            Logger::Debug("src: 0x{:X} dst: 0x{:X} size: 0x{:X}", u64{*registers.offsetIn}, u64{*registers.offsetOut}, *registers.lineLengthIn);

            size_t dstBpp{registers.launchDma->remapEnable ? static_cast<size_t>(registers.remapComponents->NumDstComponents() * registers.remapComponents->ComponentSize()) : 1};

            TranslatedAddressRange srcMappings, dstMappings;
            if (!TryTranslateRange(srcMappings, *registers.offsetIn, *registers.lineLengthIn) || !IsFullyMapped(srcMappings) ||
                !TryTranslateRange(dstMappings, *registers.offsetOut, *registers.lineLengthIn * dstBpp) || !IsFullyMapped(dstMappings)) [[unlikely]] {
                Logger::Warn("Aborting 1D DMA copy touching unmapped memory: src 0x{:X} dst 0x{:X} lineLengthIn 0x{:X} dstBpp {}, layouts: linear->linear", u64{*registers.offsetIn}, u64{*registers.offsetOut}, *registers.lineLengthIn, dstBpp);
                return;
            }

            if (registers.launchDma->remapEnable) [[unlikely]] {
                // Remapped buffer clears
                if ((registers.remapComponents->dstX == Registers::RemapComponents::Swizzle::ConstA) &&
                    (registers.remapComponents->dstY == Registers::RemapComponents::Swizzle::ConstA) &&
                    (registers.remapComponents->dstZ == Registers::RemapComponents::Swizzle::ConstA) &&
                    (registers.remapComponents->dstW == Registers::RemapComponents::Swizzle::ConstA) &&
                    (registers.remapComponents->ComponentSize() == 4)) {
                    for (auto mapping : dstMappings)
                        interconnect.Clear(mapping, *registers.remapConstA);
                } else {
                    Logger::Warn("Remapped DMA copies are unimplemented!");
                }
            } else {
                if (srcMappings.size() != 1 || dstMappings.size() != 1) [[unlikely]]
                    channelCtx.asCtx->gmmu.Copy(u64{*registers.offsetOut}, u64{*registers.offsetIn}, *registers.lineLengthIn);
                else
                    interconnect.Copy(dstMappings.front(), srcMappings.front());
            }
        }
    }

    void MaxwellDma::HandleSplitCopy(TranslatedAddressRange srcMappings, TranslatedAddressRange dstMappings, size_t srcSize, size_t dstSize, u64 srcAddress, u64 dstAddress, auto copyCallback) {
        if (srcMappings.empty() || dstMappings.empty()) [[unlikely]] {
            // Callers validate their mappings beforehand, this is purely defensive
            Logger::Warn("Split DMA copy with an empty mapping, aborting");
            return;
        }

        bool isSrcSplit{};
        u8 *src{srcMappings.front().data()}, *dst{dstMappings.front().data()};
        if (srcMappings.size() != 1) {
            if (copyCache.size() < srcSize)
                copyCache.resize(srcSize);

            src = copyCache.data();
            channelCtx.asCtx->gmmu.Read(src, srcAddress, srcSize);

            isSrcSplit = true;
        }
        if (dstMappings.size() != 1) {
            size_t offset{isSrcSplit ? srcSize : 0};

            // The cache has to fit both the source and destination, previously only the destination was
            // reserved which overflowed the buffer whenever both sides of the copy were split
            if (copyCache.size() < (dstSize + offset))
                copyCache.resize(dstSize + offset);

            dst = copyCache.data() + offset;

            // Fill the cache with the current state of the destination so that regions not written by the
            // copy (such as pitch padding) are preserved when it is written back
            channelCtx.asCtx->gmmu.Read(dst, dstAddress, dstSize);
        }

        copyCallback(src, dst);

        if (dstMappings.size() != 1)
            channelCtx.asCtx->gmmu.Write(dstAddress, dst, dstSize);
    }

    void MaxwellDma::CopyPitchToPitch() {
        if (*registers.lineLengthIn == 0 || *registers.lineCount == 0) [[unlikely]] {
            Logger::Debug("Skipping zero-sized pitch to pitch DMA copy (lineLengthIn: 0x{:X}, lineCount: 0x{:X})", *registers.lineLengthIn, *registers.lineCount);
            return;
        }

        // The exact extent accessed by the copy, the pitch is irrelevant for the last line so it is not
        // simply pitch * lineCount; garbage registers are rejected by the extent limit below
        size_t srcSize{(u64{*registers.lineCount - 1} * *registers.pitchIn) + *registers.lineLengthIn};
        size_t dstSize{(u64{*registers.lineCount - 1} * *registers.pitchOut) + *registers.lineLengthIn};
        if (srcSize > MaxDmaCopyExtent || dstSize > MaxDmaCopyExtent) [[unlikely]] {
            Logger::Warn("Aborting pitch to pitch DMA copy with implausible extents: srcSize 0x{:X}, dstSize 0x{:X}, lineLengthIn 0x{:X}, lineCount 0x{:X}", srcSize, dstSize, *registers.lineLengthIn, *registers.lineCount);
            return;
        }

        TranslatedAddressRange srcMappings, dstMappings;
        if (!TryTranslateRange(srcMappings, *registers.offsetIn, srcSize) || !IsFullyMapped(srcMappings) ||
            !TryTranslateRange(dstMappings, *registers.offsetOut, dstSize) || !IsFullyMapped(dstMappings)) [[unlikely]] {
            Logger::Warn("Aborting pitch to pitch DMA copy touching unmapped memory: src 0x{:X}-0x{:X} dst 0x{:X}-0x{:X}, pitchIn 0x{:X} pitchOut 0x{:X}, lineLengthIn 0x{:X}, lineCount 0x{:X}, bpp 1, layouts: pitch->pitch", u64{*registers.offsetIn}, u64{*registers.offsetIn} + srcSize, u64{*registers.offsetOut}, u64{*registers.offsetOut} + dstSize, *registers.pitchIn, *registers.pitchOut, *registers.lineLengthIn, *registers.lineCount);
            return;
        }

        if (srcMappings.size() != 1 || dstMappings.size() != 1) [[unlikely]] {
            HandleSplitCopy(srcMappings, dstMappings, srcSize, dstSize, u64{*registers.offsetIn}, u64{*registers.offsetOut}, [&](u8 *src, u8 *dst) {
                // Both Linear, copy as is.
                if ((*registers.pitchIn == *registers.pitchOut) && (*registers.pitchIn == *registers.lineLengthIn))
                    std::memcpy(dst, src, *registers.lineLengthIn * *registers.lineCount);
                else
                    for (size_t linesToCopy{*registers.lineCount}, srcCopyOffset{}, dstCopyOffset{}; linesToCopy; --linesToCopy, srcCopyOffset += *registers.pitchIn, dstCopyOffset += *registers.pitchOut)
                        std::memcpy(dst + dstCopyOffset, src + srcCopyOffset, *registers.lineLengthIn);
            });
        } else [[likely]] {
            // Both Linear, copy as is.
            if ((*registers.pitchIn == *registers.pitchOut) && (*registers.pitchIn == *registers.lineLengthIn))
                interconnect.Copy(dstMappings.front(), srcMappings.front());
            else
                for (size_t linesToCopy{*registers.lineCount}, srcCopyOffset{}, dstCopyOffset{}; linesToCopy; --linesToCopy, srcCopyOffset += *registers.pitchIn, dstCopyOffset += *registers.pitchOut)
                    interconnect.Copy(dstMappings.front().subspan(dstCopyOffset, u64{*registers.lineLengthIn}), srcMappings.front().subspan(srcCopyOffset, u64{*registers.lineLengthIn}));
        }
    }

    void MaxwellDma::CopyBlockLinearToPitch() {
        if (registers.srcSurface->blockSize.Width() != 1) [[unlikely]] {
            Logger::Error("Blocklinear surfaces with a non-one block width are unsupported on the Tegra X1: {}", registers.srcSurface->blockSize.Width());
            return;
        }

        gpu::texture::Dimensions srcDimensions{registers.srcSurface->width, registers.srcSurface->height, registers.srcSurface->depth};
        gpu::texture::Dimensions dstDimensions{*registers.lineLengthIn, *registers.lineCount, registers.srcSurface->depth};

        if (srcDimensions.width == 0 || srcDimensions.height == 0 || srcDimensions.depth == 0 || dstDimensions.width == 0 || dstDimensions.height == 0) [[unlikely]] {
            Logger::Warn("Skipping zero-sized block-linear to pitch DMA copy: src {}x{}x{} -> dst {}x{}x{}", srcDimensions.width, srcDimensions.height, srcDimensions.depth, dstDimensions.width, dstDimensions.height, dstDimensions.depth);
            return;
        }

        if (srcDimensions.width > MaxDmaSurfaceDimension || srcDimensions.height > MaxDmaSurfaceDimension || srcDimensions.depth > MaxDmaSurfaceDimension ||
            dstDimensions.width > MaxDmaSurfaceDimension || dstDimensions.height > MaxDmaSurfaceDimension ||
            registers.srcSurface->layer > MaxDmaLayerCount || *registers.pitchOut > MaxDmaPitch) [[unlikely]] {
            Logger::Warn("Aborting block-linear to pitch DMA copy with implausible parameters: src {}x{}x{} (GOB 1x{}x{}, layer {}), dst {}x{}x{}, pitchOut {}", srcDimensions.width, srcDimensions.height, srcDimensions.depth, registers.srcSurface->blockSize.Height(), registers.srcSurface->blockSize.Depth(), registers.srcSurface->layer, dstDimensions.width, dstDimensions.height, dstDimensions.depth, *registers.pitchOut);
            return;
        }

        size_t srcLayerStride{gpu::texture::GetBlockLinearLayerSize(srcDimensions, 1, 1, 1, registers.srcSurface->blockSize.Height(), registers.srcSurface->blockSize.Depth())};
        // The swizzle of the copy is based on the copy dimensions which may be larger than the declared
        // surface, the mapping has to cover the larger of the two extents
        size_t srcExtent{std::max(srcLayerStride, gpu::texture::GetBlockLinearLayerSize(dstDimensions, 1, 1, 1, registers.srcSurface->blockSize.Height(), registers.srcSurface->blockSize.Depth()))};
        size_t dstSize{size_t{*registers.pitchOut} * dstDimensions.height * dstDimensions.depth}; // If remapping is not enabled there are only 1 bytes per pixel

        if (srcLayerStride == 0 || srcExtent > MaxDmaCopyExtent || dstSize > MaxDmaCopyExtent) [[unlikely]] {
            Logger::Warn("Aborting block-linear to pitch DMA copy with implausible extents: srcLayerStride 0x{:X}, srcExtent 0x{:X}, dstSize 0x{:X}", srcLayerStride, srcExtent, dstSize);
            return;
        }

        u64 srcLayerAddress{u64{*registers.offsetIn} + (u64{registers.srcSurface->layer} * srcLayerStride)};

        // Subrect copies write relative to the surface origin, a copy extending past the surface bounds would access outside of the mapping
        bool isSubrect{(util::AlignDown(dstDimensions.width, 64) != util::AlignDown(srcDimensions.width, 64)) || registers.srcSurface->origin.x || registers.srcSurface->origin.y};
        if (isSubrect && (u64{registers.srcSurface->origin.x} + dstDimensions.width > srcDimensions.width || u64{registers.srcSurface->origin.y} + dstDimensions.height > srcDimensions.height)) [[unlikely]] {
            Logger::Warn("Aborting block-linear to pitch subrect DMA copy exceeding surface bounds: origin {}x{}, src {}x{}x{}, dst {}x{}x{}", registers.srcSurface->origin.x, registers.srcSurface->origin.y, srcDimensions.width, srcDimensions.height, srcDimensions.depth, dstDimensions.width, dstDimensions.height, dstDimensions.depth);
            return;
        }

        TranslatedAddressRange srcMappings, dstMappings;
        if (!TryTranslateRange(srcMappings, srcLayerAddress, srcExtent) || !IsFullyMapped(srcMappings) ||
            !TryTranslateRange(dstMappings, *registers.offsetOut, dstSize) || !IsFullyMapped(dstMappings)) [[unlikely]] {
            Logger::Warn("Aborting block-linear to pitch DMA copy touching unmapped memory: src 0x{:X}-0x{:X} ({}x{}x{}, GOB 1x{}x{}, layer {}), dst 0x{:X}-0x{:X} ({}x{}x{}, pitch {}), bpp 1, layouts: block-linear->pitch", srcLayerAddress, srcLayerAddress + srcExtent, srcDimensions.width, srcDimensions.height, srcDimensions.depth, registers.srcSurface->blockSize.Height(), registers.srcSurface->blockSize.Depth(), registers.srcSurface->layer, u64{*registers.offsetOut}, u64{*registers.offsetOut} + dstSize, dstDimensions.width, dstDimensions.height, dstDimensions.depth, *registers.pitchOut);
            return;
        }

        auto copyFunc{[&](u8 *src, u8 *dst) {
            if (isSubrect) {
                gpu::texture::CopyBlockLinearToPitchSubrect(
                    dstDimensions, srcDimensions,
                    1, 1, 1, *registers.pitchOut,
                    registers.srcSurface->blockSize.Height(), registers.srcSurface->blockSize.Depth(),
                    src, dst,
                    registers.srcSurface->origin.x, registers.srcSurface->origin.y
                );
            } else [[likely]] {
                gpu::texture::CopyBlockLinearToPitch(
                    dstDimensions,
                    1, 1, 1, *registers.pitchOut,
                    registers.srcSurface->blockSize.Height(), registers.srcSurface->blockSize.Depth(),
                    src, dst
                );
            }
        }};

        Logger::Debug("{}x{}x{}@0x{:X} -> {}x{}x{}@0x{:X}", srcDimensions.width, srcDimensions.height, srcDimensions.depth, srcLayerAddress, dstDimensions.width, dstDimensions.height, dstDimensions.depth, u64{*registers.offsetOut});

        if (srcMappings.size() != 1 || dstMappings.size() != 1) [[unlikely]]
            HandleSplitCopy(srcMappings, dstMappings, srcExtent, dstSize, srcLayerAddress, u64{*registers.offsetOut}, copyFunc);
        else [[likely]]
            copyFunc(srcMappings.front().data(), dstMappings.front().data());
    }

    void MaxwellDma::CopyPitchToBlockLinear() {
        if (registers.dstSurface->blockSize.Width() != 1) [[unlikely]] {
            Logger::Error("Blocklinear surfaces with a non-one block width are unsupported on the Tegra X1: {}", registers.dstSurface->blockSize.Width());
            return;
        }

        gpu::texture::Dimensions srcDimensions{*registers.lineLengthIn, *registers.lineCount, registers.dstSurface->depth};
        gpu::texture::Dimensions dstDimensions{registers.dstSurface->width, registers.dstSurface->height, registers.dstSurface->depth};

        if (srcDimensions.width == 0 || srcDimensions.height == 0 || srcDimensions.depth == 0 || dstDimensions.width == 0 || dstDimensions.height == 0) [[unlikely]] {
            Logger::Warn("Skipping zero-sized pitch to block-linear DMA copy: src {}x{}x{} -> dst {}x{}x{}", srcDimensions.width, srcDimensions.height, srcDimensions.depth, dstDimensions.width, dstDimensions.height, dstDimensions.depth);
            return;
        }

        if (srcDimensions.width > MaxDmaSurfaceDimension || srcDimensions.height > MaxDmaSurfaceDimension || srcDimensions.depth > MaxDmaSurfaceDimension ||
            dstDimensions.width > MaxDmaSurfaceDimension || dstDimensions.height > MaxDmaSurfaceDimension ||
            registers.dstSurface->layer > MaxDmaLayerCount || *registers.pitchIn > MaxDmaPitch) [[unlikely]] {
            Logger::Warn("Aborting pitch to block-linear DMA copy with implausible parameters: src {}x{}x{}, pitchIn {}, dst {}x{}x{} (GOB 1x{}x{}, layer {})", srcDimensions.width, srcDimensions.height, srcDimensions.depth, *registers.pitchIn, dstDimensions.width, dstDimensions.height, dstDimensions.depth, registers.dstSurface->blockSize.Height(), registers.dstSurface->blockSize.Depth(), registers.dstSurface->layer);
            return;
        }

        size_t srcSize{size_t{*registers.pitchIn} * srcDimensions.height * srcDimensions.depth}; // If remapping is not enabled there are only 1 bytes per pixel

        size_t dstLayerStride{gpu::texture::GetBlockLinearLayerSize(dstDimensions, 1, 1, 1, registers.dstSurface->blockSize.Height(), registers.dstSurface->blockSize.Depth())};
        // The swizzle of the copy is based on the copy dimensions which may be larger than the declared
        // surface, the mapping has to cover the larger of the two extents
        size_t dstExtent{std::max(dstLayerStride, gpu::texture::GetBlockLinearLayerSize(srcDimensions, 1, 1, 1, registers.dstSurface->blockSize.Height(), registers.dstSurface->blockSize.Depth()))};

        if (dstLayerStride == 0 || dstExtent > MaxDmaCopyExtent || srcSize > MaxDmaCopyExtent) [[unlikely]] {
            Logger::Warn("Aborting pitch to block-linear DMA copy with implausible extents: srcSize 0x{:X}, dstLayerStride 0x{:X}, dstExtent 0x{:X}", srcSize, dstLayerStride, dstExtent);
            return;
        }

        u64 dstLayerAddress{u64{*registers.offsetOut} + (u64{registers.dstSurface->layer} * dstLayerStride)};

        // Subrect copies write relative to the surface origin, a copy extending past the surface bounds would access outside of the mapping
        bool isSubrect{(util::AlignDown(srcDimensions.width, 64) != util::AlignDown(dstDimensions.width, 64)) || registers.dstSurface->origin.x || registers.dstSurface->origin.y};
        if (isSubrect && (u64{registers.dstSurface->origin.x} + srcDimensions.width > dstDimensions.width || u64{registers.dstSurface->origin.y} + srcDimensions.height > dstDimensions.height)) [[unlikely]] {
            Logger::Warn("Aborting pitch to block-linear subrect DMA copy exceeding surface bounds: origin {}x{}, src {}x{}x{}, dst {}x{}x{}", registers.dstSurface->origin.x, registers.dstSurface->origin.y, srcDimensions.width, srcDimensions.height, srcDimensions.depth, dstDimensions.width, dstDimensions.height, dstDimensions.depth);
            return;
        }

        TranslatedAddressRange srcMappings, dstMappings;
        bool srcTranslated{TryTranslateRange(srcMappings, *registers.offsetIn, srcSize)};
        bool dstTranslated{srcTranslated ? TryTranslateRange(dstMappings, dstLayerAddress, dstExtent) : false};
        if (!srcTranslated || !IsFullyMapped(srcMappings) || !dstTranslated || !IsFullyMapped(dstMappings)) [[unlikely]] {
            pitchToBlockLinearAborted.fetch_add(1, std::memory_order_relaxed);
            Logger::Warn("Aborting pitch to block-linear DMA copy touching unmapped memory: src 0x{:X}-0x{:X} ({}x{}x{}, pitch {}), dst 0x{:X}-0x{:X} ({}x{}x{}, GOB 1x{}x{}, layer {}), bpp 1, layouts: pitch->block-linear, totals: {} ok / {} aborted", u64{*registers.offsetIn}, u64{*registers.offsetIn} + srcSize, srcDimensions.width, srcDimensions.height, srcDimensions.depth, *registers.pitchIn, dstLayerAddress, dstLayerAddress + dstExtent, dstDimensions.width, dstDimensions.height, dstDimensions.depth, registers.dstSurface->blockSize.Height(), registers.dstSurface->blockSize.Depth(), registers.dstSurface->layer, pitchToBlockLinearOk.load(std::memory_order_relaxed), pitchToBlockLinearAborted.load(std::memory_order_relaxed));

            // Detail which side failed and whether the offending pages are sparse
            // (reserved VA with no pages committed - hardware silently discards
            // writes there and reads return zero) or truly unmapped (hardware
            // raises a GPU MMU fault). A sparse verdict on the dst side would
            // indicate a legitimate copy into an uncommitted region of a
            // streaming heap rather than a garbage address.
            auto describeSide{[this](const char *side, bool translated, const TranslatedAddressRange &mappings, u64 base, size_t size) {
                if (!translated) {
                    Logger::Warn("DMA abort detail: {} side untranslatable (out of GMMU bounds): VA 0x{:X}-0x{:X} (size 0x{:X})", side, base, base + size, size);
                    return;
                }
                u64 va{base};
                for (size_t i{}; i < mappings.size(); i++) {
                    const auto &mapping{mappings[i]};
                    if (mapping.data() == nullptr)
                        Logger::Warn("DMA abort detail: {} span {} VA 0x{:X}-0x{:X} (size 0x{:X}) is {}", side, i, va, va + mapping.size(), mapping.size(), channelCtx.asCtx->gmmu.IsSparseMapped(va) ? "sparse (reserved, uncommitted)" : "unmapped");
                    va += mapping.size();
                }
            }};
            if (!srcTranslated || !IsFullyMapped(srcMappings))
                describeSide("src", srcTranslated, srcMappings, u64{*registers.offsetIn}, srcSize);
            if (!dstTranslated || !IsFullyMapped(dstMappings))
                describeSide("dst", dstTranslated, dstMappings, dstLayerAddress, dstExtent);
            return;
        }

        pitchToBlockLinearOk.fetch_add(1, std::memory_order_relaxed);

        Logger::Debug("{}x{}x{}@0x{:X} -> {}x{}x{}@0x{:X}", srcDimensions.width, srcDimensions.height, srcDimensions.depth, u64{*registers.offsetIn}, dstDimensions.width, dstDimensions.height, dstDimensions.depth, dstLayerAddress);

        auto copyFunc{[&](u8 *src, u8 *dst) {
            if (isSubrect) {
                gpu::texture::CopyPitchToBlockLinearSubrect(
                    srcDimensions, dstDimensions,
                    1, 1, 1, *registers.pitchIn,
                    registers.dstSurface->blockSize.Height(), registers.dstSurface->blockSize.Depth(),
                    src, dst,
                    registers.dstSurface->origin.x, registers.dstSurface->origin.y
                );
            } else [[likely]] {
                gpu::texture::CopyPitchToBlockLinear(
                    srcDimensions,
                    1, 1, 1, *registers.pitchIn,
                    registers.dstSurface->blockSize.Height(), registers.dstSurface->blockSize.Depth(),
                    src, dst
                );
            }
        }};

        if (srcMappings.size() != 1 || dstMappings.size() != 1) [[unlikely]]
            HandleSplitCopy(srcMappings, dstMappings, srcSize, dstExtent, u64{*registers.offsetIn}, dstLayerAddress, copyFunc);
        else [[likely]]
            copyFunc(srcMappings.front().data(), dstMappings.front().data());
    }

    void MaxwellDma::ReleaseSemaphore() {
        if (registers.launchDma->reductionEnable) [[unlikely]]
            Logger::Warn("Semaphore reduction is unimplemented!");

        u64 address{registers.semaphore->address};
        u64 payload{registers.semaphore->payload};
        switch (registers.launchDma->semaphoreType) {
            case Registers::LaunchDma::SemaphoreType::ReleaseOneWordSemaphore:
                channelCtx.asCtx->gmmu.Write(address, payload);
                Logger::Debug("address: 0x{:X} payload: {}", address, payload);
                break;
            case Registers::LaunchDma::SemaphoreType::ReleaseFourWordSemaphore: {
                // Write timestamp first to ensure correct ordering
                u64 timestamp{GetGpuTimeTicks()};
                channelCtx.asCtx->gmmu.Write(address + 8, timestamp);
                channelCtx.asCtx->gmmu.Write(address, payload);
                Logger::Debug("address: 0x{:X} payload: {} timestamp: {}", address, payload, timestamp);
                break;
            }
            default:
                break;
        }
    }

    void MaxwellDma::CallMethodBatchNonInc(u32 method, span<u32> arguments) {
        for (u32 argument : arguments)
            HandleMethod(method, argument);
    }
}
