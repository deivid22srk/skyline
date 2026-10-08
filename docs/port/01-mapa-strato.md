# Port Strato → Skyline — Documento A1: Mapa do núcleo do Strato

> Fonte: clone local `/home/z/my-project/work/strato` (branch `master`, HEAD `ae1566a4`,
> "Fix accidental recursion with trap handling", 2024-04-28). Todos os caminhos, contagens
> e hashes abaixo foram obtidos de comandos executados neste clone. Data da coleta: 2026-10-05.

## 1. Estrutura do núcleo

O código nativo NÃO está em `app/src/main/cpp/strato` — o diretório raiz do core é
**`app/src/main/cpp/skyline/`** (fork do Skyline rebatizado; pacote Java `org.stratoemu.strato`).
Na raiz `app/src/main/cpp/` há apenas 3 pontes JNI: `driver_jni.cpp`, `emu_jni.cpp`,
`loader_jni.cpp` (486 LOC no total).

Núcleo completo: **602 arquivos, ~67.748 LOC C++** (`.cpp/.hpp/.h/.inc`) em `app/src/main/cpp/skyline`.

| Área | Caminho raiz real | Arquivos | ~LOC | Responsabilidade |
|---|---|---|---|---|
| kernel | `skyline/kernel/` | 25 | 5.663 | SVCs (`svc.cpp/h`, `SvcTable[0x80]`, `SvcContext`), memória (`memory.cpp/h`), IPC, scheduler, tipos K* (`types/KProcess/KThread/KMemory/KSharedMemory/KTransferMemory/KSyncObject/KSession/KEvent`, `KObject.h`), `results.h` |
| services | `skyline/services/` | 303 | 20.260 | HLE dos serviços HOS; maiores: nvdrv (28/3.627), am (45/2.312), timesrv (19/2.299), hosbinder (8/1.700), fssrv (15/905), audio (12/807), socket (6/685), hid (6/685), visrv (14/598), account (10/579), nifm (8/476), ldn (4/475), glue (9/445); também sm, spl, pctl, pl, ro, settings, bcat, bt/btm, capsrv, clkrst, codec, friends, irs, lbl, mii, mmnv, nfp, nim, ntc, olsc, prepo, psm, ssl, ts, fatalsrv, applet |
| gpu | `skyline/gpu/` | 98 | 20.908 | Vulkan: texture cache (`texture/`, `texture_manager`), pipeline (`interconnect/maxwell_3d/pipeline_manager`, `graphics_pipeline_assembler`, `pipeline_cache_manager`), engines HLE (`interconnect/`: maxwell_3d, fermi_2d, kepler_compute, maxwell_dma, inline2memory, `command_executor`), buffers (`buffer*.cpp`, `megabuffer`), `presentation_engine`, `shader_manager`, `descriptor_allocator`, `command_scheduler`, `trait_manager` |
| soc | `skyline/soc/` | 43 | 6.672 | HW Tegra: `gm20b/` (GMMU, GPFIFO, canais, engines de registers maxwell/fermi/kepler/dma, `macro/`), `host1x/`, `smmu` |
| jit/cpu | `skyline/nce/` + `skyline/nce.cpp/h` | 3 + 2 | 450 + ~470 | NCE (Native Code Execution): recompilação de blocos ARMv8→host com hooks de memória; `guest.S/guest.h/instructions.h` |
| loader | `skyline/loader/` | 13 | 1.146 | Formatos NCA, NSO, NRO, NSP, XCI + `loader.cpp` (patch de ELF, dynsym, hooks de símbolo). Chaves ficam em `crypto/key_store.cpp/h` |
| input | `skyline/input/` + `input.cpp/h` | 18 | 1.915 | Dispositivos HID (teclado, mouse, touch, motion, vibration) e conversão Android→HID |
| audio | `skyline/audio.cpp/h` + `services/audio/` | 12 (svc) | 807 (svc) | AudioOut/Renderer via cubeb + audio-core (yuzu); serviços `IAudioOut`, `IAudioRenderer`, `IAudioDevice` |
| vfs | `skyline/vfs/` | 26 | 1.983 | Backings (`os_backing`, `android_asset_backing`, `ctr_encrypted_backing`), filesystems (`partition_filesystem`, `rom_filesystem`, `os_filesystem`), metadados (`nacp`, `npdm`, `nca`, `ticket`) |
| common | `skyline/common/` | 35 | 4.774 | Utilitários: `result.h`, `interval_map.h`, `interval_list.h`, `circular_queue.h`, `spin_lock.h`, `trap_manager.cpp/h`, `settings.h`, `signal.cpp`, `trace.h` (Perfetto), `uuid`, `address_space` |
| applet | `skyline/applet/` | 14 | 1.159 | Applets HLE: swkbd (teclado), controller, error, player_select, web |
| crypto | `skyline/crypto/` | 4 | 261 | AES-CTR (`aes_cipher`) e key store (chaves de firmware/prod) |
| hle | `skyline/hle/` | 3 | 122 | Tabela de hooks de símbolos guest (`symbol_hook_table.h`, `symbol_hooks.cpp`) |
| logger | `skyline/logger/` | 2 | 374 | `AsyncLogger` (fila assíncrona, LOGE/LOGW/LOGI/LOGD) |
| raiz `skyline/` | `skyline/*.cpp/h` | 15 | 2.061 | Cola: `os.cpp/h` (kernel::OS, boot), `jvm.cpp/h`, `gpu.cpp/h`, `nce.cpp/h`, `audio.cpp/h`, `input.cpp/h`, `soc.h`, `common.h` (`DeviceState`) |

