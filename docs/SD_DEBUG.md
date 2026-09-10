# STM32F769I-DISC1 SD/MMC Debug Guide

This guide walks through debugging an SD card mount failure on the STM32F769I-DISC1 using the `pin`, `inp`, `sd inspect`, and `sd init` commands built into the saramOS example.

## Quick diagnostic flow

Run these commands in order and compare the output with the expected values below.

```text
saramOS> sd inspect
saramOS> pin pullup PG2
saramOS> pin read PG2 10 ascii
saramOS> sd info
```

If `PG2/CD` reads as `high` while a card is inserted, the hardware detection path is broken. Fix that before continuing.

If `PG2/CD` reads as `low`, continue with the SD initialization steps below.

---

## 1. Verify card detection (PG2)

PG2 is the card-detect input, **active low**.

```text
saramOS> pin pullup PG2
saramOS> pin read PG2 10 ascii
```

Expected:
- Card inserted: `0000000000` (PG2 pulled low by the slot switch)
- No card:       `1111111111` (PG2 pulled up externally/internally)

If you always see `1`s with a card inserted:
- Check that PG2 is connected to the SD slot CD switch.
- Check whether the slot switch pulls to GND when the card is inserted.
- Try `pin pulldown PG2` and `pin inp PG2` to see if the pin is floating.

If `sd info` reports `no card detected` but `pin read PG2` shows `0`, the `hal_sdmmc_card_present()` logic may be using the wrong polarity. Inspect the code and the board schematic.

---

## 2. Verify SDMMC clock and power

```text
saramOS> sd inspect
```

Check these values:

```text
SDMMC POWER:     0x00000003    # PWRCTRL = 11 (power on)
SDMMC CLKCR:     0x00008114    # CLKEN + ~400 kHz divider (depends on input clock)
SDMMC STA:       0x00000000 or static flags
```

If `POWER` is `0x00`, the SDMMC peripheral is not powered on.
If `CLKCR` is `0x00`, the clock is not enabled.

Use direct register writes to test:

```text
# These are not CLI commands; write a small test function or use a debugger
RCC_AHB2ENR |= RCC_AHB2ENR_SDMMC1EN;
SDMMC1->POWER = (SDMMC_POWER_PWRCTRL_ON << SDMMC_POWER_PWRCTRL_Pos);
SDMMC1->CLKCR = SDMMC_CLKCR_CLKEN | (20U << 0); /* 400 kHz example */
```

---

## 3. Test each SD signal line

Use the `pin` CLI to reconfigure each SDMMC line temporarily as a GPIO input/output and verify electrical connectivity.

### Pull-up presence test

For each data line (PC8..PC11) and CMD (PD2):

```text
saramOS> pin pullup PC8
saramOS> pin read PC8 10 ascii
```

Expected: `1111111111` (pulled high)

Then ground the pin externally (with a jumper or scope probe) and repeat:

```text
saramOS> pin read PC8 10 ascii
```

Expected: `0000000000`

If the pin does not go low when grounded, the pull-up resistor is too strong or the pin is not connected.

### Output drive test

```text
saramOS> pin out PC8
saramOS> pin write PC8 1
saramOS> pin read PC8 1 ascii    # should be 1
saramOS> pin write PC8 0
saramOS> pin read PC8 1 ascii    # should be 0
```

Repeat for PC9, PC10, PC11, PD2.

### Clock output test

```text
saramOS> pin out PC12
saramOS> pin write PC12 1
saramOS> pin read PC12 1 ascii   # should be 1
saramOS> pin write PC12 0
saramOS> pin read PC12 1 ascii   # should be 0
```

Use an oscilloscope or logic analyzer to verify the SDMMC clock is toggling when SD initialization runs.

---

## 4. Step through SD initialization

The SD initialization sequence is:

```text
CMD0   -> GO_IDLE_STATE        (no response)
CMD8   -> SEND_IF_COND         (R7)
ACMD41 -> SD_SEND_OP_COND      (R3)
CMD2   -> ALL_SEND_CID          (R2)
CMD3   -> SEND_RELATIVE_ADDR    (R6)
CMD9   -> SEND_CSD              (R2)
CMD7   -> SELECT_CARD           (R1b)
ACMD6  -> SET_BUS_WIDTH         (R1)
CMD16  -> SET_BLOCKLEN          (R1)
```

Add temporary debug prints to `hal_sdmmc_init()` after each command and rebuild:

```c
rc = _sdmmc_send_cmd(0, 0, 0, NULL);
snprintf(buf, sizeof(buf), "CMD0 rc=%d\r\n", rc);
hal_uart_puts(buf);
if (rc != 0) return HAL_SDMMC_ERR;
```

Do this for CMD0, CMD8, ACMD41, CMD2, CMD3, CMD9, CMD7, ACMD6, CMD16.

Expected results:

| Step | Expected `rc` | Failure cause |
|------|---------------|---------------|
| CMD0 | 0 | Clock/power missing, CMD line stuck, no pull-up |
| CMD8 | 0 | Card not SDHC/SDXC, voltage mismatch |
| ACMD41 | 0, OCR bit31=1 | Card not powered, voltage window wrong |
| CMD2 | 0 | Card left idle state incorrectly |
| CMD3 | 0 | RCA read failed |
| CMD9 | 0 | Card not selected or CSD corrupt |
| CMD7 | 0 | Card not responding in transfer state |
| ACMD6 | 0 | 4-bit bus negotiation failed |
| CMD16 | 0 | Block length not accepted |

