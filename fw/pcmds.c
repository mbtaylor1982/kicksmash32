/*
 * This is free and unencumbered software released into the public domain.
 * See the LICENSE file for additional details.
 *
 * Designed by Chris Hooper in 2020.
 *
 * ---------------------------------------------------------------------
 *
 * Command implementations.
 */

#include "printf.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "board.h"
#include "main.h"
#include "cmdline.h"
#include "prom_access.h"
#include <stdbool.h>
#include "timer.h"
#include "uart.h"
#include "cmds.h"
#include "gpio.h"
#include "pcmds.h"
#include "adc.h"
#include "utils.h"
#include "usb.h"
#include "irq.h"
#include "kbrst.h"
#include "m29f160xt.h"

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/memorymap.h>
#define GPIOA_BASE GPIO_PORT_A_BASE
#define GPIOB_BASE GPIO_PORT_B_BASE
#define GPIOC_BASE GPIO_PORT_C_BASE
#define GPIOD_BASE GPIO_PORT_D_BASE
#define GPIOE_BASE GPIO_PORT_E_BASE

#ifndef SRAM_BASE
#define SRAM_BASE 0x20000000U
#endif

#if defined(STM32F4)
#define FLASH_BASE FLASH_MEM_INTERFACE_BASE
#endif

const char cmd_cpu_help[] =
"cpu regs - show CPU registers";

const char cmd_gpio_help[] =
"gpio [name=value/mode/?] - display or set GPIOs";

const char cmd_prom_help[] =
"prom bank <bits> <mode> - override A18 & A19 (\"7 0\" = force bank 0)\n"
"prom cmd <cmd> [<addr>] - send a 16-bit command to the EEPROM chip\n"
"prom id                 - report EEPROM chip vendor and id\n"
"prom disable            - disable and power off EEPROM\n"
"prom erase chip|<addr>  - erase EEPROM chip or 128K sector; <len> optional\n"
"prom log [<count>]      - show log of Amiga address accesses\n"
"prom mode 0|1|2         - set EEPROM access mode (0=32, 1=16lo, 2=16hi)\n"
"prom read <addr> <len>  - read binary data from EEPROM (to terminal)\n"
"prom status [clear]     - display or clear EEPROM status\n"
"prom temp               - show STM32 die temperature\n"
"prom verify             - verify PROM is connected\n"
"prom write <addr> <len> - write binary data to EEPROM (from terminal)";

const char cmd_reset_help[] =
"reset              - reset CPU\n"
"reset amiga [hold] - reset Amiga using KBRST (hold leaves it in reset)\n"
"reset dfu          - reset into DFU programming mode\n"
"reset usb          - reset and restart USB interface";

const char cmd_snoop_help[] =
"snoop        - capture and report ROM transactions\n"
"snoop addr   - hardware capture A0-A19\n"
"snoop lo     - hardware capture A0-A15 D0-D15\n"
"snoop hi     - hardware capture A0-A15 D16-D31";

const char cmd_usb_help[] =
"usb disable - reset and disable USB\n"
"usb regs    - display USB device registers\n"
"usb reset   - reset and restart USB device";

typedef struct {
    const char *const name;
    uintptr_t         addr;
} memmap_t;

static const memmap_t memmap[] = {
    { "ADC1",   ADC1_BASE },
    { "APB1",   PERIPH_BASE_APB1 },
    { "APB2",   PERIPH_BASE_APB2 },
#ifdef STM32F1
    { "AFIO",   AFIO_BASE },
    { "BKP",    BACKUP_REGS_BASE },
#endif
#if defined(STM32F103xE)
    { "BKP",    RTC_BKP_BASE },
#endif
    { "DAC",    DAC_BASE },
    { "DMA1",   DMA1_BASE },
    { "DMA2",   DMA2_BASE },
    { "EXTI",   EXTI_BASE },
    { "FLASH",  FLASH_BASE },
    { "FPEC",   FLASH_MEM_INTERFACE_BASE },
    { "GPIOA",  GPIOA_BASE },
    { "GPIOB",  GPIOB_BASE },
    { "GPIOC",  GPIOC_BASE },
    { "GPIOD",  GPIOD_BASE },
    { "GPIOE",  GPIOE_BASE },
    { "IWDG",   IWDG_BASE },
    { "PWR",    POWER_CONTROL_BASE },
    { "RCC",    RCC_BASE },
    { "RTC",    RTC_BASE },
    { "SCB",    SCB_BASE },
    { "SRAM",   SRAM_BASE },
    { "TIM1",   TIM1_BASE },
    { "TIM2",   TIM2_BASE },
    { "TIM3",   TIM3_BASE },
    { "TIM4",   TIM4_BASE },
    { "TIM5",   TIM5_BASE },
    { "TIM8",   TIM8_BASE },
    { "USART1", USART1_BASE },
    { "USART3", USART3_BASE },
    { "USB",    USB_PERIPH_BASE },
    { "WWDG",   WWDG_BASE },
};