## 2. Convenções de código

- **Licença**: todo arquivo com cabeçalho `SPDX-License-Identifier: MPL-2.0` + "Copyright © Skyline Team".
- **Namespaces** (amostrados de headers reais): `skyline::kernel` (`os.h`, `svc.h` = `skyline::kernel::svc`), `skyline::service` (+ `skyline::service::nvdrv`), `skyline::gpu` (`gpu.h`), `skyline::gpu::interconnect::maxwell`, `skyline::soc`, `skyline::nce`, `skyline::input`, `skyline::vfs`, `skyline::loader`, `skyline::applet`. Exceção: `audio.h` usa `AudioCore::AudioOut` (lib audio-core do yuzu).
- **Erros/resultados**:
  - `common/result.h`: `union Result { u32 raw; { u16 module:9; u16 id:12; } }` e `ResultValue<T>` (opcional + result). `constexpr Result()` = sucesso.
  - Constantes em namespace `result::` — ex. `kernel/results.h`: `constexpr Result InvalidSize(1, 101)`, `InvalidHandle(1, 114)`, `TimedOut(1, 117)`, etc. SVCs retornam via `ctx.w0 = result::X` (nada é lançado para erros HOS).
  - **Não foram encontradas macros `THROW_RESULT`/`UNIMPLEMENTED*`** em `common/` (grep em `common/*.h`). O estilo é: erro = `Result`; bug interno = `throw skyline::exception("fmt {}", args)` (`common/exception.h`, runtime_error + stack trace, usado p.ex. em `services/base_service.cpp:40`).
  - Macros em `common/macros.h` são só p/ enums: `ENUM_CASE`, `ENUM_STRING`, `ENUM_CASE_PAIR`, `ENUM_SWITCH`.
