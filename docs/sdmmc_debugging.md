# SDMMC Debugging Journal / SDMMC 디버깅 일지

## Problem / 문제

On the STM32F769I-DISC1 board, `sd init` fails during SD card initialization.
The command path reaches CMD0 successfully, but CMD8 (and the fallback CMD55/ACMD41)
returns a command timeout (`CTIMEOUT`). The card is detected (`PG2/CD` is low),
but the SDMMC peripheral never receives a response.

STM32F769I-DISC1 보드에서 `sd init` 명령이 SD 카드 초기화 도중 실패합니다.
CMD0은 정상적으로 전송되지만, CMD8(그리고 폭백 CMD55/ACMD41)에서 명령 타임아웃
(`CTIMEOUT`)이 발생합니다. 카드는 감지되지만(`PG2/CD`가 low), SDMMC 주변장치가
응답을 받지 못합니다.

## Environment / 환경

- Board: STM32F769I-DISC1
- MCU: STM32F769NI (Cortex-M7)
- Clock source: HSI 16 MHz, SYSCLK = 16 MHz
- SDMMC1 clock source: SYSCLK (`RCC_DCKCFGR2` bit 28 = 1)
- SDMMC1 bus: APB2 (`RCC_APB2ENR` bit 11)
- SDMMC1 base address: `0x40012C00`
- Card detect: PG2, active low (requires external/internal pull-up)
- Pin mapping:
  - PC8  -> D0  (AF12)
  - PC9  -> D1  (AF12)
  - PC10 -> D2  (AF12)
  - PC11 -> D3  (AF12)
  - PC12 -> CK  (AF12)
  - PD2  -> CMD (AF12)
  - PG2  -> CD  (input, active low)

## Fixes Applied / 적용한 수정

### 1. PG2 card-detect glitch / PG2 카드 감지 글리치

`configure_sd_pins()` was re-initializing PG2, which momentarily changed its
state and made `card_present()` read high even though a card was inserted.
PG2 configuration is now skipped in `configure_sd_pins()`; the user must
configure it beforehand, for example with `pin pullup PG2`.

`configure_sd_pins()`가 PG2를 재초기화하면서 핀 상태가 순간적으로 바뀌어
카드가 삽입되어 있음에도 `card_present()`가 high를 읽는 문제가 있었습니다.
이제 `configure_sd_pins()`에서는 PG2 설정을 건드리지 않으며, 사용자는 미리
`pin pullup PG2` 등으로 PG2를 입력 풀업으로 설정해야 합니다.

### 2. Wrong peripheral clock register / 주변장치 클럭 레지스터 오류

SDMMC1 is on the APB2 bus, but the driver was enabling bit 6 of `RCC_AHB2ENR`.
Fixed to use `RCC_APB2ENR` bit 11 (`SDMMC1EN`).

SDMMC1은 APB2 버스에 연결되어 있으나, 드라이버가 `RCC_AHB2ENR`의 bit 6을
켜고 있었습니다. `RCC_APB2ENR` bit 11 (`SDMMC1EN`)으로 수정했습니다.

### 3. Wrong `RCC_DCKCFGR2` offset / `RCC_DCKCFGR2` 오프셋 오류

The clock-source selection register offset was wrong (`0x94`).
Corrected to `0x90`.

클록 소스 선택 레지스터 오프셋이 `0x94`로 잘못되어 있었습니다. `0x90`으로
수정했습니다.

### 4. Incomplete interrupt-clear mask / 인터럽트 클리어 마스크 누락

`SDMMC_ICR_STATIC_MASK` did not include `CMDSENTC` (bit 7), so the `CMDSENT`
flag could never be cleared. Updated the mask to `0x00C007FF`.

`SDMMC_ICR_STATIC_MASK`에 `CMDSENTC`(bit 7)가 포함되지 않아 `CMDSENT` 플래그를
지울 수 없었습니다. 마스크를 `0x00C007FF`로 수정했습니다.

### 5. SDMMC reset pulse / SDMMC 리셋 펄스