static uint
time_check(const char *text, uint diff, uint min, uint max)
{
    int errs = 0;
    if ((min <= diff) && (max >= diff)) {
        printf("PASS: ");
    } else {
        printf("FAIL: ");
        errs++;
    }
    printf("%-24s %u usec\n", text, diff);
    return (errs);
}

static rc_t
timer_test(void)
{
    uint64_t start;
    uint64_t diff;
    uint     errs = 0;

    start = timer_tick_get();
    timer_delay_ticks(0);
    diff = timer_tick_to_usec(timer_tick_get() - start);
    errs += time_check("timer_delay_ticks(0)", (uint)diff, 0, 5);

    start = timer_tick_get();
    timer_delay_ticks(100);
    diff = timer_tick_to_usec(timer_tick_get() - start);
    errs += time_check("timer_delay_ticks(100)", (uint)diff, 2, 5);

    start = timer_tick_get();
    timer_delay_usec(1);
    diff = timer_tick_to_usec(timer_tick_get() - start);
    errs += time_check("timer_delay_usec(1)", (uint)diff, 1, 5);

    start = timer_tick_get();
    timer_delay_usec(10);
    diff = timer_tick_to_usec(timer_tick_get() - start);
    errs += time_check("timer_delay_usec(10)", (uint)diff, 10, 15);

    start = timer_tick_get();
    timer_delay_usec(1000);
    diff = timer_tick_to_usec(timer_tick_get() - start);
    errs += time_check("timer_delay_usec(1000)", (uint)diff, 1000, 1005);

    start = timer_tick_get();
    timer_delay_msec(1);
    diff = timer_tick_to_usec(timer_tick_get() - start);
    errs += time_check("timer_delay_msec(1)", (uint)diff, 1000, 1005);

    start = timer_tick_get();
    timer_delay_msec(10);
    diff = timer_tick_to_usec(timer_tick_get() - start);
    errs += time_check("timer_delay_msec(10)", (uint)diff, 10000, 10007);

    start = timer_tick_get();
    timer_delay_msec(1000);
    diff = timer_tick_to_usec(timer_tick_get() - start);
    errs += time_check("timer_delay_msec(1000)", (uint)diff, 1000000, 1000007);

    // XXX: Replace one second above with RTC tick?

    if (errs > 0)
        return (RC_FAILURE);
    return (RC_SUCCESS);
}

static rc_t
timer_watch(void)
{
    bool_t   fail = FALSE;
    uint64_t last = timer_tick_get();
    uint64_t now;

    while (1) {
        now = timer_tick_get();
        if (last >= now) {
            printf("\nLast=%llx now=%llx Current=%012llx",
                   (long long) last, (long long) now, timer_tick_get());
        } else {
            if ((last >> 32) != (now >> 32))
                putchar('.');
            last = now;
        }
        if (input_break_pending()) {
            printf("^C\n");
            break;
        }
    }
    return ((fail == TRUE) ? RC_FAILURE : RC_SUCCESS);
}