- **Registro de serviços HOS** (`services/base_service.h`): classe deriva de `BaseService` e usa `SERVICE_DECL(...)` que cria `frozen::make_unordered_map` de `SFUNC(id, Class, Function)` / `SFUNC_TIPC` (flag `1<<31`) / `SFUNC_BASE`; dispatch por `GetServiceFunction(id, isTipc)` retornando `ServiceFunctionDescriptor`. Serviços são criados com `SRVREG(Class, ...)` = `std::make_shared` + `(state, manager)`; `ServiceManager` em `services/serviceman.cpp/h`; `sm` resolve nomes (u64 de 8 bytes).
- **Assinatura de handler**: `Result F(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)`; SVC: `void F(const DeviceState &state, SvcContext &ctx)` com registradores em `ctx.x*/w*` (`common/wregister.h`, `kernel/svc_context.h`).
- **Threading/scheduler** (`kernel/scheduler.h`): 1 thread host por thread guest; scheduler por núcleos com `constant::CoreCount = 4` e `ParkedCoreId = 4`, prioridade i8 com herança (atomics em `KThread`); `DeviceState` (`common.h:59`) carrega `process`, e `thread_local` estático `thread`/`ctx` (contexto NCE). IPC roda nas próprias threads guest; GPU tem `command_scheduler` com filas (`common/circular_queue.h`).
- **Tipos custom**: `KObject` (base, `state` + `KType`), `KProcess/KThread/KSession/KEvent/...`; `DeviceState` como DI central; `span<T>` próprio (`common/span.h`); `frozen` (containers constexpr) nas declarações de serviço.
- **Logging**: `logger/logger.h` define `LOGE/LOGW/LOGI/LOGD` (+ variantes `*NF` sem flush) sobre `AsyncLogger` (níveis Verbose..Disabled, `logLevel` em settings).

## 3. Build & dependências

- **CMake**: `app/CMakeLists.txt` (401 linhas; projeto `Skyline 0.3`, C++20, `min 3.16`).
  - Flags: geral `-fno-strict-aliasing -Wno-unused-command-line-argument -fwrapv`; RELEASE `-Ofast -flto=full -fno-stack-protector -DNDEBUG`; DEBUG/RELWITHDEBINFO também `-Ofast` p/ libs, core com `-O0 -g3 -glldb -gdwarf-5` / `-O1 -g3 ...`.
  - Lib final: `add_library(skyline SHARED ...)` listando ~60 fontes a partir de `src/main/cpp` (JNI + core).
  - Defines Vulkan-Hpp: `VK_USE_PLATFORM_ANDROID_KHR`, `VULKAN_HPP_NO_SPACESHIP_OPERATOR/NO_STRUCT_CONSTRUCTORS/NO_SETTERS/NO_SMART_HANDLE`, `VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1`, `VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=0`.
- **Gradle** (`app/build.gradle`): `compileSdk 34`, `minSdk 29`, `targetSdk 33`, `abiFilters "arm64-v8a"`, **`ndkVersion '26.1.10909125'`**, CMake `3.22.1+`; flavors por dimensão `version` (incl. `dev` com sufixo `.dev`); Vulkan validation layers em `libraries/vklayers` para reldebug/debug.
- **`.gitmodules`** (21 submódulos, em `app/libraries/`): `{fmt}`, Oboe (branch 1.3-stable), LZ4, Frozen, tzcode (fork skyline-emu/tz), Perfetto (releases/v12.x), Vulkan-Hpp, Vulkan Memory Allocator, Mbed TLS, Opus, **Boost (fork strato-emu/boost)**, range-v3, **Sirit (fork strato-emu/sirit)**, **shader-compiler (fork strato-emu/shader-compiler, "yuzu Shader Compiler")**, libadrenotools (bylaws), robin-map, thread-pool, **cubeb (fork skyline-emu)**, **audio-core (fork skyline-emu, "yuzu Audio Core")**. Diretórios extras não-submódulo: `adrenotools`, `renderdoc` (API in-app), `vklayers`, `vkma.cpp`.

## 4. Histórico recente — 80 commits (git log --oneline --no-merges -80 -- app/src/main/cpp)

