Input/Output Framework (UART Submodule)
======================

The UART Submodule provides functions for Input/Ouptu Framework to work with UART via stdio function.

The submodules should be registered by `iofw_uart_register()` function.
Additionally Implementation-specific parameters, like pin-assignment should be done either in `HAL_UART_MSPInit` function or by separate calls.