Added a pulse on `RCC_APB2RSTR` bit 11 before initialization to ensure the
peripheral starts from a clean state.

초기화 전에 `RCC_APB2RSTR` bit 11로 리셋 펄스를 추가하여 주변장치가 깨끗한
상태에서 시작하도록 했습니다.

### 6. CMD8 retry and ACMD41 fallback / CMD8 재시도 및 ACMD41 폭백

CMD8 is mandatory only for SD v2.0+. Added three retries, and if CMD8 still
fails the driver continues with ACMD41 to support older cards.

CMD8는 SD v2.0 이상에서만 필수입니다. 3회 재시도를 추가했고, 그래도 실패하면
구형 카드를 지원하기 위해 ACMD41로 넘어갑니다.

### 7. Correct identification clock / 식별 단계 클럭 수정

The STM32F7 SDMMC clock formula is:

```
f_SDMMC_CK = f_SDMMCCLK / (CLKDIV + 2)
```

With `f_SDMMCCLK = 16 MHz`, `CLKDIV = 158` gives exactly 100 kHz for the
identification phase.

STM32F7의 SDMMC 클럭 공식은 `f_SDMMC_CK = f_SDMMCCLK / (CLKDIV + 2)`입니다.
`f_SDMMCCLK = 16 MHz`일 때 `CLKDIV = 158`이면 식별 단계에 딱 100 kHz가
됩니다.

### 8. Power-on sequence / 전원 켜기 시퀀스

The clock is now enabled **before** the card is powered on, and the driver
transitions `SDMMC_POWER` explicitly from power-off (`0`) to power-on (`3`).
This matches the reference manual and gives the card the required 74+
initialization clocks during wake-up.

이제 카드에 전원을 켜기 **전에** 클럭을 먼저 활성화하고, `SDMMC_POWER`를
전원 off(`0`)에서 on(`3`)으로 명시적으로 전환합니다. 레퍼런스 매뉴얼에 맞춘
것이며, 카드가 깨어나는 동안 필수적인 74개 이상의 초기화 클럭을 제공합니다.

## Current Status / 현재 상태

After all the above changes, `sd init` still fails:

- `CMD0` is sent successfully (`CMDSENT` flag set).
- `CMD8` times out (`CTIMEOUT`) on every retry.
- `CMD55` (sent before `ACMD41`) also times out.
- `PG2/CD` stays low, the peripheral registers look correct, and the same
  hardware/SD card works under Zephyr.

위의 모든 수정 후에도 `sd init`은 여전히 실패합니다:

- `CMD0`은 정상 전송됩니다(`CMDSENT` 플래그 설정).
- `CMD8`이 모든 재시도에서 타임아웃(`CTIMEOUT`)됩니다.
- `ACMD41` 전에 본 `CMD55`도 타임아웃됩니다.
- `PG2/CD`는 low를 유지하고, 주변장치 레지스터는 정상으로 보이며, 동일한
  하드웨어/SD 카드는 Zephyr에서는 동작합니다.

Typical register state at failure / 실패 시의 대표 레지스터 상태:

```text
PG2/CD pin:      low
card_present():  1
SDMMC POWER:     0x00000003
SDMMC CLKCR:     0x0000019E   (CLKDIV=158, CLKEN=1)
SDMMC STA:       0x00000000   (after flags are cleared)
RCC APB2ENR:     0x00000810   (SDMMC1EN set)
RCC DCKCFGR2:    0x10000000   (SDMMC1SEL = SYSCLK)
GPIOC MODER:     0x02AA0000   (PC8-PC11 AF, PC12 AF)
GPIOC AFRH:      0x000CCCCC   (AF12 on PC8-PC12)
GPIOC PUPDR:     0x00550000   (pull-up on PC8-PC11)
GPIOD MODER:     0x00000020   (PD2 AF)
GPIOD AFRL:      0x00000C00   (AF12 on PD2)
```

## Open Leads / 추가 확인 사항

