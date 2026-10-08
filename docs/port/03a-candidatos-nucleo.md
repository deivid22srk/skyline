# 03a — Candidatos ao port: bugfixes de núcleo do Strato

Sub-agente: A3-a (Analista de Diff — núcleo) · Task ID: 5-a

## Metodologia

- **Merge-base**: `6aef7fdd1ee55bac5ef78a4d4a4ff2945efc24d7` ("Stub some services"), confirmada com
  `git merge-base d693cac5 6aef7fdd...` no repo `/home/z/my-project/work/skyline` (branch `feat/sifu-deepseek`).
- Os 18 commits foram confirmados **strato-only** (`git merge-base --is-ancestor <h> HEAD` falhou para todos).
- Para cada commit: `git show <hash> --stat` + patch completo; depois verificação no Skyline HEAD via `rg`
  por identificadores distintos do patch (constantes hexadecimais, nomes de função, comentários) e,
  quando necessário, `git log -S` e `git diff --stat 6aef7fdd..HEAD -- <path>` para medir divergência.
- **Fato estrutural chave**: `kernel/memory.cpp` e `kernel/types/KProcess.cpp` do Skyline HEAD são
  **idênticos** ao merge-base (`git diff 6aef7fdd..HEAD` vazio para ambos), enquanto o Strato os
  reestruturou logo após o fork (série `b8bc2107` "Restructure the memory manager" → `MapInternal` com
  `std::map` de `ChunkDescriptor` + `GetChunk`). Isso torna os fixes de memória do Strato inaplicáveis
  diretamente (ver seções 1 e 2).
- Vereditos: ALREADY_PRESENT / PORTABLE / DIVERGED / SKIP / INCERTO. Esforço: S (<1h) / M (~1-3h) / L (>1d).

Resumo: **0 ALREADY_PRESENT · 15 PORTABLE (4 com adaptação) · 2 DIVERGED · 0 SKIP · 1 INCERTO-parcial**.

---

## 1. d693cac5 — memory: fix insertion at the beginning of the chunks map

- **Área**: kernel/memory · **Veredito**: DIVERGED · **Esforço**: n/a · **Risco**: — · **Prioridade**: P3 (reavaliar só se o gerenciador de memória do Strato for portado em bloco)
- **Patch**: em `MemoryManager::MapInternal`, protege `--firstChunkBase` quando `lower_bound` retorna
  `chunks.begin()` (`if (newDesc.first <= firstChunkBase->first && firstChunkBase != chunks.begin())`),
  e troca `chunks[x] = v` / `chunks.insert(...)` por `chunks.insert_or_assign(...)` (14 trocas) para
  não quebrar o invariante do mapa.
- **Evidência de divergência**: Skyline HEAD não possui `MapInternal`/`lower_bound`/`GetChunk`
  (`rg` em `app/src/main/cpp/skyline/kernel/memory.cpp` = vazio); usa vetor:
  `memory.h:215` `std::vector<ChunkDescriptor> chunks;`, `memory.cpp:238` `InsertChunk(...)` com guarda
  explícita `if (upper == chunks.begin()) throw exception("InsertChunk: Chunk inserted outside address space...")`.
  O `MapInternal` de mapa só existe no Strato (criado em `b8bc2107`..`4c0ed5ba`, pós-merge-base).
- **O que exigiria**: portar antes a reestruturação completa do memory manager do Strato (fora do escopo
  deste lote). O bug específico (iteração no início do mapa) não existe no Skyline.
- **Dependências**: depende da série de reestruturação de memória do Strato (b8bc2107+).

## 2. 80b3b223 — KProcess: correctly handle empty optional chunk

- **Área**: kernel/types/KProcess · **Veredito**: DIVERGED · **Esforço**: n/a · **Risco**: — · **Prioridade**: P3 (idem seção 1)
- **Patch**: em `InitializeTlsSlots`/alocação de stack, `GetChunk(pageCandidate)` pode retornar optional
  vazio; o código antigo fazia `.value()` (throw) e mantinha `chunk` fora do loop. Fix: checa `if (!chunk) break;`
  e usa `chunk->` local (2 blocos), removendo `[[unlikely]]`.
