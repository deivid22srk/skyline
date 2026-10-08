# 03b — Candidatos de Port: Serviços/Stubs/Rede (Strato → Skyline)

Análise dos commits de serviços do Strato e comparação com o estado atual do Skyline
(branch `feat/sifu-deepseek`, HEAD `9c39760f`). Documento gerado pelo sub-agente A3-b.

## Metodologia

1. Para cada commit listado: `git show <hash> --stat` + patch completo, extraindo os
   comandos HOS reais adicionados (IDs `SFUNC`, nomes de métodos, structs e constantes
   `Result`). Nenhum nome/ID foi presumido — todos vieram do patch.
2. Para cada comando: busca no código atual do Skyline (`rg` em
   `app/src/main/cpp/skyline/services/...`) pelo nome do método e/ou pelo ID `SFUNC`,
   incluindo leitura direta dos blocos `SERVICE_DECL` atuais.
3. Vereditos: **ALREADY_PRESENT** (stub equivalente já existe no Skyline atual),
   **PORTABLE** (comandos ausentes; patch aplicável com adaptação), **DIVERGED**
   (implementações incompatíveis), **SKIP** (não vale a pena), **INCERTO** (não
   verificado). Esforço: S (< ~1h) / M (~meio dia) / L (> 1 dia).

Nota geral: no merge-base `6aef7fdd` ("Stub some services"), o Skyline já tinha os
mesmos arquivos de serviços que a branch atual usa; o upstream evoluiu pouco nesses
arquivos (ex.: capsrv continua com classes vazias só-construtor), então a maioria dos
patches do Strato se aplica quase direto.

## Tabela-resumo

