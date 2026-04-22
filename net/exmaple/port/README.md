# Project Binding Layer

This directory contains project-side binding code for reusable content from `rep/`.

## Scope

- Driver link IDs, GPIO mappings, bus instances, addresses, and timing constants
- Module assembly providers and project default configuration providers
- Thin wrappers that translate current board resources to reusable `rep` interfaces

## Non-scope

- Reusable module business logic
- Reusable driver core logic
- Product service orchestration
- System startup sequencing

## Layout Rule

`User/port` follows the same flat layout as `rep/example/port`: all binding source files live directly in this directory.

- Driver-layer bindings: `drvadc_port.*`, `drvgpio_port.*`, `drviic_port.*`, `drvspi_port.*`, `drvuart_port.*`
- Module bindings: `mpu6050_port.*`, `pca9535_port.*`, `tm1651_port.*`, `w25qxxx_port.*`
- RTOS binding: `rtos_port.*`
- Documentation: `README.md`, `MIGRATED_FROM_PROJECT_CPRBOOT.md`

Do not create nested folders under `User/port`. If logic can be reused by another project without changing board assumptions, move it back to `rep/`.