- **Evidência de divergência**: Skyline `KProcess.cpp:57-69` (`AllocateTlsSlot`) usa lista `tlsPages` e
  **nunca itera chunks**; main thread stack é criado direto em `CreateThread` via `KPrivateMemory`
  (`KProcess.cpp:71-76`). `rg "memory.Get\(" KProcess.cpp` = vazio. O bug (`.value()` em optional vazio)
  não existe no Skyline; é consequência do `GetChunk` do Strato.
- **Dependências**: depende de `b8bc2107`+ (mesma série da seção 1).

## 3. 41928737 — Fix TLS writes from X2/3 (nce)

- **Área**: nce · **Veredito**: PORTABLE · **Esforço**: S · **Risco**: baixo · **Prioridade**: P1
- **Patch**: 1 linha — no path de MSR writes para `TPIDR_EL0`, quando os scratch são X2/X3 (em vez de
  X0/X1), o `STR` final usava base X0 com valor em X3:
  `0xF9015C03` → `0xF9015C43` (`STR X3, [X2, #0x4B8]`).
- **Evidência**: Skyline `app/src/main/cpp/skyline/nce.cpp:505-507` contém o código pré-fix idêntico
  (linha 507: `*patch++ = x0x1 ? 0xF9015C01 : 0xF9015C03; // STR X(1/3), [X0, #0x4B8] (ThreadContext::tpidrEl0)`).
- **Aplicação**: trocar a constante na linha 507 por `0xF9015C43` e ajustar o comentário. Offset 0x4B8
  de ThreadContext é o mesmo nos dois (mesma struct).
- **Dependências**: mesma série/arquivo de 5bcc79ef (hunks disjuntos; independente, mas portar juntos).

## 4. 5bcc79ef — Ensure SVC trampoline is always correctly sized (nce)

- **Área**: nce · **Veredito**: PORTABLE · **Esforço**: S · **Risco**: médio (layout de código gerado) · **Prioridade**: P1
- **Patch**: `TrampolineSize` 17→18 e `RescaleClockSize` 17→19; nos loops `instructions::MoveRegister(...)`
  passa a emitir `NOP (0xD503201F)` quando o elemento é 0 ("não requerido"), garantindo que o nº de
  instruções emitidas == constante declarada.
- **Evidência**: Skyline `nce.cpp:259` `constexpr size_t TrampolineSize{17};`, `:300` `RescaleClockSize{17}`,
  e os dois loops sem padding em `:283-285` e `:319-321`. `instructions.h:251-266`: `MoveRegister` retorna
  `std::array<u32, N>` com `0` para instrução não requerida — mesmo comportamento pré-fix do Strato.
- **Aplicação**: replicar hunk a hunk (4 blocos em `nce.cpp`).
- **Dependências**: ver seção 3 (mesmo arquivo; a base do patch de 41928737 é o resultado deste).

## 5. e0c487f6 — Fix system register state handling (nce)

- **Área**: nce/guest context · **Veredito**: PORTABLE · **Esforço**: M · **Risco**: médio (asm + struct) · **Prioridade**: P1
- **Patch**: salva/restaura `NZCV` em SaveCtx/LoadCtx (`guest.S`), corrige o comentário duplicado
  "Store FPCR/FPSR" no LoadCtx, adiciona campo `u32 nzcv` ao `ThreadContext` (offset 0x2C0),
  atualiza `SaveCtxSize` 34→38 / `LoadCtxSize` 34→36, e zera NZCV no init asm de `KThread.cpp`.
- **Evidência**: Skyline `nce/guest.S:41-45` e `:75-79` só salvam FPSR/FPCR (sem NZCV); `nce/guest.h:94-103`
  `struct ThreadContext` sem `nzcv`; `guest.h:106-107` `SaveCtxSize{34}/LoadCtxSize{34}`;
  `kernel/types/KThread.cpp:143` asm init tem `MSR FPCR, XZR` sem `MSR NZCV, XZR` seguinte.
