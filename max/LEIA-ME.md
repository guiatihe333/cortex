# AMB-1 FM6 breaks (Max/MSP)

Um patch de Max para tocar junto com o FM-1 rodando o AMB-1 FM6. Ele recebe o **clock e as notas** do FM-1 pelo MIDI
USB e o **som** do FM-1 pelo áudio USB. Com isso, faz uma **batida rápida de breakbeat**, no espírito drill'n'bass,
em sincronia com o jardim, enquanto os pads lentos do FM-1 passam por cima.

Arquivos (deixe os dois na mesma pasta):
- `amb1-fm6-breaks.maxpat`: o patch;
- `breaks.js`: a lógica da batida (o objeto `[js breaks.js]` do patch).

`make_patch.py` gera o patch de novo, se você quiser mudar algo pelo código.

## Ligar

1. **No FM-1:** página **Setup**, **MIDI OUT On**. Aperte **PLAY**.
2. **No Max (8 ou mais novo):** abra `amb1-fm6-breaks.maxpat`.
3. **Áudio:** *Options › Audio Status*:
   - **Input device:** o FM-1 (o áudio USB "AMB-1 FM-1");
   - **Output device:** suas caixas ou fones.

   No Windows, use o driver **MME** ou **DirectSound**, que deixam escolher entrada e saída diferentes; com ASIO,
   use o ASIO4ALL. No Mac, se precisar, crie um "Aggregate Device" com o FM-1 e a saída no *Configuração de Áudio e
   MIDI*.
4. **MIDI:** no menu do canto de cima, à esquerda, escolha a porta **AMB-1 FM-1**.
5. **Ligue o áudio:** clique no alto-falante, embaixo à esquerda.

A batida começa quando o FM-1 manda **start**: quando você aperta PLAY nele, ou ele próprio começa a tocar. Ela para
no **stop**. Sem o FM-1, ligue o **clock interno** (o toggle, com o BPM ao lado).

## O que faz

- **Sincronia:** conta os 24 pulsos por tempo do clock do FM-1. Mudar o Tempo no FM-1 muda a batida junto.
- **Velocidade** (`speed`): **2** é o dobro do tempo do FM-1 (com o jardim a 72, a batida fica a 144). **3** é o
  triplo; **1** é o mesmo tempo. É o contraste dos pads lentos com a batida acelerada.
- **A batida:** cada compasso escolhe um padrão de break clássico e o modifica. Além disso:
  - caixas fantasmas;
  - **ratchets**, rolos de 2 a 8 golpes dentro de um passo, acelerando ou não;
  - uma **corrida de caixa** no fim de cada quarto compasso;
  - de vez em quando, meio compasso que some.
- **Os sons:** bumbo com queda de altura, caixa (ruído e corpo), chimbal e **sub-baixo**. Tudo é feito com objetos
  do próprio MSP, sem samples. O sub toca uma oitava abaixo da última nota da voz de baixo do FM-1 (`bassch`: o canal
  MIDI dela, 3 para a Voz 3). Os sons ainda passam por **bits** (o degrade~) e drive, para dar sujeira.
- **O som do FM-1:**
  - os pads passam limpos (`pads`), mais uma **lavagem** escura de delay (`wash`);
  - os últimos 4 s ficam guardados num buffer, e de vez em quando um **stutter** repete um pedacinho deles no tempo
    da batida (`stutter`).

## Controles

| Controle | O que faz |
| --- | --- |
| density | quantos golpes: de esparso (0) a cheio (1) |
| ratchet | quantos rolos e quão longos |
| stutter | quantos stutters do som do FM-1 |
| swing | o balanço das semicolcheias pares (0 a 0,5) |
| speed | 1, 2 ou 3 vezes o tempo do FM-1 |
| bassch | o canal MIDI da voz de baixo do FM-1 (1, 2 ou 3) |
| bits | a resolução da batida: 12 limpa, 6 suja |
| drums, pads, wash | os volumes |

A linha **state** mostra se está rodando, o compasso, a duração de um passo e o BPM que chega do FM-1.

## Se não funcionar

- **Sem batida:**
  - confira o MIDI OUT On no FM-1 e a porta escolhida no Max;
  - o state deve mostrar "running" depois do PLAY;
  - feche outros programas que usem o MIDI do FM-1.
- **Sem o som do FM-1:** confira o *Input device* no Audio Status. O FM-1 aparece como dispositivo de áudio USB.
- **O tempo oscila:** o clock MIDI pelo USB tem um pouco de variação. A batida segue a média, então é normal ela levar
  um instante para firmar depois de uma mudança de tempo.
