# Continuar no seu PC

Tudo o que foi feito está nesta pasta, na branch `claude/amb1-jiant-fm6` do repositório
`guiatihe333/lostgear`. Nada ficou só na sessão anterior.

## 1. Pegar o trabalho

```sh
git clone -b claude/amb1-jiant-fm6 https://github.com/guiatihe333/lostgear
cd lostgear/fm1-amb-fm6
```

Abra a sessão do Claude Code nesta pasta (`lostgear/fm1-amb-fm6`) e cole o prompt da seção 4.

## 2. Onde as coisas estão

| Caminho | O que é |
| --- | --- |
| `README.md` | Tudo o que foi feito, as medições e os limites |
| `patches/0001..0004` | As mudanças sobre o AMB-1 (`charlesvestal/fm1-amb` @ `188b0a4`), para `git am` |
| `amb1-fm6.patch` | As mesmas mudanças num patch só |
| `files/` | Os arquivos já modificados, para ler sem aplicar nada |
| `instalar.sh` | Detecta o FM-1, compila e instala (Linux ou macOS) |
| `build_release.sh` | Só a compilação: toolchain JieLi, SDK AC79, testes, `.fwsc`, instalador web |
| `analysis/` | Ferramentas de medição (níveis, espectro, cliques, feedback) e o wrapper `cc-linux` |

## 3. Estado

**Feito e testado** (testes do AMB-1 todos passando, no simulador):
- Vozes do AMB-1 (pad, sinos, baixo) no motor FM6 do JIANT, com núcleo seguro para a CPU do FM-1.
- Página MOD: Depth, Rate, Chaos e Target, com 8 targets (Timbre, Ratio, Metal, Feedback, Decay, Pitch, Space, All).
- Níveis iguais ao original (até 0,3 dB).

**Falta:**
1. **FM-1 no V16 → voltar ao V15** pelo M-UPGRADE da M-VAVE (arquivo oficial `FM-1.fwsc` V15). Nenhum instalador
   foi testado no V16. O `instalar.sh` para se não encontrar o V15.
2. **Compilar o `.fwsc`**: nunca foi feito, porque a sessão na nuvem não alcançava `pkgman.jieliapp.com` nem
   `gitee.com`. No seu PC deve funcionar. O toolchain é Linux x86-64: no Mac roda via Docker, no Windows via WSL.
3. **Instalar e ouvir.** Ajustes de timbre ficam em `fm_patches()`, `bell_patches()` e `mod_of()`, em
   `app/dsp/amb.c`.

## 4. Prompt para a sessão nova

> Estou na pasta `fm1-amb-fm6` da branch `claude/amb1-jiant-fm6` do repositório `guiatihe333/lostgear`. É um fork do
> firmware AMB-1 (charlesvestal/fm1-amb @ 188b0a4) para o M-VAVE FM-1. As vozes usam o motor FM6 do JIANT e há uma
> página nova de modulação generativa (MOD). Leia `CONTINUAR.md` e `README.md`. O FM-1 está ligado no USB deste
> computador e roda o firmware de fábrica V16 da M-VAVE. Quero:
> 1. verificar se o FM-1 é encontrado (`instalar.sh`, passo 2, ou `fm1_install.py --info`) e me mostrar a
>    identidade que ele reporta;
> 2. me guiar na volta ao V15 se for preciso;
> 3. compilar o pacote com `build_release.sh` e rodar os testes;
> 4. instalar, pedindo minha confirmação antes de gravar.
> Não grave nada no FM-1 sem me perguntar.