- **Aplicação**: 3 arquivos (`guest.S`, `guest.h`, `KThread.cpp`). Verificar que nada mais serializa
  `ThreadContext` por offset rígido no Skyline (o asm usa 0x2C0 absoluto, coerente com o campo novo
  entre `tpidrEl0` 0x2B8 e `state` 0x2C8).
- **Impacto**: sem isso, flags (NZCV) do guest são corrompidas através de SVC/sinais — correção de
  correção de execução relevante.
- **Dependências**: nenhuma; independente de 3/4 (hunks disjuntos), mas mesma família NCE.

## 6. 7d0b7f0b — Handle OOB blits by adding to the texture base offset (gpu)

- **Área**: gpu/fermi_2d + helper shaders · **Veredito**: PORTABLE · **Esforço**: M · **Risco**: médio (muda layout de push constants do shader) · **Prioridade**: P2
- **Patch**: `GetGuestTexture` passa a retornar `std::pair<GuestTexture, bool>` e recebe
  `oobReadStart/oobReadWidth`; para layout Pitch, se a leitura sai dos limites (comportamento OpenGL de
  wrap para a próxima linha), adiciona `oobReadStart * bpb` ao IOVA base e zera `centredSrcRectX`.
  Remove o workaround `srcHeightRecip` do `blit.frag` e do `FragmentPushConstantLayout`.
- **Evidência**: Skyline pré-fix equivalente: `gpu/interconnect/fermi_2d.cpp:16`
  (`gpu::GuestTexture Fermi2D::GetGuestTexture(const Surface &surface)`), `:118-119` (duas chamadas sem
  args OOB), `:131-132` (centredSrcRect após attach, como no pré-patch), `fermi_2d.h:36`;
  `gpu/shaders/helper_shaders.cpp:178,257` e `app/src/main/shaders/blit.frag:11,18` ainda têm `srcHeightRecip`.
- **Aplicação**: 4 arquivos; contexto fecha bem. Cuidado: mudança de push constant exige rebuild do
  shader e confere hashing de pipeline (layout muda).
- **Dependências**: autocontido.

## 7. 6a57fd16 — Don't bail out when invalid desc types are encountered (gpu)

- **Área**: gpu/maxwell_3d pipeline_manager · **Veredito**: PORTABLE · **Esforço**: S · **Risco**: médio (downstream precisa tolerar 0 writes) · **Prioridade**: P2
- **Patch**: remove de `Pipeline::SyncDescriptors` o early-return
  `if (!writeIdx) return nullptr;` ("Since we don't implement all descriptor types...") — a pipeline
  passa a emitir `DescriptorUpdateInfo` mesmo sem writes.
- **Evidência**: Skyline tem as duas ocorrências (o Strato também tinha duas e removeu **só a primeira**):
  `pipeline_manager.cpp:903-905` dentro de `SyncDescriptors` (função começa em :746) — remover esta;
  `:1028-1030` dentro de `SyncDescriptorsQuickBind` (:918) — **manter**, igual ao Strato pós-patch.
  Confirmado com `git show 6a57fd16^:<path> | rg` (ocorrências em 839 e 933; patch remove 839).
- **Dependências**: autocontido.

## 8. 34611ba1 — Optimise waiter queue push (gpu)

- **Área**: gpu/command_executor · **Veredito**: PORTABLE · **Esforço**: S · **Risco**: baixo · **Prioridade**: P3
- **Patch**: em `ExecutionWaiterThread::Queue`, encurta o escopo do lock: push dentro de um bloco
  `{ std::unique_lock lock{mutex}; ... }` e `condition.notify_all()` fora do lock.
- **Evidência**: Skyline `gpu/interconnect/command_executor.cpp:291-295` idêntico ao pré-patch
  (lock cobrindo o notify).
