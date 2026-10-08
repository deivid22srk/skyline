# 02 — Mapa do núcleo do Skyline (branch `feat/sifu-deepseek`)

> Sub-agente A2 (Explorador Skyline) — Task 4-b. Documento gerado somente a partir de
> comandos executados no clone `/home/z/my-project/work/skyline` (branch `feat/sifu-deepseek`,
> confirmada com `git branch --show-current`). Read-only: nenhum arquivo-fonte foi alterado.
> Datas/hash/números citados vêm de saída real de `git`/`ls`/`wc -l`.

## 1. Estrutura do núcleo

Código nativo real: **`app/src/main/cpp/skyline/`** (JNI wrappers em `app/src/main/cpp/*.cpp`,
CMake em **`app/CMakeLists.txt`**). Contagens por `rg --files <dir> | wc -l` e
`wc -l` sobre `.cpp/.h` (LOC ≈ linhas físicas):

| Área | Caminho | Arquivos | ~LOC | Responsabilidade |
|---|---|---|---|---|
| Comum | `app/src/main/cpp/skyline/common/` | 33 | 4.039 | Utilitários: logger, `result.h` (union `Result`), settings, spin_lock, interval_map, trace (Perfetto), uuid. |
| Raiz | `app/src/main/cpp/skyline/*.{cpp,h}` | 18 | 2.225 | Ponto de montagem: `common.h` (struct `DeviceState`), `os.cpp/h` (boot), `gpu`, `soc`, `audio`, `input`, `nce`, `jvm` (bridge JVM). |
| Kernel | `app/src/main/cpp/skyline/kernel/` (24; `kernel/types/` 15 / 1.764 LOC) | 24 | 5.240 | HLE do kernel HOS: `svc.cpp/h` (tabela `SvcTable[0x80]` em `svc.h:269`), `ipc.cpp/h` (`IpcRequest`/`IpcResponse`), `scheduler`, `memory`, `results.h`; KProcess/KThread/KSession/KEvent em `types/`. |
| Services | `app/src/main/cpp/skyline/services/` | 279 | 17.889 | Todos os serviços HOS (ver abaixo). Framework: `base_service.{h,cpp}` + `serviceman.{h,cpp}`. |
| GPU | `app/src/main/cpp/skyline/gpu/` (98; `texture/` 8/4.329, `interconnect/` 14/2.038, `shaders/` 2/519, `cache/` 4/501) | 98 | 21.172 | Vulkan: `gpu.h` classe `GPU` agregando `TraitManager` (quirks de driver), `memory::MemoryManager`, `CommandScheduler`, `PresentationEngine`, `TextureManager`, `BufferManager`, `DescriptorAllocator`, `Megabuffer`, `PipelineCacheManager`, `GraphicsPipelineAssembler`, `ShaderManager` (consome `shader_recompiler` do submodule yuzu). |
| SoC | `app/src/main/cpp/skyline/soc/` (`gm20b/` com `engines/` 17/2.895, `macro/` 4/532; `host1x/` 6/465) | 43 | 6.845 | `soc.h` classe `SOC` = SMMU + host1x; gm20b: GPFIFO, GMMU, engines `maxwell_3d`, `fermi_2d`, `kepler_compute`, `maxwell_dma`, `inline2memory`, `gpfifo`; macro interpreter. |
| NCE | `app/src/main/cpp/skyline/nce/` + `nce.cpp/h` (raiz) | 3 (+2) | 462 (h) | Native Code Execution: `guest.S`/`guest.h` (trampolino), `instructions.h` (patching A64), `NCE::SvcHandler` em `nce.cpp` despacha via `kernel::svc::SvcTable`. |
| HLE hooks | `app/src/main/cpp/skyline/hle/` | 3 | 62 | Hooks de símbolos de sysmodules (`symbol_hook_table.h`). |
| Loader | `app/src/main/cpp/skyline/loader/` | 13 | 1.108 | Formatos: `nro`, `nso`, `nca`, `nsp`, `xci`; base `loader.cpp/h`, `executable.h`. |
| VFS | `app/src/main/cpp/skyline/vfs/` | 26 | 1.978 | Backings/filesystems: partition, rom, OS, android_asset, ctr_encrypted, nacp/npdm/nca/ticket. |
| Input | `app/src/main/cpp/skyline/input/` | 18 | 1.915 | NPADs, npad_device, touch, shared_mem (espelho HID). |
| Audio | `app/src/main/cpp/skyline/audio.{cpp,h}` + `services/audio/` | 1+6 | ~— | `audio.h` usa `AudioCore::AudioOut/AudioRenderer` (submodule `audio-core`, fork do autor) + serviços `audout/audren/hwopus`. |
| Crypto | `app/src/main/cpp/skyline/crypto/` | 4 | 261 | `KeyStore` (prod.keys/title.keys, Key128/256), `aes_cipher` (mbedTLS). |
| Applets | `app/src/main/cpp/skyline/applet/` | 14 | 1.159 | swkbd, error, controller, player_select, web applets. |