| # | Grupo/Serviço | Commits Strato | Veredito | Comandos faltando | Esf. | Risco | Prio |
|---|---------------|----------------|----------|-------------------|------|-------|------|
| 1 | socket/bsd IClient | 20a9ab65 | PORTABLE | Socket 0x2, GetPeerName 0xF, GetSockName 0x10, GetSockOpt 0x11, Fcntl 0x14, EventFd 0x1F (+registro bsd:s) | M | Médio | P1 |
| 2 | socket/sfdnsres IResolver | 43195927, 68bb8256* | PORTABLE | GetAddrInfoRequest 0x6, GetHostByNameRequestWithOptions 0xA, GetAddrInfoRequestWithOptions 0xC, GetNameInfoRequestWithOptions 0xD | M | Médio | P1 |
| 3 | nifm IRequest/IGeneralService | a299bb3c, 68bb8256, e4f9fd62* | PORTABLE | GetCurrentNetworkProfile 0x5, GetCurrentIpConfigInfo 0xF, GetInternetConnectionStatus 0x12, IRequest.Cancel 0x3 (+ RequestState) | M | Médio | P1 |
| 4 | ldn IUserLocalCommunicationService | 460e1448 | PORTABLE | 12 comandos (0x1–0x5, 0x65, 0x66, 0xC8, 0xCA, 0xCB, 0xCE, 0x12C) + ~18 structs | M/L | Médio | P2 |
| 5 | account (IAuthorizationRequest, IAsyncContext) | 6ac7d22e, 41882aab | PORTABLE | CreateAuthorizationRequest 0x96, EnsureIdTokenCacheAsync 0x2, LoadIdTokenCache 0x3; serviço novo + IAsyncContext | M | Baixo | P2 |
| 6 | pctl IParentalControlService | a523af21, 41882aab* | PORTABLE | EndFreeCommunication 0x3F9 + 6 cmds StereoVision (0x3F5, 0x425–0x429) | S | Baixo | P3 |
| 7 | socket/nsd + ssl | 41882aab* | nsd: ALREADY_PRESENT / ssl: PORTABLE | ssl: RegisterInternalPki 0x8 | S | Baixo | P3 |
| 8 | ntc | 8113413a | PORTABLE | serviço novo: StartTask 0x0 | S | Baixo | P2 |
| 9 | bcat RequestSyncDeliveryCache | 7026470b, aa0a45eb | PORTABLE | RequestSyncDeliveryCache 0x2774 + serviço novo IDeliveryCacheProgressService (0x0, 0x1) | S | Baixo | P2 |
| 10 | Novos serviços: clkrst, psm, ts | e4f9fd62, 3ca24b9f | PORTABLE | clkrst (OpenSession 0x0); psm (0x0,0x1,0x7 + sessão 0x0–0x4); ts (0x1, 0x3, 0x4) | M | Baixo | P2 |
| 11 | friends, fssrv, settings (cmds avulsos) | e4f9fd62 | PORTABLE | friends: 0x0, 0x2788; fssrv: 0xE + 0x3C/0x3D/0x3E + ISaveDataInfoReader; settings: GetColorSetId 0x17 | S/M | Baixo | P2 |
| 12 | hid IHidServer | 35b90c96, 13f88f75, e05de1ac | PORTABLE | ActivateConsoleSixAxisSensor 0x12C, InitializeSevenSixAxisSensor 0x132, ResetSevenSixAxisSensorTimestamp 0x136, IsVibrationDeviceMounted 0xD3 | S | Baixo | P1 |
| 13 | capsrv (4 serviços) | 4f6d5b28, c43912c1, f0430e46 | PORTABLE | SetShimLibraryVersion (0x20/0x21/0x20), GetAlbumFileList0AafeAruidDeprecated 0x66, GetAlbumFileList3AaeAruid 0x8E | S | Baixo | P1 |
| 14 | am ICommonStateGetter | 937eacef, ffb93d21 | PORTABLE | SetVrModeEnabled 0x33, SetLcdBacklighOffEnabled 0x34, BeginVrModeEx 0x35, EndVrModeEx 0x36 | S | Baixo | P1 |
| 15 | am ISelfController | c3eef507, 80911207, 1bfdcaa3 | PORTABLE | ReportUserIsActive 0x41, IsIlluminanceAvailable 0x43, SetAutoSleepDisabled 0x44, IsAutoSleepDisabled 0x45, GetCurrentIlluminanceEx 0x47 | S | Baixo | P1 |
| 16 | am IApplicationFunctions | eb2d04ea | PORTABLE | GetSaveDataSizeMax 0x1C | S | Baixo | P3 |
| 17 | am ILibraryAppletAccessor | 395e08d0 | PORTABLE | GetIndirectLayerConsumerHandle 0xA0 | S | Baixo | P2 |
| 18 | lbl | d32aa861 | PORTABLE | serviço novo ILblController: 9 comandos (0x11–0x1C) | S | Baixo | P2 |
| 19 | irs IIrSensorServer | d241e846, 9465bfeb | PORTABLE | CheckFirmwareVersion 0x13A, StopImageProcessorAsync 0x13E | S | Baixo | P3 |
| 20 | System archives (fsp-srv) | 0eee5b81 | PORTABLE | leitura de NCA em nand/system/Contents/registered dentro de OpenDataStorageByDataId | M | Médio | P2 |
| 21 | Extração de fontes (loader/JNI) | 54d4586e | PORTABLE | decodeBfttfFont + JNI extractFonts + FirmwareImportPreference.kt | M | Médio | P2 |
| 22 | vfs/os_filesystem permissões | f85ecc23, a6c26993 | PORTABLE | criar arquivos com 0666 (constantes S_I*) | S | Baixo | P3 |

---

## 1. socket/bsd IClient — `20a9ab65` — PORTABLE (M, risco médio, P1)

Comandos adicionados no patch (IClient.h, SERVICE_DECL):
`Socket(0x2)`, `GetPeerName(0xF)`, `GetSockName(0x10)`, `GetSockOpt(0x11)`,
`Fcntl(0x14)`, `EventFd(0x1F)`, além de helpers `PushBsdResult`/`PushBsdResultErrno`
e enum `OptionName`. O patch removeu `Select(0x5)` do SERVICE_DECL e trocou por
`Socket(0x2)` (Select é 0x5; Socket é 0x2 — no patch Strato Select saiu e Socket entrou).

