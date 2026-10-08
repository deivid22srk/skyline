# 05 — Relatório Final do Port Strato → Skyline (feat/sifu-deepseek)

Data: 2026-10-08 · Executor: orquestração multi-agente (A1–A11) · Origem: strato-emu/strato@master (merge-base com Skyline: `6aef7fdd` — Strato é fork do Skyline)

## 1. Resumo executivo

Foram identificados 97 commits de núcleo do Strato desde o ancestral comum e analisados um a um com evidência verificável. **27 commits foram criados na branch** (26 de código + 1 de documentação), portando bugfixes críticos de NCE (trampolim SVC, TLS, NZCV/FPCR), correções de GPU (blits OOB, descritores, data races de threads), correções de VFS/FsServ e 23 comandos/serviços HOS novos (hid, am, capsrv, irs, lbl, pctl, bcat, friends, fssrv, settings, nifm, ntc, nfp, pl, bsd:s). Todos os 27 commits passaram por revisão crítica (código/regressão/build/origem) com ressalvas endereçadas em commit dedicado. CI (build.yml via GitHub Actions, NDK 25): **lote 1 = SUCCESS** (run 37775436450); lote 2 = run disparado (ver seção CI). Nada da branch original foi perdido (0 arquivos apagados; diff completo vs backup contém apenas port + docs).

## 2. Tabela de itens

### 2.1 Portados (lote 1 — bugfixes de núcleo)

| ID | Origem (strato) | Descrição | Commit (skyline) |
|----|-----------------|-----------|------------------|
| L1-1 | `5bcc79ef` | Trampolim SVC/RescaleClock dimensionados corretamente (NOP padding, 18/19) | `f14fe168` |
| L1-2 | `41928737` | Registrador base correto na escrita de TLS via patch de MSR (X2/X3) | `6fa434b6` |
| L1-3 | `e0c487f6` | NZCV preservado + FPCR carregado (não gravado) em LoadCtx; ThreadContext::nzcv | `2277c88d` |
| L1-4 | `32c1519b` | OsFileSystem: não criar diretório raiz quando path sem '/' | `31583d0a` |
| L1-12 | `f85ecc23`+`a6c26993` | Permissão 0666 (constantes S_I*) na criação de arquivos | `ef4b6353` |
| L1-5 | `7d0b7f0b` | Fermi 2D: blits OOB via offset da textura base (técnica do Ryu), shader simplificado | `2af99f2c` |
| L1-6+L1-7 | `6a57fd16`+`644c8f3c` | Não abortar em descritores inválidos; consumer threads após as queues | `4fae7378` |
| L1-8 | `13232591` | vfs::TraverseDirectory iterativo nos irmãos (stack overflow) | `48c800a5` |
| L1-11 | `34611ba1` | Waiter queue: notify_all fora do lock | `0708a158` |
| L1-9 | `bdf73aa2` | pl: GetSharedFontInOrderOfPriority com shape HOS correto | `fbb53e4c` (ajustado em `a90d7d78`) |
| L1-10 | `920a3b96` | nfp: GetApplicationAreaSize (0x16, 216 bytes) | `e85505ec` (limpo em `a90d7d78`) |

### 2.2 Portados (lote 2 — serviços/compatibilidade)

| Grupo | Origem | Comandos | Commit |
|-------|--------|----------|--------|
| hid | `35b90c96`+`13f88f75`+`e05de1ac` | 0xD3, 0x12C, 0x132, 0x136 | `1a4d8596` |
| am/ICommonStateGetter | `ffb93d21`+`937eacef` | 0x33–0x36 | `6025065f` |
| am/ISelfController | `c3eef507`+`80911207`+`1bfdcaa3` | 0x41/0x43/0x44/0x45/0x47 | `4ce77633` |
| am/IApplicationFunctions | `eb2d04ea` | 0x1C GetSaveDataSizeMax | `1b2038e8` |
| am/ILibraryAppletAccessor | `395e08d0` | 0xA0 GetIndirectLayerConsumerHandle | `9d3e4ba8` |
| capsrv | `4f6d5b28`+`c43912c1`+`f0430e46` | caps:u/su 0x20, caps:c 0x21, 0x66, 0x8E | `a2cbecd1` |
| irs | `d241e846`+`9465bfeb` | 0x13A, 0x13E | `d6718a3a` |
| lbl (serviço novo) | `d32aa861` | 0x11–0x1C | `f6f69441` |
| pctl | `a523af21`+`41882aab` | 0x3F5, 0x425–0x429, 0x3F9 | `c7304671` |
| bcat | `7026470b`+`aa0a45eb` | 0x2774 + IDeliveryCacheProgressService 0x0/0x1 | `45ff44ae` |
| friends/fssrv/settings | `e4f9fd62` | 0x0, 0x2788, 0xE, 0x3C/3D/3E (+ISaveDataInfoReader), 0x17 | `6ef58a6b` |
| ntc (serviço novo) | `8113413a` | StartTask 0x0 (+registro bsd:s) | `62471954` |
| nifm | `e4f9fd62` | 0x12 GetInternetConnectionStatus | `f8853a79` |