Serviços presentes em `services/` (listagem de `ls services/`): account, am, aocsrv, apm, applet,
audio, bcat, bt, btm, capsrv, codec, common, fatalsrv, friends, fssrv, glue, hid, hosbinder, irs,
ldn, lm, mii, mmnv, nfp, nifm, nim, nvdrv (≈ gpu/nvdrv), olsc, pctl, pl, prepo, ro, settings, sm,
socket, spl, ssl, timesrv, visrv (≈ vi). **Não existem** diretórios `services/nvn` nem `services/omm`
(`ls` retorna "No such file or directory" para ambos).

## 2. Convenções de código

- **Namespace**: `skyline` e subnamespaces por camada: `skyline::kernel`, `skyline::service` (e
  `skyline::service::<área>::result`), `skyline::gpu`, `skyline::soc`, `skyline::nce`,
  `skyline::kernel::svc`, `skyline::hle`. Confirmado nos headers citados.
- **Resultados**: `union Result` com bitfields `module:9 / id:12` em `common/result.h` (não há macro
  `THROW_RESULT`; INCERTO se existir em outra forma — não encontrada por grep em `common/result.h`).
  Constantes por módulo: `constexpr Result Name(module, id)` em `kernel/results.h` e
  `services/<área>/results.h` (ex.: `skyline::service::fssrv::result::PathDoesNotExist(2, 1)`).
  Erros internos: `throw exception(...)` (`common/exception.h`), capturado por SVC/service com stack
  trace via `loader->GetStackTrace` (`nce.cpp`, `base_service.cpp`).
- **Framework de serviços**: `BaseService` (`services/base_service.h`) + macros `SFUNC(id, Class, Fn)`,
  `SFUNC_TIPC`, `SFUNC_BASE`, `SERVICE_DECL` (constrói `frozen::make_unordered_map` de
  função-id) e `SRVREG`. Despacho: `BaseService::HandleRequest` (`services/base_service.cpp:22`)
  → `GetServiceFunction(id, isTipc)`. Registro por nome: macro `SERVICE_CASE(class, "nome")`
  dentro de `ServiceManager::CreateOrGetService` (`services/serviceman.cpp`, ~linha 67+);
  entrada de clients: `sm::IUserInterface::GetService` → `manager.NewService` (`services/sm/IUserInterface.cpp`).
  Estado global compartilhado em `GlobalServiceState` (`timesrv`, `sharedFontCore`, `sharedIirCore`, `nvdrv`).
- **Threading**: não há `type::NiceConstant` (grep sem resultado em `kernel/`). Threads guest:
  `KThread::StartThread` cria `std::thread` (`kernel/types/KThread.cpp:223`); escalonamento em
  `kernel/scheduler.cpp`. Acesso ao estado por thread via `DeviceState::thread` (thread_local,
  `common.h:70-71`) e `state.ctx` (`nce::ThreadContext*`).
- **CPU**: **somente NCE** (nenhuma ocorrência de "dynarmic" em `CMakeLists.txt`, `nce.*` ou
  fontes — grep sem matches). SVCs do guest são interceptados (patching `instructions::Svc` em
  `nce.cpp:359-425`) e despachados por `kernel::svc::SvcTable` (array `constexpr` de
  `SvcDescriptor`, `svc.h:253-269`), handlers `void svc::X(const DeviceState&)`.
- **Estilo**: C++20 (`CMAKE_CXX_STANDARD 20`), Vulkan-Hpp sem setters/construtores/designated
  initializers (`app/CMakeLists.txt:62-65`), `[[unlikely]]`, fmt-lib para logs (`Logger::Warn/Debug/...`).

## 3. Estado da branch `feat/sifu-deepseek`