Estado atual do Skyline: `services/socket/bsd/IClient.h:108-126` registra apenas
0x0,0x1,**0x5 (Select)**,0x6,0x8–0xE,0x12,0x15–0x1A. Nenhum dos 6 comandos acima existe
(`rg "Socket|GetPeerName|GetSockName|GetSockOpt|Fcntl|EventFd" IClient.h` = só Select).

Ação de port: portar implementações (207 linhas em IClient.cpp usam sockets POSIX
nativos) mantendo `Select` (não remover como o Strato fez); adicionar `bsd:s` no
serviceman (feito pelo Strato em 8113413a: `SERVICE_CASE(socket::IClient, "bsd:s")`).

## 2. socket/sfdnsres IResolver — `43195927` (+fixes em `68bb8256`) — PORTABLE (M, risco médio, P1)

Patch cria a implementação real de resolução DNS: `GetAddrInfoRequest(0x6)`,
`GetHostByNameRequestWithOptions(0xA)`, `GetAddrInfoRequestWithOptions(0xC)`,
`GetNameInfoRequestWithOptions(0xD)`, com `SerializeAddrInfo`, `NetDbError` etc.
(169 linhas em IResolver.cpp).

Estado atual: `services/socket/sfdnsres/IResolver.h` e `.cpp` estão **totalmente vazios**
(só construtor, IResolver.cpp:7 — idêntico ao estado pré-patch no Strato). O serviço já
está registrado: serviceman.cpp:129 `SERVICE_CASE(socket::IResolver, "sfdnsres")`.

Ação: port quase 1:1; 68bb8256 tocou IResolver apenas para ajustes (refs. do Strato).

## 3. nifm — `a299bb3c` + `68bb8256` + parte de `e4f9fd62` — PORTABLE (M, risco médio, P1)

- a299bb3c: structs `IpAddressSetting`/`DnsSetting`/`ProxySetting`/`IpSettingData`/
  `SfWirelessSettingData`/`NifmWirelessSettingData` (IGeneralService.h) e comandos
  `GetCurrentNetworkProfile(0x5)` e `GetCurrentIpConfigInfo(0xF)`; inclui ponte JVM
  (jvm.cpp/jvm.h +20/+14, EmulationActivity.kt, AndroidManifest) para obter config de IP
  do Android. `INTERNET` permission já existe no manifest atual (linha 5); `jvm.h` atual
  não tem nenhum método de rede (`rg Network|IpAddress jvm.h` vazio).
- 68bb8256: IRequest — enum `RequestState`, `GetRequestState` respeitando
  `settings->isInternetEnabled`, novo `Cancel(0x3)`.
- e4f9fd62: `GetInternetConnectionStatus(0x12)` (struct {type:1, wifiStrength:3, state:4}).

Estado atual: IGeneralService.h:41-44 tem só 0x1, 0x4, 0xC (GetCurrentIpAddress), 0x15
(IsAnyInternetRequestAccepted, com result::NoInternetConnection no GetCurrentIpAddress).
IRequest.h:57-63 tem 0x0,0x1,0x2,0x4,0xB,0x15 — sem Cancel, sem enum RequestState
(GetRequestState ainda empurra constante Unsubmitted=1, IRequest.cpp:17-20).
**Dependência**: 68bb8256/460e1448 usam `state.settings->isInternetEnabled`, que NÃO
existe no fork atual (rg negativo em todo o tree; common/settings.h não tem a flag) —
port exige adicionar `Setting<bool> isInternetEnabled` (ou hardcodar "online").
Veredito: PORTABLE; parte JVM + setting é o que dá esforço M.

## 4. ldn IUserLocalCommunicationService — `460e1448` — PORTABLE (M/L, risco médio, P2)