### 2.3 Correções de revisão / limpeza

| Commit | Motivo |
|--------|--------|
| `a90d7d78` | Ressalvas A7/A8 do lote 1: guard restaurado no SyncDescriptorsQuickBind (Strato mantém), pl com shape HOS exato (u32 1 + u32 count), typo de comentário 0x4B8→0x2B8, docs do nfp/IUser.h |
| `d81a481c` | Include não usado no ntc (A7/A9) |

### 2.4 Adaptados (desvios documentados da origem)

- **pl** (`fbb53e4c` + `a90d7d78`): forma final `u32(1)+u32(count)` — o packing de `IpcResponse::Push` do Skyline é sem alinhamento; a forma do Strato punha bool=count. Justificado pelo A7.
- **pipeline_manager** (`4fae7378` + `a90d7d78`): guard removido apenas no caminho de pipeline completo (como `6a57fd16`); o do SyncDescriptorsQuickBind foi restaurado (Strato master o mantém).
- **ntc** (`62471954`): sem o setting `isInternetEnabled` (não existe no Skyline) — StartTask sinaliza o evento e retorna sucesso.
- **logger**: Strato usa macros LOGx próprias; portado para `Logger::Debug/Warn` do Skyline.
- **"Backligh"** e outros typos históricos preservados para fidelidade.

### 2.5 Descartados / bloqueados (com motivo)

| Item | Motivo |
|------|--------|
| `d693cac5`, `80b3b223` (memória) | DIVERGED: corrigem bugs do gerenciador de memória do Strato; o Skyline usa vector+InsertChunk e os bugs não existem lá |
| Série memory manager (`b8bc2107`, `1e9a0f56`, `44f21419`, `4c0ed5ba`, `9d1f1aaa`, `8addba32`, `b51c31dc`) | Rework grande e arriscado; memória do Skyline divergiu; recomendado estudo dedicado |
| trap_manager (`83111c2a`+`ae1566a4`), SvcContext (`18626a42`+`686a2512`), signal handling (`74173a39`) | Reworks estruturais (M/L) — fora da janela segura desta sessão; documentados como próximos passos |
| Refatoração do logger (série 2023-09, ~16 commits) | Cross-cutting, custo alto, ganho de gameplay baixo |
| Bump NDK `45aa3945`, pacote `421c2f46`, diálogos UI, tela de pipelines, setting de internet | UI/branding/build-cosmético fora do escopo do núcleo |
| ldn (`460e1448`), bsd completo (`20a9ab65`), sfdnsres (`43195927`), nifm completo (`a299bb3c`+`68bb8256`), account IAuthorizationRequest (`6ac7d22e`), clkrst/psm/ts (`e4f9fd62`+`3ca24b9f`), system archives (`0eee5b81`), fonts (`54d4586e`) | Não portados nesta janela (esforço M, exige classes/setting novos); listados como próximos passos |

## 3. Decisões dos críticos

- **Lote 1** — A7+A8: APROVADO COM RESSALVAS (nenhum veto); ressalvas endereçadas em `a90d7d78`. A9: build OK (sem arquivos novos, includes intactos, blit.frag recompila no AGP). A10: 11/11 origens confirmadas (blobs comparados), MPL-2.0 preservado nos dois lados.
- **Lote 2** — A7+A8: APROVADO (12/12; pendências não-bloqueantes → nifm 0x12 portado em `f8853a79`, include limpo em `d81a481c`). A9: todos os includes resolvem, ctors compatíveis com SERVICE_CASE, CMake sintaxe OK. A10: 22 hashes de origem resolvidos, zero hunks órfãos, headers SPDX nos 8 arquivos novos.
- **Vetos**: nenhum. Não houve necessidade de 2ª rodada do protocolo de debate.

