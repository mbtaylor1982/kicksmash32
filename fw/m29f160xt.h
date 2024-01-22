/*
 * This is free and unencumbered software released into the public domain.
 * See the LICENSE file for additional details.
 *
 * Designed by Chris Hooper in 2020.
 *
 * ---------------------------------------------------------------------
 *
 * MX29F1615-specific code (read, write, erase, status, etc).
 */

#ifndef __MX29F1615_H
#define __MX29F1615_H

void     ee_enable(void);
void     ee_disable(void);
int      ee_read(uint32_t addr, void *data, uint count);
int      ee_write(uint32_t addr, void *data, uint count);
void     ee_id(uint32_t *part1, uint32_t *part2);
void     ee_init(void);
void     ee_read_mode(void);
int      ee_erase(uint mode, uint32_t addr, uint32_t len, int verbose);
uint16_t ee_status_read(char *status, uint status_len);
void     ee_status_clear(void);
void     ee_cmd(uint32_t addr, uint32_t cmd);
void     ee_poll(void);
void     ee_snoop(uint mode);
int      ee_verify(int verbose);
void     ee_address_override(uint8_t bits, uint override);
const char *ee_id_string(uint32_t id);
const char *ee_vendor_string(uint32_t id);
int      address_log_replay(uint max);

/* Internal functions for manipulating GPIOs */
void     oe_output(uint value);
void     oe_output_enable(void);
void     oe_output_disable(void);
void     data_output_disable(void);
void     data_output(uint32_t data);
uint32_t data_input(void);

#define MX_ERASE_MODE_CHIP   0
#define MX_ERASE_MODE_SECTOR 1

#define EE_MODE_32      0  // 32-bit flash
#define EE_MODE_16_LOW  1  // 16-bit flash low device (bits 0-15)
#define EE_MODE_16_HIGH 2  // 16-bit flash high device (bits 16-31)

extern uint ee_mode;
extern uint ee_default_mode;

#endif /* __MX29F1615_H */