Patch adiciona ~260 linhas de structs/enums (NetworkInfo, NodeInfo, Ssid, SecurityConfig,
SecurityParameter, UserConfig, NetworkConfig, NodeLatestUpdate, State, DisconnectReason…)
e 12 comandos: `GetNetworkInfo(0x1)`, `GetIpv4Address(0x2)`, `GetDisconnectReason(0x3)`,
`GetSecurityParameter(0x4)`, `GetNetworkConfig(0x5)`, `GetNetworkInfoLatestUpdate(0x65)`,
`Scan(0x66)`, `OpenAccessPoint(0xC8)`, `CreateNetwork(0xCA)`,
`CreateNetworkPrivate(0xCB)`, `SetAdvertiseData(0xCE)`, `OpenStation(0x12C)`; tudo stub
retornando estado "desconectado"/AirplaneModeEnabled(203,23) quando sem internet
(também usa `isInternetEnabled` — ver dependência do item 3).

Estado atual: IUserLocalCommunicationService.h:50-54 só tem GetState(0x0),
AttachStateChangeEvent(0x64), InitializeSystem(0x190), FinalizeSystem(0x191),
InitializeSystem2(0x192). Veredito: PORTABLE (grande volume de structs, mas tudo stub).

## 5. account — `6ac7d22e` + `41882aab` — PORTABLE (M, risco baixo, P2)

- 6ac7d22e: classe nova `IAuthorizationRequest` (IAuthorizationRequest.cpp/h) com
  `InvokeWithoutInteractionAsync(0xA)`, `IsAuthorized(0x13)`, `GetAuthorizationCode(0x15)`;
  `IManagerForApplication.CreateAuthorizationRequest(0x96)` retorna o objeto.
- 41882aab: classe nova `IAsyncContext` (`GetSystemEvent 0x0`, `Cancel 0x1`,
  `HasDone 0x2`, `GetResult 0x3`); `IManagerForApplication.EnsureIdTokenCacheAsync(0x2)`
  (retorna IAsyncContext) e `LoadIdTokenCache(0x3)`.

Estado atual: services/account/ tem só IAccountServiceForApplication, IManagerForApplication
(h:34-36 = 0x0, 0x1, 0xA0), IProfile. Sem IAuthorizationRequest, sem IAsyncContext.
Veredito: PORTABLE (2 classes novas + 3 comandos; lembrar de adicionar .cpp no CMakeLists).

## 6. pctl IParentalControlService — `a523af21` + `41882aab` — PORTABLE (S, risco baixo, P3)

- 41882aab: `EndFreeCommunication(0x3F9)`.
- a523af21: StereoVision — `ConfirmStereoVisionPermission(0x3F5)`,
  `ConfirmStereoVisionRestrictionConfigurable(0x425)`, `GetStereoVisionRestriction(0x426)`,
  `SetStereoVisionRestriction(0x427)`, `ResetConfirmedStereoVisionPermission(0x428)`,
  `IsStereoVisionPermitted(0x429)` + helper `IsStereoVisionPermittedImpl()` com Results
  StereoVisionDenied(142,104)/PermissionDenied(142,133)/…(142,181).

Estado atual: IParentalControlService.h:27-29 só tem Initialize(0x1),
CheckFreeCommunicationPermission(0x3E9), IsFreeCommunicationAvailable(0x3FA). PORTABLE.

## 7. socket/nsd + ssl — `41882aab` — nsd: ALREADY_PRESENT / ssl: PORTABLE (S)

- nsd `ResolveEx(0x15)`: o Skyline atual **já cobre** o 0x15 com stub de sucesso:
  `services/socket/nsd/IManager.h:23` `SFUNC(0x15, IManager, UnknownCommand)` e
  IManager.cpp comenta explicitamente "The semantics of nsd command 0x15 are unverified,
  stub it out with a success response so callers proceed" — comportamento idêntico ao
  patch Strato. Nada a fazer.
- ssl `RegisterInternalPki(0x8)`: não existe. Atual ISslContext.h:23 só tem
  `ImportServerPki(0x4)`. PORTABLE (S).