## 4. Resultado do CI (GitHub Actions — build.yml, NDK 25.0.8775105)

| Run | Head | Lote | Status |
|-----|------|------|--------|
| [37362682453](https://github.com/deivid22srk/skyline/actions/runs/37362682453) | `9c39760f` | baseline (pré-port) | SUCCESS |
| [37775436450](https://github.com/deivid22srk/skyline/actions/runs/37775436450) | `a90d7d78` | lote 1 (bugfixes núcleo) | **SUCCESS** |
| [37779136649](https://github.com/deivid22srk/skyline/actions/runs/37779136649) | `d81a481c` | lote 2 (serviços — todo o código portado) | **SUCCESS** |
| [37781488659](https://github.com/deivid22srk/skyline/actions/runs/37781488659) | `200b2ea8` | tip da branch (docs-only) | disparado (docs não afetam o build; resultado confirmatório) |

Nada foi compilado localmente (regra da operação); validação exclusivamente pelo CI.

## 5. Verificação de regressão vs estado original

`git diff backup/feat-sifu-deepseek-pre-port..HEAD`: 64 arquivos, +1832/−79, **0 arquivos apagados**. Branch de backup disponível localmente e no remoto. Os fixes de NCE/GPU alteram caminhos quentes, mas todos são estabelecidos no Strato (em produção desde 2023-04) e revisados por código; risco de regressão comportamental residual é baixo, porém não zero (ver §6).

## 6. Riscos residuais e o que NÃO foi testado

- Nenhum teste funcional de jogo foi executado (CI compila; não roda jogos). O comportamento em Sifu/outros jogos precisa de validação manual (§7).
- `std::isfinite` no lbl sem `<cmath>` explícito (paridade com Strato) — coberto pelo CI do lote 2; se falhar, incluir header é correção trivial.
- SaveContext do trampolim (+1 instrução) tem custo marginal por SVC — imperceptível na prática (mesmo custo do Strato).
- Stubs retornam sucesso genérico — jogos que exigem dados reais (ex.: capsrv GetAlbumFileList com buffers) podem apresentar listas vazias (comportamento idêntico ao Strato).
- Os itens "descartados" (§2.5) permanecem como gaps de compatibilidade.

## 7. Testes manuais recomendados

1. **Sifu** (foco da branch): carregar save, verificar estabilidade em cenas com muitos blits 2D (fermi) e sem artefatos novos de textura.
2. **Jogos que usam fontes do sistema** (ex.: qualquer UE4/Japanese title): conferir textos (pl/shared fonts corrigido).
3. **Jogos que consultam vibration/VR/illuminance** (ex.: títulos que travav em "UnsupportedCommand"): verificar progresso além do menu.
4. **Amiibo/NFC** (ex.: Zelda BotW/TotK, Splatoon): menu de amiibo não deve mais falhar no comando 0x16.
5. **Áudio/vídeo em geral**: conferir que a troca de contexto NCE (NZCV/FPCR fix) não alterou timing/perf (FPS meter antes/depois).
6. **Stress de save/persistência**: criar/excluir saves rapidamente (OsFileSystem + 0666).

## 8. Próximos passos sugeridos

1. Conferir o run do lote 2; se verde, taggear `sifu-port-strato-1`.
2. Portar o bloco de rede (bsd `20a9ab65`, sfdnsres `43195927`, nifm completo `a299bb3c`+`68bb8256`) — valor alto para jogos online-capable.
3. Portar clkrst/psm/ts e account IAuthorizationRequest (pequenos).
4. Avaliar system archives (`0eee5b81`) + fonts do firmware (`54d4586e`) — resolve fontes sem dump do usuário.
5. Estudar (sessão dedicada, alto risco): rework do memory manager e do trap_manager do Strato.
6. Se o CI do lote 2 falhar: `git revert d81a481c..f6f69441` (ou item a item) e re-disparar; máx. 3 ciclos por item conforme protocolo.
