# GuitarPad

Toque guitarra, aperte teclas. O GuitarPad escuta a guitarra pela interface de áudio, detecta a nota tocada e simula teclas de teclado para controlar jogos de PC.

## Download

Cada push no `main` gera os executáveis pelo GitHub Actions:

1. Abra a aba **Actions** do repositório e entre no último build verde.
2. Em **Artifacts**, baixe `GuitarPad-Windows` (ou `GuitarPad-macOS`).
3. Descompacte e rode o `GuitarPad.exe`.

Para gerar uma release com link fixo, crie uma tag `v0.1.0` (ou outra `v*`) e faça push dela.

O app não é assinado, então o Windows pode mostrar o aviso do SmartScreen. Clique em "Mais informações" e "Executar assim mesmo".

## Primeiro uso

1. **Áudio...**: escolha a interface e ative **só a entrada onde a guitarra está**. No Windows, prefira o tipo "Windows Audio (Exclusive Mode)" com buffer pequeno (128 ou 256 amostras) para ter menos atraso.
2. **Sensibilidade (gate)**: com a guitarra parada, a barra de nível deve ficar à esquerda da linha laranja. Toque uma nota: a barra passa da linha e o nome da nota aparece. Suba o gate se o ruído dispara teclas; desça se notas fracas não disparam.
3. **Mapeamento**: em cada linha, clique em **Aprender** e toque a nota na guitarra. Depois escolha a tecla e o modo.
4. Abra o jogo e toque.

O app salva tudo sozinho. Os presets (botões Salvar/Abrir preset) servem para ter um mapeamento por jogo.

## Modos

| Modo | O que faz | Bom para |
|---|---|---|
| Toque | aperta e solta rápido | pular, atacar, confirmar |
| Sustentação | segura a tecla enquanto a nota soa | andar, correr, mirar |
| Trava | 1º toque aperta, 2º toque solta | andar sem precisar manter a nota soando |

**Sustentação global**: o botão grande liga e desliga. Desligada, todas as notas viram Toque e qualquer tecla segurada é solta na hora.

Três jeitos de ligar/desligar a sustentação sem largar a guitarra:

- **F12** no teclado, funciona com o jogo em foco
- **Nota de comando**: uma nota escolhida (padrão C6, corda 1 casa 20) que só alterna o modo e não manda tecla nenhuma
- o botão na tela

**Soltar tudo** libera qualquer tecla presa. **Ativo** desmarcado pausa a saída (o app continua escutando).

## Ajustes finos

- **Duração do toque**: 30 a 60 ms costuma funcionar em jogos a 60 fps. Se o jogo "perde" pulos, aumente.
- **Modo rápido**: confirma a nota com uma leitura em vez de duas. Ganha uns 5 ms, com um pouco mais de risco de ler a nota errada no ataque.
- Notas repetidas rápidas na mesma tecla de Toque: o app solta a tecla e espera 20 ms antes de apertar de novo, para o jogo enxergar os dois apertos.

## O que esperar

- **Monofônico**: uma nota por vez. Acordes não são reconhecidos como acordes.
- **Atraso de detecção**: nos testes automáticos com cordas sintéticas, de 20 a 45 ms entre a palhetada e a tecla, maior nas notas graves (precisam de mais tempo para revelar a altura). Com guitarra real, vale medir.
- Faixa de notas: D2 (drop D) até F#6.
- Se um jogo roda como administrador, o GuitarPad também precisa rodar como administrador para as teclas chegarem.
- Jogos com anti-cheat agressivo podem ignorar teclas simuladas.
- No macOS, o app pede permissão de Acessibilidade (Ajustes do Sistema > Privacidade e Segurança > Acessibilidade).

## Código

```
Source/Core/NoteDetector     detecção: filtro, decimação, gate, ataque, YIN
Source/Core/MappingEngine    nota -> tecla, modos, sustentação global, thread de saída
Source/Core/Keys.h           tabela de teclas (scancodes Windows, keycodes macOS)
Source/Platform/KeySender*   saída de teclado por sistema
Source/App/                  interface JUCE
tests/                       testes do detector e do mapeamento (rodam no CI)
```

O núcleo não depende do JUCE, então dá para trocar a saída depois (por exemplo, gamepad virtual) sem mexer na detecção.

### Compilar localmente

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release
```

O JUCE é baixado automaticamente pelo CMake. Para compilar só o núcleo e os testes: `-DGUITARPAD_BUILD_APP=OFF`.

## Próximos passos possíveis

- Saída como gamepad virtual, para várias guitarras virarem controles separados no multiplayer local
- Pedal (MIDI ou USB) para ligar/desligar a sustentação
- Mapear por faixa de casas ou por corda, não só por nota exata
- Bend e volume como eixos analógicos