| Hash | Assunto |
|---|---|
| ae1566a4 | Fix accidental recursion with trap handling |
| 83111c2a | Move memory trapping infrastructure outside of NCE |
| 686a2512 | Move SvcTable definition out of header files |
| 18626a42 | Introduce a generic register context for SVCs |
| 80b3b223 | KProcess: correctly handle empty optional chunk |
| b51c31dc | memory: update KMemory to use guest addresses |
| 8addba32 | memory: use guest addresses everywhere |
| d693cac5 | memory: fix insertion at the beginning of the chunks map |
| f9f9b6de | Loader: skip patching non 64-bit executables |
| bdb4e3fd | Loader: make dynsym handling more generic |
| 1102f427 | Move symbol hooking setup code out of loader |
| 0b0e48c6 | Fix invalid switch-case syntax resulting from `nvdrv` macro expansion (#242) |
| 135b7f31 | Implement info reading from Ro section (#173) |
| 13232591 | Iterate through siblings without recursion in `vfs::TraverseDirectory` (#205) |
| 45aa3945 | Update NDK to `26.1.10909125` |
| 421c2f46 | Change package name to `org.stratoemu.strato` |
| 74173a39 | Rework signal handling to remove per-thread handlers |
| 644c8f3c | Construct queue consumer threads after queues |
| b0207ab6 | Address feedback |
| 35b90c96 | Stub ActivateConsoleSixAxisSensor and InitializeSevenSixAxisSensor |
| 4f6d5b28 | Stub SetShimLibraryVersion |
| 395e08d0 | Stub GetIndirectLayerConsumerHandle |
| c3eef507 | Stub SetAutoSleepDisabled and IsAutoSleepDisabled |
| 937eacef | Stub SetLcdBacklighOffEnabled |
| a523af21 | Stub some services related to StereoVision |
| 13f88f75 | Stub ResetSevenSixAxisSensorTimestamp |
| d32aa861 | Stub some services in ILblController |
| ffb93d21 | Stub SetVrModeEnabled, BeginVrModeEx and EndVrModeEx |
| c43912c1 | Stub GetAlbumFileList0AafeAruidDeprecated |
| e05de1ac | Stub IsVibrationDeviceMounted |
| 80911207 | Stub IsIlluminanceAvailable and GetCurrentIlluminanceEx |
| f0430e46 | Stub GetAlbumFileList3AaeAruid |
| d241e846 | Stub CheckFirmwareVersion |
| 1bfdcaa3 | Stub ReportUserIsActive |
| 3ca24b9f | Stub GetTemperatureMilliC |
| 9465bfeb | Stub StopImageProcessorAsync |
| eb2d04ea | Stub GetSaveDataSizeMax |
| 0789ba1a | Address feedback |
| 54d4586e | Extract fonts from firmware |
| 32c1519b | Fix file creation in OsFileSystem |
| b0cee4a2 | Show installed firmware version |
| 555a9d07 | utils: Remove `Format` proxy utility for implicit pointer formatting |
| 288f016f | logger: Migrate remaining calls to the new logger and remove old logger |
| 772970d1 | logger: Enable the new async logger |
| e0d4dde4 | settings: Add logLevel as a proper setting |
| c18e13fa | logger: Refactor all Verbose log calls to the new macros |
| d151daff | logger: Refactor all Debug log calls to the new macros |
| 4747107d | logger: Refactor all Info log calls to the new macros |
| e8949701 | logger: Refactor all Warning log calls to the new macros |
| 3c6e357e | logger: Refactor all Error log calls to the new macros |
| 3a1252a5 | logger: Introduce a new AsyncLogger |
| 2c363ab9 | common: Improve CircularQueue |
| 1447b0aa | common: Simplify some includes |
| 5787c7e7 | common: Remove compile-time formatting utilities |
| e2fe6fee | services: Add missing log argument |
| 60bd64b7 | common: Remove redundant formatter from base.h |
| 558de46c | common: Introduce compile-time pointer formatting utilities |
| aa0a45eb | RequestSyncDeliveryCache service stub |
| 0eee5b81 | Implement simple system archives reading |
| 7026470b | RequestSyncDeliveryCache service stub |
| 95de8f9d | Don't show dialog when exiting gracefully |
| 17ebf18f | Report crash in a dialog |
| 75cc4215 | Fix missing comma in service declaration |
| 191cc411 | Save log files with the `.log` file extension |
| d95bb121 | Implement recursive delete `IFileSystem` service calls |
| 920a3b96 | Implement `nfp::IUser::GetApplicationAreaSize` |
| e4f9fd62 | Some stubs for better compatibility |
| 869eecf2 | Fix a whitespace inconsistency in bcat service |
| 8255308d | Show pipelines loading screen while GPU is loading them |
| e41ff6b1 | Implement graphics pipelines loading screen core logic |
| 20a9ab65 | Implement required bsd calls |
| 460e1448 | Stub some ldn calls |
| 6ac7d22e | Stub IAuthorizationRequest |
| 41882aab | Stub some required calls |
| a299bb3c | Implement GetCurrentIpConfigInfo |
| 68bb8256 | Stub correctly some nifm calls |
| 43195927 | Implement GetAddrInfoRequest |
| 8113413a | Stub ntc |
| 32f61e77 | Add setting to enable internet |
| 96308706 | Address remaining feedback |

(Último commit tocando cpp: `ae1566a4`, 2024-04-28. Total do histórico: 1.684 commits não-merge tocando `app/src/main/cpp`.)

## 5. Commits de núcleo destacados (bugfix/perf/compat — arquivos verificados com `git show --stat`)

| Hash | Assunto | Arquivos principais / por quê |
|---|---|---|
| ae1566a4 | Fix accidental recursion with trap handling | `common/trap_manager.cpp` (1 linha); corrige regressão do próprio 83111c2a |
| 83111c2a | Move memory trapping infrastructure outside of NCE | cria `common/trap_manager.cpp/h` (272 linhas); refatora `gpu/buffer.cpp`, `shader_cache`, `kepler_compute` — desacopla trapping de memória do JIT |
| 74173a39 | Rework signal handling to remove per-thread handlers | `input.cpp`, `kernel/scheduler.cpp`, …; corrige crash com ART/sigchain após patches de segurança Android 2023-08 |
| d693cac5 | memory: fix insertion at the beginning of the chunks map | `kernel/memory.cpp`; bug de loop infinito ao inserir chunk no início do mapa (insert_or_assign) |
| 8addba32 | memory: use guest addresses everywhere | `kernel/memory.cpp/h` (+217/−114), `svc.cpp`, `KProcess`, `loader.cpp`, `ro`; rework central do gerenciador de memória |
| b51c31dc | memory: update KMemory to use guest addresses | `KMemory/KSharedMemory/KTransferMemory`; acompanha 8addba32 |
| 80b3b223 | KProcess: correctly handle empty optional chunk | `kernel/types/KProcess.cpp`; correção de borda no mapa de chunks |
| 18626a42 | Introduce a generic register context for SVCs | cria `common/wregister.h`, `kernel/svc_context.h`; reescreve `svc.cpp` (526 linhas tocadas) — desacopla SVCs do ThreadContext |
| 686a2512 | Move SvcTable definition out of header files | `kernel/svc.cpp`/`svc.h`; reduce header bloat da tabela de SVCs |
| f9f9b6de | Loader: skip patching non 64-bit executables | `loader/loader.cpp` (+26/−13); evita patch inválido em ELF não-64bit |
| bdb4e3fd | Loader: make dynsym handling more generic | `loader/loader.cpp/h`; ResolveSymbol template (Elf64/Elf32) |
| 1102f427 | Move symbol hooking setup code out of loader | cria `hle/symbol_hooks.cpp/h`; limpa `loader.cpp`/`nce.h` |
| 135b7f31 | Implement info reading from Ro section | `loader/nso.cpp/h` (+45); lê .rodata p/ info de módulo |
| 0b0e48c6 | Fix invalid switch-case syntax from `nvdrv` macro expansion | `nvdrv/devices/deserialisation/macro_def.inc` + 6 devices; compat de GPUs via nvdrv |
| 13232591 | Iterate siblings without recursion in `vfs::TraverseDirectory` | `vfs/rom_filesystem.cpp`; corrige stack overflow |
| 644c8f3c | Construct queue consumer threads after queues | `gpu/command_scheduler.h`, `gpu/presentation_engine.h`; race na inicialização da GPU |
| 2c363ab9 | common: Improve CircularQueue | `common/circular_queue.h` (+69); base das filas do command scheduler |
| 32c1519b | Fix file creation in OsFileSystem | `vfs/os_filesystem.cpp`; bugfix de FS |
| d95bb121 | Implement recursive delete `IFileSystem` | `services/fssrv/IFileSystem.cpp/h`; compat FS (DeleteRecursively) |
| 20a9ab65 | Implement required bsd calls | `services/socket/bsd/IClient.cpp/h` (+238/−25); rede: sockets p/ jogos online |
| 43195927 | Implement GetAddrInfoRequest | `services/socket/sfdnsres/IResolver.cpp` (+169); resolução DNS |
| 3a1252a5 | logger: Introduce a new AsyncLogger | `logger/logger.cpp/h` (+374), `CMakeLists`; perf de logging (perde tempo no emulador real) |
| 54d4586e | Extract fonts from firmware | `loader_jni.cpp` (+102); fontes compartilhadas p/ swkbd |
| e41ff6b1 | Implement graphics pipelines loading screen core logic | `jvm.cpp/h`, UI; UX de carregamento de pipelines |
| 45aa3945 | Update NDK to `26.1.10909125` | toolchain; relevante p/ reprodutibilidade do build |

## 6. Observações para o port

1. **Strato É um fork do Skyline** (cabeçalhos "Copyright © Skyline Team", projeto CMake `Skyline`, pacote rebatizado p/ `org.stratoemu.strato` em `421c2f46`). O destino `deivid22srk/skyline` é um fork mais antigo do mesmo código — o diff de núcleo entre eles tende a ser pequeno e semanticamente claro.
2. **Divergência de toolchain**: Strato usa NDK `26.1.10909125`/compileSdk 34; o fork Skyline (worklog T1) usa NDK `25.0.8775105`/compileSdk 33. Commits de kernel/trap usam APIs que podem depender do NDK novo — validar antes de portar `83111c2a`/`74173a39`.
3. **Rework de memória (8addba32/b51c31dc/d693cac5/80b3b223)** é uma série coesa sobre `kernel/memory.cpp` — portar como bloco único; conflita com qualquer mudança local de KMemory no destino.
4. **Infra de traps (83111c2a + ae1566a4)** introduz `common/trap_manager` usado por GPU (`buffer.cpp`, `shader_cache`) — requer que o destino tenha os mesmos call-sites de `gpu/`.
5. **Convenções a preservar no port**: `Result`/`ResultValue` e constantes `result::` (sem macros THROW); `SERVICE_DECL/SFUNC` com `frozen`; assinaturas de SVC com `SvcContext` (só existem após `18626a42` — commits anteriores de svc.cpp usam ThreadContext direto e aplicarão conflito).
6. **Compat de serviços**: a onda de stubs (35b90c96…eb2d04ea, ~20 commits) é de baixo risco e alta taxa de acerto para jogos; candidatos rápidos a port se o destino não os tiver.
7. INCERTO: comparação linha-a-linha com o destino (quais commits já estão lá) não foi feita neste passo — cabe ao A2 (mapa Skyline) cruzar os hashes.