rc_t
cmd_time(int argc, char * const *argv)
{
    rc_t rc;

    if (argc <= 1)
        return (RC_USER_HELP);

    if (strncmp(argv[1], "cmd", 1) == 0) {
        uint64_t time_start;
        uint64_t time_diff;

        if (argc <= 2) {
            printf("error: time cmd requires command to execute\n");
            return (RC_USER_HELP);
        }
        time_start = timer_tick_get();
        rc = cmd_exec_argv(argc - 2, argv + 2);
        time_diff = timer_tick_get() - time_start;
        printf("%lld us\n", timer_tick_to_usec(time_diff));
        if (rc == RC_USER_HELP)
            rc = RC_FAILURE;
    } else if (strncmp(argv[1], "now", 1) == 0) {
        uint64_t now = timer_tick_get();
        printf("tick=0x%llx uptime=%lld usec\n", now, timer_tick_to_usec(now));
        rc = RC_SUCCESS;
    } else if (strncmp(argv[1], "watch", 1) == 0) {
        rc = timer_watch();
    } else if (strncmp(argv[1], "test", 1) == 0) {
        rc = timer_test();
    } else {
        printf("Unknown argument %s\n", argv[1]);
        return (RC_USER_HELP);
    }
    return (rc);
}

static rc_t
cmd_prom_temp(int argc, char * const *argv)
{
    adc_show_sensors();
    return (RC_SUCCESS);
}

rc_t
cmd_prom(int argc, char * const *argv)
{
    enum {
        OP_NONE,
        OP_READ,
        OP_WRITE,
        OP_ERASE_CHIP,
        OP_ERASE_SECTOR,
    } op_mode = OP_NONE;
    rc_t        rc;
    const char *arg = argv[0];
    const char *cmd_prom = "prom";
    uint32_t    addr = 0;
    uint32_t    len = 0;

    while (*arg != '\0') {
        if (*arg != *cmd_prom)
            break;
        arg++;
        cmd_prom++;
    }
    if (*arg == '\0') {
        argv++;
        argc--;
        if (argc < 1) {
            printf("error: prom command requires operation to perform\n");
            return (RC_USER_HELP);
        }
        arg = argv[0];
    }
    if (strcmp("bank", arg) == 0) {
        uint bits;
        uint mode;
        // XXX: The "prom bank" command will be changed significantly
        //      in the near future.
        if ((argc < 2) || (argc > 3)) {
            printf("prom bank <bits> <mode>\n"
                   "   bit 0   1=Drive A17     mode 0 = Assign new override\n"
                   "   bit 1   1=Drive A18     mode 1 = Temp disable override\n"
                   "   bit 2   1=Drive A19     mode 2 = Restore override\n"
                   "   bit 4   A17\n"
                   "   bit 5   A18\n"
                   "   bit 6   A19\n");
            return (RC_FAILURE);
        }
        rc = parse_value(argv[1], (uint8_t *) &bits, 2);
        if (rc != RC_SUCCESS)
            return (rc);
        rc = parse_value(argv[2], (uint8_t *) &mode, 4);
        if (rc != RC_SUCCESS)
            return (rc);
        ee_address_override(bits, mode);
        return (RC_SUCCESS);
    } else if (strcmp("cmd", arg) == 0) {
        uint32_t cmd;
        if ((argc < 2) || (argc > 3)) {
            printf("error: prom cmd <cmd> [<addr>]\n");
            return (RC_USER_HELP);
        }
        rc = parse_value(argv[1], (uint8_t *) &cmd, 4);
        if (rc != RC_SUCCESS)
            return (rc);

        if (argc == 3) {
            rc = parse_value(argv[2], (uint8_t *) &addr, 4);
            if (rc != RC_SUCCESS)
                return (rc);
        } else {
            addr = 0x00555;  // Default address for commands
        }

        prom_cmd(addr, cmd);
        return (RC_SUCCESS);
    } else if (strcmp("disable", arg) == 0) {
        prom_disable();
        return (RC_SUCCESS);
    } else if (strncmp(arg, "erase", 2) == 0) {
        if (argc < 2) {
            printf("error: prom erase requires either chip or "
                   "<addr> argument\n");
            return (RC_USER_HELP);
        }
        if (strcmp(argv[1], "chip") == 0) {
            op_mode = OP_ERASE_CHIP;
            argc--;
            argv++;
        } else {
            op_mode = OP_ERASE_SECTOR;
        }
    } else if (strcmp("id", arg) == 0) {
        return (prom_id());
    } else if (strcmp("log", arg) == 0) {
        uint max = 10;
        rc = parse_value(argv[1], (uint8_t *) &max, 2);
        if (rc != RC_SUCCESS)
            return (rc);
        if (max == 0)
            max = 10;
        return (address_log_replay(max));
    } else if (strcmp("mode", arg) == 0) {
        if ((argc > 1) && (argv[1][0] >= '0') && (argv[1][0] <= '3')) {
            uint mode = argv[1][0] - '0';
            prom_mode(mode);
        } else {
            prom_show_mode();
        }
        return (RC_SUCCESS);
    } else if (strcmp("read", arg) == 0) {
        op_mode = OP_READ;
    } else if (strcmp("status", arg) == 0) {
        if ((argc > 1) && (strcmp("clear", argv[1]) == 0))
            prom_status_clear();
        else
            prom_status();
        return (RC_SUCCESS);
    } else if (strcmp("temp", arg) == 0) {
        return (cmd_prom_temp(argc - 1, argv + 1));
    } else if (strcmp("verify", arg) == 0) {
        int verbose = 1;
        if ((argc > 1) && (argv[1][0] == 'v'))
            verbose++;
        return (prom_verify(verbose));
    } else if (strcmp("write", arg) == 0) {
        op_mode = OP_WRITE;
    } else {
        printf("error: unknown prom operation %s\n", arg);
        return (RC_USER_HELP);
    }

    if (argc > 1) {
        rc = parse_value(argv[1], (uint8_t *) &addr, 4);
        if (rc != RC_SUCCESS)
            return (rc);
    }

    if (argc > 2) {
        rc = parse_value(argv[2], (uint8_t *) &len, 4);
        if (rc != RC_SUCCESS)
            return (rc);
    }

    switch (op_mode) {
        case OP_READ:
            if (argc != 3) {
                printf("error: prom %s requires <addr> and <len>\n", arg);
                return (RC_USER_HELP);
            }
            rc = prom_read_binary(addr, len);
            break;
        case OP_WRITE:
            if (argc != 3) {
                printf("error: prom %s requires <addr> and <len>\n", arg);
                return (RC_USER_HELP);
            }
            rc = prom_write_binary(addr, len);
            break;
        case OP_ERASE_CHIP:
            printf("Chip erase\n");
            if (argc != 1) {
                printf("error: prom erase chip does not have arguments\n");
                return (RC_USER_HELP);
            }
            rc = prom_erase(ERASE_MODE_CHIP, 0, 0);
            break;
        case OP_ERASE_SECTOR:
            printf("Sector erase %lx", addr);
            if (len > 0)
                printf(" len %lx", len);
            printf("\n");
            if ((argc < 2) || (argc > 3)) {
                printf("error: prom erase sector requires <addr> and "
                       "allows optional <len>\n");
                return (RC_USER_HELP);
            }
            rc = prom_erase(ERASE_MODE_SECTOR, addr, len);
            break;
        default:
            printf("BUG: op_mode\n");
            return (RC_FAILURE);
    }

    if (rc != 0)
        printf("FAILURE %d\n", rc);
    return (rc);
}