- **Dependências**: autocontido.

## 9. 644c8f3c — Construct queue consumer threads after queues (gpu)

- **Área**: gpu/command_scheduler + presentation_engine · **Veredito**: PORTABLE · **Esforço**: S · **Risco**: baixo · **Prioridade**: P2
- **Patch**: reordena membros para que `std::thread waiterThread` / `choreographerThread` /
  `presentationThread` sejam declarados **depois** das `CircularQueue`s que consomem (evita corrida de
  inicialização: a thread pode arrancar antes da fila estar construída).
- **Evidência**: Skyline `gpu/command_scheduler.h:46-48` (`waiterThread` ANTES de `cycleQueue`) e
  `gpu/presentation_engine.h:59-60` (`choreographerThread` antes de `choreographerLooper`) e `:78-80`
  (`presentationThread` antes de `presentQueue`) — bug presente.
- **Dependências**: autocontido.

## 10. 0b0e48c6 — Fix invalid switch-case syntax resulting from `nvdrv` macro expansion (#242)

- **Área**: services/nvdrv · **Veredito**: PORTABLE · **Esforço**: M (mecânico, 7 arquivos) · **Risco**: baixo · **Prioridade**: P2 (P1 se o port subir o NDK — ver nota)
- **Patch**: em `macro_def.inc`, remove o `;` solto após `cases` (3 macros); nos devices, remove os
  wrappers de statement-expression `({...})` dos argumentos dos macros `IOCTL_HANDLER_FUNC` /
  `VARIABLE_IOCTL_HANDLER_FUNC` / `INLINE_IOCTL_HANDLER_FUNC` e adiciona `// @fmt:off/on`.
- **Evidência**: Skyline pré-fix idêntico: `services/nvdrv/devices/deserialisation/macro_def.inc:10,20,35`
  (`cases;`), `nvhost/as_gpu.cpp:360,380` e `ctrl.cpp` com `({...})`.
- **Nota**: Skyline usa NDK 25.0 (`app/build.gradle:136`) e compila com a sintaxe atual; o Strato
  corrigiu ao subir para NDK 26.1. Se o projeto de port planeja bump de NDK/clang, priorizar.
- **Dependências**: autocontido.

## 11. 13232591 — Iterate through siblings without recursion in `vfs::TraverseDirectory` (#205)

- **Área**: vfs/rom_filesystem · **Veredito**: PORTABLE · **Esforço**: S · **Risco**: baixo · **Prioridade**: P2
- **Patch**: reescreve `TraverseDirectory` como loop `do { ... offset = entry.siblingOffset; } while
  (sibling != empty)` em vez de recursão para os irmãos (recursão permanece só para filhos), evitando
  estouro de stack em RomFS com muitos diretórios irmãos.
- **Evidência**: Skyline `vfs/rom_filesystem.cpp:30-49` idêntico ao pré-patch (chamada recursiva de
  irmãos na linha ~48: `if (entry.siblingOffset != constant::RomFsEmptyEntry) TraverseDirectory(entry.siblingOffset, path);`).
- **Dependências**: autocontido.

## 12. 32c1519b — Fix file creation in OsFileSystem

- **Área**: vfs/os_filesystem · **Veredito**: PORTABLE · **Esforço**: S · **Risco**: baixo · **Prioridade**: P1
- **Patch**: `CreateFile` só chama `CreateDirectory(path.substr(0, path.find_last_of('/')), true)` se
  `find_last_of('/') != npos` (caminhos sem barra, ex. "file.txt", hoje geram `substr(0, npos)` = string
  inteira → cria um **diretório** com o nome do arquivo e o `open()` falha).
- **Evidência**: Skyline `vfs/os_filesystem.cpp:21-23` idêntico ao pré-patch (linha 22 sem guarda npos).
- **Dependências**: autocontido.

## 13. d95bb121 — Implement recursive delete `IFileSystem` service calls

