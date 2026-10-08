# 04 — Plano de Port (Classificação A4 + Estratégia A5)

Metodologia: candidatos extraídos de `git log 6aef7fdd..strato/master -- app/src/main/cpp` (97 commits de núcleo desde o fork), analisados um a um nos docs 03a/03b com evidência `file:line`. Vereditos: ALREADY_PRESENT / PORTABLE / DIVERGED / SKIP / INCERTO.

## Resumo da classificação

- **PORTÁVEIS**: 15 bugfixes de núcleo + 21 grupos de serviços (lotes 1–3 abaixo).
- **DIVERGED (não aplicar)**: `d693cac5` e `80b3b223` — corrigem bugs do gerenciador de memória do Strato (lista de chunks); o Skyline usa vector+`InsertChunk` (memory.cpp:238) e **os bugs não existem lá**.
- **REJEITADOS/INAPLICÁVEIS**: renome de pacote `421c2f46` (branding), bump NDK `45aa3945` (risco de build, sem ganho de gameplay), diálogos UI (`95de8f9d`, `17ebf18f`, `b0cee4a2`), refatoração completa do logger (série 2023-09, ~16 commits cross-cutting — custo alto, ganho de gameplay baixo), tela de pipelines (`8255308d`+`e41ff6b1` — dependente de UI), setting de internet `32f61e77` (UI; portaremos os comandos de rede sem o gate, com nota).
- **FUTURO (grandes reworks, risco alto — documentados, não aplicados nesta sessão)**: rework do memory manager (`b8bc2107`, `1e9a0f56`, `44f21419`, `4c0ed5ba`, `9d1f1aaa`, `8addba32`, `b51c31dc`), trap_manager fora do NCE (`83111c2a`+`ae1566a4`), SvcContext genérico (`18626a42`+`686a2512`), signal handling (`74173a39`).

## Lote 1 — Bugfixes de núcleo (esta sessão)

| ID | Descrição | Arquivos (Skyline) | Tipo | Prio | Risco | Dep. | Origem (strato) | Estratégia |
|----|-----------|--------------------|------|------|-------|------|-----------------|-----------|
| L1-1 | Dimensionar trampolim de SVC corretamente | nce/nce.cpp | BUGFIX | P1 | médio | — | `5bcc79ef` | cherry-pick adaptado |
| L1-2 | Corrigir escritas TLS de X2/X3 | nce/nce.cpp | BUGFIX | P1 | baixo | L1-1 | `41928737` | cherry-pick adaptado |
| L1-3 | Corrigir estado de registradores de sistema (NZCV) | nce/nce.cpp | BUGFIX | P1 | médio | — | `e0c487f6` | patch manual adaptado |
| L1-4 | Corrigir criação de arquivo em OsFileSystem | services/fssrv | BUGFIX | P1 | baixo | — | `32c1519b` | patch manual |
| L1-5 | Blits OOB → offset da textura base | gpu/texture* | BUGFIX | P2 | médio | — | `7d0b7f0b` | patch manual adaptado |
| L1-6 | Não abortar em descritores inválidos | gpu/soc | COMPATIBILIDADE | P2 | médio | — | `6a57fd16` | patch manual |
| L1-7 | Criar consumer threads após as queues | services/hosbinder | BUGFIX | P2 | baixo | — | `644c8f3c` | patch manual |
| L1-8 | TraverseDirectory sem recursão | vfs | BUGFIX | P2 | baixo | — | `13232591` | cherry-pick adaptado |
| L1-9 | Corrigir GetSharedFontInOrderOfPriority | services/fssrv | BUGFIX | P2 | baixo | — | `bdf73aa2` | patch manual |
| L1-10 | GetApplicationAreaSize (nfp::IUser) | services/nfp? | NOVA_FEATURE | P3 | baixo | — | `920a3b96` | patch manual |
| L1-11 | Otimizar push da waiter queue | kernel | PERFORMANCE | P3 | baixo | — | `34611ba1` | patch manual |
| L1-12 | Permissões de arquivo 0666 + constantes | services/fssrv | BUGFIX | P3 | baixo | — | `f85ecc23`+`a6c26993` | patch manual |