rc_t
cmd_map(int argc, char * const *argv)
{
    uint third = (ARRAY_SIZE(memmap) + 2) / 3;
    uint ent;
    for (ent = 0; ent < third; ent++) {
        printf("    %-6s %08x", memmap[ent].name, memmap[ent].addr);
        if (ent + third < ARRAY_SIZE(memmap))
            printf("    %-6s %08x",
                   memmap[ent + third].name, memmap[ent + third].addr);
        if (ent + third * 2 < ARRAY_SIZE(memmap))
            printf("    %-6s %08x",
                   memmap[ent + third * 2].name, memmap[ent + third * 2].addr);
        printf("\n");
    }
    return (RC_SUCCESS);
}

rc_t
cmd_reset(int argc, char * const *argv)
{
    if (argc < 2) {
        printf("Resetting...\n");
        uart_flush();
        timer_delay_msec(1);
        reset_cpu();
        return (RC_FAILURE);
    } else if (strcmp(argv[1], "dfu") == 0) {
        printf("Resetting to DFU...\n");
        uart_flush();
        usb_shutdown();
        usb_signal_reset_to_host(1);
        timer_delay_msec(30);
        reset_dfu();
        return (RC_SUCCESS);
    } else if (strcmp(argv[1], "usb") == 0) {
        timer_delay_msec(1);
        usb_shutdown();
        usb_signal_reset_to_host(1);
        usb_startup();
        return (RC_SUCCESS);
    } else if (strcmp(argv[1], "amiga") == 0) {
        uint hold = 0;
        uint longreset = 0;
        while (argc > 2) {
            if (strcmp(argv[2], "hold") == 0)
                hold = 1;
            else if (strcmp(argv[2], "long") == 0)
                longreset = 1;
            else
                printf("Invalid reset amiga \"%s\"\n", argv[2]);
            argc--;
            argv++;
        }
        kbrst_amiga(hold, longreset);
        if (hold)
            printf("Holding Amiga in reset\n");
        else
            printf("Resetting Amiga\n");
        return (RC_SUCCESS);
    } else {
        printf("Unknown argument %s\n", argv[1]);
        return (RC_USER_HELP);
    }
}