## 8. ntc — `8113413a` — PORTABLE (S, risco baixo, P2)

Serviço novo `ntc::IEnsureNetworkClockAvailabilityService` com `StartTask(0x0)`
retornando `Result NetworkTimeNotAvailable{116, 1000}`; registro
`SERVICE_CASE(ntc::..., "ntc")` no serviceman. Estado atual: diretório
services/ntc não existe e "ntc" não está no serviceman. PORTABLE.

## 9. bcat RequestSyncDeliveryCache — `7026470b` + `aa0a45eb` — PORTABLE (S, P2)

- 7026470b: `IBcatService.RequestSyncDeliveryCache(0x2774)` (retorna
  IDeliveryCacheStorageService, que já existe no Skyline atual).
- aa0a45eb: serviço novo `IDeliveryCacheProgressService` (`GetEvent 0x0`,
  `GetImpl 0x1`), retornado por RequestSyncDeliveryCache na versão final.

Estado atual: IBcatService é classe vazia (só construtor, IBcatService.cpp:7);
não há IDeliveryCacheProgressService. Diretório bcat/ existe com Storage/File/Directory
services. PORTABLE.

## 10. Novos serviços: clkrst, psm, ts — `e4f9fd62` (+`3ca24b9f`) — PORTABLE (M, P2)

Três serviços criados no mesmo commit, nenhum existe no Skyline atual (sem diretórios
services/clkrst|psm|ts; sem SERVICE_CASE "clkrst"/"psm"/"ts" no serviceman.cpp):

- **clkrst**: `IClkrstManager.OpenSession(0x0)` → `IClkrstSession` (sem comandos).
- **psm**: `IPsmServer.GetBatteryChargePercentage(0x0)` (retorna 100 no Strato), `GetChargerType(0x1)`, `OpenSession(0x7)` → `IPsmSession` com
  `BindStateChangeEvent(0x0)`, `UnbindStateChangeEvent(0x1)`,
  `SetChargerTypeChangeEventEnabled(0x2)`, `SetPowerSupplyChangeEventEnabled(0x3)`,
  `SetBatteryVoltageStateChangeEventEnabled(0x4)`.
- **ts**: `IMeasurementServer.GetTemperature(0x1)`, `OpenSession(0x4)` → `ISession`
  (sem comandos); `3ca24b9f` adiciona `GetTemperatureMilliC(0x3)`.

Registros no serviceman: "clkrst", "psm", "ts". PORTABLE.

## 11. Comandos avulsos em serviços existentes — `e4f9fd62` — PORTABLE (S/M, P2)

- **friends**: `GetCompletionEvent(0x0)`, `CheckFriendListAvailability(0x2788)` —
  atuais IFriendService.h:30-35 têm 0x2775/0x28A0/0x2968/0x2969/0x2972/0x29CC; nenhum dos dois existe.
- **fssrv**: `IFileSystem.GetFileTimeStampRaw(0xE)` (atual IFileSystem.h:71-79 vai até
  0xB GetFreeSpaceSize); `IFileSystemProxy.OpenSaveDataInfoReader(0x3C)`,
  `...BySaveDataSpaceId(0x3D)`, `...OnlyCacheStorage(0x3E)` (atual IFileSystemProxy.h:118-127
  não tem nenhum); classe nova `ISaveDataInfoReader` (iterador vazio).
- **settings**: `GetColorSetId(0x17)` — atual ISystemSettingsServer.h:43 só tem
  GetFirmwareVersion(0x3). PORTABLE.

## 12. hid IHidServer — `35b90c96` + `13f88f75` + `e05de1ac` — PORTABLE (S, P1)

- `ActivateConsoleSixAxisSensor(0x12C)` + `InitializeSevenSixAxisSensor(0x132)`
  (35b90c96); `ResetSevenSixAxisSensorTimestamp(0x136)` (13f88f75);
  `IsVibrationDeviceMounted(0xD3)` (e05de1ac, retorna true).