Base: merge-base com `main` = `8e521889` ("ci: add build workflow; point sirit/shader-compiler
submodules to mirrors"). Último commit: `9c39760f` (2026-10-05). **11 commits** em
`git log --oneline --no-merges main..feat/sifu-deepseek`:

| Hash | Assunto |
|---|---|
| `9c39760f` | diag(gpu): add stall watchdogs for GPFIFO, fence waits and blocking submits |
| `ca3a82e4` | fix(gpu): honor VK_EXT_texel_buffer_alignment for texel buffer offsets |
| `b970ab04` | fix(gpu): correct DEVICE_LOST diagnostic references (slots, LockedBuffer) |
| `96e5ccfc` | fix(gpu): fix u32->u16 narrowing in texel buffer/storage image descriptor indices |
| `0df75b76` | feat(gpu): enrich DEVICE_LOST diagnostics and stub extra nsd commands |
| `01cb15a3` | feat(gpu): implement texel buffer and storage image descriptor bindings |
| `c4c3b78a` | fix(gpu): validate Maxwell DMA copy regions before accessing them |
| `77c41fbf` | fix(settings): register activity-result launchers from the hosting fragment |
| `e44e61b7` | fix(settings): use 3-arg MaterialColors.getColor (2-arg overload expects View) |
| `cc1a1008` | fix(settings): escape apostrophe in French display category summary |
| `de0c477d` | feat(settings): organize settings into category screens |

`git diff --shortstat main...feat/sifu-deepseek`: **86 arquivos, +2147/−456**. Áreas nativas
alteradas (`git diff --name-only | grep cpp/skyline`, 22 arquivos): `gpu/` (buffer, command_scheduler,
fence_cycle, interconnect/command_executor, interconnect/common/textures, interconnect/kepler_compute/
pipeline_manager, interconnect/maxwell_3d/pipeline_manager, trait_manager, gpu.cpp), `soc/gm20b/`
(gpfifo, engines/maxwell_dma), `services/socket/nsd/IManager`, `common/{settings,android_settings}.h`.

**Foco confirmado**: correções/diagnóstico GPU + stubs de rede para o jogo Sifu. Evidência direta no
código: `services/socket/nsd/IManager.h` contém o comentário "Sifu (UE4) calls command 0x15 during
network subsystem initialisation, unimplemented commands are non-fatal but produce log spam".
Os commits `01cb15a3`/`96e5ccfc`/`ca3a82e4` (texel buffer + storage image bindings, alinhamento
`VK_EXT_texel_buffer_alignment` — típico de uso de storage images/compute por UE4), `c4c3b78a`
(Maxwell DMA) e `9c39760f` (watchdogs de stall GPFIFO/fence) tocam exatamente o caminho
gm20b→interconnect→Vulkan. 5 commits restantes são UI/settings (reorganização de categorias).

## 4. Build & dependências

- **CMake**: `app/CMakeLists.txt` (não há CMakeLists em `app/src/main/cpp/`). Projeto `Skyline` v0.3,
  C/C++20/ASM; `ANDROID_STL=none` + libcxx/libcxxabi próprios (submodule llvm); libs estáticas;
  release com `-Ofast -flto=full`; alvo final `skyline` SHARED (≈240 fontes listados, linhas
  156-403) linkando `shader_recompiler` e `audio_core`.
- **Gradle**: `app/build.gradle` — `compileSdk 33`, `minSdk 29`, `targetSdk 33`,
  `ndkVersion '25.0.8775105'`, cmake `3.22.1+`, AGP `7.2.2` (`build.gradle:16`), Kotlin `1.7.21`
  (`build.gradle:5`), flavors `full`/`dev`, buildTypes release/reldebug/debug com
  `-DCMAKE_BUILD_TYPE=RELEASE|RELWITHDEBINFO`.
- **Submodules** (`.gitmodules`, 21): fmt, oboe (1.3-stable), lz4, frozen, tzcode, perfetto (v12.x),
  vkhpp (Vulkan-Hpp), vkma (VMA), mbedtls, opus, boost (fork skyline), llvm, range (range-v3),
  sirit (espelho yuzu), shader-compiler (strato-emu — shader_recompiler), adrenotools, robin-map,
  thread-pool, cubeb, **audio-core (`github.com/deivid22srk/audio-core` — fork do dono desta branch,
  apontado pelo commit `01cb15a3`)**.

## 5. Observações para o port (onde entrariam melhorias do Strato)

1. **GPU pipeline**: melhorias de texture/pipeline/descriptor do Strato entrariam em
   `gpu/texture*`, `gpu/interconnect/{maxwell_3d,kepler_compute,common}` e `gpu/trait_manager.*`
   — é onde a branch já concentra atividade (risco de conflito com os 8 commits GPU acima).
2. **Engines gm20b**: mudanças em Maxwell 3D/DMA/GPFIFO do Strato mapeiam para
   `soc/gm20b/engines/*` e `soc/gm20b/gpfifo.*`; a branch adicionou watchdogs locais em `gpfifo.*`
   (commit `9c39760f`) que precisam ser reconciliados.
3. **Serviços novos/faltantes**: não há `nvn` nem `omm` no Skyline; se o Strato portado exigir,
   o padrão é criar `services/<área>/{I...}.{h,cpp}` + `SERVICE_CASE(...,"nome")` em
   `services/serviceman.cpp` + entradas no `app/CMakeLists.txt` (linhas ~156-403).
4. **Result codes**: Strato usa convenção própria de `Result`; no Skyline definir constantes
   `constexpr Result` em `services/<área>/results.h` (padrão `Result(módulo, id)` de 9/12 bits).
5. **CPU**: sem JIT alternativo — qualquer melhoria de dynarmic do Strato não tem destino
   direto; melhorias de NCE iriam para `nce.cpp`/`kernel/{svc,scheduler}`.
6. **Settings**: flags de jogo/hacks passam por `common/settings.h` + `common/android_settings.h`
   (ambos já tocados pela branch) e UI Kotlin `emu/skyline/settings/` — cuidado com o duplo
   commit de settings desta branch.