rc_t
cmd_cpu(int argc, char * const *argv)
{
    if (argc < 2)
        return (RC_USER_HELP);
    if (strncmp(argv[1], "regs", 1) == 0) {
        fault_show_regs(NULL);
    } else {
        printf("Unknown argument %s\n", argv[1]);
        return (RC_USER_HELP);
    }
    return (RC_SUCCESS);
}

rc_t
cmd_usb(int argc, char * const *argv)
{
    if (argc < 2)
        return (RC_USER_HELP);
    if (strncmp(argv[1], "disable", 1) == 0) {
        timer_delay_msec(1);
        usb_shutdown();
        usb_signal_reset_to_host(0);
        return (RC_SUCCESS);
    } else if (strncmp(argv[1], "regs", 3) == 0) {
        usb_show_regs();
    } else if (strcmp(argv[1], "reset") == 0) {
        timer_delay_msec(1);
        usb_shutdown();
        usb_signal_reset_to_host(1);
        usb_startup();
        return (RC_SUCCESS);
    } else {
        printf("Unknown argument %s\n", argv[1]);
        return (RC_USER_HELP);
    }
    return (RC_SUCCESS);
}

rc_t
cmd_gpio(int argc, char * const *argv)
{
    int arg;
    if (argc < 2) {
        gpio_show(-1, -1);
        return (RC_SUCCESS);
    }

    for (arg = 1; arg < argc; arg++) {
        const char *ptr = argv[arg];
        int port = -1;
        int pin = -1;
        const char *assign = NULL;
        if ((*ptr == 'p') || (*ptr == 'P'))
            ptr++;

        /* Find port, if specified */
        if ((*ptr >= 'a') && (*ptr <= 'f'))
            port = *(ptr++) - 'a';
        else if ((*ptr >= 'A') && (*ptr <= 'F'))
            port = *(ptr++) - 'A';

        /* Find pin, if specified */
        if ((*ptr >= '0') && (*ptr <= '9')) {
            pin = *(ptr++) - '0';
            if ((*ptr >= '0') && (*ptr <= '9'))
                pin = pin * 10 + *(ptr++) - '0';
        }

        if (*ptr == '=') {
            assign = ptr + 1;
            ptr = "";
            if ((port == -1) || (pin == -1)) {
                printf("You must specify the GPIO to assign: %s\n", argv[arg]);
                return (RC_BAD_PARAM);
            }
        }
        if (((port == -1) && (pin == -1)) || (pin > 15) || (*ptr != '\0')) {
            printf("Invalid argument %s\n", argv[arg]);
            return (RC_BAD_PARAM);
        }

        if (assign != NULL)
            gpio_assign(port, pin, assign);
        else
            gpio_show(port, pin);
    }

    return (RC_SUCCESS);
}

rc_t
cmd_snoop(int argc, char * const *argv)
{
    uint mode = CAPTURE_SW;
    if (argc > 1) {
        if (strcmp(argv[1], "addr") == 0) {
            mode = CAPTURE_ADDR;
        } else if (strncmp(argv[1], "low", 2) == 0) {
            mode = CAPTURE_DATA_LO;
        } else if (strncmp(argv[1], "high", 2) == 0) {
            mode = CAPTURE_DATA_HI;
        } else {
            printf("snoop \"%s\" unknown argument\n", argv[1]);
            return (RC_USER_HELP);
        }
    }
    prom_snoop(mode);

    return (RC_SUCCESS);
}
