# O que aproveitar do inFAMOUSreborn

Análise de 23/10/2026 sobre o commit
[`2e9405c`](https://github.com/whompay/inFAMOUSreborn/commit/2e9405c72408d28f505b6636a2f9cb6b6125aa85),
comparado com o runtime local e o perfil de [PERFORMANCE.md](PERFORMANCE.md).
Referência consultada em `work/references/inFAMOUSreborn`. Após a análise,
foram adaptados o hash completo XXH3 e o cache de registradores SPU. A
implementação, medições locais e limitações estão em
[PERFORMANCE.md](PERFORMANCE.md).

Em 25/10 foram também implementados e validados o preload/pack de bytecode
e a biblioteca persistente D3D12, ligados por padrão. No par A/B repetido,
as pausas >50 ms caíram de 22 para 11 e o p99 médio das janelas de 39,13
para 24,76 ms; FPS 55,82 → 57,05, sem redução consistente da CPU total.
São resultados locais com cache aquecido, não garantia para toda a gameplay.
Código, formatos, testes, fallback e medições completas em
[PERFORMANCE.md](PERFORMANCE.md). Os demais itens continuam como propostas.

A investigação seguinte acrescentou batching síncrono de pacotes no FIFO,
com cerca de +10% nesta cena, e testou uma fila limitada para um worker D3D12.
Essa fila por pacotes piorou o FPS e ficou desligada por padrão. Ela ainda
depende dos buffers do jogo e conserva drenagens frequentes; não substitui
os snapshots por quadro e o replay de `RF_ASYNC`. Detalhes e todos os casos
comparados em [PERFORMANCE.md](PERFORMANCE.md).

No DeS, o hash usa XXH3-128 para manter as duas partes da chave. O passe SPU
mantém nossos helpers atuais, sem importar `spu_helpers_vec.h`, gera versões
paralelas e seleciona a função uma vez por entrada do registro. Os originais
continuam acessíveis com `SPU_REGCACHE=0`; `LD_GEOM_XXH3=0` compara o hash
anterior. Ambos os controles são definidos antes de iniciar o processo.
O CMake executa `scripts/spu_regcache.py` e inclui somente imagens presentes
na lista de fontes do build. Licenças: `scripts/INFAMOUS_REGCACHE_LICENSE.txt`
e `third_party/ps3recomp/libs/video/xxhash_LICENSE.txt` (incluída no patch).

## Nova conferência em 24/10/2026 — diagnóstico anterior à implementação

Consulta remota com `git fetch --filter=blob:none origin main` e
`git ls-remote --heads --tags origin`: a única branch publicada, `main`,
continua em `2e9405c72408d28f505b6636a2f9cb6b6125aa85` (23/10, mensagem
`Renderer frame overlap`). Nenhum commit novo desde a análise anterior.
A tag `1.1` também é ancestral desse commit. Isso verifica o código publicado;
não é auditoria dos binários de releases.

O perfil local da etapa 12 mudou a prioridade das adaptações já disponíveis:
ao girar a câmera, apareceram pausas de 34–75 ms preparando PSOs, mesmo com
VS/PS encontrados no cache. Em um exemplo, o segmento PS/cache levou
47,480 ms e a criação D3D12 27,882 ms. O primeiro segmento inclui procura,
leitura, validação/alocação/cópia do bytecode; não mede exclusivamente I/O.

| Prioridade atual | Recurso existente no donor | Aplicação ao DeS |
| --- | --- | --- |
| 1 | `SHC_PACK`: pré-carregamento e cache de bytecode em memória a partir de um arquivo único | Evitar abrir/ler um arquivo por shader no meio de um frame. Ainda não temos esse pré-carregamento; o cache em RAM atual é preenchido sob demanda. |
| 2 | `PSO_LIB`: biblioteca persistente de pipelines D3D12 | Evitar repetir parte da criação do pipeline do driver entre execuções. Nosso PSO vive somente em memória; o cache em disco contém shaders, não o pipeline completo. |
| 3 | `RF_EARLY`/`RF_ASYNC`: preparação e gravação sobrepostas | Mudança estrutural posterior: requer snapshots próprios e preservação dos fences/recursos. Nosso worker de apresentação não equivale a esse mecanismo. |

O [SHC_PACK](https://github.com/whompay/inFAMOUSreborn/blob/2e9405c72408d28f505b6636a2f9cb6b6125aa85/ps3recomp/libs/video/rsx_d3d12_backend.c#L2942)
carrega `shcpack.bin` e os `.cso` ainda ausentes em uma thread de prioridade
reduzida. A consulta copia o bytecode encontrado para um blob; faltas continuam
no caminho original. Novas compilações escrevem arquivos soltos, reunidos no
próximo início. Esse recurso não estava detalhado na análise inicial.

O [PSO_LIB](https://github.com/whompay/inFAMOUSreborn/blob/2e9405c72408d28f505b6636a2f9cb6b6125aa85/ps3recomp/libs/video/rsx_d3d12_backend.c#L3672)
carrega/salva `ID3D12PipelineLibrary`, tenta `LoadGraphicsPipeline` e recua à
criação normal em caso de falta/incompatibilidade. Ambos são opt-in no backend
do donor, mas estão ligados no `installer/infamous.toml` desse commit.

Adaptação: conservar nossa validação do HLSL completo e formato do cache,
limitar bytes retidos/precarregados, manter fallback síncrono, definir uma
identidade versionada para shaders/root signature/estado/layout do pipeline e
encerrar/juntar as threads antes de liberar recursos. O worker periódico do
donor não deve ser copiado com seu loop infinito sem um ciclo de desligamento
próprio. Cache inválido ou incompatível com driver deve permitir recriação.

Esses caches dependem de dados já produzidos em execuções anteriores. Não
eliminam a primeira compilação de um shader/material realmente novo. Precisam
de A/B com cache vazio/preenchido e medição de tempos de quadro; os números
do donor não são ganho demonstrado no DeS. XXH3 e cache de registradores SPU
já foram adaptados. O experimento local de filtrar texturas não usadas ficou
desligado por falta de ganho consistente de FPS/CPU/p99.

Na conferência de 24/10, somente a análise foi atualizada; os dois caches
ainda não estavam incorporados. A implementação seguinte, de 25/10, está
registrada no início deste documento e em PERFORMANCE.md.

## Viabilidade aprofundada no DeS (24/10) — plano anterior à implementação

Sonda nativa separada, usando `D3D12CreateDevice(NULL, FL11_0)` como o nosso
renderizador: **AMD Radeon RX 7600**, `D3D12_FEATURE_SHADER_CACHE=0x7F`,
incluindo `D3D12_SHADER_CACHE_SUPPORT_LIBRARY`. Query de `ID3D12Device1`,
criação de biblioteca vazia, serialização de 872 bytes e recarga retornaram
`S_OK`. Isso confirma suporte básico da API nesta máquina; não testa um PSO
real persistido, latência de pipelines do jogo ou ganho de FPS.
Sonda/fontes/resultados em `work/cache_analysis_20261024`.

Inventário somente de leitura dos caches atuais, incluindo cabeçalho/HLSL/DXBC:

| Cache | Arquivos | Total | HLSL | Bytecode |
| --- | ---: | ---: | ---: | ---: |
| Usado nos testes de desempenho | 330 | 4.466.548 B (4,26 MiB) | 2.774.052 B | 1.681.936 B |
| Normal do launcher (`work/scratch/rsx_shader_cache_v2`) | 888 | 15.279.199 B (14,57 MiB) | 9.810.727 B | 5.440.056 B |

O volume torna um primeiro experimento com teto de 64 MiB razoável; esse teto
é uma proposta, não configuração implementada nem estimativa de toda a RAM
do renderizador. Haverá também índice, blobs quentes e alocações temporárias.
Quantidade pequena de bytes não exclui pausas por abertura de muitos arquivos,
validação, agendamento, cópias ou comportamento do driver.

### Onde adaptar

1. **Bytecode primeiro:** consultar um índice pré-carregado antes de
   `shader_disk_cache_load`, mantendo `shader_blob_cache_find` como primeiro
   nível. O preload deve usar armazenamento próprio; não escrever diretamente
   nas tabelas `g.vs_blobs/g.ps_blobs`, hoje sem sincronização. Publicar entradas
   completas sob lock, respeitar `RSX_SHADER_CACHE_DIR`/desativação de cache e
   preservar o fallback atual para faltas, limite de memória e erro de I/O.
2. **Identidade completa:** o nosso formato v2 confere stage, tamanho, hash e
   os bytes completos do HLSL. O pack do donor usa hash/target e não é
   intercambiável com nossos `.bin`. Empacotar/validar nosso formato; não
   substituir a comparação de origem por igualdade de hash.
3. **Pipeline depois:** em `build_pso`, com bytecodes e descrição D3D12
   completos, tentar a biblioteca antes de `CreateGraphicsPipelineState`.
   Conservar o cache PSO em RAM. A chave persistente precisa incluir versão,
   shaders, root signature serializada e campos de estado/layout/formatos,
   sem ponteiros ou padding como identidade. Falha de load deve criar o PSO
   normal; falha de store/save não pode impedir o desenho.
4. **Encerramento:** sinalizar e juntar preload/writer antes de liberar blobs,
   biblioteca, root signature ou device em `rsx_live_draw_shutdown`. Um único
   escritor deve serializar as gravações, com arquivo temporário e troca
   atômica; rearmar o estado dirty em qualquer falha, inclusive alocação.

No donor, o pack permanece alocado até o fim do processo e os blobs consultados
são copiados. Uma cópia direta acrescentaria duplicação de RAM ao nosso cache.
O writer da biblioteca também roda indefinidamente e compartilha o nome de
arquivo temporário com o save de saída. Esses detalhes exigem ciclo de vida
próprio e exclusão entre saves na adaptação.

A [documentação de CreatePipelineLibrary](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device1-createpipelinelibrary)
confirma: consultar suporte, manter o buffer serializado vivo durante a vida
da biblioteca, aceitar invalidação por driver/adaptador e sincronizar loads
simultâneos do mesmo PSO. Hoje nossa criação ocorre em uma thread; eventual
pré-aquecimento paralelo precisa evitar duplicar esse load. Já
[LoadGraphicsPipeline](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12pipelinelibrary-loadgraphicspipeline)
valida a descrição e pode rejeitar nomes/estados incompatíveis.

### Critério de teste antes de habilitar

- Exercitar cache vazio e cache já preenchido em processos separados. Usar
  quatro configurações no mesmo executável: ambos desligados, só preload,
  só biblioteca e ambos ligados, com snapshots iniciais iguais e diretórios
  de cache/saves isolados. Alternar a ordem das execuções.
- Medir abertura/leitura/validação/cópia separadamente, hits/faltas/bytes de
  preload e tempo de load/create de PSO. O profiler atual agrupa essas etapas;
  os 39–47 ms do segmento PS não podem ser atribuídos inteiramente ao disco.
- Comparar p95/p99, quadros acima de 33/50 ms e o primeiro giro de câmera,
  além de FPS/CPU em cenário limitado pela CPU. Não contar o custo apenas
  transferido para o boot como redução do tempo total de carregamento.
- Validar arquivos truncados/incompatíveis, limite de RAM, shader alterado,
  criação/encerramento repetidos e imagens equivalentes; conferir áudio e
  erros D3D12. Primeira criação de material sem histórico continuará exigindo
  compilação/criação normal.

Conclusão de viabilidade: **preload/pack primeiro, biblioteca persistente em
seguida**. Ambos têm encaixe no renderizador atual sem importar o scheduler
SPURS do donor ou substituir o FIFO. Nesta análise, o jogo não foi recompilado
ou alterado; somente a sonda externa foi executada.

## Prioridades da análise inicial

| Adaptação | Por que se aplica ao DeS | Esforço relativo |
| --- | --- | --- |
| XXH3 no hash completo das malhas | Nosso `geom_key` ainda lê e mistura os bytes com hashes multiplicativos; essa etapa permanece no caminho de cada draw, inclusive em hits | Pequeno |
| Cache de registradores SPU + helpers vetoriais | O job SPURS quente ainda ocupa um núcleo; os fontes gerados acessam `ctx->gpr` continuamente | Médio |
| Hash/conversão de vértices em worker | A thread do FIFO ainda faz esse trabalho e prepara os draws; tirá-lo dela pode adiantar os fences aguardados por HighGraphics | Alto |
| Gravação D3D12 separada do FIFO e prepass sobreposto | Nosso worker separa apresentação, mas a gravação dos desenhos continua no consumidor RSX | Alto |
| Biblioteca persistente de pipelines D3D12 | Complementa o nosso cache de shaders em disco e cache PSO em memória; pode ajudar travadas ao encontrar materiais novos | Médio |
| Cache de resolução HLE | Nosso bridge ainda resolve a origem e procura handlers repetidamente; candidato menor, dependente do perfil | Pequeno/médio |

Essas prioridades são inferências a partir dos fontes e do nosso perfil;
não representam percentuais de FPS já demonstrados.

## 1. XXH3 com cobertura integral dos dados

O donor usa `XXH3_64bits_withSeed` em `vc_hash_bytes`, preservando a leitura de
todos os bytes que compõem a chave da geometria. Fonte:
[VC_XXH](https://github.com/whompay/inFAMOUSreborn/blob/2e9405c72408d28f505b6636a2f9cb6b6125aa85/ps3recomp/libs/video/rsx_d3d12_backend.c#L13952).

No DeS, o ponto equivalente é `gk_bytes`/`geom_key` em
`third_party/ps3recomp/libs/video/rsx_live_draw.c`. Hoje há duas saídas de
64 bits; uma adaptação deve preservar uma chave de 128 bits, por exemplo
com XXH3-128, todos os descritores/índices/defaults e todos os bytes das regiões.
Mudar o hash não muda a geometria: exige invalidar as entradas anteriores,
manter checagem das duas partes da chave e testar alterações dos buffers.

É a primeira experiência que eu faria pela relação entre escopo e risco.
Nosso `fetch` ainda leva cerca de 0,6 s por janela de 5 s na cena pesada;
essa categoria inclui outras etapas, portanto não se pode atribuir todo esse
tempo ao hash nem tratar uma aceleração dele como igual ganho de FPS.

## 2. Registradores SPU em variáveis locais

O passo
[`spu_regcache.py`](https://github.com/whompay/inFAMOUSreborn/blob/2e9405c72408d28f505b6636a2f9cb6b6125aa85/ps3recomp/tools/spu_regcache.py)
gera uma segunda versão das funções SPU usando variáveis locais para GPRs.
Isso reduz recargas causadas pela possibilidade de a memória local do SPU
compartilhar endereços com o contexto na análise de alias do compilador.
Antes de chamadas/canais/retornos, grava os registradores alterados no contexto;
depois recarrega os necessários. `spu_helpers_vec.h` acrescenta operações
vetoriais sobre esses valores. O runtime escolhe a versão por tabela de pares,
com a original disponível para A/B.

Na versão avaliada inicialmente, nosso build não tinha esse gerador, header
vetorial ou seleção de versões.
Os arquivos gerados têm formato compatível com a ideia, incluindo o job
`spu_f5cc...` que apareceu no perfil. A adaptação precisa abranger CMake,
geração reproduzível, registro das funções e lookup/overlays. Preservar a
visibilidade de `ctx->gpr` nas chamadas, interrupções e trocas de imagem é
essencial. Começaria com um módulo quente, comparando GPRs, LS e efeitos das
chamadas, antes de habilitar em todos os SPUs. Não basta definir
`SPU_REGCACHE=1` no ambiente atual.

## 3. Preparação assíncrona de vértices

O donor tem
[`VTX_ASYNC`](https://github.com/whompay/inFAMOUSreborn/blob/2e9405c72408d28f505b6636a2f9cb6b6125aa85/ps3recomp/libs/video/rsx_d3d12_backend.c#L12620)
para conversões em blocos e
[`VC_ASYNC`](https://github.com/whompay/inFAMOUSreborn/blob/2e9405c72408d28f505b6636a2f9cb6b6125aa85/ps3recomp/libs/video/rsx_d3d12_backend.c#L14569)
para hash, lookup e conversão dos misses do cache. Ele drena os jobs antes de
labels, semáforos, SET_REFERENCE, blits e outros pontos que permitem ao jogo
observar progresso ou reutilizar memória. Também ajusta os workers ociosos
para não gastar CPU excessiva em spin.

Nosso `geom_key`, lookup e conversão ainda são síncronos. Um primeiro worker
poderia processar jobs com descritores/defaults/índices imutáveis e regiões de
memória protegidas até a drenagem. Adicionar threads sem essas fronteiras
poderia ler dados que o jogo já reutilizou.

Há um detalhe do donor que precisa de tratamento diferente aqui: o caminho
assíncrono escreve índices degenerados se a área temporária/cache fica cheia,
omitindo o batch. A adaptação para DeS deve recuar ao caminho síncrono e
preservar todos os desenhos.

## 4. Separar gravação dos desenhos e sobrepor o prepass

[`RF_ASYNC`](https://github.com/whompay/inFAMOUSreborn/blob/2e9405c72408d28f505b6636a2f9cb6b6125aa85/ps3recomp/libs/video/rsx_d3d12_backend.c#L11516)
permite ao FIFO registrar o próximo quadro enquanto outra thread reproduz os
draws D3D12. Isso exige snapshots dos desenhos, constantes, índices, vértices
e recursos por quadro. É mais amplo que o nosso `framegen_present`, que já
separa Present e a cópia do quadro real, mas mantém a gravação dos desenhos
no consumidor do FIFO.

[`RF_EARLY`](https://github.com/whompay/inFAMOUSreborn/blob/2e9405c72408d28f505b6636a2f9cb6b6125aa85/ps3recomp/libs/video/rsx_d3d12_backend.c#L9322)
prepara descritores, uploads e pipelines antes da espera pela GPU anterior,
usando áreas alternadas e liberação adiada dos recursos. O commit consultado
habilita `RF_TRIPLE=1` e `RF_EARLY=1` no launcher. A implementação já existia;
o próprio fonte registra que triple buffering isolado não demonstrou ganho
na experiência antiga deles. Nossos dois allocators e rings assíncronos já
oferecem parte da sobreposição CPU/GPU, mas não equivalem a esse prepass.

Essa é a mudança estrutural mais interessante depois dos passos menores.
Exige preservar os fences visíveis ao jogo, a vida dos recursos e o atendimento
das mensagens da janela durante esperas. Copiar as duas variáveis do launcher
não habilita a arquitetura no nosso backend.

## Outros pontos úteis e limites

- [PSO_LIB](https://github.com/whompay/inFAMOUSreborn/blob/2e9405c72408d28f505b6636a2f9cb6b6125aa85/ps3recomp/libs/video/rsx_d3d12_backend.c#L3672)
  usa `ID3D12PipelineLibrary` persistente. O nosso cache de shaders já evita
  parte do trabalho; a biblioteca acrescentaria persistência do pipeline do
  driver. Medir em visitas a materiais novos e nos piores tempos de quadro,
  além de FPS médio com cache aquecido.
- O [cache HLE](https://github.com/whompay/inFAMOUSreborn/blob/2e9405c72408d28f505b6636a2f9cb6b6125aa85/ps3recomp/runtime/ppu/ppu_hle.cpp#L198)
  evita resolução repetida de origem, varredura de handlers de contexto e
  lookup de NID. Nosso `ppu_hle.cpp` ainda possui esses caminhos. A adaptação
  deve conservar precedência dos exports do firmware e invalidar misses quando
  os registros crescem; só priorizar se aparecer no perfil.
- [FSR temporal/DLSS](https://github.com/whompay/inFAMOUSreborn/blob/2e9405c72408d28f505b6636a2f9cb6b6125aa85/ps3recomp/libs/video/rsx_dlss.cpp#L1)
  já têm integração de SDK com profundidade, jitter e vetores reconstruídos
  pela câmera. É uma referência mais completa que o nosso FSR 1 espacial e FG
  por blocos, mas precisa adaptar câmera, profundidade e HUD ao DeS. Vetores
  reconstruídos por câmera não são automaticamente os vetores corretos dos
  personagens animados. Não é a primeira resposta para nosso limite de CPU.
- `VC_FASTKEY` permite usar uma chave completa antiga entre sondagens parciais,
  podendo exibir geometria antiga por alguns quadros; não usaria como padrão.
  IDs de workloads SPURS, substituições de funções do jogo e ajustes LOD do
  donor também dependem do inFamous. Nosso port executa o kernel SPURS real e
  precisa conservar esse comportamento.
- AVX2, musttail, cache de geometria, arena persistente, cache de shaders e
  apresentação assíncrona já existem no DeS em alguma forma. Comparar as
  diferenças concretas, em vez de contabilizá-los novamente como ganhos novos.

## Validação proposta para qualquer adaptação

Manter um controle A/B por recurso no mesmo executável. Usar a mesma cópia
de save, câmera, resolução e cache; medir as janelas de gameplay e tempos por
draw, além de CPU por quadro. Para vértices/SPU, conferir também saída e
fronteiras de sincronização; para pipelines, medir travadas com cache frio.
Os percentuais citados nos comentários do donor pertencem aos testes dele,
não constituem resultados no Demon's Souls.