## Lote 2 — Serviços/compatibilidade (stubs que faltam; esta sessão)

| ID | Descrição | Área | Tipo | Prio | Risco | Origem |
|----|-----------|------|------|------|-------|--------|
| L2-1 | Stubs hid: 0x12C, 0x132, 0x136, 0xD3 | hid | COMPATIBILIDADE | P1 | baixo | `35b90c96`+`13f88f75`+`e05de1ac` |
| L2-2 | ICommonStateGetter 0x33–0x36 | am | COMPATIBILIDADE | P1 | baixo | `ffb93d21`+`937eacef` |
| L2-3 | ISelfController 0x41/0x43/0x44/0x45/0x47 | am | COMPATIBILIDADE | P1 | baixo | `c3eef507`+`80911207`+`1bfdcaa3` |
| L2-4 | IApplicationFunctions GetSaveDataSizeMax 0x1C | am | COMPATIBILIDADE | P2 | baixo | `eb2d04ea` |
| L2-5 | ILibraryAppletAccessor 0xA0 | am | COMPATIBILIDADE | P2 | baixo | `395e08d0` |
| L2-6 | capsrv: SetShimLibraryVersion + GetAlbumFileList* | capsrv | COMPATIBILIDADE | P2 | baixo | `4f6d5b28`+`c43912c1`+`f0430e46` |
| L2-7 | irs: CheckFirmwareVersion 0x13A + StopImageProcessorAsync 0x13E | irs | COMPATIBILIDADE | P2 | baixo | `d241e846`+`9465bfeb` |
| L2-8 | Serviço lbl (9 cmds 0x11–0x1C) | lbl | COMPATIBILIDADE | P2 | baixo | `d32aa861` |
| L2-9 | pctl: EndFreeCommunication + StereoVision | pctl | COMPATIBILIDADE | P2 | baixo | `a523af21`+`41882aab` |
| L2-10 | account: 0x96/0x2/0x3 + IAuthorizationRequest/IAsyncContext | account | COMPATIBILIDADE | P2 | baixo | `6ac7d22e`+`41882aab` |
| L2-11 | bcat: RequestSyncDeliveryCache + progress service | bcat | COMPATIBILIDADE | P2 | baixo | `7026470b`+`aa0a45eb` |
| L2-12 | Serviços clkrst/psm/ts | soc/services | COMPATIBILIDADE | P2 | baixo | `e4f9fd62`+`3ca24b9f` |
| L2-13 | friends/fssrv/settings: stubs restantes | services | COMPATIBILIDADE | P2 | baixo | `e4f9fd62` |
| L2-14 | Serviço ntc: StartTask | ntc | COMPATIBILIDADE | P3 | baixo | `8113413a` |

## Lote 3 — Rede (se houver tempo de CI; senão fica documentado)

| ID | Descrição | Área | Tipo | Prio | Risco | Origem |
|----|-----------|------|------|------|-------|--------|
| L3-1 | GetAddrInfoRequest (sfdnsres) | sfdnsres | COMPATIBILIDADE | P2 | médio | `43195927` |
| L3-2 | bsd: Socket/GetPeerName/GetSockName/GetSockOpt/Fcntl/EventFd | bsd | COMPATIBILIDADE | P2 | médio | `20a9ab65` |
| L3-3 | nifm: GetCurrentIpConfigInfo + IRequest.Cancel (SEM gate de setting) | nifm | COMPATIBILIDADE | P2 | médio | `a299bb3c`+`68bb8256` |

## Estratégia de execução (A5)

1. Lote 1 primeiro (bugfixes críticos) → revisão A7–A10 → push + `workflow_dispatch` do `build.yml`.
2. Durante o run de CI: implementar Lote 2 (commits locais) → revisão → push #2 + CI.
3. Lote 3 só com janela de CI disponível. Máx. 3 ciclos de correção por item; depois disso `git revert` e marcar "bloqueado".
4. Todo commit: `port(strato): <área>: <resumo>` + corpo citando hash(es) de origem e nota de adaptação. Docs via `docs(port): ...` separados.
5. Novos .cpp (lbl, ntc, clkrst, psm, ts, etc.) devem ser adicionados ao `app/CMakeLists.txt` (papel do Crítico A9 conferir).