---

## 5. Inspect SDMMC status flags

After each failing command, print `SDMMC1->STA` before clearing it:

```c
snprintf(buf, sizeof(buf), "STA=0x%08lX\r\n", (unsigned long)SDMMC1->STA);
hal_uart_puts(buf);
sdmmc_clear_static_flags();
```

Interpretation:

| Flag | Meaning |
|------|---------|
| `SDMMC_STA_CTIMEOUT` | Card did not respond |
| `SDMMC_STA_CCRCFAIL` | Response CRC failed |
| `SDMMC_STA_CMDREND` | Response received OK |
| `SDMMC_STA_CMDSENT` | Command sent OK (no-response commands) |

---

## 6. CMD0 common pitfall

CMD0 has **no response**. The firmware must wait for `CMDSENT`, not `CMDREND`.

If you see `CMD0 rc=-2` (timeout), verify that `sdmmc_wait_cmd()` checks both flags:

```c
if (sta & SDMMC_STA_CMDREND) return 0;
if (sta & SDMMC_STA_CMDSENT) return 0;
```

---

## 7. Verify SD card addressing

After successful initialization, `sd_capacity_blocks` and `sd_high_capacity` must be correct.

Add debug output:

```c
snprintf(buf, sizeof(buf), "ccs=%d capacity=%lu blocks\r\n",
         sd_high_capacity, (unsigned long)sd_capacity_blocks);
hal_uart_puts(buf);
```

For SDHC/SDXC cards (`ccs=1`), read/write commands use **block addresses**. For SDSC (`ccs=0`), they use **byte addresses**. The current driver stores the CCS bit and uses it in `sdmmc_read_block()` / `sdmmc_write_block()`.

---

## 8. FatFs mount debugging

Once `disk_initialize(0)` succeeds and returns `0` (no `STA_NOINIT`), move to FatFs.

```text
saramOS> sd init
```

Possible results:

| Code | Name | Meaning |
|------|------|---------|
| 0 | FR_OK | Mounted |
| 1 | FR_DISK_ERR | Low-level read failed |
| 3 | FR_NOT_READY | `disk_initialize` failed |
| 12 | FR_NOT_ENABLED | Volume not mounted |
| 13 | FR_NO_FILESYSTEM | No FAT volume found |

For GPT-partitioned cards, `FF_LBA64` and `FF_FS_EXFAT` must be enabled in `ffconf.h`. The current project sets both to `1`.

To verify the partition is recognized:

1. Format an SD card with a single FAT32 partition using GPT.
2. Ensure the partition GUID is `EBD0A0A2-B9E5-4433-87C0-68B6B72699C7` (Microsoft Basic Data). This is the default for most tools when creating a FAT32 partition on GPT.
3. Re-run `sd init`.

If mount still fails with `FR_NO_FILESYSTEM`, add prints inside FatFs `find_volume()` or use raw sector reads to verify the boot sector is readable.

---

## 9. Minimal raw read test

From the CLI or a temporary function, read sector 0 and print the first bytes:

```c
uint8_t sector[512];
if (hal_sdmmc_read_blocks(0, sector, 1) == HAL_SDMMC_OK) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%02X %02X %02X %02X\r\n",
             sector[0], sector[1], sector[2], sector[3]);
    hal_uart_puts(buf);
}
```

For a valid FAT boot sector, bytes 510-511 should be `0x55 0xAA`.
For a GPT protective MBR, byte 450 (partition type) should be `0xEE`.

---

## 10. Checklist before asking for help

- [ ] PG2 reads `0` with card inserted
- [ ] `sd inspect` shows `POWER=0x03` and `CLKCR` non-zero
- [ ] PC8..PC11, PD2, PC12 pass pull-up/output/read tests
- [ ] CMD0 returns success (`CMDSENT` set)
- [ ] CMD8 returns success (`CMDREND` set, check pattern echoed)
- [ ] ACMD41 returns OCR with bit 31 set
- [ ] Capacity/CCS look reasonable
- [ ] Raw sector 0 read succeeds and contains `0x55 0xAA`
- [ ] Card is FAT32 on a GPT partition with MS Basic Data GUID

Collect the output of `sd inspect`, the CMD0/CMD8/ACMD41 step results, and the first 16 bytes of sector 0.

---

## SDMMC2 refactor status (2026-06-24)

The SDMMC HAL has been refactored from the board-mismatched SDMMC1 to the
on-board SDMMC2 peripheral:

- `src/hal/stm32f769i-disc1/hal_sdmmc.c` now drives SDMMC2.
- `src/hal/stm32f769i-disc1/hal_sd.c` and `include/hal/hal_sd.h` were removed.
- `include/hal/stm32f769i-disc1.h` gained SDMMC2 register definitions and the
  `RCC_APB2ENR_SDMMC2EN` / `RCC_APB2RSTR_SDMMC2RST` bits.

Board test result after the refactor:

- The OS boots and the CLI responds.
- `sd init` returns `sd: mount failed (3)` (`FR_NOT_READY`).
- `sd ls /` therefore returns `FR_NOT_ENABLED`.
- The exact failing step (CMD0/ACMD41/card detect/etc.) has not been narrowed
  down yet; this is scheduled for the next debug session.

Note: I-Cache and D-Cache were left disabled because enabling either without an
MPU region table that marks peripheral space as Device/Strongly-ordered causes
a HardFault early in boot on this board. Cache bring-up requires MPU setup and
is separate from the SDMMC peripheral debug.

