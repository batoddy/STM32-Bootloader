# STM32-Bootloader
A bootloader code for STM32 (H7A3x) microcontroller which transmits application codes via USART to microcontrollers flash.  
The bootloader can switch between 4 Application code slots. These slots can be used for versioning the main Application.
The flash addresses for STM32H7A3x microcontrollers:
Bootloader (APP_0) : 0x08000000
APP_1:  0x08040000  (sector 20)
APP_2:  0x08060000  (sector 30)
APP_1:  0x08080000  (sector 40)
APP_2:  0x080A0000  (sector 50)

The STM32H7A3x microcontrollers flash memorys banks are divided to 128 sectors which are 8Kb. 
!!! Reference Manual has to been read before writing to flash memory, most of the stm32 microcontrollers are divided to 8 sectors.

How this code works:
When the STM powers on, the bootloader code starts running. After 5 secs, stack pointer jumps to APP1 reset handler.  
If the user button pressed 2secs, bootloader enters the listening mode and waits for the start packet of the file upload from PC.
After that PC_file_transmitter.c code has to run and the file transmitting starts. Here is the data flow:

Bootloader <<=======    UPLOAD START FRAME    =========== PC
Bootloader ========= READY TO UPLOAD RESPONSE =========>> PC
Bootloader <<========      CHUNK 1 (DATA)     =========== PC
Bootloader =========        OK RESPONSE       =========>> PC
Bootloader <<========      CHUNK 2 (DATA)     =========== PC
Bootloader =========        OK RESPONSE       =========>> PC
Bootloader <<========      CHUNK 3 (DATA)     =========== PC
Bootloader =========        OK RESPONSE       =========>> PC
                                 .
                                 .
                                 .
                                 .
                                 .
Bootloader <<========      CHUNK N (DATA)     =========== PC
Bootloader =========        OK RESPONSE       =========>> PC
Bootloader <<========     UPLOAD END FRAME    =========== PC
Bootloader ========= ALL CHUNKS RECEIVED RPNS =========>> PC

                      FILE SENT COMPLETE !!!

