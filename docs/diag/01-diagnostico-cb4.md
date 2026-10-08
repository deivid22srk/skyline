# Diagnóstico Crash Bandicoot 4 (010073401175E000) — build de instrumentação

Branch: `feat/sifu-deepseek`. Objetivo desta rodada: **observabilidade** (Fase 1) —
identificar a causa real dos sintomas abaixo com logs, antes de correções amplas.

## Sintomas observados (pelo usuário)

1. Cutscene/loading "Despertar Indelicado": imagem quebrada em faixas horizontais
   (~múltiplos de 256 px), 25 FPS.
2. Primeira fase: HUD renderiza, mundo 3D não aparece (fundo chapado), 4 FPS
   (frametime 272 ms ± 304 ms).

## Tabela de hipóteses

| ID | Hipótese | Evidência atual | Status |
|----|----------|-----------------|--------|
| H1 | Cópias DMA pitch→block-linear abortadas tocam heap **sparse** (não-commitado) — hardware completaria a cópia (escrita descartada); o aborto deixa texturas stale (faixas) | 3x aborto em `0x5719C0400-0x571AB0000`, `5120x20x4 bpp 1`; heap de streaming UE4 usa `AllocSpace(sparse)`+Remap | PENDENTE — novo log `DMA abort detail: … is sparse/unmapped` decide |
| H2 | Instrução Maxwell stubada gera shader incorreto/vazio para o mundo 3D | 44x `(STUBBED) called` sem identificação (sites: VOTE_vtg, ISBERD, EmitInvocationInfo) | PENDENTE — logs agora identificam site + hash + estágio |
| H3 | Serviços de rede ausentes fazem init online do UE4 falhar de forma suja/repetida | 10x "Cannot find HIPC function" (IClient 0x2/0x14, IResolver 0xC); sequência se repete +30 s | MITIGADO — retornos de erro corretos (Socket→ENETDOWN, Fcntl→EBADF, DNS→falha imediata) |
| H4 | Gralloc 0x38/0x3b (4x4) — sondagens do driver Turnip, provavelmente inofensivas | 4 ocorrências em boot | PENDENTE (prioridade baixa; só agir se ligado a texturas faltando) |
| H5 | Driver customizado Turnip (WN-Turnip-1.18-p) contribui para parte dos bugs | traits: Atomic64=false, MutableFormat costly, Subgroup 128 | PENDENTE — teste A/B com driver do sistema (roteiro abaixo) |
| H6 | Pipelines geometry-passthrough recebem `OpExecutionMode Invocations 0` (inválido) → pipelines com geometry falham/mal-comportados; HUD (sem geometry) funciona | shader-compiler db43a3a sem o fix upstream 0e22c80 (`invocations = 1`); fix aplicado (pin 8717c8f) | CORRIGIDO (aguardando confirmação do usuário) |
| H7 | Method writes do subcanal Copy poluíam registradores do Fermi2D (falta de `break` no dispatch) — blits 2D após rajadas DMA com estado corrompido | gpfifo.cpp SendPure sem break (batch variant tem); presente em upstream/Strato | CORRIGIDO (aguardando confirmação) |

## O que foi instrumentado nesta build

| Commit | Instrumento | O que revela |
|--------|-------------|--------------|
| 51ccf0e3 | `fix(soc)`: break ausente no subcanal Copy | Corrige poluição de registradores do Fermi2D (H7) |
| 9052afd5 | `feat(log)`: contexto de shader nos logs do compilador | Cada warning do shader-compiler vira `… [shader 0x<hash>, stage <x>]` |
| cdea43fa | `port(strato)`: shader-compiler → 8717c8f | Fix upstream `invocations = 1` (H6) |
| 51c8031d | `feat(submodule)`: fork com STUBBED desambiguado | `(STUBBED) called` → `VOTE_vtg…` / `ISBERD…` / `InvocationInfo…` (H2) |
| 81c7023f | `feat(log)`: flush 250 ms + supressão de spam do guest | Log sobrevive a SIGKILL; UE4 LocalPrint não consome o log |
| ca150dbe | `feat(gpu)`: diagnóstico DMA | `DMA abort detail: <side> span N VA … is sparse/unmapped` + totais ok/abortados (H1) |
| f4604229 | `feat(gpu)`: timing de pipelines + stats 1Hz | `Compiled new graphics pipeline synchronously in Nms`, `GPFIFO blocked Nms…`, `GPFIFO stats: N submissions in Xms` |
| ac9b4ce2 | `feat(hos)`: bsd/sfdnsres fail-fast | Início online falha limpo e rápido (H3) |

## Roteiro de teste (usuário)

1. Instalar o APK do build desta branch e limpar logs antigos.
2. Abrir o jogo e chegar até: (a) cutscene "Despertar Indelicado"; (b) início da
   primeira fase; ficar ~60 s andando/pulando/girando a câmera.
3. Sair pelo menu do emulador (não matar o app) — garante flush do log.
4. Enviar: `emulation.sklog` completo + logcat filtrado + 2 capturas de tela.

Filtro de logcat recomendado:

```bash
adb logcat | grep -E "emu-cpp|skyline"
```

(Ruído esperado: centenas de `libc: Access denied finding property "vendor.mesa.*"`
— vêm do driver Turnip, ignorar.)

5. Teste A/B para H5 (driver): repetir o roteiro com (i) driver do sistema e
   (ii) outro Turnip, em execuções separadas, anotando diferenças visuais/FPS.

## O que procurar no novo log

- `DMA abort detail: dst span … is sparse (reserved, uncommitted)` → H1 confirmada
  (mudança para semântica write-discard na Fase 2).
- `VOTE_vtg…` / `ISBERD…` / `InvocationInfo…` com hash+estágio → implementa-se o
  opcode que aparecer nos shaders do mundo 3D (H2).
- `Compiled new graphics pipeline synchronously in Nms` com N alto / vários
  `GPFIFO blocked Nms` → gargalo de compilação síncrona confirmado.
- `GPFIFO stats` caindo a ~4/s com poucas compilações → investigar stalls de
  fence/upload (habilitar `waitIdlePerSubmit` no menu de hacks/debug se pedido).