Because CMD0 succeeds but CMD8/CMD55 do not, the host can drive the CMD line
but the card does not (or cannot) respond. Possible remaining causes:

CMD0은 성공하고 CMD8/CMD55는 실패하므로, 호스트는 CMD 라인을 구동할 수 있지만
카드가 응답하지 않거나 응답할 수 없는 상황입니다. 남은 가능한 원인:

1. **Clock source frequency / 클럭 소스 주파수**  
   Zephyr runs the board with HSE + main PLL at 216 MHz and uses PLL48CLK
   (48 MHz) as the SDMMC source. saramOS currently uses HSI 16 MHz only.
   The peripheral should work at 16 MHz, but some cards may be sensitive
   to the very low `SDMMC_CK` edge rate or to the APB2 clock being only
   16 MHz.

   Zephyr는 HSE와 216 MHz PLL을 사용하고 PLL48CLK(48 MHz)를 SDMMC 소스로
   사용합니다. saramOS는 현재 HSI 16 MHz만 사용합니다. 16 MHz에서도 동작해야
   하지만, 일부 카드는 낮은 `SDMMC_CK` 상승/하강 속도나 APB2 클럭이 16 MHz인
   점에 민감할 수 있습니다.

2. **External pull-ups on CMD/D0-D3 / CMD/D0-D3의 외부 풀업**  
   The STM32F769I-DISC1 schematic should have pull-ups on CMD and D0-D3.
   Internal pull-ups are enabled in software, but if the board lacks
   external pull-ups, signal integrity during the response window can be
   marginal.

   STM32F769I-DISC1 회로도에 CMD와 D0-D3에 외부 풀업이 있어야 합니다.
   소프트웨어에서 내장 풀업을 활성화했지만, 외부 풀업이 없다면 응답
   구간에서 신호 무결성이 부족할 수 있습니다.

3. **Physical layer verification / 물리 계층 확인**  
   An oscilloscope or logic analyzer on PC12 (CK) and PD2 (CMD) would show
   whether the card is actually receiving the clock/command and whether it
   tries to drive a response.

   PC12(CK)와 PD2(CMD)에 오실로스코프나 논리 분석기를 연결하면 카드가 실제로
   클럭과 명령을 받고 응답을 구동하려 하는지 확인할 수 있습니다.

4. **Longer power-up delay / 더 긴 전원 안정화 시간**  
   SD cards can require up to ~1 s after power-up. The current delay is in
   the tens of milliseconds range; extending it to several hundred
   milliseconds or one second is worth testing.

   SD 카드는 전원 인가 후 최대 약 1초가 필요할 수 있습니다. 현재 지연은 수십
   ms 수준으로, 수백 ms에서 1초로 늘리는 것도 시도해 볼 가치가 있습니다.

5. **Signal pin output speed / 신호 핀 출력 속도**  
   The alternate-function pins are configured with `GPIO_SPEED_HIGH`. Some
   layouts may need `GPIO_SPEED_VERY_HIGH` for clean edges at the card slot.

   대체 기능 핀을 `GPIO_SPEED_HIGH`로 설정했습니다. 일부 레이아웃에서는 카드
   슬롯에서 깨끗한 에지를 얻기 위해 `GPIO_SPEED_VERY_HIGH`가 필요할 수
   있습니다.

## Reproduction Steps / 재현 방법

1. Build and flash `os/default`:

   ```bash
   make flash TARGET=stm32f769i-disc1
   ```

2. Connect to the serial console (115200 8N1).

3. Make sure PG2 has an internal pull-up:

   ```text
   saramOS> pin pullup PG2
   ```

4. Run the SD init command:

   ```text
   saramOS> sd init
   ```

5. Observe the timeout on CMD8 / CMD55.

   CMD8 / CMD55에서 타임아웃이 발생하는지 확인합니다.

## Related Files / 관련 파일

- `src/hal/stm32f769i-disc1/hal_sdmmc.c`
- `src/hal/stm32f769i-disc1/hal_gpio.c`
- `include/hal/stm32f769i-disc1.h`
- `os/default/main.c`