Estado atual: IHidServer.h:242-249 termina em 0x20D SetPalmaBoostMode;
`rg "0x12C|0x132|0x136|0xD3" IHidServer.h` = nada. PORTABLE — todos stubs triviais.

## 13. capsrv — `4f6d5b28` + `c43912c1` + `f0430e46` — PORTABLE (S, P1)

- `SetShimLibraryVersion`: 0x20 em IAlbumApplicationService, 0x21 em
  ICaptureControllerService, 0x20 em IScreenShotApplicationService (4f6d5b28).
- `GetAlbumFileList0AafeAruidDeprecated(0x66)` (c43912c1) e
  `GetAlbumFileList3AaeAruid(0x8E)` (f0430e46) em IAlbumApplicationService.

Estado atual: as 4 classes capsrv existem (registradas em serviceman.cpp:139-142) mas
são **vazias** (só construtor, ex. IAlbumApplicationService.h:15-17) — nenhum comando.
PORTABLE direto.

## 14. am ICommonStateGetter — `ffb93d21` + `937eacef` — PORTABLE (S, P1)

`SetVrModeEnabled(0x33)`, `BeginVrModeEx(0x35)`, `EndVrModeEx(0x36)` (ffb93d21);
`SetLcdBacklighOffEnabled(0x34)` (937eacef). Estado atual ICommonStateGetter.h:132-141:
tem IsVrModeEnabled(0x32) mas nenhum dos 4 comandos de escrita. PORTABLE (stubs triviais;
0x33 no Strato sincroniza com o estado usado por 0x32).

## 15. am ISelfController — `1bfdcaa3` + `80911207` + `c3eef507` — PORTABLE (S, P1)

`ReportUserIsActive(0x41)`, `IsIlluminanceAvailable(0x43)` (retorna false),
`GetCurrentIlluminanceEx(0x47)`, `SetAutoSleepDisabled(0x44)`,
`IsAutoSleepDisabled(0x45)`. Estado atual ISelfController.h:130-143 vai até 0x3F
(GetIdleTimeDetectionExtension); nenhum dos 5 existe. PORTABLE (stubs triviais).

## 16. am IApplicationFunctions — `eb2d04ea` — PORTABLE (S, P3)

`GetSaveDataSizeMax(0x1C)` retornando os mesmos constantes de GetSaveDataSize
(o patch também refatorou GetSaveDataSize/0x1A para usar constantes compartilhadas
`SaveDataSize`/`JournalSaveDataSize`). Estado atual: IApplicationFunctions.h:144-149
tem 0x1A GetSaveDataSize (com constantes locais) mas não 0x1C. PORTABLE.

## 17. am ILibraryAppletAccessor — `395e08d0` — PORTABLE (S, P2)

`GetIndirectLayerConsumerHandle(0xA0)` (stub, retorna sucesso + handle 0). Estado atual
ILibraryAppletAccessor.h:92-100 vai até 0x6A GetPopInteractiveOutDataEvent;
`rg 0xA0` = ausente. PORTABLE.

## 18. lbl — `d32aa861` — PORTABLE (S, P2)

Serviço novo `lbl::ILblController` com 9 comandos: `SetBrightnessReflectionDelayLevel(0x11)`,
`GetBrightnessReflectionDelayLevel(0x12)`, `SetCurrentAmbientLightSensorMapping(0x15)`,
`GetCurrentAmbientLightSensorMapping(0x16)`, `SetCurrentBrightnessSettingForVrMode(0x18)`,
`GetCurrentBrightnessSettingForVrMode(0x19)`, `EnableVrMode(0x1A)`, `DisableVrMode(0x1B)`,
`IsVrModeEnabled(0x1C)`. Registro `SERVICE_CASE(lbl::ILblController, "lbl")`. Estado
atual: sem diretório services/lbl e sem "lbl" no serviceman. PORTABLE (stubs triviais).

## 19. irs IIrSensorServer — `d241e846` + `9465bfeb` — PORTABLE (S, P3)