- **Área**: services/fssrv · **Veredito**: PORTABLE (com adaptação) · **Esforço**: M · **Risco**: médio (operação destrutiva no FS do host) · **Prioridade**: P2
- **Patch**: adiciona `DeleteDirectoryRecursively` (cmd 0x4), `GetTotalSpaceSize` (0xC, stub 90000000) e
  `CleanDirectoryRecursively` (0xD; remove_all + recria) em `IFileSystem.cpp/.h` + entradas `SFUNC`.
- **Evidência**: Skyline `services/fssrv/IFileSystem.h:71-79` tabela termina em 0xB (nem tem 0xE
  GetFileTimeStampRaw que a tabela do Strato tinha — ajustar vírgulas); `IFileSystem.cpp:63-67` usa
  `backing->DeleteDirectory(path)`.
- **Adaptação necessária**: o patch do Strato chama `std::filesystem::remove_all(path)` **direto no
  caminho do guest, sem prefixar basePath** — no Skyline, `OsFileSystem::DeleteDirectoryImpl`
  (`vfs/os_filesystem.cpp:46-48`) prefixa `basePath`. Portar roteando via `backing` (novo método
  `DeleteDirectoryRecursivelyImpl`) para não operar sobre caminhos relativos do host. Mesmo cuidado no
  `CleanDirectoryRecursively` (`backing->CreateDirectory(path, true)`).
- **Dependências**: autocontido (serviço).

## 14. 920a3b96 — Implement `nfp::IUser::GetApplicationAreaSize`

- **Área**: services/nfp · **Veredito**: PORTABLE · **Esforço**: S · **Risco**: baixo · **Prioridade**: P3
- **Patch**: handler que retorna `u32 0xD8` (216 bytes) + `SFUNC(0x16, ...)` na tabela.
- **Evidência**: Skyline `services/nfp/IUser.h:44-47` tabela pré-patch idêntica (0x0, 0x2, 0x13, 0x17).
- **Dependências**: autocontido.

## 15. bdf73aa2 — Fix GetSharedFontInOrderOfPriority

- **Área**: services/pl · **Veredito**: PORTABLE · **Esforço**: S · **Risco**: baixo · **Prioridade**: P2
- **Patch**: em vez de `response.Push<u8>(1)` fixo ("fonts loaded"), retorna
  `static_cast<u32>(core.fonts.size())` como `u32`.
- **Evidência**: Skyline `services/pl/IPlatformServiceManager.cpp:62` com o bug; `core.fonts.size()` já
  usado em `:47` — sem dependência nova.
- **Dependências**: autocontido.

## 16. f9f9b6de — Loader: skip patching non 64-bit executables

- **Área**: loader · **Veredito**: PORTABLE (com adaptação) · **Esforço**: M · **Risco**: médio · **Prioridade**: P3
- **Patch**: em `LoadExecutable`, deriva `is64bit` de `process->npdm.meta.flags.is64Bit`; só chama
  `GetPatchData`, reserva/mapeia patch+hook e roda `PatchCode`/`WriteHookSection` quando 64-bit.
- **Evidência**: Skyline `loader/loader.cpp:14-130` **divergiu estruturalmente** do pai do Strato:
  mapeamentos via `process->NewHandle<KPrivateMemory>` (`:92-99`, `patchType` Heap/Reserved em `:92`),
  varredura de símbolos inline (`:35-85`) em vez de `hle::GetExecutableSymbols`, chamadas alvo em
  `:30` (`GetPatchData`) e `:125` (`PatchCode`) — mesmo alvo conceitual, hunks diferentes.
- **Aplicação**: reimplementar o gating `is64Bit` nos pontos :30, :91-99 e :125 do Skyline, decidindo o
  que fazer quando `patch.size == 0` (hoje sempre mapeia região patch+hook).
- **Dependências**: no Strato aplica-se sobre bdb4e3fd (base blob `ba0da184` = resultado de bdb4e3fd);
  no Skyline pode ser portado antes/depois, mas convém junto com a seção 17.

