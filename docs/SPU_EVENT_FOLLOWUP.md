# Eventos SPU e travadas — 26/10/2026, etapa 22

Corrigida uma falha real na leitura de SPU_RdEventStat. A leitura devolvia
todos os bits pendentes, incluindo os desabilitados pela máscara. Agora
retorna event_status & event_mask, em concordância com a condição usada
por rchcnt e pela espera do canal. A leitura preserva os eventos pendentes;
somente o ACK explícito os limpa. Não foram fabricadas conclusões de jobs,
flags ou respostas de semáforos, nem alterados os padrões de desempenho.

Referência primária consultada: o caso SPU_RdEventStat de
[RPCS3/SPUThread.cpp](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/SPUThread.cpp#L5399)
retorna os eventos filtrados pela máscara. A mudança local é pontual e
mantém o comportamento anterior do runtime para leitura com máscara zero.

**O congelamento do Nexus permanece aberto. Não foi comprovado ganho
sustentado de FPS ou redução de CPU com esta correção.**

## Reprodução e teste nativo

Antes: status 0x401, máscara 0x400, leitura 0x401. Depois: leitura 0x400,
status ainda 0x401. O evento desabilitado permanece disponível para uma
troca posterior de máscara, em vez de ser exposto antes dela.

tests/spu_event_status.c usa spu_rdch, spu_wrch e spu_rchcnt da biblioteca
real do SDK. Verifica máscara, bits pendentes, ACK, prontidão, máscara zero,
independência do interrupt-enable, ordem/endian das quatro lanes e espera:
eventos mascarados não liberam o leitor; evento habilitado seguido de wake
libera. Repete 10.000 disputas entre ACK do bit LR e produção de outro bit,
que deve sobreviver. Os hooks de jogo não exercitados abortam se chamados;
não há implementação substituta dos canais no teste.

O mesmo teste ligado ao objeto original de spu_channels.c falha na leitura
esperada 0x400 (linha 36). Ligado à implementação corrigida, passa. Build
incremental e reprodução de patches/ps3recomp.patch a partir do SDK base
39a36296511c445707adabcd312f0b740cfbd694 também passaram.

## Execuções de jogo

Saves/cache privados, 720p, limite 60, FSR/FG/debug/sampler desligados.
Quatro controles alternados no Nexus usaram o mesmo roteiro PAD_PATH de
câmera das etapas anteriores, worker de apresentação desligado e regcache
ligado. A unidade abaixo é o relógio relativo do logger, iniciado após boot.

| Caso em work/perf22_20261026 | Exe | Duração | Último frame registrado | Resultado |
|---|---|---:|---:|---|
| baseline_1 | anterior | 110 s | 70,429 s | parada antes do prazo |
| candidate_1 | corrigido | 110 s | 40,098 s | parada antes do prazo |
| baseline_2 | anterior | 110 s | 104,093 s | frames até o prazo |
| candidate_2 | corrigido | 110 s | 103,968 s | frames até o prazo |
| flamelurker_mask_fix | corrigido | 140 s | 133,964 s | frames/áudio até o prazo |
| candidate_capture2 | corrigido | 150 s | 144,254 s | frames até o prazo |

A repetição final do Nexus usou worker ligado e observação de crescimento
do arquivo. Nenhuma parada foi detectada nessa execução; o observador não
capturou um estado congelado. Um snapshot externo somente de leitura em
candidate_2 registrou contextos/cadeias guest enquanto os frames avançavam;
não localiza a causa da parada. Amostras sem suspensão são aproximadas.

O controle Flamelurker usou a cópia privada dentro da arena, movimento e
ataques pelos gatilhos normais da etapa 20, sem editar AI/flags. Terminou
perto de (133,396, -83,476, -67,070), com áudio ativo e zero frames de áudio
perdidos nas últimas amostras. Não produziu nova comprovação visual do
boss: o limiar configurado para captura não foi atingido. É um teste de
continuidade do trecho, sem validar todas as fases, vitória ou progressão.

candidate_capture, uma tentativa adicional, terminou durante startup sem
linhas de gameplay ou status de saída disponível no relatório. Foi excluída
das seis execuções acima; origem desse encerramento não classificada.
A repetição candidate_capture2 chegou ao prazo. Não se contabiliza a
tentativa sem frames como passagem nem se atribui sua origem à máscara.

Esses dados não permitem comparar FPS/CPU como ganho: há paradas em ambas
as versões, caminhos/AI variam, e os diagnósticos têm instrumentação. O
defeito de máscara foi corrigido por reprodução e semântica do canal;
o protocolo que deixa o Nexus esperando ainda precisa ser localizado.

## Diagnóstico

O detector agora observa o crescimento do frame log enquanto o processo
está vivo. Mais de 10 s sem avanço até o prazo reprova o ensaio. O mtime
permanece informativo: no Windows pode mudar ao fechar o handle, mesmo sem
novos frames. secondsSinceProgress registra o tempo monotônico observado;
há resolução de aproximadamente um segundo e atraso do buffer do logger.
Isso não mede FPS de quadros únicos. Ausência de linhas continua reprovando.

A última linha é convertida para string antes de serializar; Windows
PowerShell podia anexar metadados de provider/drive ao JSON. O processo
mantém o handle aberto para consultar saída; status indisponível é
explicitamente inconclusivo. elapsedSeconds registra a espera medida.

Fixtures de processo pelo script real: atualizações contínuas aprovadas;
log com FPS 60 e sem progresso por mais de 10 s reprovado; variáveis antes
ausentes restauradas como ausentes. Resumos UTF-8 e última linha textual
conferidos na validação final.

## Preservação

14 arquivos dos saves e todos os 1.515 arquivos do cache do jogador
preservados por SHA-256. Novos dados dos ensaios ficam apenas nas cópias.
Nenhum processo de jogo/controlador remanescente na conferência final.

- Exe anterior: 105C933F6FA61AE7E31BB029595375D109B0D3037BAD5F6F6CFB674C7D83C029.
- Exe corrigido: 43FFB4AF7617AB8DFA9AF86B40DD1EE535D09BEAE54BC5C5D7DB6E3008459F26.
- Evidências: work/perf22_20261026 e work/diagnostics/perf22_*.