`CheckFirmwareVersion(0x13A)` (d241e846) e `StopImageProcessorAsync(0x13E)` (9465bfeb).
Estado atual IIrSensorServer.h:40-42 tem 0x130/0x137/0x13F — nenhum dos dois. PORTABLE.

## 20. Leitura de system archives — `0eee5b81` — PORTABLE (M, risco médio, P2)

O patch reescreve o corpo de `OpenDataStorageByDataId` (fsp-srv) para, ao receber um
dataId, varrer `publicAppFilesPath + "/switch/nand/system/Contents/registered/"` via
`OsFileSystem`, abrir cada NCA (`vfs::NCA(backing, keyStore)` com KeyStore de
`privateAppFilesPath + "keys"`), casar `nca.header.programId == dataId` e devolver
`nca.romFs`; fallback para o comportamento antigo (`romfs/{:016X}` no assetFileSystem).
Também expõe `NcaHeader header` como membro pública em vfs/nca.h (3 linhas).

Estado atual: IFileSystemProxy.cpp:85-94 só faz `assetFileSystem->OpenFile("romfs/{:016X}")`
— não lê NCAs de sistema. vfs/nca.h já tem `NcaHeader` (linha 148) com `programId` (157) e
membro `romFs` (194), mas não expõe `header`. PORTABLE; depende de o usuário ter importado
firmware+keys (ver item 21).

## 21. Extração de fontes do firmware — `54d4586e` — PORTABLE (M, risco médio, P2)

Patch adiciona em `loader_jni.cpp`: `decodeBfttfFont()` (XOR com chave 0x06186249 sobre
BFTTF), função JNI `Java_emu_skyline_preference_FirmwareImportPreference_extractFonts`
(extrai fontes dos NCAs de sistema listados no commit) e mudança em
`FirmwareImportPreference.kt` para chamar o método. Estado atual: loader_jni.cpp atual
não tem nenhuma menção a font/BFTTF (`rg` vazio) e o FirmwareImportPreference.kt atual é
da versão do fork. PORTABLE, mas toca camada JNI/Kotlin (testar no device) e complementa
o item 20 (jogos que usam pl:u/shared font esperam as fontes reais).

## 22. Permissões de criação de arquivo — `f85ecc23` + `a6c26993` — PORTABLE (XS, P3)

Efeito líquido dos dois commits: criar arquivos com modo `0666` expresso em constantes
(`S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH`). Estado atual
vfs/os_filesystem.cpp:23: `open(fullPath.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)`
(= 0600). Troca de 1 linha; útil para acesso compartilhado ao storage do app.

---

## Recomendações de ordem de ataque

1. **P1, esforço S**: itens 12 (hid), 13 (capsrv), 14 (ICommonStateGetter), 15
   (ISelfController) — stubs triviais que destravam muitos jogos com custo mínimo.
2. **P1, esforço M**: itens 1 (bsd), 2 (sfdnsres), 3 (nifm) — rede completa; portar os
   três juntos (tocam o mesmo sub-sistema socket/ e settings de internet).
3. **P2**: itens 8+9+10+18 (ntc, bcat, clkrst/psm/ts, lbl — serviços novos independentes),
   4 (ldn), 5 (account), 7-ssl, 11, 17; itens 20+21 (system archives + fontes) como bloco.
4. **P3**: itens 6, 16, 19, 22.

Riscos gerais: (a) patches de 2023 aplicam sobre `ServiceName`/`SERVICE_DECL` antigos —
conferir diff de contexto com a branch (a API de BaseService atual usa SFUNC igual);
(b) itens 3/4 dependem da setting `isInternetEnabled` que **não existe** no fork atual
(`rg isInternetEnabled app/src/main/cpp/skyline/` = vazio) — criar a flag ou stub
positivo; (c) itens 20/21 dependem de import de firmware e keys; (d) novos .cpp devem
ser adicionados ao app/CMakeLists.txt (padrão do repo).