## 17. bdb4e3fd — Loader: make dynsym handling more generic

- **Área**: loader · **Veredito**: PORTABLE (com adaptação) · **Esforço**: M · **Risco**: baixo · **Prioridade**: P3
- **Patch**: `ExecutableSymbolicInfo::symbols/symbolStrings` viram `span<u8>`/`span<char>` (raw), adiciona
  `concept ElfSymbol` (Elf32_Sym|Elf64_Sym), `ResolveSymbol` vira template e `ResolveSymbol64` mantém o
  uso atual; stack trace passa a chamar `ResolveSymbol64`.
- **Evidência**: Skyline ainda no pré-patch: `loader/loader.cpp:31` (`span<Elf64_Sym>`),
  `:116` (`.symbols = {dynsym.begin(), dynsym.end()}`), `:137-153` (`ResolveSymbol` com
  `find_if(... const Elf64_Sym &sym ...)`) e `loader.h:63-64` (`std::vector<Elf64_Sym> symbols;
  std::vector<char> symbolStrings;`), `loader.h:113` declaração não-template.
- **Aplicação**: conceito C++20 + spans — direto; conferir todos os consumidores de
  `ExecutableSymbolicInfo::symbols` no Skyline (ex. uso em debugger/logging).
- **Dependências**: pré-requisito natural da seção 16 (32-bit precisa de Elf32_Sym).

## 18. 135b7f31 — Implement info reading from Ro section (#173)

- **Área**: loader/nso · **Veredito**: PORTABLE (com adaptação) · **Esforço**: M · **Risco**: baixo · **Prioridade**: P3
- **Patch**: `NsoLoader::PrintRoContentsInfo` extrai e loga Module Path (`x:\...\*.nss`), SDK Version
  (`sdk_version: ...`) e SDK MW libraries via **boost::regex**; linka `Boost::regex` no CMake.
- **Evidência**: Skyline `loader/nso.cpp:56,59` (âncoras do patch existem; função ausente);
  CMake do Skyline usa boost **vendored minimal** com nomes próprios — `app/CMakeLists.txt:45`
  (`add_subdirectory("libraries/boost")`) e `target_link_libraries_system(skyline ... nintrusive
  ncontainer range-v3 ...)`; **não há `Boost::regex`** disponível.
- **Aplicação**: reimplementar com `std::regex` (ou parser manual) e `Logger::Info` no estilo do Skyline
  (o patch usa macro `LOGI` do Strato); pular a mudança de CMake.
- **Dependências**: autocontido (apenas diagnóstico de logs).

---

## Síntese de dependências

- **Série memória (1, 2)**: ambos dependem da reestruturação do memory manager do Strato (`b8bc2107`+),
  ausente no Skyline → DIVERGED, sem ação neste lote.
- **Série NCE (3, 4, 5)**: independentes entre si em termos de hunk, mas 3 e 4 tocam o mesmo arquivo
  (`nce.cpp`; base do 4 = resultado do 5... na ordem do Strato: 5bcc79ef → 41928737). Portar juntos.
- **Série loader (16, 17)**: 16 constrói sobre 17 no Strato; 18 é independente (só diagnóstico).
- Demais (6-15) autocontidos.

## Priorização sugerida

- **P1**: 41928737 (TLS X2/3), 5bcc79ef (trampoline size), e0c487f6 (NZCV), 32c1519b (criação de arquivo) — correção de comportamento.
- **P2**: 644c8f3c (init-order threads), 6a57fd16 (desc types), 7d0b7f0b (OOB blits), 13232591 (recursão RomFS), d95bb121 (recursive delete), bdf73aa2 (fonts), 0b0e48c6 (nvdrv macros — P1 se bump de NDK).
- **P3**: 34611ba1 (lock scope), 920a3b96 (nfp), f9f9b6de/bdb4e3fd (loader 32-bit/diag), 135b7f31 (Ro info), d693cac5/80b3b223 (DIVERGED, reavaliar).
